#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>

#include "encode_wic.h"
#include "path_utils.h"
#include "time_utils.h"

namespace {

struct TestContext {
  int passed = 0;
  int failed = 0;
};

void Assert(bool condition, const char* message, TestContext& ctx) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    ++ctx.failed;
  } else {
    ++ctx.passed;
  }
}

ImageBuffer MakeTestPattern(uint32_t width, uint32_t height) {
  ImageBuffer buffer;
  buffer.width = width;
  buffer.height = height;
  buffer.stride = width * 4;
  buffer.pixel_format = GUID_WICPixelFormat32bppBGRA;
  buffer.pixels.resize(static_cast<size_t>(buffer.stride) * height);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      size_t idx = static_cast<size_t>(y) * buffer.stride + x * 4;
      buffer.pixels[idx + 0] = static_cast<uint8_t>(x % 256);
      buffer.pixels[idx + 1] = static_cast<uint8_t>(y % 256);
      buffer.pixels[idx + 2] = static_cast<uint8_t>((x + y) % 256);
      buffer.pixels[idx + 3] = 255;
    }
  }
  return buffer;
}

std::wstring MakeTempDir() {
  wchar_t temp_path[MAX_PATH] = {};
  if (!GetTempPathW(ARRAYSIZE(temp_path), temp_path)) {
    return L"";
  }
  wchar_t temp_file[MAX_PATH] = {};
  if (!GetTempFileNameW(temp_path, L"p2t", 0, temp_file)) {
    return L"";
  }
  DeleteFileW(temp_file);
  CreateDirectoryW(temp_file, nullptr);
  return temp_file;
}

void TestSanitize(TestContext& ctx) {
  Assert(SanitizeName(L"PC:*?\"<>|") == L"PC", "sanitize invalid chars", ctx);
  Assert(SanitizeName(L"Name. ") == L"Name", "sanitize trailing dot/space", ctx);
  Assert(SanitizeName(L"\t") == L"UNKNOWN", "sanitize empty", ctx);
}

void TestTimeFormat(TestContext& ctx) {
  DateTimeParts dt;
  dt.year = 2026;
  dt.month = 1;
  dt.day = 9;
  dt.hour = 5;
  dt.minute = 7;
  dt.second = 3;
  Assert(FormatYearMonth(dt) == L"2026-01", "format year-month", ctx);
  Assert(FormatDate(dt) == L"2026-01-09", "format date", ctx);
  Assert(FormatTime(dt) == L"05-07-03", "format time", ctx);
}

void TestBuildFileName(TestContext& ctx) {
  DateTimeParts dt;
  dt.year = 2026;
  dt.month = 1;
  dt.day = 9;
  dt.hour = 5;
  dt.minute = 7;
  dt.second = 3;
  std::wstring name =
      BuildFileName(L"PC", L"User", dt, 0, 1);
  Assert(name == L"PC_User_2026-01-09_05-07-03.jpg", "filename single display",
         ctx);
  std::wstring name2 =
      BuildFileName(L"PC", L"User", dt, 0, 2);
  Assert(name2 == L"PC_User_2026-01-09_05-07-03_Display01.jpg",
         "filename multi display", ctx);
}

void TestDirectories(TestContext& ctx) {
  std::wstring root = MakeTempDir();
  Assert(!root.empty(), "create temp dir", ctx);
  DateTimeParts dt;
  dt.year = 2026;
  dt.month = 1;
  dt.day = 9;
  OutputPaths paths;
  std::wstring error;
  Assert(BuildOutputPaths(root, L"PC_USER", dt, &paths, &error),
         "build output paths", ctx);
  std::vector<std::wstring> created;
  Assert(EnsureDirectories(paths, &created, &error), "ensure directories", ctx);
  Assert(std::filesystem::exists(paths.day_dir), "day dir exists", ctx);
  std::error_code ec;
  std::filesystem::remove_all(std::filesystem::path(root), ec);
}

void TestEncode(TestContext& ctx) {
  std::wstring root = MakeTempDir();
  Assert(!root.empty(), "create temp dir for encode", ctx);
  std::wstring file = JoinPath(root, L"test.jpg");
  ImageBuffer buffer = MakeTestPattern(32, 32);
  std::wstring error;
  HRESULT hr = S_OK;
  bool ok = SaveJpeg(buffer, file, 0.05f, &error, &hr);
  Assert(ok, "save jpeg", ctx);
  if (ok) {
    std::error_code ec;
    auto size = std::filesystem::file_size(file, ec);
    Assert(!ec && size > 0, "jpeg size > 0", ctx);
  }
  std::error_code ec;
  std::filesystem::remove_all(std::filesystem::path(root), ec);
}

}  // namespace

int main() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    std::cerr << "COM init failed\n";
    return 1;
  }

  TestContext ctx;
  TestSanitize(ctx);
  TestTimeFormat(ctx);
  TestBuildFileName(ctx);
  TestDirectories(ctx);
  TestEncode(ctx);

  CoUninitialize();
  std::cout << "Passed: " << ctx.passed << ", Failed: " << ctx.failed << "\n";
  return ctx.failed == 0 ? 0 : 1;
}
