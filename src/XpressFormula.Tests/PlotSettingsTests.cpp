// PlotSettingsTests.cpp - Unit tests for UI plotting settings helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/PlotSettings.h"

#include <cstring>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::UI;
namespace XFModel = XpressFormula::Model;

namespace XpressFormulaTests {

static XFModel::SceneSummary sceneWith(bool has2D, bool has3D) {
    XFModel::SceneSummary scene;
    scene.hasVisibleCurve2D = has2D;
    scene.hasVisibleExplicitSurface3D = has3D;
    return scene;
}

TEST_CASE(CoordinateOverlayPolicy_DisablesAxisTriadWhenCoordinatesAreOn) {
    bool showAxisTriad = true;

    const bool changed = resolveCoordinateOverlayPolicy(true, showAxisTriad);

    Assert::IsTrue(changed);
    Assert::IsFalse(showAxisTriad);
}

TEST_CASE(CoordinateOverlayPolicy_LeavesAxisTriadWhenCoordinatesAreOff) {
    bool showAxisTriad = true;

    const bool changed = resolveCoordinateOverlayPolicy(false, showAxisTriad);

    Assert::IsFalse(changed);
    Assert::IsTrue(showAxisTriad);
}

TEST_CASE(CoordinateOverlayPolicy_ReportsEffectiveAxisTriadVisibility) {
    Assert::IsFalse(isAxisTriadVisible(true, true));
    Assert::IsFalse(isAxisTriadVisible(false, false));
    Assert::IsTrue(isAxisTriadVisible(false, true));
}

TEST_CASE(PlotSettings_DefaultOverlayStateHasNoConflict) {
    PlotSettings settings;

    Assert::IsTrue(settings.showCoordinates);
    Assert::IsFalse(settings.showAxisTriad);
    Assert::IsFalse(settings.effectiveShowAxisTriad());
}

TEST_CASE(PlotSettings_Phase6DefaultsPreferReadableSurfaces) {
    PlotSettings settings;

    Assert::AreEqual(static_cast<int>(PlotHudMode::Minimal), static_cast<int>(settings.hudMode));
    Assert::AreEqual(1.0f, settings.surfaceOpacity);
    Assert::AreEqual(0.25f, settings.wireOpacity);
    Assert::AreEqual(1.0f, settings.wireThickness);
    Assert::AreEqual(2, settings.wireStride);
}

TEST_CASE(PlotSettings_WireClampHelpersStayInSafeRange) {
    Assert::AreEqual(0.0f, clampWireOpacity(-0.5f));
    Assert::AreEqual(0.55f, clampWireOpacity(0.55f));
    Assert::AreEqual(1.0f, clampWireOpacity(1.5f));

    Assert::AreEqual(1, clampWireStride(-2));
    Assert::AreEqual(6, clampWireStride(6));
    Assert::AreEqual(16, clampWireStride(99));
}

TEST_CASE(PlotSettings_HudModeLabelsAreStable) {
    Assert::IsTrue(std::strcmp("Off", plotHudModeLabel(PlotHudMode::Off)) == 0);
    Assert::IsTrue(std::strcmp("Minimal", plotHudModeLabel(PlotHudMode::Minimal)) == 0);
    Assert::IsTrue(std::strcmp("Detailed", plotHudModeLabel(PlotHudMode::Detailed)) == 0);
    Assert::IsTrue(std::strcmp("Only While Interacting",
                               plotHudModeLabel(PlotHudMode::OnlyWhileInteracting)) == 0);
}

TEST_CASE(PlotSettings_ApplyCoordinateOverlayPolicyMutatesConflict) {
    PlotSettings settings;
    settings.showCoordinates = true;
    settings.showAxisTriad = true;

    const bool changed = settings.applyCoordinateOverlayPolicy();

    Assert::IsTrue(changed);
    Assert::IsFalse(settings.showAxisTriad);
    Assert::IsFalse(settings.effectiveShowAxisTriad());
}

TEST_CASE(PlotSettings_AutoModeKeepsExistingScenePolicy) {
    Assert::AreEqual(XYRenderMode::Heatmap2D,
                     resolveXYRenderMode(XYRenderModePreference::Auto, sceneWith(false, false)));
    Assert::AreEqual(XYRenderMode::Heatmap2D,
                     resolveXYRenderMode(XYRenderModePreference::Auto, sceneWith(true, false)));
    Assert::AreEqual(XYRenderMode::Surface3D,
                     resolveXYRenderMode(XYRenderModePreference::Auto, sceneWith(false, true)));
    Assert::AreEqual(XYRenderMode::Heatmap2D,
                     resolveXYRenderMode(XYRenderModePreference::Auto, sceneWith(true, true)));
}

TEST_CASE(PlotSettings_ForceModesOverrideSceneCapabilities) {
    Assert::AreEqual(XYRenderMode::Surface3D,
                     resolveXYRenderMode(XYRenderModePreference::Force3D, sceneWith(false, false)));
    Assert::AreEqual(XYRenderMode::Surface3D,
                     resolveXYRenderMode(XYRenderModePreference::Force3D, sceneWith(true, true)));
    Assert::AreEqual(XYRenderMode::Heatmap2D,
                     resolveXYRenderMode(XYRenderModePreference::Force2D, sceneWith(false, true)));
    Assert::AreEqual(XYRenderMode::Heatmap2D,
                     resolveXYRenderMode(XYRenderModePreference::Force2D, sceneWith(true, true)));
}

} // namespace XpressFormulaTests
