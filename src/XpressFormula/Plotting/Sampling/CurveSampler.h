// CurveSampler.h - Pure curve sampling for 2D formula plots.
#pragma once

#include "../Geometry/Polyline.h"
#include "../../Core/ASTNode.h"

#include <vector>

namespace XpressFormula::Plotting::Sampling {

struct CurveSampleOptions {
    double xMin = 0.0;
    double xMax = 0.0;
    int sampleCount = 0;
    double maxYJump = 0.0;
};

[[nodiscard]] std::vector<Geometry::Polyline> sampleCurve2D(
    const Core::ASTNodePtr& ast,
    const CurveSampleOptions& options);

} // namespace XpressFormula::Plotting::Sampling
