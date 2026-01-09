#include "logging.h"

#include "win_helpers.h"

namespace {

std::wstring MakeTimestamp() {
  SYSTEMTIME st = {};
  GetLocalTime(&st);
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"%02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond,
             st.wMilliseconds);
  return buffer;
}

}  // namespace

Logger::Logger(const std::wstring& path) {
  file_ = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file_ == INVALID_HANDLE_VALUE) {
    return;
  }
  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file_, &size) || size.QuadPart == 0) {
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    DWORD written = 0;
    WriteFile(file_, bom, sizeof(bom), &written, nullptr);
  }
  SetFilePointer(file_, 0, nullptr, FILE_END);
}

Logger::~Logger() {
  if (file_ != INVALID_HANDLE_VALUE) {
    CloseHandle(file_);
  }
}

bool Logger::IsOpen() const {
  return file_ != INVALID_HANDLE_VALUE;
}

void Logger::Info(const std::wstring& message) {
  LogLine(L"ИНФО", message);
}

void Logger::Error(const std::wstring& message) {
  LogLine(L"ОШИБКА", message);
}

void Logger::Flush() {
  if (file_ != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(file_);
  }
}

void Logger::LogLine(const std::wstring& level,
                     const std::wstring& message) {
  std::wstring line = MakeTimestamp() + L" [" + level + L"] " + message;
  WriteUtf8Line(line);
}

bool Logger::WriteUtf8Line(const std::wstring& line) {
  if (file_ == INVALID_HANDLE_VALUE) {
    return false;
  }
  std::string utf8 = WideToUtf8(line);
  utf8.append("\r\n");
  DWORD written = 0;
  return WriteFile(file_, utf8.data(), static_cast<DWORD>(utf8.size()),
                   &written, nullptr) != FALSE;
}
