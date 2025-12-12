// Implementations for watcher thread entrypoints.

#include "FileWatchers.hpp"

#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <cctype>

#include <windows.h>

#include "TailReaderWin.hpp"
#include "rapidcsv.h"
#include "FileWatchers.hpp"

namespace {
    // Join dir and filename with a single backslash if needed
    std::string joinPath(const std::string& dir, const std::string& name)
    {
        if (dir.empty()) return name;
        char last = dir.back();
        if (last == '\\' || last == '/') return dir + name;
        return dir + "\\" + name;
    }

    // Find newest file (by LastWriteTime) in dir whose name starts with prefix and ends with .csv
    // Returns empty path if none found.
    std::string findLatestCsvByPrefix(const std::string& dir, const std::string& prefix)
    {
        std::string pattern = joinPath(dir, prefix + "*");
        WIN32_FIND_DATAA ffd{};
        HANDLE hFind = ::FindFirstFileA(pattern.c_str(), &ffd);
        if (hFind == INVALID_HANDLE_VALUE) {
            return std::string();
        }

        auto toU64 = [](const FILETIME& ft) -> unsigned long long {
            ULARGE_INTEGER uli{}; uli.HighPart = ft.dwHighDateTime; uli.LowPart = ft.dwLowDateTime; return uli.QuadPart;
        };

        unsigned long long bestTime = 0;
        std::string bestName;
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            // must end with .csv (case-insensitive)
            std::string name = ffd.cFileName;
            auto endsCsv = [](const std::string& s) {
                if (s.size() < 4) return false;
                std::string ext = s.substr(s.size() - 4);
                for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
                return ext == ".csv";
            };
            if (!endsCsv(name)) continue;

            unsigned long long t = toU64(ffd.ftLastWriteTime);
            if (t > bestTime || (t == bestTime && name > bestName)) {
                bestTime = t;
                bestName = name;
            }
        } while (::FindNextFileA(hFind, &ffd));
        ::FindClose(hFind);

        if (bestName.empty()) return std::string();
        return joinPath(dir, bestName);
    }

    // Read whole file with shared access into string buffer.
    bool readFileAllShared(const std::string& path, std::string& out)
    {
        HANDLE h = ::CreateFileA(path.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER size{};
        if (!::GetFileSizeEx(h, &size)) { ::CloseHandle(h); return false; }
        if (size.QuadPart < 0) { ::CloseHandle(h); return false; }
        unsigned long long total = static_cast<unsigned long long>(size.QuadPart);
        out.clear();
        out.reserve(static_cast<size_t>(total));

        const DWORD kBuf = 64 * 1024;
        std::string buf; buf.resize(kBuf);
        DWORD readBytes = 0;
        for (;;) {
            if (!::ReadFile(h, &buf[0], kBuf, &readBytes, nullptr)) { ::CloseHandle(h); return false; }
            if (readBytes == 0) break;
            out.append(buf.data(), readBytes);
        }
        ::CloseHandle(h);

        // Strip BOM if present
        if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF &&
            static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF) {
            out.erase(0, 3);
        }
        return true;
    }
}

namespace FileWatchers {

static void emit_rows(const std::string& sourcePath, rapidcsv::Document& doc, RowHandler& onRow)
{
    const auto nRows = doc.GetRowCount();
    for (size_t i = 0; i < nRows; ++i) {
        auto rowVals = doc.GetRow<std::string>(i);
        onRow(sourcePath, rowVals);
    }
}

static DWORD to_ms(int seconds) { return seconds <= 0 ? 0 : static_cast<DWORD>(seconds) * 1000; }

std::thread CreateWatchAppend(const std::string& filePath, RowHandler onRow, int waitSeconds)
{
    return std::thread([filePath, onRow = std::move(onRow), waitMs = to_ms(waitSeconds)]() mutable {
        TailReaderWin tail(filePath, /*skipHeader=*/true);
        for (;;) {
            try {
                if (!tail.Open()) { ::Sleep(500); continue; }
                auto lines = tail.ReadAppendedLines();
                if (!lines.empty()) {
                    std::stringstream ss;
                    for (auto& l : lines) ss << l << "\n"; // Reconstitute a proper CSV chunk
                    rapidcsv::Document doc(ss, rapidcsv::LabelParams(-1, -1));
                    emit_rows(filePath, doc, onRow);
                }
                ::Sleep(waitMs);
            } catch (const std::exception& e) {
                std::cerr << "错误: " << e.what() << std::endl;
                ::Sleep(1000);
            }
        }
    });
}

std::thread CreateWatchNew(const std::string& dir, const std::string& prefix, RowHandler onRow, int waitSeconds)
{
    return std::thread([dir, prefix, onRow = std::move(onRow), waitMs = to_ms(waitSeconds)]() mutable {
        std::string lastPath; unsigned long long lastTime = 0;
        for (;;) {
            try {
                std::string latest = findLatestCsvByPrefix(dir, prefix);
                if (!latest.empty()) {
                    // Re-read if path changed or timestamp increased
                    WIN32_FILE_ATTRIBUTE_DATA fad{}; unsigned long long t = 0;
                    if (::GetFileAttributesExA(latest.c_str(), GetFileExInfoStandard, &fad)) {
                        ULARGE_INTEGER uli{}; uli.HighPart = fad.ftLastWriteTime.dwHighDateTime; uli.LowPart = fad.ftLastWriteTime.dwLowDateTime; t = uli.QuadPart;
                    }
                    if (latest != lastPath || t > lastTime) {
                        std::string content;
                        if (readFileAllShared(latest, content)) {
                            std::stringstream ss(content);
                            rapidcsv::Document doc(ss, rapidcsv::LabelParams(-1, -1));
                            emit_rows(latest, doc, onRow);
                            lastPath = latest;
                            lastTime = t;
                        } else {
                            std::cerr << "错误: 无法读取文件 " << latest << std::endl;
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "错误: " << e.what() << std::endl;
            }
            ::Sleep(waitMs);
        }
    });
}

} // namespace FileWatchers
