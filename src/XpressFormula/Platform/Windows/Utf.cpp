// SPDX-License-Identifier: MIT
// Utf.cpp - Checked UTF-8/UTF-16 conversion helpers for Windows boundaries.
#include "Utf.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <limits>
#include <utility>

namespace XpressFormula::Platform::Windows {
namespace {

template <typename Result>
Result conversionFailure(std::string message, unsigned long win32Error = 0) {
    Result result;
    result.success = false;
    result.win32Error = win32Error;
    if (win32Error != 0) {
        message += ": Win32 error " + std::to_string(win32Error) + ".";
    }
    result.error = std::move(message);
    return result;
}

bool fitsInt(std::size_t size) {
    return size <= static_cast<std::size_t>((std::numeric_limits<int>::max)());
}

} // namespace

Utf8ToUtf16Result utf8ToUtf16(std::string_view text) {
    Utf8ToUtf16Result result;
    if (text.empty()) {
        result.success = true;
        return result;
    }
    if (!fitsInt(text.size())) {
        return conversionFailure<Utf8ToUtf16Result>("UTF-8 input is too large");
    }

    const int sourceLength = static_cast<int>(text.size());
    const int required = ::MultiByteToWideChar(CP_UTF8,
                                               MB_ERR_INVALID_CHARS,
                                               text.data(),
                                               sourceLength,
                                               nullptr,
                                               0);
    if (required <= 0) {
        return conversionFailure<Utf8ToUtf16Result>("Invalid UTF-8 input", ::GetLastError());
    }

    result.text.assign(static_cast<std::size_t>(required), L'\0');
    const int written = ::MultiByteToWideChar(CP_UTF8,
                                              MB_ERR_INVALID_CHARS,
                                              text.data(),
                                              sourceLength,
                                              result.text.data(),
                                              required);
    if (written != required) {
        return conversionFailure<Utf8ToUtf16Result>("Could not convert UTF-8 input",
                                                   ::GetLastError());
    }

    result.success = true;
    return result;
}

Utf16ToUtf8Result utf16ToUtf8(std::wstring_view text) {
    Utf16ToUtf8Result result;
    if (text.empty()) {
        result.success = true;
        return result;
    }
    if (!fitsInt(text.size())) {
        return conversionFailure<Utf16ToUtf8Result>("UTF-16 input is too large");
    }

    const int sourceLength = static_cast<int>(text.size());
    const int required = ::WideCharToMultiByte(CP_UTF8,
                                               WC_ERR_INVALID_CHARS,
                                               text.data(),
                                               sourceLength,
                                               nullptr,
                                               0,
                                               nullptr,
                                               nullptr);
    if (required <= 0) {
        return conversionFailure<Utf16ToUtf8Result>("Invalid UTF-16 input", ::GetLastError());
    }

    result.text.assign(static_cast<std::size_t>(required), '\0');
    const int written = ::WideCharToMultiByte(CP_UTF8,
                                              WC_ERR_INVALID_CHARS,
                                              text.data(),
                                              sourceLength,
                                              result.text.data(),
                                              required,
                                              nullptr,
                                              nullptr);
    if (written != required) {
        return conversionFailure<Utf16ToUtf8Result>("Could not convert UTF-16 input",
                                                   ::GetLastError());
    }

    result.success = true;
    return result;
}

std::wstring utf8ToUtf16OrEmpty(std::string_view text) {
    Utf8ToUtf16Result result = utf8ToUtf16(text);
    return result ? std::move(result.text) : std::wstring();
}

std::string utf16ToUtf8OrEmpty(std::wstring_view text) {
    Utf16ToUtf8Result result = utf16ToUtf8(text);
    return result ? std::move(result.text) : std::string();
}

} // namespace XpressFormula::Platform::Windows
