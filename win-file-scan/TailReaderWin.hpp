// Incremental tail reader for Windows.
// - Opens with shared read/write/delete
// - Maintains byte offset and only reads appended bytes
// - Emits only complete lines (LF-terminated), handles CRLF and UTF-8 BOM
// - Detects truncation and file replacement (by probing file id)
//
// Note: Designed for ASCII/UTF-8 text files with single-line records.
// If producer may write multi-line CSV records (newlines inside quotes),
// line-based tailing will not preserve CSV semantics.

#pragma once

#include <windows.h>
#include <string>
#include <vector>

class TailReaderWin {
public:
	explicit TailReaderWin(const std::string& path, bool skipHeader)
		: _path(path), _skipHeader(skipHeader) {
	}

	// Open the file if not already opened. Returns true on success.
	bool Open() {
		if (_h != INVALID_HANDLE_VALUE) return true;
		return open_internal();
	}

	// Read newly appended complete lines since the last call.
	// May return an empty vector if no new lines or on transient errors.
	std::vector<std::string> ReadAppendedLines() {
		std::vector<std::string> out;

		if (!Open()) {
			return out;
		}

		// If the file was replaced, reset state and re-open.
		if (refresh_if_replaced()) {
			// Newly opened, fall-through to read from the beginning (skip header if requested).
		}

		LARGE_INTEGER size{};
		if (!::GetFileSizeEx(_h, &size)) {
			_lastErr = ::GetLastError();
			return out;
		}

		unsigned long long fileSize = static_cast<unsigned long long>(size.QuadPart);
		if (fileSize < _offset) {
			// Truncated
			reset_state();
		}
		if (fileSize == _offset) {
			return out; // No new data
		}

		const DWORD kBuf = 64 * 1024;
		std::string buf;
		buf.resize(kBuf);

		LARGE_INTEGER li{};
		li.QuadPart = static_cast<LONGLONG>(_offset);
		if (!::SetFilePointerEx(_h, li, nullptr, FILE_BEGIN)) {
			_lastErr = ::GetLastError();
			return out;
		}

		unsigned long long remaining = fileSize - _offset;
		while (remaining > 0) {
			DWORD toRead = static_cast<DWORD>(remaining > kBuf ? kBuf : remaining);
			DWORD readBytes = 0;
			if (!::ReadFile(_h, &buf[0], toRead, &readBytes, nullptr)) {
				_lastErr = ::GetLastError();
				break;
			}
			if (readBytes == 0) break;
			_offset += readBytes;
			remaining -= readBytes;
			_carry.append(buf.data(), readBytes);

			// Strip UTF-8 BOM once at the beginning
			if (!_bomStripped && !_carry.empty()) {
				if (_carry.size() >= 3 &&
					static_cast<unsigned char>(_carry[0]) == 0xEF &&
					static_cast<unsigned char>(_carry[1]) == 0xBB &&
					static_cast<unsigned char>(_carry[2]) == 0xBF) {
					_carry.erase(0, 3);
				}
				_bomStripped = true;
			}

			// Skip header line once after open/reset
			if (_skipHeader && !_headerSkipped) {
				auto pos = _carry.find('\n');
				if (pos != std::string::npos) {
					_carry.erase(0, pos + 1);
					_headerSkipped = true;
				}
				else {
					// Still inside the first line; continue reading
					continue;
				}
			}

			// Extract complete lines
			for (;;) {
				auto pos = _carry.find('\n');
				if (pos == std::string::npos) break;
				std::string line = _carry.substr(0, pos);
				if (!line.empty() && line.back() == '\r') line.pop_back(); // CRLF -> LF
				out.emplace_back(std::move(line));
				_carry.erase(0, pos + 1);
			}
		}

		return out;
	}

	DWORD last_error() const { return _lastErr; }

	void Close() {
		if (_h != INVALID_HANDLE_VALUE) {
			::CloseHandle(_h);
			_h = INVALID_HANDLE_VALUE;
		}
	}

	~TailReaderWin() { Close(); }

private:
	struct FileId { DWORD volume{ 0 }; DWORD idxHi{ 0 }; DWORD idxLo{ 0 }; };

	bool open_internal() {
		_h = ::CreateFileA(
			_path.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
			nullptr);

		if (_h == INVALID_HANDLE_VALUE) {
			_lastErr = ::GetLastError();
			return false;
		}

		BY_HANDLE_FILE_INFORMATION info{};
		if (::GetFileInformationByHandle(_h, &info)) {
			_fileId.volume = info.dwVolumeSerialNumber;
			_fileId.idxHi = info.nFileIndexHigh;
			_fileId.idxLo = info.nFileIndexLow;
			_hasFileId = true;
		}
		else {
			_hasFileId = false;
		}

		// New handle -> reset state to read from beginning
		reset_state();
		return true;
	}

	void reset_state() {
		_offset = 0;
		_carry.clear();
		_bomStripped = false;
		_headerSkipped = false;
	}

	// Check if the path now refers to a different file than our open handle.
	// If so, close and reopen, returning true.
	bool refresh_if_replaced() {
		if (!_hasFileId) return false;

		HANDLE hProbe = ::CreateFileA(
			_path.c_str(),
			FILE_READ_ATTRIBUTES,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);

		if (hProbe == INVALID_HANDLE_VALUE) {
			// Could be temporarily missing; ignore
			return false;
		}

		BY_HANDLE_FILE_INFORMATION info{};
		bool replaced = false;
		if (::GetFileInformationByHandle(hProbe, &info)) {
			if (info.dwVolumeSerialNumber != _fileId.volume ||
				info.nFileIndexHigh != _fileId.idxHi ||
				info.nFileIndexLow != _fileId.idxLo) {
				replaced = true;
			}
		}
		::CloseHandle(hProbe);

		if (replaced) {
			Close();
			open_internal();
			return true;
		}
		return false;
	}

	std::string _path;
	HANDLE _h{ INVALID_HANDLE_VALUE };
	unsigned long long _offset{ 0 };
	std::string _carry;
	bool _skipHeader{ false };
	bool _headerSkipped{ false };
	bool _bomStripped{ false };
	DWORD _lastErr{ 0 };

	FileId _fileId{};
	bool _hasFileId{ false };
};

