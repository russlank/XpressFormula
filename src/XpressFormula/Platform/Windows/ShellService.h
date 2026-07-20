// SPDX-License-Identifier: MIT
// ShellService.h - Narrow Win32 shell actions.
#pragma once

#include <string>
#include <string_view>

namespace XpressFormula::Platform::Windows {

[[nodiscard]] std::wstring explorerRevealArguments(std::wstring_view path);

class ShellService {
public:
    [[nodiscard]] bool openUrl(std::wstring_view url) const;
    [[nodiscard]] bool openPath(std::wstring_view path) const;
    [[nodiscard]] bool revealPath(std::wstring_view path) const;
};

} // namespace XpressFormula::Platform::Windows

