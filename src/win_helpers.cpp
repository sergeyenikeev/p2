#include "win_helpers.h"

std::wstring GetWin32ErrorMessage(DWORD code) {
  if (code == 0) {
    code = GetLastError();
  }
  LPWSTR buffer = nullptr;
  DWORD size = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  if (size == 0 || buffer == nullptr) {
    return L"Неизвестная ошибка";
  }
  std::wstring message(buffer, size);
  LocalFree(buffer);
  while (!message.empty() &&
         (message.back() == L'\r' || message.back() == L'\n')) {
    message.pop_back();
  }
  return message;
}

std::wstring GetHresultMessage(HRESULT hr) {
  LPWSTR buffer = nullptr;
  DWORD size = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  if (size == 0 || buffer == nullptr) {
    return L"Неизвестная ошибка";
  }
  std::wstring message(buffer, size);
  LocalFree(buffer);
  while (!message.empty() &&
         (message.back() == L'\r' || message.back() == L'\n')) {
    message.pop_back();
  }
  return message;
}

std::wstring FormatHresult(HRESULT hr) {
  wchar_t buffer[16] = {};
  swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(hr));
  return buffer;
}

std::wstring FormatWin32Error(DWORD code) {
  wchar_t buffer[16] = {};
  swprintf_s(buffer, L"0x%08X", static_cast<unsigned int>(code));
  return buffer;
}

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return std::string();
  }
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                                        static_cast<int>(value.size()), nullptr,
                                        0, nullptr, nullptr);
  if (size_needed <= 0) {
    return std::string();
  }
  std::string result(static_cast<size_t>(size_needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
                      static_cast<int>(value.size()), result.data(),
                      size_needed, nullptr, nullptr);
  return result;
}

bool SetBestDpiAwareness() {
  if (SetProcessDpiAwarenessContext(
          DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
    return true;
  }
  return SetProcessDPIAware() != FALSE;
}
