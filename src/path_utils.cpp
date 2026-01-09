#include "path_utils.h"

#include <filesystem>

namespace {

bool IsInvalidChar(wchar_t ch) {
  if (ch < 0x20) {
    return true;
  }
  switch (ch) {
    case L'\\':
    case L'/':
    case L':':
    case L'*':
    case L'?':
    case L'"':
    case L'<':
    case L'>':
    case L'|':
      return true;
    default:
      return false;
  }
}

std::wstring TrimTrailingDotsSpaces(const std::wstring& input) {
  size_t end = input.size();
  while (end > 0 &&
         (input[end - 1] == L' ' || input[end - 1] == L'.')) {
    --end;
  }
  return input.substr(0, end);
}

}  // namespace

std::wstring SanitizeName(const std::wstring& input) {
  std::wstring cleaned;
  cleaned.reserve(input.size());
  for (wchar_t ch : input) {
    if (!IsInvalidChar(ch)) {
      cleaned.push_back(ch);
    }
  }
  cleaned = TrimTrailingDotsSpaces(cleaned);
  if (cleaned.empty()) {
    return L"UNKNOWN";
  }
  return cleaned;
}

std::wstring JoinPath(std::wstring_view a, std::wstring_view b) {
  if (a.empty()) {
    return std::wstring(b);
  }
  if (b.empty()) {
    return std::wstring(a);
  }
  if (a.back() == L'\\' || a.back() == L'/') {
    return std::wstring(a) + std::wstring(b);
  }
  return std::wstring(a) + L"\\" + std::wstring(b);
}

bool BuildOutputPaths(const std::wstring& root,
                      const std::wstring& pc_user,
                      const DateTimeParts& dt, OutputPaths* out,
                      std::wstring* error) {
  if (!out) {
    if (error) {
      *error = L"Внутренняя ошибка: отсутствует структура путей.";
    }
    return false;
  }
  if (root.empty()) {
    if (error) {
      *error = L"Не задан корневой путь.";
    }
    return false;
  }
  out->root = root;
  out->pc_user_dir = JoinPath(root, pc_user);
  out->month_dir = JoinPath(out->pc_user_dir, FormatYearMonth(dt));
  out->day_dir = JoinPath(out->month_dir, FormatDate(dt));
  return true;
}

bool EnsureDirectories(const OutputPaths& paths,
                       std::vector<std::wstring>* created,
                       std::wstring* error) {
  const std::wstring targets[] = {paths.root, paths.pc_user_dir, paths.month_dir,
                                  paths.day_dir};
  for (const auto& target : targets) {
    std::error_code ec;
    std::filesystem::path path(target);
    if (std::filesystem::exists(path, ec)) {
      if (!std::filesystem::is_directory(path, ec)) {
        if (error) {
          *error = L"Путь существует, но не является папкой: " + target;
        }
        return false;
      }
      continue;
    }
    if (!std::filesystem::create_directories(path, ec)) {
      if (error) {
        *error = L"Не удалось создать папку: " + target;
      }
      return false;
    }
    if (created) {
      created->push_back(target);
    }
  }
  return true;
}

std::wstring BuildFileName(const std::wstring& computer,
                           const std::wstring& user,
                           const DateTimeParts& dt, int display_index,
                           int display_count) {
  std::wstring base = computer + L"_" + user + L"_" + FormatDate(dt) + L"_" +
                      FormatTime(dt);
  if (display_count > 1) {
    wchar_t suffix[32] = {};
    swprintf_s(suffix, L"_Display%02d", display_index + 1);
    base += suffix;
  }
  base += L".jpg";
  return base;
}
