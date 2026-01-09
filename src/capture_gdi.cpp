#include "capture_gdi.h"

#include <cstring>

bool CaptureRectGdi(const RECT& rect, ImageBuffer* out,
                    std::wstring* error) {
  if (!out) {
    if (error) {
      *error = L"Не передан буфер для захвата.";
    }
    return false;
  }
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  if (width <= 0 || height <= 0) {
    if (error) {
      *error = L"Некорректный размер прямоугольника захвата.";
    }
    return false;
  }

  HDC screen_dc = GetDC(nullptr);
  if (!screen_dc) {
    if (error) {
      *error = L"Не удалось получить DC экрана.";
    }
    SetLastError(GetLastError());
    return false;
  }
  HDC mem_dc = CreateCompatibleDC(screen_dc);
  if (!mem_dc) {
    DWORD last_error = GetLastError();
    ReleaseDC(nullptr, screen_dc);
    if (error) {
      *error = L"Не удалось создать совместимый DC.";
    }
    SetLastError(last_error);
    return false;
  }

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr,
                                 0);
  if (!dib || !bits) {
    DWORD last_error = GetLastError();
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    if (error) {
      *error = L"Не удалось создать DIB секцию.";
    }
    SetLastError(last_error);
    return false;
  }

  HGDIOBJ old = SelectObject(mem_dc, dib);
  BOOL blt_ok =
      BitBlt(mem_dc, 0, 0, width, height, screen_dc, rect.left, rect.top,
             SRCCOPY | CAPTUREBLT);

  bool success = false;
  if (!blt_ok) {
    DWORD last_error = GetLastError();
    if (error) {
      *error = L"Не удалось выполнить BitBlt.";
    }
    SetLastError(last_error);
  } else {
    const size_t stride = static_cast<size_t>(width) * 4;
    out->width = static_cast<uint32_t>(width);
    out->height = static_cast<uint32_t>(height);
    out->stride = static_cast<uint32_t>(stride);
    out->pixel_format = GUID_WICPixelFormat32bppBGRA;
    out->pixels.resize(stride * static_cast<size_t>(height));
    std::memcpy(out->pixels.data(), bits, out->pixels.size());
    success = true;
  }

  SelectObject(mem_dc, old);
  DeleteObject(dib);
  DeleteDC(mem_dc);
  ReleaseDC(nullptr, screen_dc);
  return success;
}

bool CaptureMonitorGdi(const DisplayInfo& display, ImageBuffer* out,
                       std::wstring* error) {
  return CaptureRectGdi(display.rect, out, error);
}
