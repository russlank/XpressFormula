// Polyline.h - Pure 2D geometry outputs produced by plotting samplers.
#pragma once

#include "Vec2.h"

#include <vector>

namespace XpressFormula::Plotting::Geometry {

struct Polyline {
    std::vector<Vec2> points;
    bool closed = false;
};

struct LineSegment2D {
    Vec2 a;
    Vec2 b;
};

} // namespace XpressFormula::Plotting::Geometry
