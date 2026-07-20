// SPDX-License-Identifier: MIT
// WicImageEncoder.h - Windows image encoding service.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace XpressFormula::Platform::Windows {

struct ImageEncodeResult {
    bool success = false;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

class WicImageEncoder {
public:
    [[nodiscard]] ImageEncodeResult savePngBgra(const std::filesystem::path& path,
                                                std::span<const std::uint8_t> pixels,
                                                int width,
                                                int height) const;
    [[nodiscard]] ImageEncodeResult saveBmpBgra(const std::filesystem::path& path,
                                                std::span<const std::uint8_t> pixels,
                                                int width,
                                                int height) const;
    [[nodiscard]] ImageEncodeResult saveByExtensionBgra(const std::filesystem::path& path,
                                                        std::span<const std::uint8_t> pixels,
                                                        int width,
                                                        int height) const;
};

} // namespace XpressFormula::Platform::Windows

