// UiKitLayoutTests.cpp - Unit tests for pure UI toolkit layout planners.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/UiKit/ResponsiveLayout.h"
#include "../XpressFormula/UI/UiKit/UiMetrics.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::UI::UiKit;

namespace XpressFormulaTests {

TEST_CASE(PlotToolbarPlan_Wide3DUsesOneRowWithCameraButtons) {
    const UiMetrics& m = metrics();

    const PlotToolbarLayoutPlan plan = planPlotToolbar({
        m.toolbarOneRowWidth3D,
        720.0f,
        true
    });

    Assert::AreEqual(static_cast<int>(PlotToolbarLayout::OneRow), static_cast<int>(plan.layout));
    Assert::AreEqual(1, plan.rowCount);
    Assert::IsTrue(plan.showCameraButtons);
    Assert::IsFalse(plan.showCameraCombo);
    Assert::IsFalse(plan.putCameraInMoreMenu);
}

TEST_CASE(PlotToolbarPlan_Medium3DUsesTwoRows) {
    const UiMetrics& m = metrics();

    const PlotToolbarLayoutPlan plan = planPlotToolbar({
        m.toolbarOneRowWidth3D - 1.0f,
        720.0f,
        true
    });

    Assert::AreEqual(static_cast<int>(PlotToolbarLayout::TwoRow), static_cast<int>(plan.layout));
    Assert::AreEqual(2, plan.rowCount);
    Assert::IsTrue(plan.showCameraButtons);
}

TEST_CASE(PlotToolbarPlan_Narrow3DUsesCompactWithCameraCombo) {
    const UiMetrics& m = metrics();

    const PlotToolbarLayoutPlan plan = planPlotToolbar({
        m.toolbarCompactWidth3D - 1.0f,
        720.0f,
        true
    });

    Assert::AreEqual(static_cast<int>(PlotToolbarLayout::Compact), static_cast<int>(plan.layout));
    Assert::AreEqual(3, plan.rowCount);
    Assert::IsFalse(plan.showEffectiveMode);
    Assert::IsTrue(plan.showCameraCombo);
    Assert::IsFalse(plan.putCameraInMoreMenu);
}

TEST_CASE(PlotToolbarPlan_VeryNarrow3DUsesExtraCompact) {
    const UiMetrics& m = metrics();

    const PlotToolbarLayoutPlan plan = planPlotToolbar({
        m.toolbarExtraCompactWidth - 1.0f,
        720.0f,
        true
    });

    Assert::AreEqual(static_cast<int>(PlotToolbarLayout::ExtraCompact), static_cast<int>(plan.layout));
    Assert::AreEqual(2, plan.rowCount);
    Assert::IsFalse(plan.showCameraCombo);
    Assert::IsTrue(plan.putCameraInMoreMenu);
}

TEST_CASE(PlotToolbarPlan_Short3DUsesExtraCompact) {
    const UiMetrics& m = metrics();

    const PlotToolbarLayoutPlan plan = planPlotToolbar({
        m.toolbarCompactWidth3D - 1.0f,
        m.toolbarShortHeight - 1.0f,
        true
    });

    Assert::AreEqual(static_cast<int>(PlotToolbarLayout::ExtraCompact), static_cast<int>(plan.layout));
    Assert::AreEqual(2, plan.rowCount);
}

TEST_CASE(PlotToolbarPlan_2DNeverAllocatesCameraRows) {
    const UiMetrics& m = metrics();

    const PlotToolbarLayoutPlan wide = planPlotToolbar({
        m.toolbarOneRowWidth2D,
        720.0f,
        false
    });
    const PlotToolbarLayoutPlan compact = planPlotToolbar({
        m.toolbarCompactWidth2D - 1.0f,
        120.0f,
        false
    });

    Assert::AreEqual(static_cast<int>(PlotToolbarLayout::OneRow), static_cast<int>(wide.layout));
    Assert::AreEqual(1, wide.rowCount);
    Assert::IsFalse(wide.showCameraButtons);
    Assert::IsFalse(wide.showCameraCombo);
    Assert::IsFalse(wide.putCameraInMoreMenu);

    Assert::AreEqual(static_cast<int>(PlotToolbarLayout::Compact), static_cast<int>(compact.layout));
    Assert::AreEqual(2, compact.rowCount);
    Assert::IsFalse(compact.showCameraButtons);
    Assert::IsFalse(compact.showCameraCombo);
    Assert::IsFalse(compact.putCameraInMoreMenu);
}

TEST_CASE(PlotToolbarPlan_ThresholdBoundariesAreDeterministic) {
    const UiMetrics& m = metrics();

    Assert::AreEqual(
        static_cast<int>(PlotToolbarLayout::Compact),
        static_cast<int>(planPlotToolbar({ m.toolbarCompactWidth3D - 0.01f, 720.0f, true }).layout));
    Assert::AreEqual(
        static_cast<int>(PlotToolbarLayout::TwoRow),
        static_cast<int>(planPlotToolbar({ m.toolbarCompactWidth3D, 720.0f, true }).layout));
    Assert::AreEqual(
        static_cast<int>(PlotToolbarLayout::OneRow),
        static_cast<int>(planPlotToolbar({ m.toolbarOneRowWidth3D, 720.0f, true }).layout));
}

TEST_CASE(FormulaCardPlan_WideKeepsDedicatedButtons) {
    const FormulaCardLayoutPlan plan = planFormulaCard({
        700.0f,
        false,
        72.0f,
        36.0f,
        42.0f,
        32.0f,
        24.0f
    });

    Assert::AreEqual(static_cast<int>(FormulaCardLayout::Wide), static_cast<int>(plan.layout));
    Assert::AreEqual(3, plan.rowCount);
    Assert::IsTrue(plan.showStatusInHeader);
    Assert::IsTrue(plan.showEditButton);
    Assert::IsTrue(plan.showDeleteButton);
}

TEST_CASE(FormulaCardPlan_CompactHidesDedicatedButtons) {
    const UiMetrics& m = metrics();

    const FormulaCardLayoutPlan plan = planFormulaCard({
        m.formulaCardCompactWidth,
        false,
        72.0f,
        36.0f,
        42.0f,
        32.0f,
        24.0f
    });

    Assert::AreEqual(static_cast<int>(FormulaCardLayout::Compact), static_cast<int>(plan.layout));
    Assert::AreEqual(3, plan.rowCount);
    Assert::IsTrue(plan.showStatusInHeader);
    Assert::IsFalse(plan.showEditButton);
    Assert::IsFalse(plan.showDeleteButton);
}

TEST_CASE(FormulaCardPlan_ExtraCompactMovesValidationToMetadata) {
    const UiMetrics& m = metrics();

    const FormulaCardLayoutPlan plan = planFormulaCard({
        m.formulaCardExtraCompactWidth,
        false,
        72.0f,
        36.0f,
        42.0f,
        32.0f,
        24.0f
    });

    Assert::AreEqual(static_cast<int>(FormulaCardLayout::ExtraCompact), static_cast<int>(plan.layout));
    Assert::AreEqual(3, plan.rowCount);
    Assert::IsFalse(plan.showStatusInHeader);
    Assert::AreEqual(1, plan.metadataRow);
    Assert::AreEqual(2, plan.expressionRow);
}

TEST_CASE(FormulaCardPlan_ZSliceAddsRow) {
    const FormulaCardLayoutPlan plan = planFormulaCard({
        700.0f,
        true,
        72.0f,
        36.0f,
        42.0f,
        32.0f,
        24.0f
    });

    Assert::AreEqual(4, plan.rowCount);
    Assert::AreEqual(3, plan.zSliceRow);
}

TEST_CASE(FormulaCardPlan_InvalidSizesFallbackSafely) {
    const FormulaCardLayoutPlan plan = planFormulaCard({
        -100.0f,
        false,
        -1.0f,
        -1.0f,
        -1.0f,
        -1.0f,
        -1.0f
    });

    Assert::AreEqual(static_cast<int>(FormulaCardLayout::ExtraCompact), static_cast<int>(plan.layout));
    Assert::AreEqual(3, plan.rowCount);
    Assert::IsTrue(plan.useActionsMenu);
}

TEST_CASE(HorizontalSplit_EnforcesPaneLimits) {
    const HorizontalSplitResult result = resolveHorizontalSplit({
        1000.0f,
        900.0f,
        280.0f,
        600.0f,
        360.0f,
        8.0f
    });

    Assert::AreEqual(600.0f, result.firstPaneWidth);
    Assert::AreEqual(392.0f, result.secondPaneWidth);
}

TEST_CASE(HorizontalSplit_EnforcesMinimumFirstPane) {
    const HorizontalSplitResult result = resolveHorizontalSplit({
        1000.0f,
        100.0f,
        280.0f,
        600.0f,
        360.0f,
        8.0f
    });

    Assert::AreEqual(280.0f, result.firstPaneWidth);
    Assert::AreEqual(712.0f, result.secondPaneWidth);
}

TEST_CASE(HorizontalSplit_EnforcesMinimumSecondPane) {
    const HorizontalSplitResult result = resolveHorizontalSplit({
        900.0f,
        700.0f,
        280.0f,
        800.0f,
        360.0f,
        8.0f
    });

    Assert::AreEqual(532.0f, result.firstPaneWidth);
    Assert::AreEqual(360.0f, result.secondPaneWidth);
}

TEST_CASE(HorizontalSplit_HandlesVerySmallTotalWidth) {
    const HorizontalSplitResult result = resolveHorizontalSplit({
        200.0f,
        -10.0f,
        280.0f,
        600.0f,
        360.0f,
        8.0f
    });

    Assert::AreEqual(0.0f, result.firstPaneWidth);
    Assert::AreEqual(192.0f, result.secondPaneWidth);
}

TEST_CASE(ModalSizePlan_ClampsPreferredSizeToWorkArea) {
    const ModalSizePlan plan = planModalSize({
        { 1000.0f, 800.0f },
        { 0.84f, 0.84f },
        { 640.0f, 480.0f },
        { 32.0f, 32.0f }
    });

    Assert::AreEqual(840.0f, plan.size.x);
    Assert::AreEqual(672.0f, plan.size.y);
    Assert::AreEqual(968.0f, plan.maximumSize.x);
    Assert::AreEqual(768.0f, plan.maximumSize.y);
}

TEST_CASE(ModalSizePlan_LargeViewportUsesPreferredFraction) {
    const ModalSizePlan plan = planModalSize({
        { 3000.0f, 2000.0f },
        { 0.84f, 0.84f },
        { 640.0f, 480.0f },
        { 32.0f, 32.0f }
    });

    Assert::AreEqual(2520.0f, plan.size.x);
    Assert::AreEqual(1680.0f, plan.size.y);
    Assert::AreEqual(2968.0f, plan.maximumSize.x);
    Assert::AreEqual(1968.0f, plan.maximumSize.y);
}

TEST_CASE(ModalSizePlan_TinyViewportKeepsUsableFallback) {
    const ModalSizePlan plan = planModalSize({
        { 120.0f, 90.0f },
        { 0.84f, 0.84f },
        { 640.0f, 480.0f },
        { 32.0f, 32.0f }
    });

    Assert::AreEqual(88.0f, plan.size.x);
    Assert::AreEqual(58.0f, plan.size.y);
    Assert::AreEqual(88.0f, plan.minimumSize.x);
    Assert::AreEqual(58.0f, plan.minimumSize.y);
}

} // namespace XpressFormulaTests
