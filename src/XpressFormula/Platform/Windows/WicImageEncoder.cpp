// SPDX-License-Identifier: MIT
// WicImageEncoder.cpp - Windows image encoding service.
#include "WicImageEncoder.h"
#include "ComPtr.h"
#include "../../Core/InputLimits.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <wincodec.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

#pragma comment(lib, "windowscodecs.lib")

namespace XpressFormula::Platform::Windows {
namespace {

ImageEncodeResult failure(std::string message) {
    ImageEncodeResult result;
    result.error = std::move(message);
    return result;
}

ImageEncodeResult hresultFailure(HRESULT hr) {
    std::ostringstream oss;
    oss << "WIC error 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
    return failure(oss.str());
}

bool validImageInput(std::span<const std::uint8_t> pixels,
                     int width,
                     int height,
                     std::size_t& rowBytes,
                     std::size_t& pixelBytes,
                     ImageEncodeResult& result) {
    if (width <= 0 || height <= 0) {
        result.error = "Invalid image dimensions.";
        return false;
    }
    const auto widthValue = static_cast<std::size_t>(width);
    const auto heightValue = static_cast<std::size_t>(height);
    if (widthValue > (std::numeric_limits<std::size_t>::max)() / 4u) {
        result.error = "Image width is too large.";
        return false;
    }
    rowBytes = widthValue * 4u;
    if (heightValue > (std::numeric_limits<std::size_t>::max)() / rowBytes) {
        result.error = "Image height is too large.";
        return false;
    }
    pixelBytes = rowBytes * heightValue;
    if (pixelBytes > Core::InputLimits::kMaxImageBufferBytes) {
        result.error = "Image pixel buffer exceeds the supported limit.";
        return false;
    }
    if (rowBytes > (std::numeric_limits<UINT>::max)() ||
        pixelBytes > (std::numeric_limits<UINT>::max)()) {
        result.error = "Image dimensions exceed the Windows encoder limit.";
        return false;
    }
    if (pixels.size() < pixelBytes) {
        result.error = "Image pixel buffer is too small.";
        return false;
    }
    return true;
}

} // namespace

ImageEncodeResult WicImageEncoder::savePngBgra(const std::filesystem::path& path,
                                               std::span<const std::uint8_t> pixels,
                                               int width,
                                               int height) const {
    ImageEncodeResult result;
    std::size_t rowBytes = 0;
    std::size_t pixelBytes = 0;
    if (!validImageInput(pixels, width, height, rowBytes, pixelBytes, result)) {
        return result;
    }

    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;

    HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory,
                                    nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    __uuidof(IWICImagingFactory),
                                    reinterpret_cast<void**>(factory.put()));
    if (FAILED(hr)) {
        return hresultFailure(hr);
    }

    hr = factory->CreateStream(stream.put());
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromFilename(path.wstring().c_str(), GENERIC_WRITE);
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put());
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Initialize(stream.get(), WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->CreateNewFrame(frame.put(), properties.put());
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Initialize(properties.get());
    }
    if (SUCCEEDED(hr)) {
        hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    }
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) {
        hr = frame->SetPixelFormat(&pixelFormat);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->WritePixels(static_cast<UINT>(height),
                                static_cast<UINT>(rowBytes),
                                static_cast<UINT>(pixelBytes),
                                const_cast<BYTE*>(pixels.data()));
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Commit();
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Commit();
    }
    if (FAILED(hr)) {
        return hresultFailure(hr);
    }

    result.success = true;
    return result;
}

ImageEncodeResult WicImageEncoder::saveBmpBgra(const std::filesystem::path& path,
                                               std::span<const std::uint8_t> pixels,
                                               int width,
                                               int height) const {
    ImageEncodeResult result;
    std::size_t rowBytes = 0;
    std::size_t pixelBytes = 0;
    if (!validImageInput(pixels, width, height, rowBytes, pixelBytes, result)) {
        return result;
    }
    if (pixelBytes > (std::numeric_limits<std::uint32_t>::max)()) {
        return failure("Image is too large for BMP encoding.");
    }

#pragma pack(push, 1)
    struct BmpFileHeader {
        std::uint16_t type;
        std::uint32_t size;
        std::uint16_t reserved1;
        std::uint16_t reserved2;
        std::uint32_t offBits;
    };
#pragma pack(pop)

    BmpFileHeader fileHeader = {};
    fileHeader.type = 0x4D42;
    fileHeader.offBits = sizeof(BmpFileHeader) + sizeof(BITMAPINFOHEADER);
    fileHeader.size = fileHeader.offBits + static_cast<std::uint32_t>(pixelBytes);

    BITMAPINFOHEADER infoHeader = {};
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = static_cast<DWORD>(pixelBytes);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return failure("Failed to open output file.");
    }

    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    out.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    for (int y = height - 1; y >= 0; --y) {
        const auto* row = pixels.data() + static_cast<std::size_t>(y) * rowBytes;
        out.write(reinterpret_cast<const char*>(row), static_cast<std::streamsize>(rowBytes));
    }

    if (!out.good()) {
        return failure("Failed while writing BMP data.");
    }

    result.success = true;
    return result;
}

ImageEncodeResult WicImageEncoder::saveByExtensionBgra(const std::filesystem::path& path,
                                                       std::span<const std::uint8_t> pixels,
                                                       int width,
                                                       int height) const {
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });

    if (extension == L".bmp") {
        return saveBmpBgra(path, pixels, width, height);
    }
    return savePngBgra(path, pixels, width, height);
}

} // namespace XpressFormula::Platform::Windows
