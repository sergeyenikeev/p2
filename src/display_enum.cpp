#include "display_enum.h"

namespace {

struct EnumState {
  std::vector<DisplayInfo>* list = nullptr;
  int index = 0;
};

BOOL CALLBACK EnumCallback(HMONITOR monitor, HDC, LPRECT, LPARAM lparam) {
  EnumState* state = reinterpret_cast<EnumState*>(lparam);
  if (!state || !state->list) {
    return FALSE;
  }
  MONITORINFOEXW info = {};
  info.cbSize = sizeof(info);
  if (!GetMonitorInfoW(monitor, &info)) {
    return TRUE;
  }
  DisplayInfo display;
  display.index = state->index++;
  display.monitor = monitor;
  display.rect = info.rcMonitor;
  display.name = info.szDevice;
  state->list->push_back(display);
  return TRUE;
}

}  // namespace

std::vector<DisplayInfo> EnumerateGdiDisplays() {
  std::vector<DisplayInfo> displays;
  EnumState state;
  state.list = &displays;
  EnumDisplayMonitors(nullptr, nullptr, EnumCallback,
                      reinterpret_cast<LPARAM>(&state));
  return displays;
}
