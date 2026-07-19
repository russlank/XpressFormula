// SPDX-License-Identifier: MIT
// ProjectSession.h - Versioned .xfplot session serialization helpers.
#pragma once

#include "../Core/ViewTransform.h"
#include "../Infrastructure/Serialization/JsonParser.h"
#include "../Infrastructure/Serialization/JsonWriter.h"
#include "../Model/Formula.h"
#include "../Model/ViewState.h"
#include "ExportMetadata.h"
#include "FormulaEntry.h"
#include "PlotSettings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace XpressFormula::UI {

inline constexpr int kProjectSessionSchemaVersion = 1;
inline constexpr std::string_view kProjectSessionFileType = "XpressFormulaProject";

struct ProjectFormulaRecord {
    std::string expression;
    std::array<float, 4> color = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool visible = true;
    float zSlice = 0.0f;
};

using ProjectViewRecord = Model::ViewState;

struct ProjectSession {
    int schemaVersion = kProjectSessionSchemaVersion;
    std::vector<ProjectFormulaRecord> formulas;
    ProjectViewRecord view;
    PlotSettings plot;
};

struct ProjectSessionParseResult {
    bool success = false;
    ProjectSession session;
    std::string error;
    std::vector<std::string> warnings;
};

namespace ProjectSessionDetail {

using JsonValue = Infrastructure::Serialization::JsonValue;
using JsonParser = Infrastructure::Serialization::JsonParser;

inline std::string quote(std::string_view text) {
    return Infrastructure::Serialization::quoteJsonString(text);
}

inline double finiteJsonNumber(double value, double fallback = 0.0) {
    return Infrastructure::Serialization::finiteJsonNumber(value, fallback);
}

inline float finiteJsonNumber(float value, float fallback = 0.0f) {
    return Infrastructure::Serialization::finiteJsonNumber(value, fallback);
}

inline void appendColor(std::ostringstream& out, const std::array<float, 4>& color) {
    out << '['
        << finiteJsonNumber(color[0]) << ", "
        << finiteJsonNumber(color[1]) << ", "
        << finiteJsonNumber(color[2]) << ", "
        << finiteJsonNumber(color[3]) << ']';
}

inline bool readString(const JsonValue& object, std::string_view key, std::string& value) {
    const JsonValue* field = object.find(key);
    if (!field || field->type != JsonValue::Type::String) {
        return false;
    }
    value = field->text;
    return true;
}

inline bool readNumber(const JsonValue& object, std::string_view key, double& value) {
    const JsonValue* field = object.find(key);
    if (!field || field->type != JsonValue::Type::Number || !std::isfinite(field->number)) {
        return false;
    }
    value = field->number;
    return true;
}

inline bool readBool(const JsonValue& object, std::string_view key, bool& value) {
    const JsonValue* field = object.find(key);
    if (!field || field->type != JsonValue::Type::Bool) {
        return false;
    }
    value = field->boolean;
    return true;
}

inline void readOptionalNumber(const JsonValue& object, std::string_view key, double& value) {
    double parsed = value;
    if (readNumber(object, key, parsed)) {
        value = parsed;
    }
}

inline void readOptionalFloat(const JsonValue& object, std::string_view key, float& value) {
    double parsed = value;
    if (readNumber(object, key, parsed) &&
        parsed >= -static_cast<double>(std::numeric_limits<float>::max()) &&
        parsed <= static_cast<double>(std::numeric_limits<float>::max())) {
        value = static_cast<float>(parsed);
    }
}

inline void readOptionalInt(const JsonValue& object, std::string_view key, int& value) {
    double parsed = value;
    if (readNumber(object, key, parsed) &&
        std::floor(parsed) == parsed &&
        parsed >= static_cast<double>((std::numeric_limits<int>::min)()) &&
        parsed <= static_cast<double>((std::numeric_limits<int>::max)())) {
        value = static_cast<int>(std::lround(parsed));
    }
}

inline void readOptionalBool(const JsonValue& object, std::string_view key, bool& value) {
    bool parsed = value;
    if (readBool(object, key, parsed)) {
        value = parsed;
    }
}

inline void readOptionalColor(const JsonValue& object,
                              std::string_view key,
                              std::array<float, 4>& color,
                              std::vector<std::string>& warnings,
                              std::string_view context) {
    const JsonValue* field = object.find(key);
    if (!field) {
        return;
    }
    if (field->type != JsonValue::Type::Array || field->array.size() != color.size()) {
        warnings.emplace_back(std::string(context) + " has an invalid color array.");
        return;
    }

    for (std::size_t i = 0; i < color.size(); ++i) {
        const JsonValue& item = field->array[i];
        if (item.type != JsonValue::Type::Number || !std::isfinite(item.number)) {
            warnings.emplace_back(std::string(context) + " has a non-numeric color component.");
            return;
        }
        color[i] = std::clamp(static_cast<float>(item.number), 0.0f, 1.0f);
    }
}

} // namespace ProjectSessionDetail

inline ProjectSession makeProjectSession(const std::vector<Model::Formula>& formulas,
                                         const Core::ViewTransform& view,
                                         const PlotSettings& plot) {
    ProjectSession session;
    session.formulas.reserve(formulas.size());
    for (const Model::Formula& formula : formulas) {
        ProjectFormulaRecord record;
        record.expression = formula.expression;
        for (std::size_t channel = 0; channel < record.color.size(); ++channel) {
            record.color[channel] = formula.color[channel];
        }
        record.visible = formula.visible;
        record.zSlice =
            (std::isfinite(formula.zSlice) &&
             formula.zSlice >= -static_cast<double>((std::numeric_limits<float>::max)()) &&
             formula.zSlice <= static_cast<double>((std::numeric_limits<float>::max)()))
                ? static_cast<float>(formula.zSlice)
                : 0.0f;
        session.formulas.push_back(std::move(record));
    }

    session.view.centerX = view.state.centerX;
    session.view.centerY = view.state.centerY;
    session.view.scaleX = view.state.scaleX;
    session.view.scaleY = view.state.scaleY;
    session.plot = plot;
    return session;
}

inline std::string serializeProjectSession(const ProjectSession& session) {
    using namespace ProjectSessionDetail;

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "{\n";
    out << "  \"schemaVersion\": " << kProjectSessionSchemaVersion << ",\n";
    out << "  \"fileType\": " << quote(kProjectSessionFileType) << ",\n";
    out << "  \"formulas\": [\n";
    for (std::size_t i = 0; i < session.formulas.size(); ++i) {
        const ProjectFormulaRecord& formula = session.formulas[i];
        out << "    {\n";
        out << "      \"expression\": " << quote(formula.expression) << ",\n";
        out << "      \"visible\": " << jsonBool(formula.visible) << ",\n";
        out << "      \"color\": ";
        appendColor(out, formula.color);
        out << ",\n";
        out << "      \"zSlice\": " << finiteJsonNumber(formula.zSlice) << "\n";
        out << "    }" << ((i + 1 < session.formulas.size()) ? "," : "") << "\n";
    }
    out << "  ],\n";
    out << "  \"view\": {\n";
    out << "    \"centerX\": " << finiteJsonNumber(session.view.centerX) << ",\n";
    out << "    \"centerY\": " << finiteJsonNumber(session.view.centerY) << ",\n";
    out << "    \"scaleX\": " << finiteJsonNumber(session.view.scaleX, 60.0) << ",\n";
    out << "    \"scaleY\": " << finiteJsonNumber(session.view.scaleY, 60.0) << "\n";
    out << "  },\n";
    out << "  \"display\": {\n";
    out << "    \"xyRenderModePreference\": "
        << quote(toStorageName(session.plot.xyRenderModePreference)) << ",\n";
    out << "    \"hudMode\": " << quote(toStorageName(session.plot.hudMode)) << ",\n";
    out << "    \"optimizeRendering\": " << jsonBool(session.plot.optimizeRendering) << ",\n";
    out << "    \"showGrid\": " << jsonBool(session.plot.showGrid) << ",\n";
    out << "    \"showCoordinates\": " << jsonBool(session.plot.showCoordinates) << ",\n";
    out << "    \"showWires\": " << jsonBool(session.plot.showWires) << ",\n";
    out << "    \"showSurfaceEnvelope\": " << jsonBool(session.plot.showSurfaceEnvelope) << ",\n";
    out << "    \"showAxisTriad\": " << jsonBool(session.plot.showAxisTriad) << ",\n";
    out << "    \"surfaceResolution\": " << session.plot.surfaceResolution << ",\n";
    out << "    \"implicitSurfaceResolution\": " << session.plot.implicitSurfaceResolution << ",\n";
    out << "    \"surfaceOpacity\": " << finiteJsonNumber(session.plot.surfaceOpacity, kDefaultSurfaceOpacity) << ",\n";
    out << "    \"wireOpacity\": " << finiteJsonNumber(session.plot.wireOpacity, kDefaultWireOpacity) << ",\n";
    out << "    \"wireThickness\": " << finiteJsonNumber(session.plot.wireThickness, kDefaultWireThickness) << ",\n";
    out << "    \"wireStride\": " << session.plot.wireStride << ",\n";
    out << "    \"envelopeThickness\": " << finiteJsonNumber(session.plot.envelopeThickness, kDefaultEnvelopeThickness) << ",\n";
    out << "    \"heatmapOpacity\": " << finiteJsonNumber(session.plot.heatmapOpacity, kDefaultHeatmapOpacity) << "\n";
    out << "  },\n";
    out << "  \"camera\": {\n";
    out << "    \"azimuthDeg\": " << finiteJsonNumber(session.plot.azimuthDeg, kDefaultAzimuthDeg) << ",\n";
    out << "    \"elevationDeg\": " << finiteJsonNumber(session.plot.elevationDeg, kDefaultElevationDeg) << ",\n";
    out << "    \"zScale\": " << finiteJsonNumber(session.plot.zScale, kDefaultZScale) << ",\n";
    out << "    \"autoRotate\": " << jsonBool(session.plot.autoRotate) << ",\n";
    out << "    \"autoRotateSpeedDegPerSec\": "
        << finiteJsonNumber(session.plot.autoRotateSpeedDegPerSec,
                            kDefaultAutoRotateSpeedDegPerSec) << "\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

inline ProjectSessionParseResult parseProjectSession(std::string_view json) {
    using namespace ProjectSessionDetail;

    ProjectSessionParseResult result;
    JsonValue root;
    JsonParser parser(json);
    if (!parser.parse(root, result.error)) {
        return result;
    }
    if (root.type != JsonValue::Type::Object) {
        result.error = "Project file root must be an object.";
        return result;
    }

    double schemaVersion = 0.0;
    if (!readNumber(root, "schemaVersion", schemaVersion)) {
        result.error = "Project file is missing schemaVersion.";
        return result;
    }
    if (schemaVersion < 0.0 ||
        std::floor(schemaVersion) != schemaVersion ||
        schemaVersion > static_cast<double>((std::numeric_limits<int>::max)())) {
        result.error = "Project file schemaVersion must be a non-negative integer.";
        return result;
    }
    if (static_cast<int>(schemaVersion) != kProjectSessionSchemaVersion) {
        result.error = "Unsupported .xfplot schema version.";
        return result;
    }

    if (const JsonValue* fileType = root.find("fileType")) {
        if (fileType->type != JsonValue::Type::String ||
            fileType->text != kProjectSessionFileType) {
            result.error = "File is not an XpressFormula project.";
            return result;
        }
    }

    const JsonValue* formulas = root.find("formulas");
    if (!formulas || formulas->type != JsonValue::Type::Array) {
        result.error = "Project file is missing formulas array.";
        return result;
    }

    result.session.schemaVersion = kProjectSessionSchemaVersion;
    result.session.formulas.clear();
    for (std::size_t i = 0; i < formulas->array.size(); ++i) {
        const JsonValue& item = formulas->array[i];
        if (item.type != JsonValue::Type::Object) {
            result.warnings.emplace_back("Skipped formula " + std::to_string(i + 1) + ": expected object.");
            continue;
        }

        ProjectFormulaRecord record;
        for (std::size_t channel = 0; channel < record.color.size(); ++channel) {
            record.color[channel] = kDefaultPalette[i % kPaletteSize][channel];
        }
        if (!readString(item, "expression", record.expression)) {
            result.warnings.emplace_back("Skipped formula " + std::to_string(i + 1) + ": missing expression.");
            continue;
        }
        readOptionalBool(item, "visible", record.visible);
        readOptionalFloat(item, "zSlice", record.zSlice);
        readOptionalColor(item, "color", record.color, result.warnings,
                          "Formula " + std::to_string(i + 1));
        result.session.formulas.push_back(std::move(record));
    }

    if (const JsonValue* view = root.find("view");
        view && view->type == JsonValue::Type::Object) {
        readOptionalNumber(*view, "centerX", result.session.view.centerX);
        readOptionalNumber(*view, "centerY", result.session.view.centerY);
        readOptionalNumber(*view, "scaleX", result.session.view.scaleX);
        readOptionalNumber(*view, "scaleY", result.session.view.scaleY);
    }

    if (const JsonValue* display = root.find("display");
        display && display->type == JsonValue::Type::Object) {
        std::string enumText;
        if (readString(*display, "xyRenderModePreference", enumText) &&
            !parseXYRenderModePreferenceStorageName(enumText,
                                                    result.session.plot.xyRenderModePreference)) {
            result.warnings.emplace_back("Ignored unknown XY render mode preference.");
        }
        if (readString(*display, "hudMode", enumText) &&
            !parsePlotHudModeStorageName(enumText, result.session.plot.hudMode)) {
            result.warnings.emplace_back("Ignored unknown HUD mode.");
        }

        readOptionalBool(*display, "optimizeRendering", result.session.plot.optimizeRendering);
        readOptionalBool(*display, "showGrid", result.session.plot.showGrid);
        readOptionalBool(*display, "showCoordinates", result.session.plot.showCoordinates);
        readOptionalBool(*display, "showWires", result.session.plot.showWires);
        readOptionalBool(*display, "showSurfaceEnvelope", result.session.plot.showSurfaceEnvelope);
        readOptionalBool(*display, "showAxisTriad", result.session.plot.showAxisTriad);
        readOptionalInt(*display, "surfaceResolution", result.session.plot.surfaceResolution);
        readOptionalInt(*display, "implicitSurfaceResolution", result.session.plot.implicitSurfaceResolution);
        readOptionalFloat(*display, "surfaceOpacity", result.session.plot.surfaceOpacity);
        readOptionalFloat(*display, "wireOpacity", result.session.plot.wireOpacity);
        readOptionalFloat(*display, "wireThickness", result.session.plot.wireThickness);
        readOptionalInt(*display, "wireStride", result.session.plot.wireStride);
        readOptionalFloat(*display, "envelopeThickness", result.session.plot.envelopeThickness);
        readOptionalFloat(*display, "heatmapOpacity", result.session.plot.heatmapOpacity);
    }

    if (const JsonValue* camera = root.find("camera");
        camera && camera->type == JsonValue::Type::Object) {
        readOptionalFloat(*camera, "azimuthDeg", result.session.plot.azimuthDeg);
        readOptionalFloat(*camera, "elevationDeg", result.session.plot.elevationDeg);
        readOptionalFloat(*camera, "zScale", result.session.plot.zScale);
        readOptionalBool(*camera, "autoRotate", result.session.plot.autoRotate);
        readOptionalFloat(*camera, "autoRotateSpeedDegPerSec",
                          result.session.plot.autoRotateSpeedDegPerSec);
    }

    normalizePlotSettings(result.session.plot);
    result.success = true;
    return result;
}

inline void applyProjectSession(const ProjectSession& session,
                                std::vector<Model::Formula>& formulas,
                                Core::ViewTransform& view,
                                PlotSettings& plot,
                                std::vector<std::string>& warnings) {
    formulas.clear();
    formulas.reserve(session.formulas.size());
    for (std::size_t i = 0; i < session.formulas.size(); ++i) {
        const ProjectFormulaRecord& record = session.formulas[i];
        Model::Formula entry;
        entry.setExpression(record.expression);

        for (std::size_t channel = 0; channel < record.color.size(); ++channel) {
            entry.color[channel] = record.color[channel];
        }
        entry.visible = record.visible;
        entry.zSlice = record.zSlice;
        entry.compile();
        if (!entry.isValid() && !record.expression.empty()) {
            const Expression::FormulaDiagnostic* diagnostic = entry.compiled.firstDiagnostic();
            warnings.emplace_back("Formula " + std::to_string(i + 1) +
                " did not parse: " + (diagnostic ? diagnostic->message : "unknown parse error"));
        }
        formulas.push_back(std::move(entry));
    }

    view.state.centerX = std::isfinite(session.view.centerX) ? session.view.centerX : 0.0;
    view.state.centerY = std::isfinite(session.view.centerY) ? session.view.centerY : 0.0;
    view.state.scaleX = std::clamp(std::isfinite(session.view.scaleX) ? session.view.scaleX : 60.0,
                             0.1, 100000.0);
    view.state.scaleY = std::clamp(std::isfinite(session.view.scaleY) ? session.view.scaleY : 60.0,
                             0.1, 100000.0);

    plot = session.plot;
    normalizePlotSettings(plot);
}

inline std::string serializeCurrentProjectSession(const std::vector<Model::Formula>& formulas,
                                                  const Core::ViewTransform& view,
                                                  const PlotSettings& plot) {
    return serializeProjectSession(makeProjectSession(formulas, view, plot));
}

} // namespace XpressFormula::UI
