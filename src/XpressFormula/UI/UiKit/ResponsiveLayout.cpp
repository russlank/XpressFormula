// ResponsiveLayout.cpp - Pure responsive layout planner implementations.
#include "ResponsiveLayout.h"

#include <algorithm>
#include <cmath>

namespace XpressFormula::UI::UiKit {

namespace {

float safeNonNegative(float value) {
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

} // namespace

PlotToolbarLayoutPlan planPlotToolbar(const PlotToolbarLayoutInput& input,
                                      const UiMetrics& metrics) {
    const float width = safeNonNegative(input.availableWidth);
    const float height = safeNonNegative(input.availableHeight);
    const bool compact = width < (input.is3DMode
        ? metrics.toolbarCompactWidth3D
        : metrics.toolbarCompactWidth2D);
    const bool extraCompact =
        input.is3DMode &&
        compact &&
        (width < metrics.toolbarExtraCompactWidth || height < metrics.toolbarShortHeight);
    const bool oneRow = !compact &&
        ((input.is3DMode && width >= metrics.toolbarOneRowWidth3D) ||
         (!input.is3DMode && width >= metrics.toolbarOneRowWidth2D));

    PlotToolbarLayoutPlan plan;
    plan.layout = extraCompact ? PlotToolbarLayout::ExtraCompact
        : (compact ? PlotToolbarLayout::Compact
        : (oneRow ? PlotToolbarLayout::OneRow : PlotToolbarLayout::TwoRow));

    switch (plan.layout) {
        case PlotToolbarLayout::OneRow:
            plan.rowCount = 1;
            plan.showEffectiveMode = true;
            plan.showDisplayTogglesInline = true;
            plan.showCameraButtons = input.is3DMode;
            break;
        case PlotToolbarLayout::TwoRow:
            plan.rowCount = 2;
            plan.showEffectiveMode = true;
            plan.showDisplayTogglesInline = true;
            plan.showCameraButtons = input.is3DMode;
            break;
        case PlotToolbarLayout::Compact:
            plan.rowCount = input.is3DMode ? 3 : 2;
            plan.showEffectiveMode = false;
            plan.showDisplayTogglesInline = false;
            plan.showCameraCombo = input.is3DMode;
            break;
        case PlotToolbarLayout::ExtraCompact:
            plan.rowCount = 2;
            plan.showEffectiveMode = false;
            plan.showDisplayTogglesInline = false;
            plan.putCameraInMoreMenu = input.is3DMode;
            break;
    }

    if (!input.is3DMode) {
        plan.showCameraButtons = false;
        plan.showCameraCombo = false;
        plan.putCameraInMoreMenu = false;
    }
    return plan;
}

FormulaCardLayoutPlan planFormulaCard(const FormulaCardLayoutInput& input,
                                      const UiMetrics& metrics) {
    const float width = safeNonNegative(input.availableWidth);
    const float controlsWidth = 72.0f; // visibility + color controls and spacing.
    const float rightButtonsWidth =
        std::max(0.0f, input.editButtonWidth) +
        std::max(0.0f, input.actionsButtonWidth) +
        std::max(0.0f, input.deleteButtonWidth) +
        metrics.normalSpacing * 2.0f;
    const float titleStatusWidth =
        std::max(0.0f, input.titleWidth) +
        std::max(0.0f, input.statusWidth) +
        metrics.normalSpacing;
    const float wideRequiredWidth =
        controlsWidth + titleStatusWidth + rightButtonsWidth + metrics.cardPaddingX * 2.0f;

    FormulaCardLayoutPlan plan;
    if (width <= metrics.formulaCardExtraCompactWidth || width <= 0.0f) {
        plan.layout = FormulaCardLayout::ExtraCompact;
    } else if (width <= metrics.formulaCardCompactWidth || width < wideRequiredWidth) {
        plan.layout = FormulaCardLayout::Compact;
    } else {
        plan.layout = FormulaCardLayout::Wide;
    }

    switch (plan.layout) {
        case FormulaCardLayout::Wide:
            plan.showStatusInHeader = true;
            plan.showEditButton = true;
            plan.showDeleteButton = true;
            plan.headerRow = 0;
            plan.expressionRow = 1;
            plan.metadataRow = 2;
            plan.rowCount = 3;
            break;
        case FormulaCardLayout::Compact:
            plan.showStatusInHeader = true;
            plan.showEditButton = false;
            plan.showDeleteButton = false;
            plan.headerRow = 0;
            plan.expressionRow = 1;
            plan.metadataRow = 2;
            plan.rowCount = 3;
            break;
        case FormulaCardLayout::ExtraCompact:
            plan.showStatusInHeader = false;
            plan.showEditButton = false;
            plan.showDeleteButton = false;
            plan.headerRow = 0;
            plan.metadataRow = 1;
            plan.expressionRow = 2;
            plan.rowCount = 3;
            break;
    }

    if (input.hasZSlice) {
        plan.zSliceRow = plan.rowCount;
        ++plan.rowCount;
    }
    return plan;
}

HorizontalSplitResult resolveHorizontalSplit(const HorizontalSplitInput& input) {
    const float total = safeNonNegative(input.totalWidth);
    const float splitter = safeNonNegative(input.splitterWidth);
    const float available = std::max(0.0f, total - splitter);
    const float minFirst = safeNonNegative(input.minimumFirstPaneWidth);
    const float minSecond = safeNonNegative(input.minimumSecondPaneWidth);

    float maxFirst = input.maximumFirstPaneWidth > 0.0f
        ? safeNonNegative(input.maximumFirstPaneWidth)
        : available;
    maxFirst = std::min(maxFirst, available);

    if (available <= 0.0f) {
        return {};
    }

    const float secondConstrainedMaxFirst = std::max(0.0f, available - minSecond);
    maxFirst = std::min(maxFirst, secondConstrainedMaxFirst);
    const float lower = std::min(minFirst, maxFirst);
    const float first = std::clamp(safeNonNegative(input.firstPaneWidth), lower, maxFirst);
    return { first, std::max(0.0f, available - first) };
}

ModalSizePlan planModalSize(const ModalSizeRequest& request) {
    const float workX = std::max(1.0f, safeNonNegative(request.workSize.x));
    const float workY = std::max(1.0f, safeNonNegative(request.workSize.y));
    const float marginX = safeNonNegative(request.maximumMargin.x);
    const float marginY = safeNonNegative(request.maximumMargin.y);

    ModalSizePlan plan;
    plan.maximumSize = {
        std::max(1.0f, workX - marginX),
        std::max(1.0f, workY - marginY)
    };
    plan.minimumSize = {
        std::min(safeNonNegative(request.minimumSize.x), plan.maximumSize.x),
        std::min(safeNonNegative(request.minimumSize.y), plan.maximumSize.y)
    };

    const float preferredX = workX * std::clamp(request.preferredFraction.x, 0.0f, 1.0f);
    const float preferredY = workY * std::clamp(request.preferredFraction.y, 0.0f, 1.0f);
    plan.size = {
        std::clamp(preferredX, plan.minimumSize.x, plan.maximumSize.x),
        std::clamp(preferredY, plan.minimumSize.y, plan.maximumSize.y)
    };
    return plan;
}

} // namespace XpressFormula::UI::UiKit
