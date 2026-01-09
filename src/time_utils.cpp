#include "time_utils.h"

#include <chrono>
#include <ctime>

DateTimeParts NowLocal() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm local = {};
  localtime_s(&local, &t);
  DateTimeParts dt;
  dt.year = local.tm_year + 1900;
  dt.month = local.tm_mon + 1;
  dt.day = local.tm_mday;
  dt.hour = local.tm_hour;
  dt.minute = local.tm_min;
  dt.second = local.tm_sec;
  return dt;
}

std::wstring FormatYearMonth(const DateTimeParts& dt) {
  wchar_t buffer[16] = {};
  swprintf_s(buffer, L"%04d-%02d", dt.year, dt.month);
  return buffer;
}

std::wstring FormatDate(const DateTimeParts& dt) {
  wchar_t buffer[16] = {};
  swprintf_s(buffer, L"%04d-%02d-%02d", dt.year, dt.month, dt.day);
  return buffer;
}

std::wstring FormatTime(const DateTimeParts& dt) {
  wchar_t buffer[16] = {};
  swprintf_s(buffer, L"%02d-%02d-%02d", dt.hour, dt.minute, dt.second);
  return buffer;
}
