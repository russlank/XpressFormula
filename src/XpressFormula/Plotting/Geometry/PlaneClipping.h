// PlaneClipping.h - Pure helpers for clipping projected geometry against a scalar plane.
#pragma once

#include "Polyline.h"
#include "Vec3.h"

#include <optional>
#include <vector>

namespace XpressFormula::Plotting::Geometry {

enum class PlaneClipSide {
    Below,
    Above
};

struct PlaneClipVertex2D {
    Vec2 point;
    double depth = 0.0;
    double value = 0.0;
};

struct PlaneClipSegment2D {
    PlaneClipVertex2D a;
    PlaneClipVertex2D b;
};

[[nodiscard]] std::vector<PlaneClipVertex2D> clipTriangleByValue(
    const PlaneClipVertex2D& a,
    const PlaneClipVertex2D& b,
    const PlaneClipVertex2D& c,
    double planeValue,
    PlaneClipSide side);

[[nodiscard]] std::optional<PlaneClipSegment2D> clipSegmentByValue(
    const PlaneClipVertex2D& a,
    const PlaneClipVertex2D& b,
    double planeValue,
    PlaneClipSide side);

[[nodiscard]] bool triangleAreaValid(const Vec3& a,
                                     const Vec3& b,
                                     const Vec3& c,
                                     double minAreaSquared) noexcept;

} // namespace XpressFormula::Plotting::Geometry
