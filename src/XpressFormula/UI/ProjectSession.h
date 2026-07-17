// SPDX-License-Identifier: MIT
// ProjectSession.h - Versioned .xfplot session serialization helpers.
#pragma once

#include "../Core/ViewTransform.h"
#include "ExportMetadata.h"
#include "FormulaEntry.h"
#include "PlotSettings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <map>
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

struct ProjectViewRecord {
    double centerX = 0.0;
    double centerY = 0.0;
    double scaleX = 60.0;
    double scaleY = 60.0;
};

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

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;

    [[nodiscard]] const JsonValue* find(std::string_view key) const {
        if (type != Type::Object) {
            return nullptr;
        }
        const auto it = object.find(std::string(key));
        return it == object.end() ? nullptr : &it->second;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input)
        : m_input(input) {
    }

    bool parse(JsonValue& value, std::string& error) {
        skipWhitespace();
        if (!parseValue(value)) {
            error = m_error.empty() ? "Invalid JSON." : m_error;
            return false;
        }
        skipWhitespace();
        if (m_pos != m_input.size()) {
            error = "Unexpected trailing data in JSON.";
            return false;
        }
        return true;
    }

private:
    void skipWhitespace() {
        while (m_pos < m_input.size() &&
               std::isspace(static_cast<unsigned char>(m_input[m_pos])) != 0) {
            ++m_pos;
        }
    }

    bool consume(char expected) {
        skipWhitespace();
        if (m_pos >= m_input.size() || m_input[m_pos] != expected) {
            return false;
        }
        ++m_pos;
        return true;
    }

    bool parseValue(JsonValue& value) {
        skipWhitespace();
        if (m_pos >= m_input.size()) {
            m_error = "Unexpected end of JSON.";
            return false;
        }

        const char ch = m_input[m_pos];
        if (ch == '{') {
            return parseObject(value);
        }
        if (ch == '[') {
            return parseArray(value);
        }
        if (ch == '"') {
            value.type = JsonValue::Type::String;
            return parseString(value.text);
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            value.type = JsonValue::Type::Number;
            return parseNumber(value.number);
        }
        if (matchLiteral("true")) {
            value.type = JsonValue::Type::Bool;
            value.boolean = true;
            return true;
        }
        if (matchLiteral("false")) {
            value.type = JsonValue::Type::Bool;
            value.boolean = false;
            return true;
        }
        if (matchLiteral("null")) {
            value.type = JsonValue::Type::Null;
            return true;
        }

        m_error = "Unexpected JSON token.";
        return false;
    }

    bool parseObject(JsonValue& value) {
        if (!consume('{')) {
            m_error = "Expected object.";
            return false;
        }
        value.type = JsonValue::Type::Object;
        skipWhitespace();
        if (m_pos < m_input.size() && m_input[m_pos] == '}') {
            ++m_pos;
            return true;
        }

        for (;;) {
            std::string key;
            if (!parseString(key)) {
                m_error = "Expected object key.";
                return false;
            }
            if (!consume(':')) {
                m_error = "Expected ':' after object key.";
                return false;
            }
            JsonValue member;
            if (!parseValue(member)) {
                return false;
            }
            value.object[std::move(key)] = std::move(member);

            skipWhitespace();
            if (m_pos < m_input.size() && m_input[m_pos] == '}') {
                ++m_pos;
                return true;
            }
            if (!consume(',')) {
                m_error = "Expected ',' or '}' in object.";
                return false;
            }
        }
    }

    bool parseArray(JsonValue& value) {
        if (!consume('[')) {
            m_error = "Expected array.";
            return false;
        }
        value.type = JsonValue::Type::Array;
        skipWhitespace();
        if (m_pos < m_input.size() && m_input[m_pos] == ']') {
            ++m_pos;
            return true;
        }

        for (;;) {
            JsonValue item;
            if (!parseValue(item)) {
                return false;
            }
            value.array.push_back(std::move(item));

            skipWhitespace();
            if (m_pos < m_input.size() && m_input[m_pos] == ']') {
                ++m_pos;
                return true;
            }
            if (!consume(',')) {
                m_error = "Expected ',' or ']' in array.";
                return false;
            }
        }
    }

    static int hexValue(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    }

    bool parseString(std::string& text) {
        skipWhitespace();
        if (m_pos >= m_input.size() || m_input[m_pos] != '"') {
            return false;
        }
        ++m_pos;
        text.clear();

        while (m_pos < m_input.size()) {
            const char ch = m_input[m_pos++];
            if (ch == '"') {
                return true;
            }
            if (ch != '\\') {
                text.push_back(ch);
                continue;
            }
            if (m_pos >= m_input.size()) {
                m_error = "Unterminated JSON escape.";
                return false;
            }
            const char escape = m_input[m_pos++];
            switch (escape) {
                case '"': text.push_back('"'); break;
                case '\\': text.push_back('\\'); break;
                case '/': text.push_back('/'); break;
                case 'b': text.push_back('\b'); break;
                case 'f': text.push_back('\f'); break;
                case 'n': text.push_back('\n'); break;
                case 'r': text.push_back('\r'); break;
                case 't': text.push_back('\t'); break;
                case 'u': {
                    if (m_pos + 4 > m_input.size()) {
                        m_error = "Incomplete JSON unicode escape.";
                        return false;
                    }
                    int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        const int value = hexValue(m_input[m_pos++]);
                        if (value < 0) {
                            m_error = "Invalid JSON unicode escape.";
                            return false;
                        }
                        codepoint = (codepoint << 4) | value;
                    }
                    text.push_back(codepoint >= 0 && codepoint <= 0x7F
                        ? static_cast<char>(codepoint)
                        : '?');
                    break;
                }
                default:
                    m_error = "Invalid JSON escape.";
                    return false;
            }
        }

        m_error = "Unterminated JSON string.";
        return false;
    }

    bool parseNumber(double& number) {
        const char* begin = m_input.data() + m_pos;
        char* end = nullptr;
        number = std::strtod(begin, &end);
        if (end == begin) {
            m_error = "Invalid JSON number.";
            return false;
        }
        const std::size_t consumed = static_cast<std::size_t>(end - begin);
        if (m_pos + consumed > m_input.size()) {
            m_error = "Invalid JSON number.";
            return false;
        }
        m_pos += consumed;
        return true;
    }

    bool matchLiteral(std::string_view literal) {
        if (m_input.substr(m_pos, literal.size()) != literal) {
            return false;
        }
        m_pos += literal.size();
        return true;
    }

    std::string_view m_input;
    std::size_t m_pos = 0;
    std::string m_error;
};

inline std::string quote(std::string_view text) {
    return std::string("\"") + jsonEscape(text) + "\"";
}

inline void appendColor(std::ostringstream& out, const std::array<float, 4>& color) {
    out << '[' << color[0] << ", " << color[1] << ", " << color[2] << ", " << color[3] << ']';
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
    if (readNumber(object, key, parsed)) {
        value = static_cast<float>(parsed);
    }
}

inline void readOptionalInt(const JsonValue& object, std::string_view key, int& value) {
    double parsed = value;
    if (readNumber(object, key, parsed)) {
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

inline const char* xyRenderModePreferenceSchemaName(XYRenderModePreference preference) {
    switch (preference) {
        case XYRenderModePreference::Auto: return "auto";
        case XYRenderModePreference::Force3D: return "force3D";
        case XYRenderModePreference::Force2D: return "force2D";
        default: return "auto";
    }
}

inline const char* plotHudModeSchemaName(PlotHudMode mode) {
    switch (mode) {
        case PlotHudMode::Off: return "off";
        case PlotHudMode::Minimal: return "minimal";
        case PlotHudMode::Detailed: return "detailed";
        case PlotHudMode::OnlyWhileInteracting: return "onlyWhileInteracting";
        default: return "minimal";
    }
}

inline bool parseXYRenderModePreference(std::string_view text,
                                        XYRenderModePreference& preference) {
    if (text == "auto" || text == "Auto") {
        preference = XYRenderModePreference::Auto;
        return true;
    }
    if (text == "force3D" || text == "3D" || text == "Force3D") {
        preference = XYRenderModePreference::Force3D;
        return true;
    }
    if (text == "force2D" || text == "2D" || text == "Force2D") {
        preference = XYRenderModePreference::Force2D;
        return true;
    }
    return false;
}

inline bool parsePlotHudMode(std::string_view text, PlotHudMode& mode) {
    if (text == "off" || text == "Off") {
        mode = PlotHudMode::Off;
        return true;
    }
    if (text == "minimal" || text == "Minimal") {
        mode = PlotHudMode::Minimal;
        return true;
    }
    if (text == "detailed" || text == "Detailed") {
        mode = PlotHudMode::Detailed;
        return true;
    }
    if (text == "onlyWhileInteracting" || text == "Only While Interacting") {
        mode = PlotHudMode::OnlyWhileInteracting;
        return true;
    }
    return false;
}

inline ProjectSession makeProjectSession(const std::vector<FormulaEntry>& formulas,
                                         const Core::ViewTransform& view,
                                         const PlotSettings& plot) {
    ProjectSession session;
    session.formulas.reserve(formulas.size());
    for (const FormulaEntry& formula : formulas) {
        ProjectFormulaRecord record;
        record.expression = std::string(formula.inputBuffer);
        for (std::size_t channel = 0; channel < record.color.size(); ++channel) {
            record.color[channel] = formula.color[channel];
        }
        record.visible = formula.visible;
        record.zSlice = formula.zSlice;
        session.formulas.push_back(std::move(record));
    }

    session.view.centerX = view.centerX;
    session.view.centerY = view.centerY;
    session.view.scaleX = view.scaleX;
    session.view.scaleY = view.scaleY;
    session.plot = plot;
    return session;
}

inline std::string serializeProjectSession(const ProjectSession& session) {
    using namespace ProjectSessionDetail;

    std::ostringstream out;
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
        out << "      \"zSlice\": " << formula.zSlice << "\n";
        out << "    }" << ((i + 1 < session.formulas.size()) ? "," : "") << "\n";
    }
    out << "  ],\n";
    out << "  \"view\": {\n";
    out << "    \"centerX\": " << session.view.centerX << ",\n";
    out << "    \"centerY\": " << session.view.centerY << ",\n";
    out << "    \"scaleX\": " << session.view.scaleX << ",\n";
    out << "    \"scaleY\": " << session.view.scaleY << "\n";
    out << "  },\n";
    out << "  \"display\": {\n";
    out << "    \"xyRenderModePreference\": "
        << quote(xyRenderModePreferenceSchemaName(session.plot.xyRenderModePreference)) << ",\n";
    out << "    \"hudMode\": " << quote(plotHudModeSchemaName(session.plot.hudMode)) << ",\n";
    out << "    \"optimizeRendering\": " << jsonBool(session.plot.optimizeRendering) << ",\n";
    out << "    \"showGrid\": " << jsonBool(session.plot.showGrid) << ",\n";
    out << "    \"showCoordinates\": " << jsonBool(session.plot.showCoordinates) << ",\n";
    out << "    \"showWires\": " << jsonBool(session.plot.showWires) << ",\n";
    out << "    \"showSurfaceEnvelope\": " << jsonBool(session.plot.showSurfaceEnvelope) << ",\n";
    out << "    \"showAxisTriad\": " << jsonBool(session.plot.showAxisTriad) << ",\n";
    out << "    \"surfaceResolution\": " << session.plot.surfaceResolution << ",\n";
    out << "    \"implicitSurfaceResolution\": " << session.plot.implicitSurfaceResolution << ",\n";
    out << "    \"surfaceOpacity\": " << session.plot.surfaceOpacity << ",\n";
    out << "    \"wireOpacity\": " << session.plot.wireOpacity << ",\n";
    out << "    \"wireThickness\": " << session.plot.wireThickness << ",\n";
    out << "    \"wireStride\": " << session.plot.wireStride << ",\n";
    out << "    \"envelopeThickness\": " << session.plot.envelopeThickness << ",\n";
    out << "    \"heatmapOpacity\": " << session.plot.heatmapOpacity << "\n";
    out << "  },\n";
    out << "  \"camera\": {\n";
    out << "    \"azimuthDeg\": " << session.plot.azimuthDeg << ",\n";
    out << "    \"elevationDeg\": " << session.plot.elevationDeg << ",\n";
    out << "    \"zScale\": " << session.plot.zScale << ",\n";
    out << "    \"autoRotate\": " << jsonBool(session.plot.autoRotate) << ",\n";
    out << "    \"autoRotateSpeedDegPerSec\": " << session.plot.autoRotateSpeedDegPerSec << "\n";
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
    if (static_cast<int>(std::lround(schemaVersion)) != kProjectSessionSchemaVersion) {
        result.error = "Unsupported .xfplot schema version.";
        return result;
    }

    std::string fileType;
    if (readString(root, "fileType", fileType) && fileType != kProjectSessionFileType) {
        result.error = "File is not an XpressFormula project.";
        return result;
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

inline void applyProjectSession(const ProjectSession& session,
                                std::vector<FormulaEntry>& formulas,
                                Core::ViewTransform& view,
                                PlotSettings& plot,
                                std::vector<std::string>& warnings) {
    formulas.clear();
    formulas.reserve(session.formulas.size());
    for (std::size_t i = 0; i < session.formulas.size(); ++i) {
        const ProjectFormulaRecord& record = session.formulas[i];
        FormulaEntry entry;
        const std::size_t copyLength =
            (std::min)(record.expression.size(), sizeof(entry.inputBuffer) - 1);
        std::memcpy(entry.inputBuffer, record.expression.data(), copyLength);
        entry.inputBuffer[copyLength] = '\0';
        if (copyLength < record.expression.size()) {
            warnings.emplace_back("Formula " + std::to_string(i + 1) +
                " was truncated to fit the editor buffer.");
        }

        for (std::size_t channel = 0; channel < record.color.size(); ++channel) {
            entry.color[channel] = record.color[channel];
        }
        entry.visible = record.visible;
        entry.zSlice = record.zSlice;
        entry.parse();
        if (!entry.isValid() && !record.expression.empty()) {
            warnings.emplace_back("Formula " + std::to_string(i + 1) +
                " did not parse: " + entry.error);
        }
        formulas.push_back(std::move(entry));
    }

    view.centerX = std::isfinite(session.view.centerX) ? session.view.centerX : 0.0;
    view.centerY = std::isfinite(session.view.centerY) ? session.view.centerY : 0.0;
    view.scaleX = std::clamp(std::isfinite(session.view.scaleX) ? session.view.scaleX : 60.0,
                             0.1, 100000.0);
    view.scaleY = std::clamp(std::isfinite(session.view.scaleY) ? session.view.scaleY : 60.0,
                             0.1, 100000.0);

    plot = session.plot;
    plot.surfaceResolution = std::clamp(plot.surfaceResolution, 16, 256);
    plot.implicitSurfaceResolution = std::clamp(plot.implicitSurfaceResolution, 16, 192);
    plot.surfaceOpacity = std::clamp(plot.surfaceOpacity, 0.0f, 1.0f);
    plot.wireOpacity = clampWireOpacity(plot.wireOpacity);
    plot.wireThickness = std::clamp(plot.wireThickness, 0.05f, 8.0f);
    plot.wireStride = clampWireStride(plot.wireStride);
    plot.envelopeThickness = std::clamp(plot.envelopeThickness, 0.05f, 8.0f);
    plot.heatmapOpacity = std::clamp(plot.heatmapOpacity, 0.0f, 1.0f);
    plot.applyCoordinateOverlayPolicy();
}

inline std::string serializeCurrentProjectSession(const std::vector<FormulaEntry>& formulas,
                                                  const Core::ViewTransform& view,
                                                  const PlotSettings& plot) {
    return serializeProjectSession(makeProjectSession(formulas, view, plot));
}

} // namespace XpressFormula::UI
