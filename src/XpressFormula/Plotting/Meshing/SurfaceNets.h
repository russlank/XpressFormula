// SurfaceNets.h - Pure implicit scalar-field sampling and surface-nets mesh generation.
#pragma once

#include "ImplicitMeshCache.h"
#include "../Geometry/Bounds.h"
#include "../../Core/ASTNode.h"

namespace XpressFormula::Plotting::Meshing {

struct SurfaceNetsOptions {
    Geometry::Bounds3D bounds;
    int resolution = 0;
};

[[nodiscard]] ImplicitMeshEntry buildSurfaceNetsMesh(
    const Core::ASTNodePtr& ast,
    const SurfaceNetsOptions& options);

} // namespace XpressFormula::Plotting::Meshing
