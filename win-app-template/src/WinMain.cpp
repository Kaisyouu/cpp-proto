// Main entry with robust crash/exit logging per README
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "utils/Environment.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <dbghelp.h>
#endif

namespace
{
// Simple helper to log and also print to stderr as a fallback
void log_reason(const std::string& msg)
{
    try
    {
        spdlog::critical(msg);
        spdlog::dump_backtrace();
        if (auto lg = spdlog::default_logger())
            lg->flush();
    }
    catch (...)
    {
        std::cerr << msg << std::endl;
    }
}

#ifdef _WIN32
// Translate SEH to C++ exceptions is optional; we mainly capture unhandled filter
LONG WINAPI UnhandledExceptionLogger(EXCEPTION_POINTERS* ep)
{
    auto code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0u;
    char buf[128]{};
    snprintf(buf, sizeof(buf), "Unhandled SEH exception: 0x%08X", code);
    log_reason(buf);
    return EXCEPTION_EXECUTE_HANDLER;  // allow process to terminate after logging
}
#endif

void on_signal(int sig)
{
    log_reason(std::string("Caught signal ") + std::to_string(sig));
    // Let default handler run after logging
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}
}  // namespace

static int app_run(int /*argc*/, char** /*argv*/)
{
    auto& env = Environment::getInstance("./config.json");
    env.init_logger_dump();
    env.register_scheduled_exit();

#ifdef _WIN32
    // Update window/console title if provided
    SetConsoleTitleA(env.title_name().c_str());
#endif

    spdlog::info("app init ok (name={}, log_dir={})", env.app_name(), env.log_dir());

    /* biz code begin */
    while (1)
    {
        spdlog::info("app running...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    /* biz code end */

    return 0;
}

#ifdef _WIN32
// Use console subsystem (main), but keep WinMain for future GUI switch if needed.
// For now we define only main to avoid duplicate entry on console builds.
int main(int argc, char** argv)
{
    // Ensure logger available as early as possible
    Environment::getInstance("./config.json").init_logger_dump();

    // Crash/exit hooks
    std::set_terminate([]()
                       {
        try {
            auto eptr = std::current_exception();
            if (eptr) std::rethrow_exception(eptr);
        } catch (const std::exception& e) {
            log_reason(std::string("std::terminate: ") + e.what());
        } catch (...) {
            log_reason("std::terminate: unknown exception");
        }
        std::_Exit(1); });

    std::signal(SIGABRT, on_signal);
    std::signal(SIGSEGV, on_signal);
    std::signal(SIGINT, on_signal);

    SetUnhandledExceptionFilter(&UnhandledExceptionLogger);

    std::atexit([]()
                { spdlog::info("program exiting via atexit"); spdlog::shutdown(); });

    try
    {
        int rc = app_run(argc, argv);
        spdlog::info("program exit normally, rc={}", rc);
        return rc;
    }
    catch (const std::exception& e)
    {
        log_reason(std::string("unhandled std::exception: ") + e.what());
    }
    catch (...)
    {
        log_reason("unhandled unknown exception");
    }
    return 1;
}
#else
int main(int argc, char** argv)
{
    Environment::getInstance("./config.json").init_logger_dump();
    std::signal(SIGABRT, on_signal);
    std::signal(SIGSEGV, on_signal);
    std::signal(SIGINT, on_signal);
    std::atexit([]()
                { spdlog::info("program exiting via atexit"); spdlog::shutdown(); });

    try
    {
        return app_run(argc, argv);
    }
    catch (const std::exception& e)
    {
        log_reason(std::string("unhandled std::exception: ") + e.what());
    }
    catch (...)
    {
        log_reason("unhandled unknown exception");
    }
    return 1;
}
#endif
