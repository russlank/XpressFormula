// TriangleMesh.h - Pure 3D geometry outputs produced by plotting mesh builders.
#pragma once

#include "Bounds.h"
#include "Vec3.h"

#include <cstdint>
#include <vector>

namespace XpressFormula::Plotting::Geometry {

struct ColoredTriangle {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    float scalar = 0.0f;
};

struct TriangleMesh {
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
    Bounds3D bounds;
};

} // namespace XpressFormula::Plotting::Geometry
