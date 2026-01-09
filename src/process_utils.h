#pragma once

#include <Windows.h>

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

struct ProcessInfo {
  DWORD pid = 0;
  std::wstring name;
  FILETIME start_time = {};
  bool start_time_valid = false;
  bool start_time_approx = false;
};

using ProcessMap = std::unordered_map<DWORD, ProcessInfo>;

// Creates a snapshot of running processes with best-effort start time.
bool SnapshotProcesses(std::vector<ProcessInfo>* out, std::wstring* error);

// Formats FILETIME to local "YYYY-MM-DD HH:MM:SS".
std::wstring FormatFileTimeLocal(const FILETIME& ft);

// Formats duration as "HH:MM:SS" (hours may exceed 24).
std::wstring FormatDuration(std::chrono::milliseconds duration);
