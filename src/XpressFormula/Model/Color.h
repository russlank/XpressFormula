// Color.h - Domain color values.
#pragma once

#include <array>
#include <cstddef>

namespace XpressFormula::Model {

struct ColorRgba {
    std::array<float, 4> channels = { 1.0f, 1.0f, 1.0f, 1.0f };

    [[nodiscard]] float* data() { return channels.data(); }
    [[nodiscard]] const float* data() const { return channels.data(); }
    [[nodiscard]] std::size_t size() const { return channels.size(); }

    [[nodiscard]] float& operator[](std::size_t index) {
        return channels[index];
    }

    [[nodiscard]] const float& operator[](std::size_t index) const {
        return channels[index];
    }
};

} // namespace XpressFormula::Model
