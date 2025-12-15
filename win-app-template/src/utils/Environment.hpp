/**
 * @file: Environment.hpp
 * @brief: 简单的环境配置单例：负责读取 config.json，初始化 spdlog，提供常用字段。
 */
#pragma once

#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>

#include "Utils.hpp"

// spdlog（header-only 已放在 3rd）
#include <spdlog/async.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/spdlog.h>

class Environment {
public:
    static Environment& getInstance(const std::string& configPath = "")
    {
        static Environment instance(configPath);
        return instance;
    }

    // 读取后的字段（带默认值，保证鲁棒）
    std::string app_name() const { return get_string({"app", "app_name"}, "app"); }
    std::string title_name() const { return get_string({"app", "title_name"}, "Win App"); }
    std::string log_mode() const { return get_string({"app", "mode"}, "debug"); }

    // 返回替换日期占位符后的绝对日志目录路径
    std::string log_dir() const
    {
        std::string base = get_string({"app", "log_path"}, "./logs/%Y%m%d");
        std::string replaced = replace_time_wildcard(base);
        return std::filesystem::absolute(replaced).string();
    }

    // HHMM（整数）返回；无或非法则返回 -1
    int crontab_stop_hhmm() const
    {
        try {
            if (!config_.contains("crontab")) return -1;
            const auto& c = config_.at("crontab");
            if (!c.contains("stop")) return -1;
            int v = c.at("stop").get<int>();
            if (v < 0 || v > 2359 || v % 100 >= 60) return -1;
            return v;
        } catch (...) {
            return -1;
        }
    }

    // 初始化异步日志到 log_dir()/app_name().log，幂等
    void init_logger_dump()
    {
        namespace fs = std::filesystem;
        std::call_once(logger_once_, [&]() {
            const std::string dir = log_dir();
            const std::string file = (fs::path(dir) / (app_name() + ".log")).string();

            // 创建目录
            std::error_code ec;
            fs::create_directories(dir, ec);
            if (ec) {
                // 保底打印到 stderr，避免完全无感知
                fprintf(stderr, "[Environment] create_directories failed: %s\n", ec.message().c_str());
            }

            try {
                spdlog::init_thread_pool(8192, 1);
                auto logger = spdlog::daily_logger_mt<spdlog::async_factory>(app_name(), file, 0, 0);
                spdlog::set_default_logger(logger);
                spdlog::flush_every(std::chrono::seconds(1));

                const std::string mode = log_mode();
                if (mode == "trace") spdlog::set_level(spdlog::level::trace);
                else if (mode == "debug") spdlog::set_level(spdlog::level::debug);
                else if (mode == "info") spdlog::set_level(spdlog::level::info);
                else if (mode == "warn") spdlog::set_level(spdlog::level::warn);
                else if (mode == "err" || mode == "error") spdlog::set_level(spdlog::level::err);
                else spdlog::set_level(spdlog::level::info);

                spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
                spdlog::enable_backtrace(128);
            } catch (const spdlog::spdlog_ex& e) {
                fprintf(stderr, "[Environment] spdlog init failed: %s\n", e.what());
            }
        });
    }

    // 注册定时退出（HHMM）。若未配置忽略。
    void register_scheduled_exit()
    {
        const int hhmm = crontab_stop_hhmm();
        if (hhmm < 0) return;

        // 只注册一次
        static std::once_flag once;
        std::call_once(once, [=]() {
            std::thread([hhmm]() {
                auto compute_due = [hhmm]() -> std::chrono::system_clock::time_point {
                    using clock = std::chrono::system_clock;
                    auto now = clock::now();
                    std::time_t tt = clock::to_time_t(now);
                    std::tm tm = *std::localtime(&tt);
                    tm.tm_hour = hhmm / 100;
                    tm.tm_min = hhmm % 100;
                    tm.tm_sec = 0;
                    auto target = clock::from_time_t(std::mktime(&tm));
                    if (target <= now) target += std::chrono::hours(24);
                    return target;
                };

                auto due = compute_due();
                std::this_thread::sleep_until(due);
                spdlog::warn("crontab.stop reached, exiting at HHMM=%d", hhmm);
                spdlog::dump_backtrace();
                spdlog::shutdown();
                std::exit(0);
            }).detach();
        });
    }

    // 提供原始配置（只读）
    const nlohmann::json& config() const { return config_; }

private:
    Environment(const std::string& configPath = "")
    {
        if (!configPath.empty()) config_path_ = configPath;
        load_config_no_throw();
    }

    ~Environment() = default;

    std::string get_string(std::initializer_list<std::string> keys, const std::string& def) const
    {
        try {
            const nlohmann::json* cur = &config_;
            for (const auto& k : keys) cur = &cur->at(k);
            if (cur->is_string()) return cur->get<std::string>();
            return def;
        } catch (...) {
            return def;
        }
    }

    void load_config_no_throw()
    {
        std::ifstream in(std::filesystem::absolute(config_path_));
        if (!in.is_open()) {
            // 默认配置，保证日志可用
            config_ = nlohmann::json{
                {"app",
                 {{"app_name", "app"}, {"title_name", "Win App"}, {"log_path", "./logs/%Y%m%d"}, {"mode", "debug"}}},
                {"crontab", {{"stop", -1}}},
            };
            return;
        }
        try {
            in >> config_;
        } catch (...) {
            // 解析错误则回退默认
            config_ = nlohmann::json{
                {"app",
                 {{"app_name", "app"}, {"title_name", "Win App"}, {"log_path", "./logs/%Y%m%d"}, {"mode", "debug"}}},
                {"crontab", {{"stop", -1}}},
            };
        }
    }

    nlohmann::json config_{};
    std::string config_path_ = "./config.json";
    std::once_flag logger_once_{};

    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
};
