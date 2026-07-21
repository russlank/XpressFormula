// MarchingSquares.cpp - Pure contour extraction from sampled scalar lattices.
#include "MarchingSquares.h"

#include <algorithm>
#include <cmath>

namespace XpressFormula::Plotting::Meshing {

namespace {

[[nodiscard]] bool interpolateIso(double x0,
                                  double y0,
                                  double v0,
                                  double x1,
                                  double y1,
                                  double v1,
                                  double isoValue,
                                  Geometry::Vec2& out) noexcept {
    if (!std::isfinite(v0) || !std::isfinite(v1)) {
        return false;
    }

    const double a = v0 - isoValue;
    const double b = v1 - isoValue;
    if (a == 0.0 && b == 0.0) {
        return false;
    }
    if (a == 0.0) {
        out = Geometry::Vec2{ x0, y0 };
        return true;
    }
    if (b == 0.0) {
        out = Geometry::Vec2{ x1, y1 };
        return true;
    }
    if ((a > 0.0 && b > 0.0) || (a < 0.0 && b < 0.0)) {
        return false;
    }

    double t = 0.5;
    const double denom = a - b;
    if (std::abs(denom) > 1e-12) {
        t = std::clamp(a / denom, 0.0, 1.0);
    }

    out = Geometry::Vec2{
        x0 + (x1 - x0) * t,
        y0 + (y1 - y0) * t
    };
    return std::isfinite(out.x) && std::isfinite(out.y);
}

[[nodiscard]] bool samePoint(const Geometry::Vec2& a,
                             const Geometry::Vec2& b) noexcept {
    return std::abs(a.x - b.x) <= 1e-12 && std::abs(a.y - b.y) <= 1e-12;
}

[[nodiscard]] bool segmentHasLength(const Geometry::LineSegment2D& segment) noexcept {
    const double dx = segment.a.x - segment.b.x;
    const double dy = segment.a.y - segment.b.y;
    return dx * dx + dy * dy > 1e-24;
}

bool appendUniqueIntersection(Geometry::Vec2* intersections,
                              int& count,
                              const Geometry::Vec2& point) noexcept {
    for (int i = 0; i < count; ++i) {
        if (samePoint(intersections[i], point)) {
            return false;
        }
    }
    intersections[count++] = point;
    return true;
}

void appendSegment(std::vector<Geometry::LineSegment2D>& segments,
                   const Geometry::Vec2& a,
                   const Geometry::Vec2& b) {
    Geometry::LineSegment2D segment{ a, b };
    if (segmentHasLength(segment)) {
        segments.push_back(segment);
    }
}

} // namespace

std::vector<Geometry::LineSegment2D> buildContourSegments(const Sampling::ScalarLattice& lattice,
                                                          double isoValue) {
    std::vector<Geometry::LineSegment2D> segments;
    if (lattice.empty() || !lattice.bounds.valid()) {
        return segments;
    }

    const double dx = lattice.stepX();
    const double dy = lattice.stepY();
    segments.reserve(static_cast<size_t>(lattice.cellsX) *
                     static_cast<size_t>(lattice.cellsY));

    for (int iy = 0; iy < lattice.cellsY; ++iy) {
        const double y0 = lattice.bounds.yMin + static_cast<double>(iy) * dy;
        const double y1 = y0 + dy;
        for (int ix = 0; ix < lattice.cellsX; ++ix) {
            const double x0 = lattice.bounds.xMin + static_cast<double>(ix) * dx;
            const double x1 = x0 + dx;

            const double v0 = lattice.valueAt(ix, iy);
            const double v1 = lattice.valueAt(ix + 1, iy);
            const double v2 = lattice.valueAt(ix + 1, iy + 1);
            const double v3 = lattice.valueAt(ix, iy + 1);

            Geometry::Vec2 intersections[4];
            int count = 0;
            Geometry::Vec2 point{};

            if (interpolateIso(x0, y0, v0, x1, y0, v1, isoValue, point)) {
                appendUniqueIntersection(intersections, count, point);
            }
            if (interpolateIso(x1, y0, v1, x1, y1, v2, isoValue, point)) {
                appendUniqueIntersection(intersections, count, point);
            }
            if (interpolateIso(x1, y1, v2, x0, y1, v3, isoValue, point)) {
                appendUniqueIntersection(intersections, count, point);
            }
            if (interpolateIso(x0, y1, v3, x0, y0, v0, isoValue, point)) {
                appendUniqueIntersection(intersections, count, point);
            }

            if (count == 2) {
                appendSegment(segments, intersections[0], intersections[1]);
            } else if (count == 4) {
                const double s0 = v0 - isoValue;
                const double s2 = v2 - isoValue;
                const double center = ((v0 + v1 + v2 + v3) * 0.25) - isoValue;
                const bool connectAroundV0V2 =
                    std::isfinite(center) &&
                    ((s0 < 0.0 && s2 < 0.0 && center < 0.0) ||
                     (s0 > 0.0 && s2 > 0.0 && center > 0.0));

                if (connectAroundV0V2) {
                    appendSegment(segments, intersections[3], intersections[0]);
                    appendSegment(segments, intersections[1], intersections[2]);
                } else {
                    appendSegment(segments, intersections[0], intersections[1]);
                    appendSegment(segments, intersections[2], intersections[3]);
                }
            }
        }
    }

    return segments;
}

} // namespace XpressFormula::Plotting::Meshing
