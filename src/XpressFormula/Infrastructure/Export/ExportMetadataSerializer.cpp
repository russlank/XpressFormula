// SPDX-License-Identifier: MIT
// ExportMetadataSerializer.cpp - Plain export metadata model and JSON serializer.
#include "ExportMetadataSerializer.h"
#include "../FileSystem/AtomicFileWriter.h"
#include "../Serialization/JsonWriter.h"

#include "../../Expression/FormulaKind.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace XpressFormula::Infrastructure::Export {
namespace {

namespace XFJson = XpressFormula::Infrastructure::Serialization;

enum class FormulaRenderKind {
    Curve2D,
    Surface3D,
    Implicit2D,
    ScalarField3D,
    Invalid
};

FormulaRenderKind formulaRenderKindFor(Expression::FormulaKind kind) {
    switch (kind) {
        case Expression::FormulaKind::Curve2D:
            return FormulaRenderKind::Curve2D;
        case Expression::FormulaKind::ExplicitSurface3D:
            return FormulaRenderKind::Surface3D;
        case Expression::FormulaKind::ImplicitContour2D:
            return FormulaRenderKind::Implicit2D;
        case Expression::FormulaKind::ScalarField3D:
        case Expression::FormulaKind::ImplicitSurface3D:
            return FormulaRenderKind::ScalarField3D;
        default:
            return FormulaRenderKind::Invalid;
    }
}

const char* formulaRenderKindLabel(FormulaRenderKind kind) {
    switch (kind) {
        case FormulaRenderKind::Curve2D: return "Curve2D";
        case FormulaRenderKind::Surface3D: return "Surface3D";
        case FormulaRenderKind::Implicit2D: return "Implicit2D";
        case FormulaRenderKind::ScalarField3D: return "ScalarField3D";
        case FormulaRenderKind::Invalid: return "Invalid";
        default: return "Invalid";
    }
}

const char* formulaTypeLabel(FormulaRenderKind renderKind, bool isEquation) {
    switch (renderKind) {
        case FormulaRenderKind::Curve2D:
            return "y = f(x)";
        case FormulaRenderKind::Surface3D:
            return "z = f(x,y)";
        case FormulaRenderKind::Implicit2D:
            return "F(x,y) = 0";
        case FormulaRenderKind::ScalarField3D:
            return isEquation ? "F(x,y,z) = 0" : "f(x,y,z)";
        default:
            return "invalid";
    }
}

void writeColor(XFJson::JsonWriter& writer, const std::array<float, 4>& color) {
    writer.beginArray();
    writer.value(color[0]);
    writer.value(color[1]);
    writer.value(color[2]);
    writer.value(color[3]);
    writer.endArray();
}

} // namespace

std::string jsonEscape(std::string_view text) {
    return XFJson::jsonEscape(text);
}

const char* jsonBool(bool value) {
    return XFJson::jsonBool(value);
}

std::filesystem::path exportMetadataSidecarPath(const std::filesystem::path& imagePath) {
    std::filesystem::path sidecar = imagePath;
    sidecar += L".json";
    return sidecar;
}

std::filesystem::path exportMetadataTempPath(const std::filesystem::path& sidecarPath) {
    return FileSystem::atomicTempPathFor(sidecarPath);
}

std::string exportActualFormatLabel(const ExportSettings& settings,
                                    const std::filesystem::path& imagePath) {
    std::string actualFormat = exportFormatLabel(settings.output.format);
    std::wstring extension = imagePath.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    if (extension == L".bmp") {
        actualFormat = "BMP";
    } else if (extension == L".png") {
        actualFormat = "PNG";
    }
    return actualFormat;
}

ExportFormulaMetadata makeExportFormulaMetadata(const Model::Formula& formula,
                                                size_t oneBasedIndex) {
    const FormulaRenderKind renderKind = formulaRenderKindFor(formula.compiled.kind);

    ExportFormulaMetadata metadata;
    metadata.index = oneBasedIndex;
    metadata.expression = formula.expression;
    metadata.visible = formula.visible;
    metadata.color = formula.color.channels;
    metadata.valid = formula.isValid();
    metadata.type = formulaTypeLabel(renderKind, formula.compiled.equation);
    metadata.renderKind = formulaRenderKindLabel(renderKind);
    metadata.equation = formula.compiled.equation;
    metadata.variableCount = Expression::variableCountForKind(formula.compiled.kind);
    metadata.variables.assign(formula.compiled.variables.begin(), formula.compiled.variables.end());
    metadata.zSlice = formula.zSlice;
    metadata.error = formula.diagnosticMessage();
    return metadata;
}

ExportViewMetadata makeExportViewMetadata(const Core::ViewTransform& view) {
    ExportViewMetadata metadata;
    metadata.centerX = view.state.centerX;
    metadata.centerY = view.state.centerY;
    metadata.scaleX = view.state.scaleX;
    metadata.scaleY = view.state.scaleY;
    metadata.screenWidth = view.viewport.width;
    metadata.screenHeight = view.viewport.height;
    metadata.screenOriginX = view.viewport.originX;
    metadata.screenOriginY = view.viewport.originY;
    metadata.worldXMin = view.worldXMin();
    metadata.worldXMax = view.worldXMax();
    metadata.worldYMin = view.worldYMin();
    metadata.worldYMax = view.worldYMax();
    return metadata;
}

ExportCameraMetadata makeExportCameraMetadata(const Model::PlotSettings& plot) {
    ExportCameraMetadata metadata;
    metadata.azimuthDeg = plot.azimuthDeg;
    metadata.elevationDeg = plot.elevationDeg;
    metadata.zScale = plot.zScale;
    metadata.autoRotate = plot.autoRotate;
    metadata.autoRotateSpeedDegPerSec = plot.autoRotateSpeedDegPerSec;
    return metadata;
}

ExportDisplayMetadata makeExportDisplayMetadata(const Model::PlotSettings& plot) {
    ExportDisplayMetadata metadata;
    metadata.xyRenderModePreference = plot.xyRenderModePreference;
    metadata.hudMode = plot.hudMode;
    metadata.optimizeRendering = plot.optimizeRendering;
    metadata.showGrid = plot.showGrid;
    metadata.showCoordinates = plot.showCoordinates;
    metadata.showWires = plot.showWires;
    metadata.showSurfaceEnvelope = plot.showSurfaceEnvelope;
    metadata.showAxisTriad = plot.showAxisTriad;
    metadata.effectiveShowAxisTriad = plot.effectiveShowAxisTriad();
    metadata.surfaceResolution = plot.surfaceResolution;
    metadata.implicitSurfaceResolution = plot.implicitSurfaceResolution;
    metadata.surfaceOpacity = plot.surfaceOpacity;
    metadata.wireOpacity = plot.wireOpacity;
    metadata.wireThickness = plot.wireThickness;
    metadata.wireStride = plot.wireStride;
    metadata.envelopeThickness = plot.envelopeThickness;
    metadata.heatmapOpacity = plot.heatmapOpacity;
    return metadata;
}

ExportMetadataModel makeExportMetadataModel(
    const ExportSettings& settings,
    std::string imagePathUtf8,
    const std::filesystem::path& imagePath,
    int width,
    int height,
    const ExportAppMetadata& application,
    const std::vector<Model::Formula>& formulas,
    const Core::ViewTransform& view,
    const Model::PlotSettings& plot) {
    ExportMetadataModel model;
    model.application = application;
    model.image.pathUtf8 = std::move(imagePathUtf8);
    model.image.width = width;
    model.image.height = height;
    model.image.format = exportActualFormatLabel(settings, imagePath);
    model.view = makeExportViewMetadata(view);
    model.camera = makeExportCameraMetadata(plot);
    model.display = makeExportDisplayMetadata(plot);
    model.settings = settings;

    model.formulas.reserve(formulas.size());
    for (size_t i = 0; i < formulas.size(); ++i) {
        model.formulas.push_back(makeExportFormulaMetadata(formulas[i], i + 1));
    }
    return model;
}

std::string serializeExportMetadata(const ExportMetadataModel& model) {
    const int presetIndex = std::clamp(
        model.settings.size.selectedPreset, 0, static_cast<int>(exportSizePresets().size()) - 1);
    const auto& sizePreset = exportSizePresets()[static_cast<size_t>(presetIndex)];
    const std::array<float, 4> resolvedBackground = resolveExportBackgroundColor(model.settings);

    XFJson::JsonWriter writer;
    writer.beginObject();
    writer.key("schemaVersion");
    writer.value(kExportMetadataSchemaVersion);

    writer.key("application");
    writer.beginObject();
    writer.key("name");
    writer.value(model.application.name);
    writer.key("version");
    writer.value(model.application.version);
    writer.key("repoUrl");
    writer.value(model.application.repoUrl);
    writer.key("branch");
    writer.value(model.application.branch);
    writer.key("commit");
    writer.value(model.application.commit);
    writer.endObject();

    writer.key("image");
    writer.beginObject();
    writer.key("path");
    writer.value(model.image.pathUtf8);
    writer.key("width");
    writer.value(model.image.width);
    writer.key("height");
    writer.value(model.image.height);
    writer.key("format");
    writer.value(model.image.format);
    writer.endObject();

    writer.key("formulas");
    writer.beginArray();
    for (const ExportFormulaMetadata& formula : model.formulas) {
        writer.beginObject();
        writer.key("index");
        writer.value(static_cast<unsigned long long>(formula.index));
        writer.key("expression");
        writer.value(formula.expression);
        writer.key("visible");
        writer.value(formula.visible);
        writer.key("color");
        writeColor(writer, formula.color);
        writer.key("valid");
        writer.value(formula.valid);
        writer.key("type");
        writer.value(formula.type);
        writer.key("renderKind");
        writer.value(formula.renderKind);
        writer.key("equation");
        writer.value(formula.equation);
        writer.key("variableCount");
        writer.value(formula.variableCount);
        writer.key("variables");
        writer.beginArray();
        for (const std::string& variable : formula.variables) {
            writer.value(variable);
        }
        writer.endArray();
        writer.key("zSlice");
        writer.value(formula.zSlice);
        writer.key("error");
        writer.value(formula.error);
        writer.endObject();
    }
    writer.endArray();

    writer.key("view");
    writer.beginObject();
    writer.key("center");
    writer.beginObject();
    writer.key("x");
    writer.value(model.view.centerX);
    writer.key("y");
    writer.value(model.view.centerY);
    writer.endObject();
    writer.key("scale");
    writer.beginObject();
    writer.key("x");
    writer.value(model.view.scaleX);
    writer.key("y");
    writer.value(model.view.scaleY);
    writer.endObject();
    writer.key("screen");
    writer.beginObject();
    writer.key("width");
    writer.value(model.view.screenWidth);
    writer.key("height");
    writer.value(model.view.screenHeight);
    writer.key("originX");
    writer.value(model.view.screenOriginX);
    writer.key("originY");
    writer.value(model.view.screenOriginY);
    writer.endObject();
    writer.key("worldBounds");
    writer.beginObject();
    writer.key("xMin");
    writer.value(model.view.worldXMin);
    writer.key("xMax");
    writer.value(model.view.worldXMax);
    writer.key("yMin");
    writer.value(model.view.worldYMin);
    writer.key("yMax");
    writer.value(model.view.worldYMax);
    writer.endObject();
    writer.endObject();

    writer.key("camera");
    writer.beginObject();
    writer.key("azimuthDeg");
    writer.value(model.camera.azimuthDeg);
    writer.key("elevationDeg");
    writer.value(model.camera.elevationDeg);
    writer.key("zScale");
    writer.value(model.camera.zScale);
    writer.key("autoRotate");
    writer.value(model.camera.autoRotate);
    writer.key("autoRotateSpeedDegPerSec");
    writer.value(model.camera.autoRotateSpeedDegPerSec);
    writer.endObject();

    writer.key("display");
    writer.beginObject();
    writer.key("xyRenderModePreference");
    writer.value(Model::toDisplayLabel(model.display.xyRenderModePreference));
    writer.key("xyRenderModePreferenceId");
    writer.value(Model::toStorageName(model.display.xyRenderModePreference));
    writer.key("hudMode");
    writer.value(Model::plotHudModeLabel(model.display.hudMode));
    writer.key("hudModeId");
    writer.value(Model::toStorageName(model.display.hudMode));
    writer.key("optimizeRendering");
    writer.value(model.display.optimizeRendering);
    writer.key("showGrid");
    writer.value(model.display.showGrid);
    writer.key("showCoordinates");
    writer.value(model.display.showCoordinates);
    writer.key("showWires");
    writer.value(model.display.showWires);
    writer.key("showSurfaceEnvelope");
    writer.value(model.display.showSurfaceEnvelope);
    writer.key("showAxisTriad");
    writer.value(model.display.showAxisTriad);
    writer.key("effectiveShowAxisTriad");
    writer.value(model.display.effectiveShowAxisTriad);
    writer.key("surfaceResolution");
    writer.value(model.display.surfaceResolution);
    writer.key("implicitSurfaceResolution");
    writer.value(model.display.implicitSurfaceResolution);
    writer.key("surfaceOpacity");
    writer.value(model.display.surfaceOpacity);
    writer.key("wireOpacity");
    writer.value(model.display.wireOpacity);
    writer.key("wireThickness");
    writer.value(model.display.wireThickness);
    writer.key("wireStride");
    writer.value(model.display.wireStride);
    writer.key("envelopeThickness");
    writer.value(model.display.envelopeThickness);
    writer.key("heatmapOpacity");
    writer.value(model.display.heatmapOpacity);
    writer.endObject();

    writer.key("export");
    writer.beginObject();
    writer.key("profile");
    writer.value(exportProfileLabel(model.settings.profile));
    writer.key("profileId");
    writer.value(toStorageName(model.settings.profile));
    writer.key("requestedWidth");
    writer.value(model.settings.size.width);
    writer.key("requestedHeight");
    writer.value(model.settings.size.height);
    writer.key("outputWidth");
    writer.value(model.image.width);
    writer.key("outputHeight");
    writer.value(model.image.height);
    writer.key("scale");
    writer.value(model.settings.size.scale);
    writer.key("sizePreset");
    writer.value(sizePreset.label);
    writer.key("sizePresetId");
    writer.value(sizePreset.storageName);
    writer.key("lockAspectRatio");
    writer.value(model.settings.size.lockAspectRatio);
    writer.key("format");
    writer.value(exportFormatLabel(model.settings.output.format));
    writer.key("formatId");
    writer.value(toStorageName(model.settings.output.format));
    writer.key("backgroundMode");
    writer.value(exportBackgroundModeLabel(model.settings.appearance.backgroundMode));
    writer.key("backgroundModeId");
    writer.value(toStorageName(model.settings.appearance.backgroundMode));
    writer.key("customBackgroundColor");
    writeColor(writer, model.settings.appearance.backgroundColor);
    writer.key("resolvedBackgroundColor");
    writeColor(writer, resolvedBackground);
    writer.key("grayscaleOutput");
    writer.value(model.settings.appearance.grayscaleOutput);
    writer.key("aspectMode");
    writer.value(exportAspectModeLabel(model.settings.output.aspectMode));
    writer.key("aspectModeId");
    writer.value(toStorageName(model.settings.output.aspectMode));
    writer.key("showGrid");
    writer.value(model.settings.scene.showGrid);
    writer.key("showCoordinates");
    writer.value(model.settings.scene.showCoordinates);
    writer.key("showWires");
    writer.value(model.settings.scene.showWires);
    writer.key("showEnvelope");
    writer.value(model.settings.scene.showEnvelope);
    writer.key("showAxisTriad");
    writer.value(model.settings.scene.showAxisTriad);
    writer.key("effectiveShowAxisTriad");
    writer.value(Model::isAxisTriadVisible(model.settings.scene.showCoordinates,
                                           model.settings.scene.showAxisTriad));
    writer.key("qualityMode");
    writer.value(exportQualityModeLabel(model.settings.quality.mode));
    writer.key("qualityModeId");
    writer.value(toStorageName(model.settings.quality.mode));
    writer.key("qualityPreset");
    writer.value(exportQualityPresetLabel(model.settings.quality.preset));
    writer.key("qualityPresetId");
    writer.value(toStorageName(model.settings.quality.preset));
    writer.key("surfaceResolution");
    writer.value(model.settings.quality.surfaceResolution);
    writer.key("implicitSurfaceResolution");
    writer.value(model.settings.quality.implicitSurfaceResolution);
    writer.key("wireThicknessScale");
    writer.value(model.settings.quality.wireThicknessScale);
    writer.key("supersampling");
    writer.value(exportSupersamplingLabel(model.settings.quality.supersampling));
    writer.key("supersamplingId");
    writer.value(toStorageName(model.settings.quality.supersampling));
    writer.key("previewQuality");
    writer.value(exportPreviewQualityLabel(model.settings.quality.previewQuality));
    writer.key("previewQualityId");
    writer.value(toStorageName(model.settings.quality.previewQuality));
    writer.key("autoRefreshPreview");
    writer.value(model.settings.quality.autoRefreshPreview);
    writer.key("openAfterSave");
    writer.value(model.settings.output.openAfterSave);
    writer.key("showInFolderAfterSave");
    writer.value(model.settings.output.showInFolderAfterSave);
    writer.key("copyPathAfterSave");
    writer.value(model.settings.output.copyPathAfterSave);
    writer.key("saveMetadataSidecar");
    writer.value(model.settings.output.saveMetadataSidecar);
    writer.endObject();

    writer.endObject();
    std::string json = writer.str();
    json += '\n';
    return json;
}

} // namespace XpressFormula::Infrastructure::Export
