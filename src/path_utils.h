#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "time_utils.h"

// Output paths for screenshots and log.
struct OutputPaths {
  std::wstring root;
  std::wstring pc_user_dir;
  std::wstring month_dir;
  std::wstring day_dir;
};

// Sanitizes name: removes invalid characters and trailing dot/space.
std::wstring SanitizeName(const std::wstring& input);
// Joins two paths, adding a separator if needed.
std::wstring JoinPath(std::wstring_view a, std::wstring_view b);

// Builds output paths from root, PC_USER and date.
// Output: populated OutputPaths, error in error.
bool BuildOutputPaths(const std::wstring& root,
                      const std::wstring& pc_user,
                      const DateTimeParts& dt, OutputPaths* out,
                      std::wstring* error);

// Creates directories (root/pc_user/month/day). Output: created list.
bool EnsureDirectories(const OutputPaths& paths,
                       std::vector<std::wstring>* created,
                       std::wstring* error);

// Generates file name for a display.
std::wstring BuildFileName(const std::wstring& computer,
                           const std::wstring& user,
                           const DateTimeParts& dt, int display_index,
                           int display_count);
