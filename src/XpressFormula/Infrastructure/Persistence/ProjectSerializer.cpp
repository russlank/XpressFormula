// SPDX-License-Identifier: MIT
// ProjectSerializer.cpp - DTO/JSON conversion for .xfplot schema v1.
#include "ProjectSerializer.h"
#include "../../Core/InputLimits.h"
#include "../Serialization/JsonParser.h"
#include "../Serialization/JsonWriter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <utility>

namespace XpressFormula::Infrastructure::Persistence {
namespace {

using Serialization::JsonParser;
using Serialization::JsonValue;

inline constexpr float kDefaultProjectPalette[][4] = {
    { 0.10f, 0.80f, 0.25f, 1.0f },
    { 0.25f, 0.60f, 1.00f, 1.0f },
    { 1.00f, 0.30f, 0.30f, 1.0f },
    { 1.00f, 0.80f, 0.10f, 1.0f },
    { 0.80f, 0.35f, 1.00f, 1.0f },
    { 0.10f, 0.80f, 0.80f, 1.0f },
    { 1.00f, 0.50f, 0.10f, 1.0f },
    { 0.60f, 0.80f, 0.25f, 1.0f },
};
inline constexpr std::size_t kDefaultProjectPaletteSize =
    sizeof(kDefaultProjectPalette) / sizeof(kDefaultProjectPalette[0]);

std::string quote(std::string_view text) {
    return Serialization::quoteJsonString(text);
}

double finiteJsonNumber(double value, double fallback = 0.0) {
    return Serialization::finiteJsonNumber(value, fallback);
}

float finiteJsonNumber(float value, float fallback = 0.0f) {
    return Serialization::finiteJsonNumber(value, fallback);
}

void appendColor(std::ostringstream& out, const std::array<float, 4>& color) {
    out << '['
        << finiteJsonNumber(color[0]) << ", "
        << finiteJsonNumber(color[1]) << ", "
        << finiteJsonNumber(color[2]) << ", "
        << finiteJsonNumber(color[3]) << ']';
}

std::string_view storageName(Model::XYRenderModePreference preference) {
    switch (preference) {
        case Model::XYRenderModePreference::Auto: return "auto";
        case Model::XYRenderModePreference::Force3D: return "force3D";
        case Model::XYRenderModePreference::Force2D: return "force2D";
        default: return "auto";
    }
}

bool parseXYRenderModePreference(std::string_view text,
                                 Model::XYRenderModePreference& preference) {
    if (text == "auto" || text == "Auto") {
        preference = Model::XYRenderModePreference::Auto;
        return true;
    }
    if (text == "force3D" || text == "3D" || text == "Force3D") {
        preference = Model::XYRenderModePreference::Force3D;
        return true;
    }
    if (text == "force2D" || text == "2D" || text == "Force2D") {
        preference = Model::XYRenderModePreference::Force2D;
        return true;
    }
    return false;
}

std::string_view storageName(Model::PlotHudMode mode) {
    switch (mode) {
        case Model::PlotHudMode::Off: return "off";
        case Model::PlotHudMode::Minimal: return "minimal";
        case Model::PlotHudMode::Detailed: return "detailed";
        case Model::PlotHudMode::OnlyWhileInteracting: return "onlyWhileInteracting";
        default: return "minimal";
    }
}

bool parsePlotHudMode(std::string_view text, Model::PlotHudMode& mode) {
    if (text == "off" || text == "Off") {
        mode = Model::PlotHudMode::Off;
        return true;
    }
    if (text == "minimal" || text == "Minimal") {
        mode = Model::PlotHudMode::Minimal;
        return true;
    }
    if (text == "detailed" || text == "Detailed") {
        mode = Model::PlotHudMode::Detailed;
        return true;
    }
    if (text == "onlyWhileInteracting" || text == "Only While Interacting") {
        mode = Model::PlotHudMode::OnlyWhileInteracting;
        return true;
    }
    return false;
}

bool readString(const JsonValue& object, std::string_view key, std::string& value) {
    const JsonValue* field = object.find(key);
    if (!field || field->type != JsonValue::Type::String) {
        return false;
    }
    value = field->text;
    return true;
}

bool readNumber(const JsonValue& object, std::string_view key, double& value) {
    const JsonValue* field = object.find(key);
    if (!field || field->type != JsonValue::Type::Number || !std::isfinite(field->number)) {
        return false;
    }
    value = field->number;
    return true;
}

bool readBool(const JsonValue& object, std::string_view key, bool& value) {
    const JsonValue* field = object.find(key);
    if (!field || field->type != JsonValue::Type::Bool) {
        return false;
    }
    value = field->boolean;
    return true;
}

void readOptionalNumber(const JsonValue& object, std::string_view key, double& value) {
    double parsed = value;
    if (readNumber(object, key, parsed)) {
        value = parsed;
    }
}

void readOptionalFloat(const JsonValue& object, std::string_view key, float& value) {
    double parsed = value;
    if (readNumber(object, key, parsed) &&
        parsed >= -static_cast<double>(std::numeric_limits<float>::max()) &&
        parsed <= static_cast<double>(std::numeric_limits<float>::max())) {
        value = static_cast<float>(parsed);
    }
}

void readOptionalInt(const JsonValue& object, std::string_view key, int& value) {
    double parsed = value;
    if (readNumber(object, key, parsed) &&
        std::floor(parsed) == parsed &&
        parsed >= static_cast<double>((std::numeric_limits<int>::min)()) &&
        parsed <= static_cast<double>((std::numeric_limits<int>::max)())) {
        value = static_cast<int>(std::lround(parsed));
    }
}

void readOptionalBool(const JsonValue& object, std::string_view key, bool& value) {
    bool parsed = value;
    if (readBool(object, key, parsed)) {
        value = parsed;
    }
}

void readOptionalColor(const JsonValue& object,
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

} // namespace

std::string serializeProjectSession(const ProjectSession& session) {
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
        out << "      \"visible\": " << Serialization::jsonBool(formula.visible) << ",\n";
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
        << quote(storageName(session.plot.xyRenderModePreference)) << ",\n";
    out << "    \"hudMode\": " << quote(storageName(session.plot.hudMode)) << ",\n";
    out << "    \"optimizeRendering\": " << Serialization::jsonBool(session.plot.optimizeRendering) << ",\n";
    out << "    \"showGrid\": " << Serialization::jsonBool(session.plot.showGrid) << ",\n";
    out << "    \"showCoordinates\": " << Serialization::jsonBool(session.plot.showCoordinates) << ",\n";
    out << "    \"showWires\": " << Serialization::jsonBool(session.plot.showWires) << ",\n";
    out << "    \"showSurfaceEnvelope\": " << Serialization::jsonBool(session.plot.showSurfaceEnvelope) << ",\n";
    out << "    \"showAxisTriad\": " << Serialization::jsonBool(session.plot.showAxisTriad) << ",\n";
    out << "    \"surfaceResolution\": " << session.plot.surfaceResolution << ",\n";
    out << "    \"implicitSurfaceResolution\": " << session.plot.implicitSurfaceResolution << ",\n";
    out << "    \"surfaceOpacity\": " << finiteJsonNumber(session.plot.surfaceOpacity, Model::kDefaultSurfaceOpacity) << ",\n";
    out << "    \"wireOpacity\": " << finiteJsonNumber(session.plot.wireOpacity, Model::kDefaultWireOpacity) << ",\n";
    out << "    \"wireThickness\": " << finiteJsonNumber(session.plot.wireThickness, Model::kDefaultWireThickness) << ",\n";
    out << "    \"wireStride\": " << session.plot.wireStride << ",\n";
    out << "    \"envelopeThickness\": " << finiteJsonNumber(session.plot.envelopeThickness, Model::kDefaultEnvelopeThickness) << ",\n";
    out << "    \"heatmapOpacity\": " << finiteJsonNumber(session.plot.heatmapOpacity, Model::kDefaultHeatmapOpacity) << "\n";
    out << "  },\n";
    out << "  \"camera\": {\n";
    out << "    \"azimuthDeg\": " << finiteJsonNumber(session.plot.azimuthDeg, Model::kDefaultAzimuthDeg) << ",\n";
    out << "    \"elevationDeg\": " << finiteJsonNumber(session.plot.elevationDeg, Model::kDefaultElevationDeg) << ",\n";
    out << "    \"zScale\": " << finiteJsonNumber(session.plot.zScale, Model::kDefaultZScale) << ",\n";
    out << "    \"autoRotate\": " << Serialization::jsonBool(session.plot.autoRotate) << ",\n";
    out << "    \"autoRotateSpeedDegPerSec\": "
        << finiteJsonNumber(session.plot.autoRotateSpeedDegPerSec,
                            Model::kDefaultAutoRotateSpeedDegPerSec) << "\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

ProjectSessionParseResult parseProjectSession(std::string_view json) {
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
    if (formulas->array.size() > Core::InputLimits::kMaxProjectFormulas) {
        result.error = "Project file contains too many formulas.";
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
            record.color[channel] = kDefaultProjectPalette[i % kDefaultProjectPaletteSize][channel];
        }
        if (!readString(item, "expression", record.expression)) {
            result.warnings.emplace_back("Skipped formula " + std::to_string(i + 1) + ": missing expression.");
            continue;
        }
        if (record.expression.size() > Core::InputLimits::kMaxFormulaLength) {
            result.warnings.emplace_back(
                "Formula " + std::to_string(i + 1) +
                " exceeds the supported expression length and will load as invalid.");
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
            !parseXYRenderModePreference(enumText, result.session.plot.xyRenderModePreference)) {
            result.warnings.emplace_back("Ignored unknown XY render mode preference.");
        }
        if (readString(*display, "hudMode", enumText) &&
            !parsePlotHudMode(enumText, result.session.plot.hudMode)) {
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

    result.success = true;
    return result;
}

} // namespace XpressFormula::Infrastructure::Persistence
