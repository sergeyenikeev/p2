#include <Lmcons.h>
#include <Windows.h>

#include <chrono>
#include <cwchar>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "capture_dxgi.h"
#include "capture_gdi.h"
#include "display_enum.h"
#include "encode_wic.h"
#include "logging.h"
#include "path_utils.h"
#include "process_utils.h"
#include "time_utils.h"
#include "win_helpers.h"

namespace {

// Обоснование: минимальное качество дает максимальное сжатие при валидном JPEG.
constexpr float kJpegQuality = 0.01f;

struct Options {
  std::wstring out_dir;
  bool test_image = false;
  int simulate_displays = 0;
  bool out_dir_from_cwd = false;
  int interval_seconds = 10;
  int capture_count = 0;
};

void PrintUsage() {
  std::wcerr
      << L"Использование:\n"
      << L"  p2_screenshot [--out \"D:\\\\Screens\"] [--interval-seconds 10]\n"
      << L"               [--count N] [--test-image] [--simulate-displays N]\n";
  std::wcerr << L"\n--out необязателен: по умолчанию используется текущая папка.\n";
  std::wcerr << L"--interval-seconds задает интервал между кадрами (>= 1).\n";
  std::wcerr << L"--count задает число циклов (0 = бесконечно).\n";
}

bool ParseIntArg(const std::wstring& value, int* out) {
  if (!out) {
    return false;
  }
  wchar_t* end = nullptr;
  long parsed = std::wcstol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != L'\0') {
    return false;
  }
  if (parsed < std::numeric_limits<int>::min() ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  *out = static_cast<int>(parsed);
  return true;
}

bool ParseArgs(int argc, wchar_t* argv[], Options* options,
               std::wstring* error) {
  if (!options) {
    if (error) {
      *error = L"Внутренняя ошибка: отсутствуют опции.";
    }
    return false;
  }
  for (int i = 1; i < argc; ++i) {
    const std::wstring arg = argv[i];
    if (arg == L"--out") {
      if (i + 1 >= argc) {
        if (error) {
          *error = L"Не указан путь после --out.";
        }
        return false;
      }
      options->out_dir = argv[++i];
    } else if (arg == L"--test-image") {
      options->test_image = true;
    } else if (arg == L"--simulate-displays") {
      if (i + 1 >= argc) {
        if (error) {
          *error = L"Не указан аргумент после --simulate-displays.";
        }
        return false;
      }
      int value = 0;
      if (!ParseIntArg(argv[++i], &value) || value < 0) {
        if (error) {
          *error = L"Некорректное значение --simulate-displays.";
        }
        return false;
      }
      options->simulate_displays = value;
    } else if (arg == L"--interval-seconds") {
      if (i + 1 >= argc) {
        if (error) {
          *error = L"Не указан аргумент после --interval-seconds.";
        }
        return false;
      }
      int value = 0;
      if (!ParseIntArg(argv[++i], &value) || value < 1) {
        if (error) {
          *error = L"Некорректное значение --interval-seconds.";
        }
        return false;
      }
      options->interval_seconds = value;
    } else if (arg == L"--count") {
      if (i + 1 >= argc) {
        if (error) {
          *error = L"Не указан аргумент после --count.";
        }
        return false;
      }
      int value = 0;
      if (!ParseIntArg(argv[++i], &value) || value < 0) {
        if (error) {
          *error = L"Некорректное значение --count.";
        }
        return false;
      }
      options->capture_count = value;
    } else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
      return false;
    } else {
      if (error) {
        *error = L"Неизвестный аргумент: " + arg;
      }
      return false;
    }
  }
  if (options->out_dir.empty()) {
    wchar_t cwd[MAX_PATH] = {};
    DWORD len = GetCurrentDirectoryW(ARRAYSIZE(cwd), cwd);
    if (len == 0 || len >= ARRAYSIZE(cwd)) {
      if (error) {
        *error = L"Не удалось определить текущую папку.";
      }
      return false;
    }
    options->out_dir.assign(cwd, len);
    options->out_dir_from_cwd = true;
  }
  if (options->simulate_displays < 0) {
    if (error) {
      *error = L"Некорректное значение --simulate-displays.";
    }
    return false;
  }
  if (options->simulate_displays > 0) {
    options->test_image = true;
  }
  return true;
}

std::wstring RectToString(const RECT& rect) {
  wchar_t buffer[64] = {};
  swprintf_s(buffer, L"[%ld,%ld,%ld,%ld]", rect.left, rect.top, rect.right,
             rect.bottom);
  return buffer;
}

ImageBuffer MakeTestPattern(uint32_t width, uint32_t height, uint32_t seed) {
  ImageBuffer buffer;
  buffer.width = width;
  buffer.height = height;
  buffer.stride = width * 4;
  buffer.pixel_format = GUID_WICPixelFormat32bppBGRA;
  buffer.pixels.resize(static_cast<size_t>(buffer.stride) * height);

  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      size_t idx = static_cast<size_t>(y) * buffer.stride + x * 4;
      buffer.pixels[idx + 0] = static_cast<uint8_t>((x + seed) % 256);
      buffer.pixels[idx + 1] = static_cast<uint8_t>((y + seed) % 256);
      buffer.pixels[idx + 2] = static_cast<uint8_t>((x + y + seed) % 256);
      buffer.pixels[idx + 3] = 255;
    }
  }
  return buffer;
}

FILETIME FileTimeNow() {
  FILETIME ft = {};
  GetSystemTimeAsFileTime(&ft);
  return ft;
}

std::chrono::milliseconds FileTimeDiffMs(const FILETIME& start,
                                         const FILETIME& end) {
  ULARGE_INTEGER a = {};
  ULARGE_INTEGER b = {};
  a.LowPart = start.dwLowDateTime;
  a.HighPart = start.dwHighDateTime;
  b.LowPart = end.dwLowDateTime;
  b.HighPart = end.dwHighDateTime;
  if (b.QuadPart <= a.QuadPart) {
    return std::chrono::milliseconds(0);
  }
  const unsigned long long diff_100ns = b.QuadPart - a.QuadPart;
  return std::chrono::milliseconds(diff_100ns / 10000ULL);
}

bool IsLikelyBlackFrame(const ImageBuffer& buffer) {
  if (buffer.width == 0 || buffer.height == 0 || buffer.stride < buffer.width * 4 ||
      buffer.pixels.empty()) {
    return true;
  }

  // Rationale: quick sampling avoids heavy scan but catches fully black frames.
  const uint32_t samples_x = 8;
  const uint32_t samples_y = 8;
  const uint8_t threshold = 8;
  for (uint32_t sy = 0; sy < samples_y; ++sy) {
    uint32_t y = buffer.height == 1 ? 0
                                    : (buffer.height - 1) * sy / (samples_y - 1);
    const uint8_t* row = buffer.pixels.data() +
                         static_cast<size_t>(y) * buffer.stride;
    for (uint32_t sx = 0; sx < samples_x; ++sx) {
      uint32_t x = buffer.width == 1 ? 0
                                     : (buffer.width - 1) * sx / (samples_x - 1);
      const uint8_t* px = row + static_cast<size_t>(x) * 4;
      if (px[0] > threshold || px[1] > threshold || px[2] > threshold) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);

  Options options;
  std::wstring parse_error;
  if (!ParseArgs(argc, argv, &options, &parse_error)) {
    if (!parse_error.empty()) {
      std::wcerr << parse_error << L"\n";
    }
    PrintUsage();
    return 1;
  }

  const bool dpi_ok = SetBestDpiAwareness();

  wchar_t computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD computer_size = ARRAYSIZE(computer_name);
  DWORD computer_err = 0;
  if (!GetComputerNameW(computer_name, &computer_size)) {
    computer_err = GetLastError();
    wcscpy_s(computer_name, L"UNKNOWN");
  }

  wchar_t user_name[UNLEN + 1] = {};
  DWORD user_size = ARRAYSIZE(user_name);
  DWORD user_err = 0;
  if (!GetUserNameW(user_name, &user_size)) {
    user_err = GetLastError();
    wcscpy_s(user_name, L"UNKNOWN");
  }

  std::wstring computer = SanitizeName(computer_name);
  std::wstring user = SanitizeName(user_name);
  std::wstring pc_user = computer + L"_" + user;

  std::unique_ptr<Logger> logger;
  OutputPaths paths;
  std::wstring current_date_key;
  bool header_logged = false;

  auto OpenLoggerForDate = [&](const DateTimeParts& dt) -> bool {
    OutputPaths new_paths;
    std::wstring error;
    if (!BuildOutputPaths(options.out_dir, pc_user, dt, &new_paths, &error)) {
      std::wcerr << error << L"\n";
      return false;
    }

    std::vector<std::wstring> created_dirs;
    if (!EnsureDirectories(new_paths, &created_dirs, &error)) {
      std::wcerr << error << L"\n";
      return false;
    }

    const std::wstring log_path =
        JoinPath(new_paths.day_dir, FormatDate(dt) + L".log");
    auto new_logger = std::make_unique<Logger>(log_path);
    if (!new_logger->IsOpen()) {
      std::wcerr << L"Не удалось открыть лог-файл: " << log_path << L"\n";
      return false;
    }

    paths = new_paths;
    logger = std::move(new_logger);
    current_date_key = FormatDate(dt);

    if (!header_logged) {
      logger->Info(L"Старт программы.");
      logger->Info(dpi_ok ? L"DPI-осведомленность включена (Per-Monitor V2)."
                          : L"DPI-осведомленность: не удалось включить.");
      if (options.out_dir_from_cwd) {
        logger->Info(L"Путь --out не задан, используется текущая папка: " +
                     options.out_dir);
      }
      if (computer_err != 0) {
        logger->Error(L"Не удалось получить имя компьютера: " +
                      FormatWin32Error(computer_err));
      }
      if (user_err != 0) {
        logger->Error(L"Не удалось получить имя пользователя: " +
                      FormatWin32Error(user_err));
      }
      logger->Info(L"Выбранный корневой путь: " + paths.root);
      logger->Info(L"Каталог пользователя: " + paths.pc_user_dir);
      logger->Info(L"Интервал захвата, сек: " +
                   std::to_wstring(options.interval_seconds));
      if (options.capture_count > 0) {
        logger->Info(L"Количество циклов: " +
                     std::to_wstring(options.capture_count));
      } else {
        logger->Info(L"Количество циклов: бесконечно.");
      }
      header_logged = true;
    } else {
      logger->Info(L"Переход на новую дату, лог переключен.");
      logger->Info(L"Выбранный корневой путь: " + paths.root);
      logger->Info(L"Каталог пользователя: " + paths.pc_user_dir);
    }

    for (const auto& dir : created_dirs) {
      logger->Info(L"Создана папка: " + dir);
    }
    return true;
  };

  DateTimeParts now = NowLocal();
  if (!OpenLoggerForDate(now)) {
    return 1;
  }

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    logger->Error(L"Не удалось инициализировать COM: " + FormatHresult(hr) +
                  L" - " + GetHresultMessage(hr));
    return 1;
  }

  bool any_failure = false;
  auto total_start = std::chrono::steady_clock::now();

  int display_count = 0;
  bool dxgi_ok = false;
  size_t total_outputs = 0;
  DxgiContext dxgi;
  std::vector<DisplayInfo> gdi_displays;
  ProcessMap known_processes;
  bool process_baseline_ready = false;

  if (options.test_image) {
    display_count = options.simulate_displays;
    if (display_count == 0) {
      auto detected = EnumerateGdiDisplays();
      display_count = static_cast<int>(detected.size());
    }
    if (display_count <= 0) {
      logger->Error(L"Не удалось определить количество дисплеев.");
      CoUninitialize();
      return 2;
    }

    logger->Info(L"Включен тестовый режим. Будут созданы синтетические кадры.");
    logger->Info(L"Количество дисплеев: " + std::to_wstring(display_count));
    for (int i = 0; i < display_count; ++i) {
      logger->Info(L"Дисплей " + std::to_wstring(i + 1) +
                   L": синтетический 256x256, координаты [0,0,256,256]");
    }
  } else {
    std::wstring dxgi_error;
    HRESULT dxgi_hr = S_OK;
    auto dxgi_enum_start = std::chrono::steady_clock::now();
    dxgi_ok = InitializeDxgiContext(&dxgi, &dxgi_error, &dxgi_hr);
    auto dxgi_enum_end = std::chrono::steady_clock::now();
    logger->Info(L"Время перечисления DXGI, мс: " +
                 std::to_wstring(std::chrono::duration_cast<
                                     std::chrono::milliseconds>(dxgi_enum_end -
                                                                dxgi_enum_start)
                                     .count()));

    if (dxgi_ok) {
      for (const auto& adapter : dxgi.adapters) {
        total_outputs += adapter.outputs.size();
      }
      if (total_outputs == 0) {
        logger->Error(L"DXGI выходы не найдены.");
        CoUninitialize();
        return 2;
      }
      logger->Info(L"Найдено DXGI выходов: " +
                   std::to_wstring(total_outputs));

      size_t global_index = 0;
      for (const auto& adapter : dxgi.adapters) {
        for (const auto& output : adapter.outputs) {
          RECT rect = output.desc.DesktopCoordinates;
          int width = rect.right - rect.left;
          int height = rect.bottom - rect.top;
          logger->Info(L"Дисплей " + std::to_wstring(global_index + 1) +
                       L": " + output.desc.DeviceName + L", " +
                       std::to_wstring(width) + L"x" +
                       std::to_wstring(height) + L", координаты " +
                       RectToString(rect));
          ++global_index;
        }
      }
    } else {
      logger->Error(L"DXGI недоступен, переход на GDI: " + dxgi_error +
                    L" (код " + FormatHresult(dxgi_hr) + L")");
      auto gdi_start = std::chrono::steady_clock::now();
      gdi_displays = EnumerateGdiDisplays();
      auto gdi_end = std::chrono::steady_clock::now();
      logger->Info(L"Найдено GDI дисплеев: " +
                   std::to_wstring(gdi_displays.size()));
      logger->Info(L"Время перечисления дисплеев, мс: " +
                   std::to_wstring(std::chrono::duration_cast<
                                       std::chrono::milliseconds>(gdi_end -
                                                                  gdi_start)
                                       .count()));

      if (gdi_displays.empty()) {
        logger->Error(L"GDI дисплеи не найдены.");
        CoUninitialize();
        return 2;
      }

      for (const auto& display : gdi_displays) {
        int width = display.rect.right - display.rect.left;
        int height = display.rect.bottom - display.rect.top;
        logger->Info(L"Дисплей " + std::to_wstring(display.index + 1) +
                     L": " + display.name + L", " + std::to_wstring(width) +
                     L"x" + std::to_wstring(height) + L", координаты " +
                     RectToString(display.rect));
      }
      total_outputs = gdi_displays.size();
    }
  }

  auto next_tick = std::chrono::steady_clock::now();
  int iteration = 0;
  while (options.capture_count == 0 || iteration < options.capture_count) {
    DateTimeParts cycle_time = NowLocal();
    std::wstring date_key = FormatDate(cycle_time);
    if (date_key != current_date_key) {
      if (!OpenLoggerForDate(cycle_time)) {
        any_failure = true;
        break;
      }
    }

    logger->Info(L"Цикл захвата: " + std::to_wstring(iteration + 1));

    {
      std::vector<ProcessInfo> snapshot;
      std::wstring process_error;
      if (SnapshotProcesses(&snapshot, &process_error)) {
        ProcessMap current;
        current.reserve(snapshot.size());
        FILETIME now_ft = FileTimeNow();
        for (auto& info : snapshot) {
          if (!info.start_time_valid) {
            info.start_time = now_ft;
            info.start_time_valid = true;
            info.start_time_approx = true;
          }
          current.emplace(info.pid, info);
        }

        if (!process_baseline_ready) {
          known_processes = std::move(current);
          process_baseline_ready = true;
          logger->Info(L"Процессы: базовый снимок, всего " +
                       std::to_wstring(known_processes.size()));
        } else {
          size_t added = 0;
          size_t removed = 0;
          for (const auto& [pid, info] : current) {
            auto it = known_processes.find(pid);
            if (it == known_processes.end()) {
              std::wstring start_time = FormatFileTimeLocal(info.start_time);
              std::wstring approx =
                  info.start_time_approx ? L" (по времени обнаружения)" : L"";
              logger->Info(L"Новый процесс: PID=" + std::to_wstring(pid) +
                           L", имя=" + info.name + L", старт=" + start_time +
                           approx);
              ++added;
            }
          }

          for (const auto& [pid, info] : known_processes) {
            if (current.find(pid) == current.end()) {
              FILETIME end_time = now_ft;
              std::wstring end_time_str = FormatFileTimeLocal(end_time);
              std::wstring start_time = FormatFileTimeLocal(info.start_time);
              std::wstring runtime =
                  FormatDuration(FileTimeDiffMs(info.start_time, end_time));
              std::wstring approx =
                  info.start_time_approx ? L" (по времени обнаружения)" : L"";
              logger->Info(L"Процесс завершен: PID=" + std::to_wstring(pid) +
                           L", имя=" + info.name + L", старт=" + start_time +
                           approx + L", конец=" + end_time_str +
                           L", время=" + runtime);
              ++removed;
            }
          }

          ProcessMap next_known;
          next_known.reserve(current.size());
          for (auto& [pid, info] : current) {
            auto it = known_processes.find(pid);
            if (it != known_processes.end()) {
              ProcessInfo merged = it->second;
              merged.name = info.name;
              next_known.emplace(pid, merged);
            } else {
              next_known.emplace(pid, info);
            }
          }
          known_processes.swap(next_known);
          logger->Info(L"Процессы: всего " + std::to_wstring(known_processes.size()) +
                       L", новых " + std::to_wstring(added) +
                       L", завершенных " + std::to_wstring(removed));
        }
      } else {
        logger->Error(L"Не удалось получить список процессов: " + process_error);
      }
    }

    if (options.test_image) {
      for (int i = 0; i < display_count; ++i) {
        auto capture_start = std::chrono::steady_clock::now();
        ImageBuffer buffer = MakeTestPattern(256, 256, static_cast<uint32_t>(i));
        auto capture_end = std::chrono::steady_clock::now();

        std::wstring filename =
            BuildFileName(computer, user, cycle_time, i, display_count);
        std::wstring filepath = JoinPath(paths.day_dir, filename);

        auto encode_start = std::chrono::steady_clock::now();
        std::wstring save_error;
        HRESULT save_hr = S_OK;
        bool saved =
            SaveJpeg(buffer, filepath, kJpegQuality, &save_error, &save_hr);
        auto encode_end = std::chrono::steady_clock::now();

        const auto capture_ms = std::chrono::duration_cast<
            std::chrono::milliseconds>(capture_end - capture_start)
                                     .count();
        const auto encode_ms = std::chrono::duration_cast<
            std::chrono::milliseconds>(encode_end - encode_start)
                                    .count();

        if (!saved) {
          any_failure = true;
          logger->Error(L"Ошибка сохранения дисплея " +
                        std::to_wstring(i + 1) + L": " + save_error + L" (код " +
                        FormatHresult(save_hr) + L")");
          continue;
        }

        logger->Info(L"Создан файл: " + filepath);
        logger->Info(L"Время синтетического кадра, мс: " +
                     std::to_wstring(capture_ms) +
                     L", кодирование, мс: " + std::to_wstring(encode_ms));
      }
    } else if (dxgi_ok) {
      size_t global_index = 0;
      for (const auto& adapter : dxgi.adapters) {
        for (const auto& output : adapter.outputs) {
          auto capture_start = std::chrono::steady_clock::now();
          ImageBuffer buffer;
          std::wstring capture_error;
          HRESULT capture_hr = S_OK;
          bool captured = CaptureDxgiOutput(adapter, output, &buffer,
                                            &capture_error, &capture_hr);
          if (!captured) {
            logger->Error(L"DXGI захват дисплея " +
                          std::to_wstring(global_index + 1) + L" не удался: " +
                          capture_error + L" (код " +
                          FormatHresult(capture_hr) + L")");
            std::wstring gdi_error;
            if (CaptureRectGdi(output.desc.DesktopCoordinates, &buffer,
                               &gdi_error)) {
              logger->Info(L"Использован резервный путь GDI для дисплея " +
                           std::to_wstring(global_index + 1));
              captured = true;
            } else {
              logger->Error(L"Резервный путь GDI тоже не удался: " + gdi_error +
                            L" (код " + FormatWin32Error(GetLastError()) + L")");
            }
          }
          auto capture_end = std::chrono::steady_clock::now();

          if (!captured) {
            any_failure = true;
            ++global_index;
            continue;
          }

          if (IsLikelyBlackFrame(buffer)) {
            logger->Info(L"Кадр DXGI выглядит пустым (почти черным), пробуем GDI.");
            std::wstring gdi_error;
            ImageBuffer gdi_buffer;
            if (CaptureRectGdi(output.desc.DesktopCoordinates, &gdi_buffer,
                               &gdi_error)) {
              buffer = std::move(gdi_buffer);
              logger->Info(L"Использован резервный путь GDI из-за черного кадра.");
            } else {
              logger->Error(L"Резервный путь GDI не удался: " + gdi_error +
                            L" (код " + FormatWin32Error(GetLastError()) + L")");
              any_failure = true;
              ++global_index;
              continue;
            }
          }

          std::wstring filename = BuildFileName(
              computer, user, cycle_time, static_cast<int>(global_index),
              static_cast<int>(total_outputs));
          std::wstring filepath = JoinPath(paths.day_dir, filename);

          auto encode_start = std::chrono::steady_clock::now();
          std::wstring save_error;
          HRESULT save_hr = S_OK;
          bool saved =
              SaveJpeg(buffer, filepath, kJpegQuality, &save_error, &save_hr);
          auto encode_end = std::chrono::steady_clock::now();

          const auto capture_ms = std::chrono::duration_cast<
              std::chrono::milliseconds>(capture_end - capture_start)
                                       .count();
          const auto encode_ms = std::chrono::duration_cast<
              std::chrono::milliseconds>(encode_end - encode_start)
                                      .count();

          if (!saved) {
            any_failure = true;
            logger->Error(L"Ошибка сохранения дисплея " +
                          std::to_wstring(global_index + 1) + L": " +
                          save_error + L" (код " + FormatHresult(save_hr) +
                          L")");
            ++global_index;
            continue;
          }

          logger->Info(L"Создан файл: " + filepath);
          logger->Info(L"Время захвата, мс: " + std::to_wstring(capture_ms) +
                       L", кодирование, мс: " + std::to_wstring(encode_ms));
          ++global_index;
        }
      }
    } else {
      for (const auto& display : gdi_displays) {
        auto capture_start = std::chrono::steady_clock::now();
        ImageBuffer buffer;
        std::wstring capture_error;
        bool captured = CaptureMonitorGdi(display, &buffer, &capture_error);
        auto capture_end = std::chrono::steady_clock::now();
        if (!captured) {
          any_failure = true;
          logger->Error(L"GDI захват дисплея " +
                        std::to_wstring(display.index + 1) +
                        L" не удался: " + capture_error + L" (код " +
                        FormatWin32Error(GetLastError()) + L")");
          continue;
        }

        std::wstring filename = BuildFileName(
            computer, user, cycle_time, display.index,
            static_cast<int>(total_outputs));
        std::wstring filepath = JoinPath(paths.day_dir, filename);

        auto encode_start = std::chrono::steady_clock::now();
        std::wstring save_error;
        HRESULT save_hr = S_OK;
        bool saved =
            SaveJpeg(buffer, filepath, kJpegQuality, &save_error, &save_hr);
        auto encode_end = std::chrono::steady_clock::now();

        const auto capture_ms = std::chrono::duration_cast<
            std::chrono::milliseconds>(capture_end - capture_start)
                                     .count();
        const auto encode_ms = std::chrono::duration_cast<
            std::chrono::milliseconds>(encode_end - encode_start)
                                    .count();

        if (!saved) {
          any_failure = true;
          logger->Error(L"Ошибка сохранения дисплея " +
                        std::to_wstring(display.index + 1) + L": " +
                        save_error + L" (код " + FormatHresult(save_hr) + L")");
          continue;
        }

        logger->Info(L"Создан файл: " + filepath);
        logger->Info(L"Время захвата, мс: " + std::to_wstring(capture_ms) +
                     L", кодирование, мс: " + std::to_wstring(encode_ms));
      }
    }

    ++iteration;
    if (options.capture_count != 0 && iteration >= options.capture_count) {
      break;
    }

    next_tick += std::chrono::seconds(options.interval_seconds);
    auto now_tick = std::chrono::steady_clock::now();
    if (now_tick < next_tick) {
      std::this_thread::sleep_until(next_tick);
    } else {
      next_tick = now_tick;
    }
  }

  auto total_end = std::chrono::steady_clock::now();
  const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            total_end - total_start)
                            .count();
  logger->Info(L"Общее время, мс: " + std::to_wstring(total_ms));
  logger->Info(L"Завершение программы.");
  logger->Flush();

  CoUninitialize();
  return any_failure ? 2 : 0;
}
