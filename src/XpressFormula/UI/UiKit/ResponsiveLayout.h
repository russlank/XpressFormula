// ResponsiveLayout.h - Pure responsive layout planners for ImGui panels.
#pragma once

#include "UiMetrics.h"

namespace XpressFormula::UI::UiKit {

enum class PlotToolbarLayout {
    OneRow,
    TwoRow,
    Compact,
    ExtraCompact
};

struct PlotToolbarLayoutInput {
    float availableWidth = 0.0f;
    float availableHeight = 0.0f;
    bool is3DMode = false;
};

struct PlotToolbarLayoutPlan {
    PlotToolbarLayout layout = PlotToolbarLayout::OneRow;
    int rowCount = 1;
    bool showEffectiveMode = true;
    bool showDisplayTogglesInline = true;
    bool showCameraButtons = false;
    bool showCameraCombo = false;
    bool putCameraInMoreMenu = false;
};

PlotToolbarLayoutPlan planPlotToolbar(const PlotToolbarLayoutInput& input,
                                      const UiMetrics& uiMetrics = metrics());

enum class FormulaCardLayout {
    Wide,
    Compact,
    ExtraCompact
};

struct FormulaCardLayoutInput {
    float availableWidth = 0.0f;
    bool hasZSlice = false;
    float titleWidth = 0.0f;
    float statusWidth = 0.0f;
    float editButtonWidth = 0.0f;
    float actionsButtonWidth = 0.0f;
    float deleteButtonWidth = 0.0f;
};

struct FormulaCardLayoutPlan {
    FormulaCardLayout layout = FormulaCardLayout::Wide;
    int rowCount = 3;
    bool showStatusInHeader = true;
    bool showEditButton = true;
    bool showDeleteButton = true;
    bool useActionsMenu = true;
    int headerRow = 0;
    int expressionRow = 1;
    int metadataRow = 2;
    int zSliceRow = -1;
};

FormulaCardLayoutPlan planFormulaCard(const FormulaCardLayoutInput& input,
                                      const UiMetrics& uiMetrics = metrics());

struct HorizontalSplitInput {
    float totalWidth = 0.0f;
    float firstPaneWidth = 0.0f;
    float minimumFirstPaneWidth = 0.0f;
    float maximumFirstPaneWidth = 0.0f;
    float minimumSecondPaneWidth = 0.0f;
    float splitterWidth = 8.0f;
};

struct HorizontalSplitResult {
    float firstPaneWidth = 0.0f;
    float secondPaneWidth = 0.0f;
};

HorizontalSplitResult resolveHorizontalSplit(const HorizontalSplitInput& input);

struct UiSize {
    float x = 0.0f;
    float y = 0.0f;
};

struct ModalSizeRequest {
    UiSize workSize{};
    UiSize preferredFraction{0.84f, 0.84f};
    UiSize minimumSize{};
    UiSize maximumMargin{32.0f, 32.0f};
};

struct ModalSizePlan {
    UiSize size{};
    UiSize minimumSize{};
    UiSize maximumSize{};
};

ModalSizePlan planModalSize(const ModalSizeRequest& request);

} // namespace XpressFormula::UI::UiKit
