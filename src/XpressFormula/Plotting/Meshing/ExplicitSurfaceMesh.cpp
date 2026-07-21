// ExplicitSurfaceMesh.cpp - Pure world-space sampling for z=f(x,y) surfaces.
#include "ExplicitSurfaceMesh.h"

#include "../../Core/Evaluator.h"
#include "../../Model/PlotPolicy.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace XpressFormula::Plotting::Meshing {

ExplicitSurfaceMesh sampleExplicitSurface(const Core::ASTNodePtr& ast,
                                          const ExplicitSurfaceOptions& options) {
    ExplicitSurfaceMesh mesh;
    mesh.bounds2D = options.bounds;
    mesh.cellsX = XpressFormula::Model::clampSurfaceResolution(options.resolution);
    mesh.cellsY = mesh.cellsX;

    if (!ast || !mesh.bounds2D.valid()) {
        mesh.cellsX = 0;
        mesh.cellsY = 0;
        return mesh;
    }

    mesh.vertices.assign(static_cast<size_t>(mesh.cellsX + 1) *
                             static_cast<size_t>(mesh.cellsY + 1),
                         SurfaceVertex{});

    const double dx = (mesh.bounds2D.xMax - mesh.bounds2D.xMin) / mesh.cellsX;
    const double dy = (mesh.bounds2D.yMax - mesh.bounds2D.yMin) / mesh.cellsY;
    double zMin = std::numeric_limits<double>::max();
    double zMax = std::numeric_limits<double>::lowest();

    Core::EvaluationContext context;
    for (int iy = 0; iy <= mesh.cellsY; ++iy) {
        const double y = mesh.bounds2D.yMin + static_cast<double>(iy) * dy;
        context.y = y;
        for (int ix = 0; ix <= mesh.cellsX; ++ix) {
            const double x = mesh.bounds2D.xMin + static_cast<double>(ix) * dx;
            context.x = x;
            const double z = Core::Evaluator::evaluate(ast, context);

            SurfaceVertex& vertex = mesh.vertices[mesh.indexOf(ix, iy)];
            vertex.position = Geometry::Vec3{ x, y, z };
            vertex.scalar = z;
            vertex.valid = std::isfinite(z);

            if (vertex.valid) {
                ++mesh.validVertexCount;
                zMin = std::min(zMin, z);
                zMax = std::max(zMax, z);
                mesh.bounds3D.include(vertex.position);
            }
        }
    }

    if (zMin < zMax) {
        mesh.scalarMin = zMin;
        mesh.scalarMax = zMax;
    } else {
        mesh.scalarMin = -1.0;
        mesh.scalarMax = 1.0;
    }

    return mesh;
}

} // namespace XpressFormula::Plotting::Meshing
