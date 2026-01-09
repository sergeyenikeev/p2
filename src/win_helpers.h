#pragma once

#include <string>
#include <windows.h>

// Returns Win32 error text for code (0 = GetLastError()).
std::wstring GetWin32ErrorMessage(DWORD code);
// Returns HRESULT error text.
std::wstring GetHresultMessage(HRESULT hr);
// Formats HRESULT as 0xXXXXXXXX.
std::wstring FormatHresult(HRESULT hr);
// Formats Win32 code as 0xXXXXXXXX.
std::wstring FormatWin32Error(DWORD code);
// Converts wide string to UTF-8.
std::string WideToUtf8(const std::wstring& value);

// Enables best-effort DPI awareness for pixel-accurate capture.
bool SetBestDpiAwareness();
