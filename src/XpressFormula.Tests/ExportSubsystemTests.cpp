// ExportSubsystemTests.cpp - Unit tests for extracted export subsystem pieces.
#include "CppUnitTest.h"
#include "../XpressFormula/Application/ExportController.h"
#include "../XpressFormula/Infrastructure/Export/ExportMetadataSerializer.h"
#include "../XpressFormula/Infrastructure/Export/ExportOutputWorkflow.h"
#include "../XpressFormula/Infrastructure/Export/ExportRenderRequest.h"
#include "../XpressFormula/Infrastructure/Export/ImageProcessor.h"
#include "../XpressFormula/Infrastructure/Serialization/JsonParser.h"

#include <string>
#include <filesystem>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFApp = XpressFormula::Application;
namespace XFExport = XpressFormula::Infrastructure::Export;
namespace XFJson = XpressFormula::Infrastructure::Serialization;

namespace XpressFormulaTests {

TEST_CASE(ExportController_OpensQueuesAndPromotesDeferredActions) {
    XFApp::ExportController controller;
    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.size.width = 320;
    settings.size.height = 240;

    controller.requestOpen();
    Assert::IsTrue(controller.consumeOpenRequest());
    Assert::IsTrue(controller.dialogOpen());
    Assert::IsTrue(controller.popupOpenNextFrame());
    Assert::IsTrue(controller.centerOnOpen());
    Assert::IsTrue(controller.previewDirty());
    Assert::IsFalse(controller.consumeOpenRequest());

    controller.queueSave(settings);
    Assert::IsTrue(controller.scheduledSave());
    Assert::IsFalse(controller.pendingSave());
    controller.promoteScheduledActions();
    Assert::IsFalse(controller.scheduledSave());
    Assert::IsTrue(controller.pendingSave());
    Assert::AreEqual(320, controller.pendingSettings().size.width);
    Assert::IsTrue(controller.exportBusy());

    controller.clearPendingActions();
    Assert::IsFalse(controller.hasPendingActions());
}

TEST_CASE(ExportController_PreviewDebounceRequestsRefresh) {
    XFApp::ExportController controller;
    controller.requestOpen();
    Assert::IsTrue(controller.consumeOpenRequest());
    controller.dialogSettings().quality.autoRefreshPreview = true;
    controller.markPreviewOutOfDate(true);

    const bool refreshed = controller.tickAutoPreviewRefresh(
        XFApp::ExportController::Clock::now() + std::chrono::seconds(1));

    Assert::IsTrue(refreshed);
    Assert::IsTrue(controller.previewRefreshRequested());
    Assert::AreEqual(std::string("Rendering preview..."), controller.previewStatus());
    Assert::IsTrue(controller.consumePreviewRefreshRequest());
    Assert::IsFalse(controller.consumePreviewRefreshRequest());
}

TEST_CASE(ExportRenderRequest_BuildsSharedPreviewAndFinalState) {
    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.size.width = 100;
    settings.size.height = 50;
    settings.quality.mode = XFExport::ExportQualityMode::Override;
    settings.quality.supersampling = XFExport::ExportSupersampling::X2;
    settings.scene.showGrid = false;

    XpressFormula::Core::ViewTransform view;
    view.viewport.width = 200.0f;
    view.viewport.height = 100.0f;
    XpressFormula::Model::PlotSettings plot;
    XpressFormula::Model::SceneSummary scene;

    const XFExport::ExportRenderRequest request =
        XFExport::buildExportRenderRequest(settings, view, plot, scene);

    Assert::AreEqual(100, request.outputWidth);
    Assert::AreEqual(50, request.outputHeight);
    Assert::AreEqual(2, request.samplingFactor);
    Assert::AreEqual(200, request.targetWidth);
    Assert::AreEqual(100, request.targetHeight);
    Assert::IsTrue(request.overrides.active);
    Assert::IsFalse(request.overrides.showGrid);
    Assert::IsTrue(request.quality.overrideQuality);

    XFExport::ExportPreviewSize previewSize;
    const XFExport::ExportSettings preview =
        XFExport::settingsForPreviewRender(settings, XFExport::ExportPreviewQuality::Draft, previewSize);
    Assert::AreEqual(100, preview.size.width);
    Assert::AreEqual(50, preview.size.height);
    Assert::AreEqual(XFExport::ExportSupersampling::Off, preview.quality.supersampling);
}

TEST_CASE(ImageProcessor_ConvertsUnpremultipliesAndGrayscales) {
    std::vector<std::uint8_t> rgba = { 10, 20, 30, 40 };
    XFExport::convertPixelsRgbaToBgra(rgba);
    Assert::AreEqual(30, static_cast<int>(rgba[0]));
    Assert::AreEqual(20, static_cast<int>(rgba[1]));
    Assert::AreEqual(10, static_cast<int>(rgba[2]));
    Assert::AreEqual(40, static_cast<int>(rgba[3]));

    std::vector<std::uint8_t> premultiplied = { 10, 20, 30, 128 };
    XFExport::unpremultiplyPixels(premultiplied);
    Assert::AreEqual(20, static_cast<int>(premultiplied[0]));
    Assert::AreEqual(40, static_cast<int>(premultiplied[1]));
    Assert::AreEqual(60, static_cast<int>(premultiplied[2]));

    std::vector<std::uint8_t> bgra = { 0, 0, 255, 255 };
    XFExport::convertPixelsToGrayscaleBgra(bgra);
    Assert::AreEqual(76, static_cast<int>(bgra[0]));
    Assert::AreEqual(76, static_cast<int>(bgra[1]));
    Assert::AreEqual(76, static_cast<int>(bgra[2]));
}

TEST_CASE(ImageProcessor_PreparesFinalBgraAndResizes) {
    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.size.width = 16;
    settings.size.height = 16;

    const std::vector<std::uint8_t> rgba = { 255, 0, 0, 255 };
    const XFExport::ProcessedImage image =
        XFExport::prepareFinalBgraImage(rgba, 1, 1, settings);

    Assert::AreEqual(16 * 16 * 4, static_cast<int>(image.pixels.size()));
    Assert::AreEqual(16, image.width);
    Assert::AreEqual(16, image.height);
    Assert::AreEqual(0, static_cast<int>(image.pixels[0]));
    Assert::AreEqual(0, static_cast<int>(image.pixels[1]));
    Assert::AreEqual(255, static_cast<int>(image.pixels[2]));
    Assert::AreEqual(255, static_cast<int>(image.pixels[3]));
}

TEST_CASE(ImageProcessor_RejectsInvalidOversizedAndTooSmallBuffers) {
    std::vector<std::uint8_t> resized;
    XFExport::resizePixelsBilinear({}, 0, 1, 1, 1, resized);
    Assert::IsTrue(resized.empty());

    const std::vector<std::uint8_t> onePixel = { 255, 0, 0, 255 };
    XFExport::resizePixelsBilinear(
        onePixel,
        XFExport::kMaxExportDimension + 1,
        XFExport::kMaxExportDimension,
        1,
        1,
        resized);
    Assert::IsTrue(resized.empty());

    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.size.width = 16;
    settings.size.height = 16;
    const XFExport::ProcessedImage tooSmall =
        XFExport::prepareFinalBgraImage(onePixel, 2, 2, settings);
    Assert::IsTrue(tooSmall.pixels.empty());
    Assert::AreEqual(0, tooSmall.width);
    Assert::AreEqual(0, tooSmall.height);
}

TEST_CASE(ImageProcessor_AcceptsLargerThanRequiredSourceBuffer) {
    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.size.width = 16;
    settings.size.height = 16;

    const std::vector<std::uint8_t> rgba = {
        1, 2, 3, 255,
        99, 99, 99, 99
    };
    const XFExport::ProcessedImage image =
        XFExport::prepareFinalBgraImage(rgba, 1, 1, settings);

    Assert::AreEqual(16, image.width);
    Assert::AreEqual(16, image.height);
    Assert::AreEqual(3, static_cast<int>(image.pixels[0]));
    Assert::AreEqual(2, static_cast<int>(image.pixels[1]));
    Assert::AreEqual(1, static_cast<int>(image.pixels[2]));
}

TEST_CASE(ExportMetadataSerializer_BuildsPlainModelAndJson) {
    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.size.width = 640;
    settings.size.height = 480;

    XpressFormula::Model::Formula formula;
    formula.setExpression("sin(x)");
    formula.compile(true);
    std::vector<XpressFormula::Model::Formula> formulas;
    formulas.push_back(formula);

    XpressFormula::Core::ViewTransform view;
    XpressFormula::Model::PlotSettings plot;
    const XFExport::ExportAppMetadata app{ "XpressFormula", "1.2.3", "repo", "branch", "commit" };

    const XFExport::ExportMetadataModel model = XFExport::makeExportMetadataModel(
        settings,
        "C:/Temp/plot.png",
        std::filesystem::path(L"C:\\Temp\\plot.png"),
        640,
        480,
        app,
        formulas,
        view,
        plot);
    const std::string json = XFExport::serializeExportMetadata(model);

    XFJson::JsonValue value;
    std::string error;
    Assert::IsTrue(XFJson::JsonParser(json).parse(value, error));
    Assert::IsTrue(json.find("\"schemaVersion\"") != std::string::npos);
    Assert::IsTrue(json.find("\"expression\": \"sin(x)\"") != std::string::npos);
    Assert::IsTrue(json.find("\"profileId\": \"currentView\"") != std::string::npos);
    Assert::IsTrue(json.find("\"format\": \"PNG\"") != std::string::npos);
}

namespace {

struct FakeEncoder final : XFExport::IExportImageEncoder {
    bool succeeds = true;
    int calls = 0;

    XFExport::ExportOperationResult saveImageBgra(const std::wstring&,
                                                  std::span<const std::uint8_t>,
                                                  int,
                                                  int) override {
        ++calls;
        return succeeds
            ? XFExport::ExportOperationResult{ true, {} }
            : XFExport::ExportOperationResult{ false, "encoder failed" };
    }
};

struct FakeClipboard final : XFExport::IExportClipboard {
    bool imageSucceeds = true;
    bool textSucceeds = true;
    int imageCalls = 0;
    int textCalls = 0;

    XFExport::ExportOperationResult copyImageBgra(std::span<const std::uint8_t>,
                                                  int,
                                                  int) override {
        ++imageCalls;
        return imageSucceeds
            ? XFExport::ExportOperationResult{ true, {} }
            : XFExport::ExportOperationResult{ false, "clipboard failed" };
    }

    XFExport::ExportOperationResult copyText(const std::wstring&) override {
        ++textCalls;
        return textSucceeds
            ? XFExport::ExportOperationResult{ true, {} }
            : XFExport::ExportOperationResult{ false, "text failed" };
    }
};

struct FakeShell final : XFExport::IExportShell {
    bool openSucceeds = true;
    bool revealSucceeds = true;
    int openCalls = 0;
    int revealCalls = 0;

    bool openPath(const std::wstring&) override {
        ++openCalls;
        return openSucceeds;
    }

    bool revealPath(const std::wstring&) override {
        ++revealCalls;
        return revealSucceeds;
    }
};

struct FakeMetadataWriter final : XFExport::IExportMetadataWriter {
    bool succeeds = true;
    int calls = 0;

    XFExport::ExportOperationResult writeSidecar(const XFExport::ExportSettings&,
                                                 const std::wstring&,
                                                 int,
                                                 int,
                                                 std::wstring& sidecarPath) override {
        ++calls;
        sidecarPath = L"C:\\Temp\\plot.png.json";
        return succeeds
            ? XFExport::ExportOperationResult{ true, {} }
            : XFExport::ExportOperationResult{ false, "metadata failed" };
    }
};

} // namespace

TEST_CASE(ExportOutputWorkflow_SaveSuccessRunsPostActions) {
    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.output.saveMetadataSidecar = true;
    settings.output.openAfterSave = true;
    settings.output.showInFolderAfterSave = true;
    settings.output.copyPathAfterSave = true;

    FakeEncoder encoder;
    FakeMetadataWriter metadata;
    FakeShell shell;
    FakeClipboard clipboard;
    const std::vector<std::uint8_t> pixels = { 0, 0, 255, 255 };

    const XFExport::ExportOutputResult result = XFExport::saveRenderedImage(
        settings, L"C:\\Temp\\plot.png", pixels, 1, 1,
        encoder, metadata, shell, clipboard, "C:/Temp/plot.png", "C:/Temp/plot.png.json");

    Assert::IsTrue(result.savedImage);
    Assert::AreEqual(1, encoder.calls);
    Assert::AreEqual(1, metadata.calls);
    Assert::AreEqual(1, shell.openCalls);
    Assert::AreEqual(1, shell.revealCalls);
    Assert::AreEqual(1, clipboard.textCalls);
    Assert::AreEqual(5, static_cast<int>(result.messages.size()));
}

TEST_CASE(ExportOutputWorkflow_MetadataFailureKeepsImageSuccess) {
    XFExport::ExportSettings settings = XFExport::defaultExportSettings();
    settings.output.saveMetadataSidecar = true;

    FakeEncoder encoder;
    FakeMetadataWriter metadata;
    metadata.succeeds = false;
    FakeShell shell;
    FakeClipboard clipboard;
    const std::vector<std::uint8_t> pixels = { 0, 0, 255, 255 };

    const XFExport::ExportOutputResult result = XFExport::saveRenderedImage(
        settings, L"C:\\Temp\\plot.png", pixels, 1, 1,
        encoder, metadata, shell, clipboard, "C:/Temp/plot.png");

    Assert::IsTrue(result.savedImage);
    Assert::AreEqual(2, static_cast<int>(result.messages.size()));
    Assert::IsTrue(result.messages[1].find("Metadata sidecar failed") != std::string::npos);
}

TEST_CASE(ExportOutputWorkflow_CopyFailureReportsClipboardError) {
    FakeClipboard clipboard;
    clipboard.imageSucceeds = false;
    const std::vector<std::uint8_t> pixels = { 0, 0, 255, 255 };

    const XFExport::ExportOutputResult result =
        XFExport::copyRenderedImage(pixels, 1, 1, clipboard);

    Assert::IsFalse(result.copiedImage);
    Assert::AreEqual(1, clipboard.imageCalls);
    Assert::AreEqual(std::string("Clipboard copy failed: clipboard failed"), result.messages[0]);
}

} // namespace XpressFormulaTests
