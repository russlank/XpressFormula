// CurveSampler.cpp - Pure curve sampling for 2D formula plots.
#include "CurveSampler.h"

#include "../../Core/Evaluator.h"
#include "../../Core/InputLimits.h"

#include <cmath>

namespace XpressFormula::Plotting::Sampling {

std::vector<Geometry::Polyline> sampleCurve2D(const Core::ASTNodePtr& ast,
                                              const CurveSampleOptions& options) {
    std::vector<Geometry::Polyline> polylines;
    if (!ast ||
        !(options.xMin < options.xMax) ||
        !std::isfinite(options.xMin) ||
        !std::isfinite(options.xMax) ||
        options.sampleCount <= 0 ||
        options.sampleCount > Core::InputLimits::kMaxCurveSamples) {
        return polylines;
    }

    const int sampleCount = options.sampleCount;
    const double dx = (options.xMax - options.xMin) / static_cast<double>(sampleCount);
    const double maxYJump = options.maxYJump > 0.0 ? options.maxYJump : 0.0;

    Core::EvaluationContext context;
    Geometry::Polyline current;
    current.points.reserve(static_cast<size_t>(sampleCount + 1));

    auto flushCurrent = [&]() {
        if (current.points.size() >= 2) {
            polylines.push_back(std::move(current));
        }
        current = Geometry::Polyline{};
    };

    for (int i = 0; i <= sampleCount; ++i) {
        const double x = options.xMin + static_cast<double>(i) * dx;
        context.x = x;
        const double y = Core::Evaluator::evaluate(ast, context);

        if (!std::isfinite(y)) {
            flushCurrent();
            continue;
        }

        if (!current.points.empty() && maxYJump > 0.0) {
            const double previousY = current.points.back().y;
            if (std::abs(y - previousY) > maxYJump) {
                flushCurrent();
            }
        }

        current.points.push_back(Geometry::Vec2{ x, y });
    }

    flushCurrent();
    return polylines;
}

} // namespace XpressFormula::Plotting::Sampling
