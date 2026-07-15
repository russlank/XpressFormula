// ExportSettingsTests.cpp - Unit tests for export dialog helper logic.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/ExportSettings.h"

#include <cmath>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::UI;

namespace XpressFormulaTests {

static void assertClose(double expected, double actual, double tolerance = 1e-6) {
    Assert::IsTrue(std::abs(expected - actual) <= tolerance,
        (std::to_wstring(expected) + L" != " + std::to_wstring(actual)).c_str());
}

TEST_CASE(ExportSize_ClampsDimensions) {
    int width = 2;
    int height = 20000;

    validateExportSize(width, height);

    Assert::AreEqual(kMinExportDimension, width);
    Assert::AreEqual(kMaxExportDimension, height);
}

TEST_CASE(ExportSize_AspectLockedHeightPreservesRatio) {
    const int height = aspectLockedHeight(1920, 1280, 720);

    Assert::AreEqual(1080, height);
}

TEST_CASE(ExportSize_AspectLockedWidthPreservesRatio) {
    const int width = aspectLockedWidth(1440, 1280, 720);

    Assert::AreEqual(2560, width);
}

TEST_CASE(ExportSize_PresetsIncludeCurrentAndCustom) {
    const auto& presets = exportSizePresets();

    Assert::IsTrue(presets.front().useCurrentSize);
    Assert::IsFalse(presets.front().custom);
    Assert::IsTrue(presets.back().custom);
    Assert::AreEqual(3840, presets[4].width);
    Assert::AreEqual(2160, presets[4].height);
}

TEST_CASE(ExportSize_MemoryEstimateIncludesSupersampling) {
    const auto oneX = estimateRgbaBufferBytes(1920, 1080, ExportSupersampling::Off);
    const auto twoX = estimateRgbaBufferBytes(1920, 1080, ExportSupersampling::X2);

    Assert::AreEqual(1920ull * 1080ull * 4ull, oneX);
    Assert::AreEqual(oneX * 4ull, twoX);
}

TEST_CASE(ExportSize_EffectiveSupersamplingPreservesMaxDimension) {
    const int scale = effectiveSupersamplingFactor(8192, 4096, ExportSupersampling::X4);

    Assert::AreEqual(1, scale);
}

TEST_CASE(ExportSize_BytesToMiB) {
    assertClose(1.0, bytesToMiB(1024ull * 1024ull));
}

TEST_CASE(ExportQuality_HighPresetMapsToExpectedValues) {
    const auto quality = qualitySettingsForPreset(ExportQualityPreset::High);

    Assert::AreEqual(ExportQualityPreset::High, quality.preset);
    Assert::AreEqual(80, quality.surfaceResolution);
    Assert::AreEqual(96, quality.implicitSurfaceResolution);
    assertClose(1.25, quality.wireThicknessScale);
    Assert::AreEqual(ExportSupersampling::X2, quality.supersampling);
}

TEST_CASE(ExportQuality_UltraUsesHighestSupersampling) {
    const auto quality = qualitySettingsForPreset(ExportQualityPreset::Ultra);

    Assert::AreEqual(ExportSupersampling::X4, quality.supersampling);
    Assert::IsTrue(quality.surfaceResolution > qualitySettingsForPreset(ExportQualityPreset::High).surfaceResolution);
}

TEST_CASE(ExportQuality_CustomStartsFromNormalValues) {
    const auto custom = qualitySettingsForPreset(ExportQualityPreset::Custom);
    const auto normal = qualitySettingsForPreset(ExportQualityPreset::Normal);

    Assert::AreEqual(ExportQualityPreset::Custom, custom.preset);
    Assert::AreEqual(normal.surfaceResolution, custom.surfaceResolution);
    Assert::AreEqual(normal.implicitSurfaceResolution, custom.implicitSurfaceResolution);
    Assert::AreEqual(normal.supersampling, custom.supersampling);
}

} // namespace XpressFormulaTests
