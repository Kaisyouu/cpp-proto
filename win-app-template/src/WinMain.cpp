// Main entry with robust crash/exit logging per README
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "../test/ExceptionTrigger.hpp"
#include "utils/Environment.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <dbghelp.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#endif

namespace
{
// Forward declaration so Windows minidump helpers can call it
static void log_reason(const std::string& msg);
#ifdef _WIN32
// --- Minidump helpers (Windows) ---
static std::string make_dump_path()
{
    namespace fs = std::filesystem;
    const auto& env = Environment::getInstance();
    std::string dir = env.log_dir();
    std::error_code ec;
    fs::create_directories(dir, ec);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char ts[32]{};
    snprintf(ts, sizeof(ts), "%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    unsigned long pid = static_cast<unsigned long>(GetCurrentProcessId());
    std::string base = env.app_name() + std::string("_") + ts + std::string("_") + std::to_string(pid);
    return (fs::path(dir) / (base + ".dmp")).string();
}

static bool write_minidump(EXCEPTION_POINTERS* ep) noexcept
{
    try
    {
        const std::string dumpPath = make_dump_path();
        HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            log_reason(std::string("MiniDump: failed to create ") + dumpPath);
            return false;
        }
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MINIDUMP_TYPE type = (MINIDUMP_TYPE) (MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules | MiniDumpWithFullMemoryInfo | MiniDumpWithIndirectlyReferencedMemory);
        BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, type, ep ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(hFile);
        if (ok)
        {
            spdlog::error("minidump written: {}", dumpPath);
            return true;
        }
        spdlog::error("minidump write failed: {}", dumpPath);
        return false;
    }
    catch (...)
    {
        return false;
    }
}
#endif

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
    write_minidump(ep);
    // Terminate immediately to avoid running into later CRT/STL assertions (e.g., join breakpoint)
    try
    {
        if (auto lg = spdlog::default_logger())
            lg->flush();
    }
    catch (...)
    {
    }
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(code ? code : 1));
    return EXCEPTION_EXECUTE_HANDLER;  // unreachable
}
#endif

void on_signal(int sig)
{
    log_reason(std::string("Caught signal ") + std::to_string(sig));
#ifdef _WIN32
    // Write minidump and suppress Windows error UI: exit immediately after logging
    write_minidump(nullptr);
    spdlog::shutdown();
    std::_Exit(128 + sig);
#else
    // Let default handler run after logging on POSIX
    std::signal(sig, SIG_DFL);
    std::raise(sig);
#endif
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

    std::thread testThread(ExceptionTestThread);
    testThread.join();

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

    // Avoid Windows runtime popups (abort/assert/GPF)
#ifdef _WIN32
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#ifdef _MSC_VER
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
    // Route CRT reports to stderr and disable dialog in Debug CRT
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    // Invalid parameter & purecall handlers log and exit without UI
    _set_invalid_parameter_handler([](const wchar_t* expr, const wchar_t* func, const wchar_t* file, unsigned int line, uintptr_t)
                                   {
        try {
            char buf[512]{};
            snprintf(buf, sizeof(buf), "invalid parameter: %ls, func=%ls, file=%ls:%u", expr ? expr : L"?", func ? func : L"?", file ? file : L"?", line);
            log_reason(buf);
        } catch (...) {}
        write_minidump(nullptr);
        std::_Exit(3); });
    _set_purecall_handler([]()
                          {
        log_reason("pure virtual function call");
        write_minidump(nullptr);
        std::_Exit(4); });
#endif  // _MSC_VER
#endif  // _WIN32

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
