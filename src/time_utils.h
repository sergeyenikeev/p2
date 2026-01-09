#pragma once

#include <string>

// Local date/time parts.
struct DateTimeParts {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
};

// Returns current local date/time.
DateTimeParts NowLocal();

// Formats YYYY-MM.
std::wstring FormatYearMonth(const DateTimeParts& dt);
// Formats YYYY-MM-DD.
std::wstring FormatDate(const DateTimeParts& dt);
// Formats HH-MM-SS.
std::wstring FormatTime(const DateTimeParts& dt);
