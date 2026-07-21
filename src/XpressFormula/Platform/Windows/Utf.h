// SPDX-License-Identifier: MIT
// Utf.h - Checked UTF-8/UTF-16 conversion helpers for Windows boundaries.
#pragma once

#include <string>
#include <string_view>

namespace XpressFormula::Platform::Windows {

struct Utf8ToUtf16Result {
    bool success = false;
    std::wstring text;
    std::string error;
    unsigned long win32Error = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

struct Utf16ToUtf8Result {
    bool success = false;
    std::string text;
    std::string error;
    unsigned long win32Error = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

[[nodiscard]] Utf8ToUtf16Result utf8ToUtf16(std::string_view text);
[[nodiscard]] Utf16ToUtf8Result utf16ToUtf8(std::wstring_view text);
[[nodiscard]] std::wstring utf8ToUtf16OrEmpty(std::string_view text);
[[nodiscard]] std::string utf16ToUtf8OrEmpty(std::wstring_view text);

} // namespace XpressFormula::Platform::Windows
