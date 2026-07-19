// SPDX-License-Identifier: MIT
// AtomicFileWriter.h - Atomic file replacement helpers for app-owned files.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace XpressFormula::Infrastructure::FileSystem {

struct AtomicWriteResult {
    bool success = false;
    std::string error;
    unsigned long win32Error = 0;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

[[nodiscard]] std::filesystem::path atomicTempPathFor(const std::filesystem::path& targetPath);
[[nodiscard]] AtomicWriteResult writeTextAtomically(const std::filesystem::path& targetPath,
                                                    std::string_view utf8Text);
[[nodiscard]] AtomicWriteResult writeBytesAtomically(const std::filesystem::path& targetPath,
                                                     std::span<const std::uint8_t> bytes);

} // namespace XpressFormula::Infrastructure::FileSystem
