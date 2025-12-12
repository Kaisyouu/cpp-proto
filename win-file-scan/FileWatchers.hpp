// Public API for creating CSV watcher threads.
// Keep main minimal by exposing creation functions and a row callback type.

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>

namespace FileWatchers {

// Row callback signature: provide file full path and parsed row values.
using RowHandler = std::function<void(const std::string& filepath, const std::vector<std::string>& row)>;

// Create a thread which tails a single CSV file and invokes `onRow` for each newly appended row.
// waitSeconds controls the polling interval between read attempts.
// The returned thread runs an infinite loop; join it in main.
std::thread CreateWatchAppend(const std::string& filePath, RowHandler onRow, int waitSeconds = 5);

// Create a thread which, every waitSeconds, locates the newest CSV file under `dir` with a given
// `prefix`, reads the whole file, and invokes `onRow` for each row in the file. Re-triggers when
// the newest file changes or its last-write time increases.
std::thread CreateWatchNew(const std::string& dir, const std::string& prefix, RowHandler onRow, int waitSeconds = 5);

} // namespace FileWatchers
