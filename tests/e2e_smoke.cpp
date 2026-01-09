#include <Lmcons.h>
#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "path_utils.h"
#include "time_utils.h"

namespace {

std::wstring MakeTempDir() {
  wchar_t temp_path[MAX_PATH] = {};
  if (!GetTempPathW(ARRAYSIZE(temp_path), temp_path)) {
    return L"";
  }
  wchar_t temp_file[MAX_PATH] = {};
  if (!GetTempFileNameW(temp_path, L"p2e", 0, temp_file)) {
    return L"";
  }
  DeleteFileW(temp_file);
  CreateDirectoryW(temp_file, nullptr);
  return temp_file;
}

bool RunProcess(const std::wstring& command_line, DWORD* exit_code) {
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};

  std::wstring cmd = command_line;
  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &si, &pi)) {
    return false;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 0;
  if (!GetExitCodeProcess(pi.hProcess, &code)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return false;
  }
  if (exit_code) {
    *exit_code = code;
  }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
  if (argc < 2) {
    std::wcerr << L"Ожидался путь к исполняемому файлу.\n";
    return 1;
  }

  std::wstring exe_path = argv[1];
  std::wstring temp_root = MakeTempDir();
  if (temp_root.empty()) {
    std::wcerr << L"Не удалось создать временную папку.\n";
    return 1;
  }

  std::wstring cmd = L"\"" + exe_path + L"\" --out \"" + temp_root +
                     L"\" --test-image --simulate-displays 1 --count 1";
  DWORD exit_code = 1;
  if (!RunProcess(cmd, &exit_code)) {
    std::wcerr << L"Не удалось запустить процесс.\n";
    return 1;
  }
  if (exit_code != 0) {
    std::wcerr << L"Процесс завершился с кодом " << exit_code << L".\n";
    return 1;
  }

  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD computer_size = ARRAYSIZE(computer_name);
  if (!GetComputerNameW(computer_name, &computer_size)) {
    wcscpy_s(computer_name, L"UNKNOWN");
  }

  wchar_t user_name[UNLEN + 1] = {};
  DWORD user_size = ARRAYSIZE(user_name);
  if (!GetUserNameW(user_name, &user_size)) {
    wcscpy_s(user_name, L"UNKNOWN");
  }

  DateTimeParts now = NowLocal();
  std::wstring pc_user = SanitizeName(computer_name) + L"_" +
                         SanitizeName(user_name);

  OutputPaths paths;
  std::wstring error;
  if (!BuildOutputPaths(temp_root, pc_user, now, &paths, &error)) {
    std::wcerr << L"Не удалось построить пути: " << error << L"\n";
    return 1;
  }

  std::wstring log_path =
      JoinPath(paths.day_dir, FormatDate(now) + L".log");
  if (!std::filesystem::exists(log_path)) {
    std::wcerr << L"Лог-файл не найден: " << log_path << L"\n";
    return 1;
  }

  if (!std::filesystem::exists(paths.day_dir)) {
    std::wcerr << L"Папка дня не найдена: " << paths.day_dir << L"\n";
    return 1;
  }

  bool has_jpg = false;
  std::error_code iter_ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(paths.day_dir, iter_ec)) {
    if (entry.path().extension() == L".jpg") {
      has_jpg = true;
      break;
    }
  }
  if (iter_ec) {
    std::wcerr << L"Ошибка чтения папки дня.\n";
    return 1;
  }
  if (!has_jpg) {
    std::wcerr << L"JPG файлы не найдены.\n";
    return 1;
  }

  std::error_code ec;
  std::filesystem::remove_all(std::filesystem::path(temp_root), ec);
  return 0;
}
