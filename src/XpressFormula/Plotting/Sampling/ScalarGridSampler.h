// ScalarGridSampler.h - Pure scalar grid sampling for heatmaps and contours.
#pragma once

#include "../Geometry/Bounds.h"
#include "../../Core/ASTNode.h"

#include <optional>
#include <vector>

namespace XpressFormula::Plotting::Sampling {

struct ScalarGridOptions {
    Geometry::Bounds2D bounds;
    int columns = 0;
    int rows = 0;
};

struct ScalarCellGrid {
    Geometry::Bounds2D bounds;
    int columns = 0;
    int rows = 0;
    std::vector<double> values;
    double minValue = -1.0;
    double maxValue = 1.0;
    bool hasFiniteValue = false;

    [[nodiscard]] bool empty() const noexcept {
        return columns <= 0 || rows <= 0 || values.empty();
    }

    [[nodiscard]] double cellWidth() const noexcept {
        return bounds.valid() && columns > 0 ? (bounds.xMax - bounds.xMin) / columns : 0.0;
    }

    [[nodiscard]] double cellHeight() const noexcept {
        return bounds.valid() && rows > 0 ? (bounds.yMax - bounds.yMin) / rows : 0.0;
    }

    [[nodiscard]] double valueAt(int column, int row) const noexcept {
        return values[static_cast<size_t>(row * columns + column)];
    }
};

struct ScalarLattice {
    Geometry::Bounds2D bounds;
    int cellsX = 0;
    int cellsY = 0;
    std::vector<double> values;

    [[nodiscard]] bool empty() const noexcept {
        return cellsX <= 0 || cellsY <= 0 || values.empty();
    }

    [[nodiscard]] double stepX() const noexcept {
        return bounds.valid() && cellsX > 0 ? (bounds.xMax - bounds.xMin) / cellsX : 0.0;
    }

    [[nodiscard]] double stepY() const noexcept {
        return bounds.valid() && cellsY > 0 ? (bounds.yMax - bounds.yMin) / cellsY : 0.0;
    }

    [[nodiscard]] double valueAt(int ix, int iy) const noexcept {
        return values[static_cast<size_t>(iy * (cellsX + 1) + ix)];
    }
};

[[nodiscard]] ScalarCellGrid sampleScalarCellGrid(
    const Core::ASTNodePtr& ast,
    const ScalarGridOptions& options,
    std::optional<double> zValue = std::nullopt);

[[nodiscard]] ScalarLattice sampleScalarLattice(
    const Core::ASTNodePtr& ast,
    const ScalarGridOptions& options,
    std::optional<double> zValue = std::nullopt);

} // namespace XpressFormula::Plotting::Sampling
