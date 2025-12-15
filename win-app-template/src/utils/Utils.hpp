/**
 * @file Utils.hpp
 * @author huaisy
 * @brief 通用inline函数 通常不会抛出异常
 * @date 2025-12-15
 */
#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif


/**
 * @brief 替换字符串中的时间表达式
 * 
 * @param path 
 * @details %Y %m %d %H %M %S 毫秒:%f
 * @return std::string 替换后的字符
 */
inline std::string replace_time_wildcard(const std::string& path) noexcept
{
    try {

        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&now_time_t);
        std::ostringstream oss;
        oss << std::put_time(&tm, path.c_str());
        std::string result = oss.str();
        size_t pos = result.find("%f");
        if (pos != std::string::npos)
        {
            oss.clear();
            oss.str("");
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            oss << std::setfill('0') << std::setw(3) << millis.count();
            result.replace(pos, 2, oss.str());
        }
        return result;
    } catch (const std::exception& ex)
    {
        std::cerr << "[ERROR] Exception at " << __FUNCTION__ << ", reason: " << ex.what() << std::endl;
        return path;
    }
}