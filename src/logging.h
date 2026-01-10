#pragma once

#include <string>
#include <windows.h>

// Logger writes UTF-16LE with BOM and captures key stages.
class Logger {
 public:
  // Input: log file path. Output: file opened for writing if possible.
  explicit Logger(const std::wstring& path);
  ~Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  bool IsOpen() const;
  // Informational message.
  void Info(const std::wstring& message);
  // Error message.
  void Error(const std::wstring& message);
  // Flushes buffers to disk.
  void Flush();

 private:
  void LogLine(const std::wstring& level, const std::wstring& message);
  bool WriteUtf16Line(const std::wstring& line);

  HANDLE file_ = INVALID_HANDLE_VALUE;
};
