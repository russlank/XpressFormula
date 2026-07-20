// ScalarGridSampler.cpp - Pure scalar grid sampling for heatmaps and contours.
#include "ScalarGridSampler.h"

#include "../../Core/Evaluator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace XpressFormula::Plotting::Sampling {

namespace {

[[nodiscard]] bool validGridRequest(const Core::ASTNodePtr& ast,
                                    const Geometry::Bounds2D& bounds,
                                    int columns,
                                    int rows) noexcept {
    return ast && bounds.valid() && columns > 0 && rows > 0;
}

void applyRangeFallback(ScalarCellGrid& grid, double lo, double hi) noexcept {
    if (lo < hi) {
        grid.minValue = lo;
        grid.maxValue = hi;
        return;
    }
    grid.minValue = -1.0;
    grid.maxValue = 1.0;
}

} // namespace

ScalarCellGrid sampleScalarCellGrid(const Core::ASTNodePtr& ast,
                                    const ScalarGridOptions& options,
                                    std::optional<double> zValue) {
    ScalarCellGrid grid;
    grid.bounds = options.bounds;
    grid.columns = options.columns;
    grid.rows = options.rows;

    if (!validGridRequest(ast, options.bounds, options.columns, options.rows)) {
        grid.columns = 0;
        grid.rows = 0;
        return grid;
    }

    grid.values.assign(static_cast<size_t>(grid.columns * grid.rows),
                       std::numeric_limits<double>::quiet_NaN());
    const double dx = grid.cellWidth();
    const double dy = grid.cellHeight();
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();

    Core::EvaluationContext context;
    if (zValue.has_value()) {
        context.z = *zValue;
    }

    for (int iy = 0; iy < grid.rows; ++iy) {
        context.y = grid.bounds.yMin + (static_cast<double>(iy) + 0.5) * dy;
        for (int ix = 0; ix < grid.columns; ++ix) {
            context.x = grid.bounds.xMin + (static_cast<double>(ix) + 0.5) * dx;
            const double value = Core::Evaluator::evaluate(ast, context);
            grid.values[static_cast<size_t>(iy * grid.columns + ix)] = value;
            if (std::isfinite(value)) {
                grid.hasFiniteValue = true;
                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
        }
    }

    applyRangeFallback(grid, lo, hi);
    return grid;
}

ScalarLattice sampleScalarLattice(const Core::ASTNodePtr& ast,
                                  const ScalarGridOptions& options,
                                  std::optional<double> zValue) {
    ScalarLattice lattice;
    lattice.bounds = options.bounds;
    lattice.cellsX = options.columns;
    lattice.cellsY = options.rows;

    if (!validGridRequest(ast, options.bounds, options.columns, options.rows)) {
        lattice.cellsX = 0;
        lattice.cellsY = 0;
        return lattice;
    }

    lattice.values.assign(static_cast<size_t>(lattice.cellsX + 1) *
                              static_cast<size_t>(lattice.cellsY + 1),
                          std::numeric_limits<double>::quiet_NaN());
    const double dx = lattice.stepX();
    const double dy = lattice.stepY();

    Core::EvaluationContext context;
    if (zValue.has_value()) {
        context.z = *zValue;
    }

    for (int iy = 0; iy <= lattice.cellsY; ++iy) {
        context.y = lattice.bounds.yMin + static_cast<double>(iy) * dy;
        for (int ix = 0; ix <= lattice.cellsX; ++ix) {
            context.x = lattice.bounds.xMin + static_cast<double>(ix) * dx;
            lattice.values[static_cast<size_t>(iy * (lattice.cellsX + 1) + ix)] =
                Core::Evaluator::evaluate(ast, context);
        }
    }

    return lattice;
}

} // namespace XpressFormula::Plotting::Sampling
