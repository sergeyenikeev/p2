#include "process_utils.h"

#include <TlHelp32.h>

#include <sstream>

namespace {

bool GetProcessStartTime(DWORD pid, FILETIME* out) {
  if (!out) {
    return false;
  }
  HANDLE handle =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!handle) {
    return false;
  }
  FILETIME create_time = {};
  FILETIME exit_time = {};
  FILETIME kernel_time = {};
  FILETIME user_time = {};
  bool ok = GetProcessTimes(handle, &create_time, &exit_time, &kernel_time,
                            &user_time) != FALSE;
  CloseHandle(handle);
  if (!ok) {
    return false;
  }
  *out = create_time;
  return true;
}

std::wstring TwoDigits(int value) {
  wchar_t buffer[4] = {};
  swprintf_s(buffer, L"%02d", value);
  return buffer;
}

}  // namespace

bool SnapshotProcesses(std::vector<ProcessInfo>* out, std::wstring* error) {
  if (!out) {
    if (error) {
      *error = L"Внутренняя ошибка: отсутствует список процессов.";
    }
    return false;
  }
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    if (error) {
      *error = L"Не удалось создать снимок процессов.";
    }
    return false;
  }

  PROCESSENTRY32W entry = {};
  entry.dwSize = sizeof(entry);
  if (!Process32FirstW(snapshot, &entry)) {
    CloseHandle(snapshot);
    if (error) {
      *error = L"Не удалось получить первый процесс.";
    }
    return false;
  }

  out->clear();
  do {
    ProcessInfo info;
    info.pid = entry.th32ProcessID;
    info.name = entry.szExeFile;
    FILETIME start_time = {};
    if (GetProcessStartTime(info.pid, &start_time)) {
      info.start_time = start_time;
      info.start_time_valid = true;
    }
    out->push_back(info);
  } while (Process32NextW(snapshot, &entry));

  CloseHandle(snapshot);
  return true;
}

std::wstring FormatFileTimeLocal(const FILETIME& ft) {
  SYSTEMTIME utc = {};
  SYSTEMTIME local = {};
  if (!FileTimeToSystemTime(&ft, &utc) ||
      !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) {
    return L"UNKNOWN";
  }
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"%04d-%02d-%02d %02d:%02d:%02d", local.wYear,
             local.wMonth, local.wDay, local.wHour, local.wMinute,
             local.wSecond);
  return buffer;
}

std::wstring FormatDuration(std::chrono::milliseconds duration) {
  auto total_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(duration).count();
  if (total_seconds < 0) {
    total_seconds = 0;
  }
  const long long hours = total_seconds / 3600;
  const long long minutes = (total_seconds % 3600) / 60;
  const long long seconds = total_seconds % 60;
  wchar_t buffer[32] = {};
  swprintf_s(buffer, L"%lld:%02lld:%02lld", hours, minutes, seconds);
  return buffer;
}

bool AppendUtf16Line(const std::wstring& path, const std::wstring& line,
                     std::wstring* error) {
  HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    if (error) {
      *error = L"Не удалось открыть файл процесса: " + path;
    }
    return false;
  }

  LARGE_INTEGER size = {};
  if (!GetFileSizeEx(file, &size)) {
    CloseHandle(file);
    if (error) {
      *error = L"Не удалось получить размер файла процесса: " + path;
    }
    return false;
  }
  if (size.QuadPart == 0) {
    const wchar_t bom = 0xFEFF;
    DWORD written = 0;
    WriteFile(file, &bom, sizeof(bom), &written, nullptr);
  }

  std::wstring output = line;
  output.append(L"\r\n");
  DWORD written = 0;
  const DWORD bytes =
      static_cast<DWORD>(output.size() * sizeof(wchar_t));
  bool ok = WriteFile(file, output.data(), bytes, &written, nullptr) != FALSE;
  CloseHandle(file);
  if (!ok && error) {
    *error = L"Не удалось записать строку в файл процесса: " + path;
  }
  return ok;
}
