// SPDX-License-Identifier: MIT
// ClipboardService.h - Win32 clipboard text and DIB image service.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct HWND__;
using HWND = HWND__*;

namespace XpressFormula::Platform::Windows {

struct ClipboardResult {
    bool success = false;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

struct DibBuildResult {
    bool success = false;
    std::vector<std::uint8_t> bytes;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

[[nodiscard]] DibBuildResult buildBottomUpDibFromBgra(std::span<const std::uint8_t> pixels,
                                                      int width,
                                                      int height);

class ClipboardService {
public:
    [[nodiscard]] ClipboardResult copyUtf16Text(HWND owner, std::wstring_view text) const;
    [[nodiscard]] ClipboardResult copyUtf8Text(HWND owner, std::string_view text) const;
    [[nodiscard]] ClipboardResult copyDibImageBgra(HWND owner,
                                                   std::span<const std::uint8_t> pixels,
                                                   int width,
                                                   int height) const;
};

} // namespace XpressFormula::Platform::Windows

