// SPDX-License-Identifier: MIT
// ClipboardService.cpp - Win32 clipboard text and DIB image service.
#include "ClipboardService.h"
#include "Utf.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstring>
#include <limits>

namespace XpressFormula::Platform::Windows {
namespace {

struct ClipboardGuard {
    explicit ClipboardGuard(HWND owner)
        : opened(::OpenClipboard(owner) != FALSE) {
    }

    ClipboardGuard(const ClipboardGuard&) = delete;
    ClipboardGuard& operator=(const ClipboardGuard&) = delete;

    ~ClipboardGuard() {
        if (opened) {
            ::CloseClipboard();
        }
    }

    bool opened = false;
};

struct GlobalMemory {
    explicit GlobalMemory(std::size_t byteCount)
        : handle(::GlobalAlloc(GMEM_MOVEABLE, byteCount)) {
    }

    GlobalMemory(const GlobalMemory&) = delete;
    GlobalMemory& operator=(const GlobalMemory&) = delete;

    ~GlobalMemory() {
        if (handle) {
            ::GlobalFree(handle);
        }
    }

    HGLOBAL detach() {
        HGLOBAL result = handle;
        handle = nullptr;
        return result;
    }

    HGLOBAL handle = nullptr;
};

bool checkedImageByteCounts(int width,
                            int height,
                            std::size_t& rowBytes,
                            std::size_t& imageBytes,
                            std::size_t& totalBytes,
                            std::string& error) {
    if (width <= 0 || height <= 0) {
        error = "Invalid image dimensions.";
        return false;
    }

    const auto widthValue = static_cast<std::size_t>(width);
    const auto heightValue = static_cast<std::size_t>(height);
    if (widthValue > (std::numeric_limits<std::size_t>::max)() / 4u) {
        error = "Image width is too large.";
        return false;
    }
    rowBytes = widthValue * 4u;
    if (heightValue > (std::numeric_limits<std::size_t>::max)() / rowBytes) {
        error = "Image height is too large.";
        return false;
    }
    imageBytes = rowBytes * heightValue;
    if (imageBytes > (std::numeric_limits<DWORD>::max)()) {
        error = "Image is too large for a DIB clipboard payload.";
        return false;
    }
    if (imageBytes > (std::numeric_limits<std::size_t>::max)() - sizeof(BITMAPINFOHEADER)) {
        error = "Image is too large for a DIB clipboard payload.";
        return false;
    }
    totalBytes = sizeof(BITMAPINFOHEADER) + imageBytes;
    return true;
}

} // namespace

DibBuildResult buildBottomUpDibFromBgra(std::span<const std::uint8_t> pixels,
                                        int width,
                                        int height) {
    DibBuildResult result;

    std::size_t rowBytes = 0;
    std::size_t imageBytes = 0;
    std::size_t totalBytes = 0;
    if (!checkedImageByteCounts(width, height, rowBytes, imageBytes, totalBytes, result.error)) {
        return result;
    }
    if (pixels.size() < imageBytes) {
        result.error = "Image pixel buffer is too small.";
        return result;
    }

    result.bytes.resize(totalBytes);
    auto* header = reinterpret_cast<BITMAPINFOHEADER*>(result.bytes.data());
    *header = {};
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = width;
    header->biHeight = height;
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(imageBytes);

    auto* dst = reinterpret_cast<std::uint8_t*>(header + 1);
    for (int y = 0; y < height; ++y) {
        const auto* srcRow = pixels.data() + static_cast<std::size_t>(height - 1 - y) * rowBytes;
        auto* dstRow = dst + static_cast<std::size_t>(y) * rowBytes;
        std::memcpy(dstRow, srcRow, rowBytes);
    }

    result.success = true;
    return result;
}

ClipboardResult ClipboardService::copyUtf16Text(HWND owner, std::wstring_view text) const {
    ClipboardResult result;
    const std::size_t byteCount = (text.size() + 1u) * sizeof(wchar_t);
    GlobalMemory memory(byteCount);
    if (!memory.handle) {
        result.error = "GlobalAlloc failed.";
        return result;
    }

    void* data = ::GlobalLock(memory.handle);
    if (!data) {
        result.error = "GlobalLock failed.";
        return result;
    }
    std::memcpy(data, text.data(), text.size() * sizeof(wchar_t));
    static_cast<wchar_t*>(data)[text.size()] = L'\0';
    ::GlobalUnlock(memory.handle);

    ClipboardGuard clipboard(owner);
    if (!clipboard.opened) {
        result.error = "Could not open clipboard.";
        return result;
    }

    ::EmptyClipboard();
    if (!::SetClipboardData(CF_UNICODETEXT, memory.handle)) {
        result.error = "SetClipboardData failed.";
        return result;
    }

    (void)memory.detach();
    result.success = true;
    return result;
}

ClipboardResult ClipboardService::copyUtf8Text(HWND owner, std::string_view text) const {
    Utf8ToUtf16Result converted = utf8ToUtf16(text);
    if (!converted) {
        ClipboardResult result;
        result.error = converted.error;
        return result;
    }
    return copyUtf16Text(owner, converted.text);
}

ClipboardResult ClipboardService::copyDibImageBgra(HWND owner,
                                                   std::span<const std::uint8_t> pixels,
                                                   int width,
                                                   int height) const {
    ClipboardResult result;
    DibBuildResult dib = buildBottomUpDibFromBgra(pixels, width, height);
    if (!dib) {
        result.error = dib.error;
        return result;
    }

    GlobalMemory memory(dib.bytes.size());
    if (!memory.handle) {
        result.error = "GlobalAlloc failed.";
        return result;
    }

    void* raw = ::GlobalLock(memory.handle);
    if (!raw) {
        result.error = "GlobalLock failed.";
        return result;
    }
    std::memcpy(raw, dib.bytes.data(), dib.bytes.size());
    ::GlobalUnlock(memory.handle);

    ClipboardGuard clipboard(owner);
    if (!clipboard.opened) {
        result.error = "Could not open clipboard.";
        return result;
    }

    ::EmptyClipboard();
    if (!::SetClipboardData(CF_DIB, memory.handle)) {
        result.error = "SetClipboardData failed.";
        return result;
    }

    (void)memory.detach();
    result.success = true;
    return result;
}

} // namespace XpressFormula::Platform::Windows

