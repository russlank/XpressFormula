// SPDX-License-Identifier: MIT
// ShellService.cpp - Narrow Win32 shell actions.
#include "ShellService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

namespace XpressFormula::Platform::Windows {

std::wstring explorerRevealArguments(std::wstring_view path) {
    std::wstring args = L"/select,\"";
    args += path;
    args += L"\"";
    return args;
}

bool ShellService::openUrl(std::wstring_view url) const {
    if (url.empty()) {
        return false;
    }
    const std::wstring value(url);
    const HINSTANCE result = ::ShellExecuteW(nullptr,
                                             L"open",
                                             value.c_str(),
                                             nullptr,
                                             nullptr,
                                             SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool ShellService::openPath(std::wstring_view path) const {
    if (path.empty()) {
        return false;
    }
    const std::wstring value(path);
    const HINSTANCE result = ::ShellExecuteW(nullptr,
                                             L"open",
                                             value.c_str(),
                                             nullptr,
                                             nullptr,
                                             SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool ShellService::revealPath(std::wstring_view path) const {
    if (path.empty()) {
        return false;
    }
    const std::wstring args = explorerRevealArguments(path);
    const HINSTANCE result = ::ShellExecuteW(nullptr,
                                             L"open",
                                             L"explorer.exe",
                                             args.c_str(),
                                             nullptr,
                                             SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

} // namespace XpressFormula::Platform::Windows
