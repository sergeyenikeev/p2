#pragma once

#include <string>
#include <vector>
#include <windows.h>

// Display info from GDI enumeration.
struct DisplayInfo {
  int index = 0;
  HMONITOR monitor = nullptr;
  RECT rect = {};
  std::wstring name;
};

// Enumerates displays via EnumDisplayMonitors.
std::vector<DisplayInfo> EnumerateGdiDisplays();
