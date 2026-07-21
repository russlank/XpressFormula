// PlaneClipping.cpp - Pure helpers for clipping projected geometry against a scalar plane.
#include "PlaneClipping.h"

#include <algorithm>
#include <cmath>

namespace XpressFormula::Plotting::Geometry {

namespace {

[[nodiscard]] bool isInside(const PlaneClipVertex2D& vertex,
                            double planeValue,
                            PlaneClipSide side) noexcept {
    return side == PlaneClipSide::Below
        ? vertex.value <= planeValue
        : vertex.value >= planeValue;
}

[[nodiscard]] PlaneClipVertex2D intersectByValue(const PlaneClipVertex2D& a,
                                                 const PlaneClipVertex2D& b,
                                                 double planeValue) noexcept {
    const double denom = b.value - a.value;
    double t = 0.0;
    if (std::abs(denom) > 1e-12) {
        t = (planeValue - a.value) / denom;
    }
    t = std::clamp(t, 0.0, 1.0);

    return PlaneClipVertex2D{
        Vec2{
            a.point.x + (b.point.x - a.point.x) * t,
            a.point.y + (b.point.y - a.point.y) * t
        },
        a.depth + (b.depth - a.depth) * t,
        a.value + (b.value - a.value) * t
    };
}

} // namespace

std::vector<PlaneClipVertex2D> clipTriangleByValue(const PlaneClipVertex2D& a,
                                                   const PlaneClipVertex2D& b,
                                                   const PlaneClipVertex2D& c,
                                                   double planeValue,
                                                   PlaneClipSide side) {
    const PlaneClipVertex2D input[3] = { a, b, c };
    std::vector<PlaneClipVertex2D> output;
    output.reserve(4);

    for (int i = 0; i < 3; ++i) {
        const PlaneClipVertex2D& curr = input[i];
        const PlaneClipVertex2D& prev = input[(i + 2) % 3];
        const bool currInside = isInside(curr, planeValue, side);
        const bool prevInside = isInside(prev, planeValue, side);

        if (currInside) {
            if (!prevInside) {
                output.push_back(intersectByValue(prev, curr, planeValue));
            }
            output.push_back(curr);
        } else if (prevInside) {
            output.push_back(intersectByValue(prev, curr, planeValue));
        }
    }

    return output;
}

std::optional<PlaneClipSegment2D> clipSegmentByValue(const PlaneClipVertex2D& a,
                                                     const PlaneClipVertex2D& b,
                                                     double planeValue,
                                                     PlaneClipSide side) {
    const bool aInside = isInside(a, planeValue, side);
    const bool bInside = isInside(b, planeValue, side);

    if (!aInside && !bInside) {
        return std::nullopt;
    }
    if (aInside && bInside) {
        return PlaneClipSegment2D{ a, b };
    }

    const PlaneClipVertex2D intersection = intersectByValue(a, b, planeValue);
    return aInside
        ? std::optional<PlaneClipSegment2D>(PlaneClipSegment2D{ a, intersection })
        : std::optional<PlaneClipSegment2D>(PlaneClipSegment2D{ intersection, b });
}

bool triangleAreaValid(const Vec3& a,
                       const Vec3& b,
                       const Vec3& c,
                       double minAreaSquared) noexcept {
    const Vec3 ab{ b.x - a.x, b.y - a.y, b.z - a.z };
    const Vec3 ac{ c.x - a.x, c.y - a.y, c.z - a.z };
    const Vec3 normal{
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x
    };
    const double lenSq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
    return lenSq > minAreaSquared && std::isfinite(lenSq);
}

} // namespace XpressFormula::Plotting::Geometry
