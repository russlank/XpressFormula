// Bounds.h - Small plotting-owned 2D/3D bounds types.
#pragma once

#include "Vec3.h"

#include <algorithm>
#include <limits>

namespace XpressFormula::Plotting::Geometry {

struct Bounds2D {
    double xMin = 0.0;
    double xMax = 0.0;
    double yMin = 0.0;
    double yMax = 0.0;

    [[nodiscard]] bool valid() const noexcept {
        return xMin < xMax && yMin < yMax;
    }
};

struct Bounds3D {
    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();
    double zMin = std::numeric_limits<double>::max();
    double zMax = std::numeric_limits<double>::lowest();

    [[nodiscard]] bool valid() const noexcept {
        return xMin < xMax && yMin < yMax && zMin < zMax;
    }

    void include(const Vec3& point) noexcept {
        xMin = std::min(xMin, point.x);
        xMax = std::max(xMax, point.x);
        yMin = std::min(yMin, point.y);
        yMax = std::max(yMax, point.y);
        zMin = std::min(zMin, point.z);
        zMax = std::max(zMax, point.z);
    }
};

} // namespace XpressFormula::Plotting::Geometry
