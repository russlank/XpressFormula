// PlotSettingsTests.cpp - Unit tests for UI plotting settings helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/PlotSettings.h"

#include <cstring>
#include <limits>

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

TEST_CASE(PlotSettings_DefaultPlotSettingsMatchesPolicyDefaults) {
    const PlotSettings settings = defaultPlotSettings();
    const PlotDefaults& defaults = plotDefaults();

    Assert::AreEqual(static_cast<int>(defaults.xyRenderModePreference),
                     static_cast<int>(settings.xyRenderModePreference));
    Assert::AreEqual(static_cast<int>(defaults.hudMode), static_cast<int>(settings.hudMode));
    Assert::AreEqual(defaults.optimizeRendering, settings.optimizeRendering);
    Assert::AreEqual(defaults.showGrid, settings.showGrid);
    Assert::AreEqual(defaults.showCoordinates, settings.showCoordinates);
    Assert::AreEqual(defaults.showWires, settings.showWires);
    Assert::AreEqual(defaults.azimuthDeg, settings.azimuthDeg);
    Assert::AreEqual(defaults.elevationDeg, settings.elevationDeg);
    Assert::AreEqual(defaults.zScale, settings.zScale);
    Assert::AreEqual(defaults.surfaceResolution, settings.surfaceResolution);
    Assert::AreEqual(defaults.implicitSurfaceResolution, settings.implicitSurfaceResolution);
    Assert::AreEqual(defaults.surfaceOpacity, settings.surfaceOpacity);
    Assert::AreEqual(defaults.wireOpacity, settings.wireOpacity);
    Assert::AreEqual(defaults.wireThickness, settings.wireThickness);
    Assert::AreEqual(defaults.wireStride, settings.wireStride);
    Assert::AreEqual(defaults.showSurfaceEnvelope, settings.showSurfaceEnvelope);
    Assert::AreEqual(defaults.envelopeThickness, settings.envelopeThickness);
    Assert::AreEqual(defaults.showAxisTriad, settings.showAxisTriad);
    Assert::AreEqual(defaults.autoRotate, settings.autoRotate);
    Assert::AreEqual(defaults.autoRotateSpeedDegPerSec, settings.autoRotateSpeedDegPerSec);
    Assert::AreEqual(defaults.heatmapOpacity, settings.heatmapOpacity);
}

TEST_CASE(PlotSettings_WireClampHelpersStayInSafeRange) {
    Assert::AreEqual(0.0f, clampWireOpacity(-0.5f));
    Assert::AreEqual(0.55f, clampWireOpacity(0.55f));
    Assert::AreEqual(1.0f, clampWireOpacity(1.5f));

    Assert::AreEqual(1, clampWireStride(-2));
    Assert::AreEqual(6, clampWireStride(6));
    Assert::AreEqual(16, clampWireStride(99));
}

TEST_CASE(PlotSettings_NormalizeClampsEveryRangeAndFallsBackForNonFinite) {
    PlotSettings settings;
    settings.xyRenderModePreference = static_cast<XYRenderModePreference>(99);
    settings.hudMode = static_cast<PlotHudMode>(99);
    settings.azimuthDeg = std::numeric_limits<float>::infinity();
    settings.elevationDeg = -999.0f;
    settings.zScale = 99.0f;
    settings.surfaceResolution = 999;
    settings.implicitSurfaceResolution = -10;
    settings.surfaceOpacity = std::numeric_limits<float>::quiet_NaN();
    settings.wireOpacity = 2.0f;
    settings.wireThickness = std::numeric_limits<float>::quiet_NaN();
    settings.wireStride = 99;
    settings.envelopeThickness = std::numeric_limits<float>::infinity();
    settings.autoRotateSpeedDegPerSec = -10.0f;
    settings.heatmapOpacity = -1.0f;
    settings.showCoordinates = true;
    settings.showAxisTriad = true;

    normalizePlotSettings(settings);

    Assert::AreEqual(static_cast<int>(XYRenderModePreference::Auto),
                     static_cast<int>(settings.xyRenderModePreference));
    Assert::AreEqual(static_cast<int>(PlotHudMode::Minimal), static_cast<int>(settings.hudMode));
    Assert::AreEqual(kDefaultAzimuthDeg, settings.azimuthDeg);
    Assert::AreEqual(plotLimits().elevationDeg.min, settings.elevationDeg);
    Assert::AreEqual(plotLimits().zScale.max, settings.zScale);
    Assert::AreEqual(plotLimits().surfaceResolution.max, settings.surfaceResolution);
    Assert::AreEqual(plotLimits().implicitSurfaceResolution.min,
                     settings.implicitSurfaceResolution);
    Assert::AreEqual(kDefaultSurfaceOpacity, settings.surfaceOpacity);
    Assert::AreEqual(plotLimits().wireOpacity.max, settings.wireOpacity);
    Assert::AreEqual(kDefaultWireThickness, settings.wireThickness);
    Assert::AreEqual(plotLimits().wireStride.max, settings.wireStride);
    Assert::AreEqual(kDefaultEnvelopeThickness, settings.envelopeThickness);
    Assert::AreEqual(plotLimits().autoRotateSpeedDegPerSec.min,
                     settings.autoRotateSpeedDegPerSec);
    Assert::AreEqual(plotLimits().heatmapOpacity.min, settings.heatmapOpacity);
    Assert::IsFalse(settings.showAxisTriad);
}

TEST_CASE(PlotSettings_ValidateReportsRangesAndOverlayConflict) {
    PlotSettings settings;
    settings.surfaceResolution = plotLimits().surfaceResolution.max + 1;
    settings.showCoordinates = true;
    settings.showAxisTriad = true;

    const ValidationResult result = validatePlotSettings(settings);

    Assert::IsFalse(result.valid);
    Assert::IsTrue(result.messages.size() >= 2);
}

TEST_CASE(PlotSettings_HudModeLabelsAreStable) {
    Assert::IsTrue(std::strcmp("Off", plotHudModeLabel(PlotHudMode::Off)) == 0);
    Assert::IsTrue(std::strcmp("Minimal", plotHudModeLabel(PlotHudMode::Minimal)) == 0);
    Assert::IsTrue(std::strcmp("Detailed", plotHudModeLabel(PlotHudMode::Detailed)) == 0);
    Assert::IsTrue(std::strcmp("Only While Interacting",
                               plotHudModeLabel(PlotHudMode::OnlyWhileInteracting)) == 0);
}

TEST_CASE(PlotSettings_StorageNamesAreStableAndParseLegacyLabels) {
    Assert::IsTrue(toStorageName(XYRenderModePreference::Auto) == "auto");
    Assert::IsTrue(toStorageName(XYRenderModePreference::Force3D) == "force3D");
    Assert::IsTrue(toStorageName(PlotHudMode::OnlyWhileInteracting) == "onlyWhileInteracting");

    XYRenderModePreference preference = XYRenderModePreference::Auto;
    PlotHudMode hudMode = PlotHudMode::Minimal;

    Assert::IsTrue(parseXYRenderModePreferenceStorageName("3D", preference));
    Assert::AreEqual(static_cast<int>(XYRenderModePreference::Force3D),
                     static_cast<int>(preference));
    Assert::IsTrue(parsePlotHudModeStorageName("Only While Interacting", hudMode));
    Assert::AreEqual(static_cast<int>(PlotHudMode::OnlyWhileInteracting),
                     static_cast<int>(hudMode));
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

TEST_CASE(PlotSettings_EffectiveSettingsApplyOverridePrecedence) {
    PlotSettings base;
    base.showGrid = true;
    base.showCoordinates = false;
    base.showAxisTriad = true;
    base.surfaceResolution = 64;
    base.implicitSurfaceResolution = 80;
    base.wireThickness = 2.0f;

    PlotRenderOverrides overrides;
    overrides.active = true;
    overrides.showGrid = false;
    overrides.showCoordinates = true;
    overrides.showAxisTriad = true;
    overrides.showWires = false;

    PlotQualityDecision quality;
    quality.overrideQuality = true;
    quality.surfaceResolution = 120;
    quality.implicitSurfaceResolution = 128;
    quality.wireThicknessScale = 2.0f;

    const EffectivePlotSettings effective =
        resolveEffectivePlotSettings(base, overrides, quality, sceneWith(false, true));

    Assert::AreEqual(XYRenderMode::Surface3D, effective.renderMode);
    Assert::IsFalse(effective.showGrid);
    Assert::IsTrue(effective.showCoordinates);
    Assert::IsFalse(effective.showAxisTriad);
    Assert::IsFalse(effective.showWires);
    Assert::AreEqual(120, effective.surfaceResolution);
    Assert::AreEqual(128, effective.implicitSurfaceResolution);
    Assert::AreEqual(0.0f, effective.wireThickness);
}

TEST_CASE(PlotSettings_EffectiveInteractiveQualityThrottleIsCentralized) {
    PlotSettings base;
    base.surfaceResolution = 90;
    base.implicitSurfaceResolution = 100;
    base.wireThickness = 1.5f;

    PlotQualityDecision quality;
    quality.interactiveThrottle = true;

    const EffectivePlotSettings effective =
        resolveEffectivePlotSettings(base, PlotRenderOverrides{}, quality, sceneWith(false, true));

    Assert::AreEqual(60, effective.surfaceResolution);
    Assert::AreEqual(40, effective.implicitSurfaceResolution);
    Assert::AreEqual(0.0f, effective.wireThickness);
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
