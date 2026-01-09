#include "capture_dxgi.h"

#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

struct FrameGuard {
  IDXGIOutputDuplication* duplication = nullptr;
  ~FrameGuard() {
    if (duplication) {
      duplication->ReleaseFrame();
    }
  }
};

}  // namespace

bool InitializeDxgiContext(DxgiContext* ctx, std::wstring* error,
                           HRESULT* hr_out) {
  if (!ctx) {
    if (error) {
      *error = L"Не передан контекст DXGI.";
    }
    if (hr_out) {
      *hr_out = E_INVALIDARG;
    }
    return false;
  }

  ComPtr<IDXGIFactory1> factory;
  HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать DXGI фабрику.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  ctx->adapters.clear();
  for (UINT adapter_index = 0;; ++adapter_index) {
    ComPtr<IDXGIAdapter1> adapter;
    hr = factory->EnumAdapters1(adapter_index, &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(hr)) {
      continue;
    }

    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1,
                                  D3D_FEATURE_LEVEL_11_0,
                                  D3D_FEATURE_LEVEL_10_1,
                                  D3D_FEATURE_LEVEL_10_0};
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL created = D3D_FEATURE_LEVEL_11_0;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                           flags, levels, ARRAYSIZE(levels),
                           D3D11_SDK_VERSION, &device, &created, &context);
    if (FAILED(hr)) {
      continue;
    }

    DxgiAdapterContext adapter_ctx;
    adapter_ctx.adapter = adapter;
    adapter_ctx.device = device;
    adapter_ctx.context = context;

    for (UINT output_index = 0;; ++output_index) {
      ComPtr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, &output);
      if (hr == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (FAILED(hr)) {
        continue;
      }
      ComPtr<IDXGIOutput1> output1;
      hr = output.As(&output1);
      if (FAILED(hr)) {
        continue;
      }
      DxgiOutputInfo info;
      info.index = static_cast<int>(adapter_ctx.outputs.size());
      output1->GetDesc(&info.desc);
      info.output = output1;
      adapter_ctx.outputs.push_back(info);
    }

    if (!adapter_ctx.outputs.empty()) {
      ctx->adapters.push_back(adapter_ctx);
    }
  }

  if (ctx->adapters.empty()) {
    if (error) {
      *error = L"DXGI адаптеры с выходами не найдены.";
    }
    if (hr_out) {
      *hr_out = E_FAIL;
    }
    return false;
  }

  if (hr_out) {
    *hr_out = S_OK;
  }
  return true;
}

bool CaptureDxgiOutput(const DxgiAdapterContext& adapter,
                       const DxgiOutputInfo& output, ImageBuffer* out,
                       std::wstring* error, HRESULT* hr_out) {
  if (!out) {
    if (error) {
      *error = L"Не передан буфер для захвата.";
    }
    if (hr_out) {
      *hr_out = E_INVALIDARG;
    }
    return false;
  }

  ComPtr<IDXGIOutputDuplication> duplication;
  HRESULT hr =
      output.output->DuplicateOutput(adapter.device.Get(), &duplication);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать дубликатор Desktop Duplication.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  DXGI_OUTDUPL_FRAME_INFO frame_info = {};
  ComPtr<IDXGIResource> resource;
  hr = duplication->AcquireNextFrame(500, &frame_info, &resource);
  if (FAILED(hr)) {
    if (error) {
      if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        *error = L"Таймаут ожидания кадра Desktop Duplication.";
      } else {
        *error = L"Не удалось получить кадр Desktop Duplication.";
      }
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }
  FrameGuard guard{duplication.Get()};

  ComPtr<ID3D11Texture2D> texture;
  hr = resource.As(&texture);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось получить текстуру кадра.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  D3D11_TEXTURE2D_DESC desc = {};
  texture->GetDesc(&desc);
  if (desc.Width == 0 || desc.Height == 0) {
    if (error) {
      *error = L"Некорректный размер текстуры кадра.";
    }
    if (hr_out) {
      *hr_out = E_FAIL;
    }
    return false;
  }

  // Обоснование: CPU может читать только staging-ресурс, поэтому нужна копия.
  D3D11_TEXTURE2D_DESC staging_desc = desc;
  staging_desc.Usage = D3D11_USAGE_STAGING;
  staging_desc.BindFlags = 0;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  staging_desc.MiscFlags = 0;

  ComPtr<ID3D11Texture2D> staging;
  hr = adapter.device->CreateTexture2D(&staging_desc, nullptr, &staging);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать staging текстуру.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  adapter.context->CopyResource(staging.Get(), texture.Get());

  D3D11_MAPPED_SUBRESOURCE mapped = {};
  hr = adapter.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось получить доступ к staging текстуре.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  const uint32_t width = desc.Width;
  const uint32_t height = desc.Height;
  const uint32_t stride = width * 4;
  out->width = width;
  out->height = height;
  out->stride = stride;
  out->pixel_format = GUID_WICPixelFormat32bppBGRA;
  out->pixels.resize(static_cast<size_t>(stride) * height);

  const uint8_t* src = reinterpret_cast<const uint8_t*>(mapped.pData);
  uint8_t* dst = out->pixels.data();
  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(dst + static_cast<size_t>(y) * stride,
                src + static_cast<size_t>(y) * mapped.RowPitch, stride);
  }

  adapter.context->Unmap(staging.Get(), 0);

  if (hr_out) {
    *hr_out = S_OK;
  }
  return true;
}
