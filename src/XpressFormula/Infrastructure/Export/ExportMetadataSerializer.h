// SPDX-License-Identifier: MIT
// ExportMetadataSerializer.h - Plain export metadata model and JSON serializer.
#pragma once

#include "ExportSettings.h"
#include "../../Core/ViewTransform.h"
#include "../../Model/Formula.h"
#include "../../Model/PlotPolicy.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Infrastructure::Export {

inline constexpr int kExportMetadataSchemaVersion = 1;

struct ExportAppMetadata {
    std::string name = "XpressFormula";
    std::string version;
    std::string repoUrl;
    std::string branch;
    std::string commit;
};

struct ExportFormulaMetadata {
    size_t index = 0;
    std::string expression;
    bool visible = true;
    std::array<float, 4> color{};
    bool valid = false;
    std::string type;
    std::string renderKind;
    bool equation = false;
    int variableCount = 0;
    std::vector<std::string> variables;
    double zSlice = 0.0;
    std::string error;
};

struct ExportViewMetadata {
    double centerX = 0.0;
    double centerY = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double screenWidth = 0.0;
    double screenHeight = 0.0;
    double screenOriginX = 0.0;
    double screenOriginY = 0.0;
    double worldXMin = 0.0;
    double worldXMax = 0.0;
    double worldYMin = 0.0;
    double worldYMax = 0.0;
};

struct ExportCameraMetadata {
    float azimuthDeg = 0.0f;
    float elevationDeg = 0.0f;
    float zScale = 1.0f;
    bool autoRotate = false;
    float autoRotateSpeedDegPerSec = 0.0f;
};

struct ExportDisplayMetadata {
    Model::XYRenderModePreference xyRenderModePreference =
        Model::XYRenderModePreference::Auto;
    Model::PlotHudMode hudMode = Model::PlotHudMode::Minimal;
    bool optimizeRendering = true;
    bool showGrid = true;
    bool showCoordinates = true;
    bool showWires = true;
    bool showSurfaceEnvelope = true;
    bool showAxisTriad = false;
    bool effectiveShowAxisTriad = false;
    int surfaceResolution = Model::kDefaultSurfaceResolution;
    int implicitSurfaceResolution = Model::kDefaultImplicitSurfaceResolution;
    float surfaceOpacity = Model::kDefaultSurfaceOpacity;
    float wireOpacity = Model::kDefaultWireOpacity;
    float wireThickness = Model::kDefaultWireThickness;
    int wireStride = Model::kDefaultWireStride;
    float envelopeThickness = Model::kDefaultEnvelopeThickness;
    float heatmapOpacity = Model::kDefaultHeatmapOpacity;
};

struct ExportImageMetadata {
    std::string pathUtf8;
    int width = 0;
    int height = 0;
    std::string format;
};

struct ExportMetadataModel {
    ExportAppMetadata application;
    ExportImageMetadata image;
    std::vector<ExportFormulaMetadata> formulas;
    ExportViewMetadata view;
    ExportCameraMetadata camera;
    ExportDisplayMetadata display;
    ExportSettings settings;
};

[[nodiscard]] std::string jsonEscape(std::string_view text);
[[nodiscard]] const char* jsonBool(bool value);
[[nodiscard]] std::filesystem::path exportMetadataSidecarPath(const std::filesystem::path& imagePath);
[[nodiscard]] std::filesystem::path exportMetadataTempPath(const std::filesystem::path& sidecarPath);

[[nodiscard]] std::string exportActualFormatLabel(const ExportSettings& settings,
                                                  const std::filesystem::path& imagePath);

[[nodiscard]] ExportFormulaMetadata makeExportFormulaMetadata(const Model::Formula& formula,
                                                              size_t oneBasedIndex);
[[nodiscard]] ExportViewMetadata makeExportViewMetadata(const Core::ViewTransform& view);
[[nodiscard]] ExportCameraMetadata makeExportCameraMetadata(const Model::PlotSettings& plot);
[[nodiscard]] ExportDisplayMetadata makeExportDisplayMetadata(const Model::PlotSettings& plot);

[[nodiscard]] ExportMetadataModel makeExportMetadataModel(
    const ExportSettings& settings,
    std::string imagePathUtf8,
    const std::filesystem::path& imagePath,
    int width,
    int height,
    const ExportAppMetadata& application,
    const std::vector<Model::Formula>& formulas,
    const Core::ViewTransform& view,
    const Model::PlotSettings& plot);

[[nodiscard]] std::string serializeExportMetadata(const ExportMetadataModel& model);

} // namespace XpressFormula::Infrastructure::Export
