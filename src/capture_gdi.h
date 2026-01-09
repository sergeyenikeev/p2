#pragma once

#include <string>

#include "display_enum.h"
#include "encode_wic.h"

// Captures a monitor via GDI BitBlt.
bool CaptureMonitorGdi(const DisplayInfo& display, ImageBuffer* out,
                       std::wstring* error);

// Captures a screen rect via GDI BitBlt.
bool CaptureRectGdi(const RECT& rect, ImageBuffer* out, std::wstring* error);
