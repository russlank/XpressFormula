// ExportSettingsTests.cpp - Unit tests for export dialog helper logic.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/ExportMetadata.h"
#include "../XpressFormula/UI/ExportSettings.h"

#include <cmath>
#include <filesystem>
#include <string>

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

TEST_CASE(ExportProfile_ListIncludesCompleteProfileSet) {
    const auto& profiles = exportProfiles();

    Assert::AreEqual(6, static_cast<int>(profiles.size()));
    Assert::IsTrue(std::string(exportProfileLabel(profiles[0])) == "Current View");
    Assert::IsTrue(std::string(exportProfileLabel(profiles[1])) == "Presentation");
    Assert::IsTrue(std::string(exportProfileLabel(profiles[2])) == "Transparent Illustration");
    Assert::IsTrue(std::string(exportProfileLabel(profiles[3])) == "Print Grayscale");
    Assert::IsTrue(std::string(exportProfileLabel(profiles[4])) == "High-Quality 3D");
    Assert::IsTrue(std::string(exportProfileLabel(profiles[5])) == "Custom");
}

TEST_CASE(ExportProfile_PresentationUsesHighQualityWidePngDefaults) {
    const auto settings = exportProfileSettings(ExportProfile::Presentation);

    Assert::AreEqual(kExportSizePreset1920x1080, settings.selectedSizePreset);
    Assert::AreEqual(ExportFormat::Png, settings.format);
    Assert::AreEqual(ExportQualityMode::Override, settings.qualityMode);
    Assert::AreEqual(ExportQualityPreset::High, settings.quality.preset);
    Assert::IsTrue(settings.showGrid);
    Assert::IsTrue(settings.showCoordinates);
    Assert::IsFalse(settings.saveMetadataSidecar);
}

TEST_CASE(ExportProfile_TransparentIllustrationUsesAlphaAndMetadata) {
    const auto settings = exportProfileSettings(ExportProfile::TransparentIllustration);

    Assert::AreEqual(kExportSizePreset2048x2048, settings.selectedSizePreset);
    Assert::AreEqual(ExportBackgroundMode::Transparent, settings.backgroundMode);
    Assert::AreEqual(ExportFormat::Png, settings.format);
    Assert::AreEqual(ExportQualityPreset::High, settings.quality.preset);
    Assert::IsFalse(settings.showGrid);
    Assert::IsFalse(settings.showCoordinates);
    Assert::IsTrue(settings.showWires);
    Assert::IsTrue(settings.saveMetadataSidecar);
}

TEST_CASE(ExportProfile_PrintGrayscaleUsesWhiteBoundsAndNoWires) {
    const auto settings = exportProfileSettings(ExportProfile::PrintGrayscale);

    Assert::AreEqual(ExportBackgroundMode::White, settings.backgroundMode);
    Assert::AreEqual(ExportAspectMode::PreserveVisibleBounds, settings.aspectMode);
    Assert::IsTrue(settings.grayscaleOutput);
    Assert::IsFalse(settings.showWires);
    Assert::IsFalse(settings.showEnvelope);
    Assert::IsTrue(settings.saveMetadataSidecar);
}

TEST_CASE(ExportProfile_HighQuality3DUsesUltraQualityAndAxisTriad) {
    const auto settings = exportProfileSettings(ExportProfile::HighQuality3D);

    Assert::AreEqual(kExportSizePreset3840x2160, settings.selectedSizePreset);
    Assert::AreEqual(ExportQualityMode::Override, settings.qualityMode);
    Assert::AreEqual(ExportQualityPreset::Ultra, settings.quality.preset);
    Assert::AreEqual(ExportPreviewQuality::Draft, settings.previewQuality);
    Assert::IsFalse(settings.showCoordinates);
    Assert::IsTrue(settings.showAxisTriad);
    Assert::IsTrue(settings.saveMetadataSidecar);
}

TEST_CASE(ExportMetadata_EscapesJsonStrings) {
    const std::string escaped = jsonEscape("line\n\"quoted\"\\path\t\x01");

    Assert::IsTrue(escaped == "line\\n\\\"quoted\\\"\\\\path\\t\\u0001");
}

TEST_CASE(ExportMetadata_SchemaVersionIsVersioned) {
    Assert::AreEqual(1, kExportMetadataSchemaVersion);
}

TEST_CASE(ExportMetadata_SidecarAndTempPathsAppendSuffixes) {
    const std::filesystem::path sidecar =
        exportMetadataSidecarPath(std::filesystem::path(L"C:\\Temp\\plot.png"));
    const std::filesystem::path temporary = exportMetadataTempPath(sidecar);
    const std::wstring sidecarName = sidecar.filename().wstring();
    const std::wstring temporaryName = temporary.filename().wstring();

    Assert::IsTrue(sidecarName == L"plot.png.json");
    Assert::IsTrue(temporaryName == L"plot.png.json.tmp");
}

TEST_CASE(ExportAspect_PreserveProportionsSquareExpandsY) {
    const ExportWorldBounds source{ -4.0, 4.0, -3.0, 3.0 };
    const auto resolved = resolveExportView(1000, 1000, source,
                                            ExportAspectMode::PreserveMathematicalScale);

    Assert::IsTrue(resolved.uniformScale);
    assertClose(125.0, resolved.scaleX);
    assertClose(resolved.scaleX, resolved.scaleY);
    assertClose(-4.0, resolved.visibleBounds.xMin);
    assertClose(4.0, resolved.visibleBounds.xMax);
    assertClose(-4.0, resolved.visibleBounds.yMin);
    assertClose(4.0, resolved.visibleBounds.yMax);
}

TEST_CASE(ExportAspect_PreserveProportionsPortraitKeepsUniformScale) {
    const ExportWorldBounds source{ -4.0, 4.0, -3.0, 3.0 };
    const auto resolved = resolveExportView(800, 1200, source,
                                            ExportAspectMode::PreserveMathematicalScale);

    Assert::IsTrue(resolved.uniformScale);
    assertClose(resolved.scaleX, resolved.scaleY);
    assertClose(100.0, resolved.scaleX);
    assertClose(12.0, worldBoundsHeight(resolved.visibleBounds));
}

TEST_CASE(ExportAspect_StretchToOutputUsesNonUniformScale) {
    const ExportWorldBounds source{ -4.0, 4.0, -3.0, 3.0 };
    const auto resolved = resolveExportView(1000, 1000, source,
                                            ExportAspectMode::StretchToOutput);

    Assert::IsFalse(resolved.uniformScale);
    assertClose(125.0, resolved.scaleX);
    assertClose(1000.0 / 6.0, resolved.scaleY);
    assertClose(source.xMin, resolved.visibleBounds.xMin);
    assertClose(source.yMin, resolved.visibleBounds.yMin);
}

TEST_CASE(ExportAspect_PreserveVisibleBoundsAddsCenteredMargins) {
    const ExportWorldBounds source{ -4.0, 4.0, -3.0, 3.0 };
    const auto resolved = resolveExportView(1000, 1000, source,
                                            ExportAspectMode::PreserveVisibleBounds);

    Assert::IsTrue(resolved.uniformScale);
    assertClose(source.xMin, resolved.visibleBounds.xMin);
    assertClose(source.xMax, resolved.visibleBounds.xMax);
    assertClose(source.yMin, resolved.visibleBounds.yMin);
    assertClose(source.yMax, resolved.visibleBounds.yMax);
    assertClose(0.0, resolved.marginLeftPx);
    assertClose(0.0, resolved.marginRightPx);
    assertClose(125.0, resolved.marginTopPx);
    assertClose(125.0, resolved.marginBottomPx);
}

TEST_CASE(ExportAspect_CropToFillCropsExpectedAxis) {
    const ExportWorldBounds source{ -4.0, 4.0, -3.0, 3.0 };
    const auto resolved = resolveExportView(1000, 1000, source,
                                            ExportAspectMode::CropToFill);

    Assert::IsTrue(resolved.uniformScale);
    assertClose(1000.0 / 6.0, resolved.scaleX);
    assertClose(resolved.scaleX, resolved.scaleY);
    assertClose(6.0, worldBoundsWidth(resolved.visibleBounds));
    assertClose(6.0, worldBoundsHeight(resolved.visibleBounds));
}

TEST_CASE(ExportAspect_NormalizesDegenerateBounds) {
    const ExportWorldBounds source{ 2.0, 2.0, 5.0, 5.0 };
    const auto resolved = resolveExportView(640, 480, source,
                                            ExportAspectMode::PreserveMathematicalScale);

    Assert::IsTrue(resolved.scaleX > 0.0);
    Assert::IsTrue(resolved.scaleY > 0.0);
    assertClose(resolved.scaleX, resolved.scaleY);
    Assert::IsTrue(worldBoundsWidth(resolved.visibleBounds) > 0.0);
    Assert::IsTrue(worldBoundsHeight(resolved.visibleBounds) > 0.0);
}

TEST_CASE(ExportAspect_SupersamplingDoesNotAlterWorldBounds) {
    const ExportWorldBounds source{ -5.0, 7.0, -2.0, 4.0 };
    const auto oneX = resolveExportView(1920, 1080, source,
                                        ExportAspectMode::PreserveMathematicalScale);
    const auto twoX = resolveExportView(3840, 2160, source,
                                        ExportAspectMode::PreserveMathematicalScale);

    assertClose(oneX.visibleBounds.xMin, twoX.visibleBounds.xMin);
    assertClose(oneX.visibleBounds.xMax, twoX.visibleBounds.xMax);
    assertClose(oneX.visibleBounds.yMin, twoX.visibleBounds.yMin);
    assertClose(oneX.visibleBounds.yMax, twoX.visibleBounds.yMax);
}

TEST_CASE(ExportPreviewSize_DraftCapsLongestSide) {
    const auto preview = resolveExportPreviewSize(3840, 2160, ExportPreviewQuality::Draft);

    Assert::AreEqual(520, preview.width);
    Assert::AreEqual(293, preview.height);
    Assert::IsTrue(preview.reducedFromOutput);
}

TEST_CASE(ExportPreviewSize_NormalKeepsSmallOutput) {
    const auto preview = resolveExportPreviewSize(640, 480, ExportPreviewQuality::Normal);

    Assert::AreEqual(640, preview.width);
    Assert::AreEqual(480, preview.height);
    Assert::IsFalse(preview.reducedFromOutput);
}

TEST_CASE(ExportPreviewSize_FinalUsesOutputSize) {
    const auto preview = resolveExportPreviewSize(3840, 2160, ExportPreviewQuality::Final);

    Assert::AreEqual(3840, preview.width);
    Assert::AreEqual(2160, preview.height);
    Assert::IsFalse(preview.reducedFromOutput);
}

} // namespace XpressFormulaTests
