// ExplicitSurfaceMesh.h - Pure world-space sampling for z=f(x,y) surfaces.
#pragma once

#include "../Geometry/Bounds.h"
#include "../Geometry/Vec3.h"
#include "../../Core/ASTNode.h"

#include <cstddef>
#include <vector>

namespace XpressFormula::Plotting::Meshing {

struct ExplicitSurfaceOptions {
    Geometry::Bounds2D bounds;
    int resolution = 0;
};

struct SurfaceVertex {
    Geometry::Vec3 position;
    double scalar = 0.0;
    bool valid = false;
};

struct ExplicitSurfaceMesh {
    Geometry::Bounds2D bounds2D;
    Geometry::Bounds3D bounds3D;
    int cellsX = 0;
    int cellsY = 0;
    std::vector<SurfaceVertex> vertices;
    double scalarMin = -1.0;
    double scalarMax = 1.0;
    int validVertexCount = 0;

    [[nodiscard]] bool empty() const noexcept {
        return cellsX <= 0 || cellsY <= 0 || vertices.empty();
    }

    [[nodiscard]] size_t indexOf(int ix, int iy) const noexcept {
        return static_cast<size_t>(iy * (cellsX + 1) + ix);
    }

    [[nodiscard]] const SurfaceVertex& vertexAt(int ix, int iy) const noexcept {
        return vertices[indexOf(ix, iy)];
    }
};

[[nodiscard]] ExplicitSurfaceMesh sampleExplicitSurface(
    const Core::ASTNodePtr& ast,
    const ExplicitSurfaceOptions& options);

} // namespace XpressFormula::Plotting::Meshing
