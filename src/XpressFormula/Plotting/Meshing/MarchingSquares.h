// MarchingSquares.h - Pure contour extraction from sampled scalar lattices.
#pragma once

#include "../Geometry/Polyline.h"
#include "../Sampling/ScalarGridSampler.h"

#include <vector>

namespace XpressFormula::Plotting::Meshing {

[[nodiscard]] std::vector<Geometry::LineSegment2D> buildContourSegments(
    const Sampling::ScalarLattice& lattice,
    double isoValue = 0.0);

} // namespace XpressFormula::Plotting::Meshing
