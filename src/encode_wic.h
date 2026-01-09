#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <wincodec.h>

// Image buffer in memory (BGRA by default).
struct ImageBuffer {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t stride = 0;
  GUID pixel_format = GUID_WICPixelFormat32bppBGRA;
  std::vector<uint8_t> pixels;
};

// Saves buffer to JPEG via WIC.
// Input: quality in 0.01..1.0. Output: true on success, else error/hr.
bool SaveJpeg(const ImageBuffer& image, const std::wstring& path, float quality,
              std::wstring* error, HRESULT* hr);
