#pragma once

#include <string>
#include <vector>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "encode_wic.h"

// Description of a DXGI output.
struct DxgiOutputInfo {
  int index = 0;
  DXGI_OUTPUT_DESC desc = {};
  Microsoft::WRL::ComPtr<IDXGIOutput1> output;
};

// Context for one adapter and its outputs.
struct DxgiAdapterContext {
  Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  std::vector<DxgiOutputInfo> outputs;
};

// Adapter set suitable for Desktop Duplication.
struct DxgiContext {
  std::vector<DxgiAdapterContext> adapters;
};

// Initializes DXGI context (adapters + outputs).
bool InitializeDxgiContext(DxgiContext* ctx, std::wstring* error,
                           HRESULT* hr);

// Captures one output via Desktop Duplication.
bool CaptureDxgiOutput(const DxgiAdapterContext& adapter,
                       const DxgiOutputInfo& output, ImageBuffer* out,
                       std::wstring* error, HRESULT* hr);
