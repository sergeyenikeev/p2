#include "encode_wic.h"

#include <algorithm>

#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {

float ClampQuality(float quality) {
  if (quality < 0.01f) {
    return 0.01f;
  }
  if (quality > 1.0f) {
    return 1.0f;
  }
  return quality;
}

}  // namespace

bool SaveJpeg(const ImageBuffer& image, const std::wstring& path, float quality,
              std::wstring* error, HRESULT* hr_out) {
  if (image.width == 0 || image.height == 0 || image.stride == 0 ||
      image.pixels.empty()) {
    if (error) {
      *error = L"Некорректные данные изображения.";
    }
    if (hr_out) {
      *hr_out = E_INVALIDARG;
    }
    return false;
  }

  quality = ClampQuality(quality);

  ComPtr<IWICImagingFactory> factory;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать WIC фабрику.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  ComPtr<IWICStream> stream;
  hr = factory->CreateStream(&stream);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать WIC поток.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось открыть файл для записи: " + path;
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  ComPtr<IWICBitmapEncoder> encoder;
  hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать JPEG кодер.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось инициализировать JPEG кодер.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  ComPtr<IWICBitmapFrameEncode> frame;
  ComPtr<IPropertyBag2> props;
  hr = encoder->CreateNewFrame(&frame, &props);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать фрейм кодера.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  PROPBAG2 option = {};
  option.pstrName = const_cast<wchar_t*>(L"ImageQuality");
  VARIANT var = {};
  var.vt = VT_R4;
  var.fltVal = quality;
  hr = props->Write(1, &option, &var);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось задать качество JPEG.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  hr = frame->Initialize(props.Get());
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось инициализировать фрейм кодера.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  GUID target_format = GUID_WICPixelFormat24bppBGR;
  hr = frame->SetPixelFormat(&target_format);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось установить формат пикселей.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  ComPtr<IWICBitmap> bitmap;
  hr = factory->CreateBitmapFromMemory(
      image.width, image.height, image.pixel_format, image.stride,
      static_cast<UINT>(image.pixels.size()),
      const_cast<BYTE*>(image.pixels.data()), &bitmap);
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось создать WIC bitmap из буфера.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  if (target_format != image.pixel_format) {
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
      if (error) {
        *error = L"Не удалось создать конвертер формата.";
      }
      if (hr_out) {
        *hr_out = hr;
      }
      return false;
    }

    hr = converter->Initialize(bitmap.Get(), target_format,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
      if (error) {
        *error = L"Не удалось инициализировать конвертер формата.";
      }
      if (hr_out) {
        *hr_out = hr;
      }
      return false;
    }

    hr = frame->WriteSource(converter.Get(), nullptr);
  } else {
    hr = frame->WriteSource(bitmap.Get(), nullptr);
  }

  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось записать данные изображения.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  hr = frame->Commit();
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось завершить запись кадра.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  hr = encoder->Commit();
  if (FAILED(hr)) {
    if (error) {
      *error = L"Не удалось завершить запись JPEG.";
    }
    if (hr_out) {
      *hr_out = hr;
    }
    return false;
  }

  if (hr_out) {
    *hr_out = S_OK;
  }
  return true;
}
