// ProjectSessionTests.cpp - Unit tests for versioned .xfplot persistence.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/ProjectSession.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace XpressFormula::UI;
namespace XFCore = XpressFormula::Core;

namespace XpressFormulaTests {

static FormulaEntry makeFormula(const char* expression) {
    FormulaEntry entry;
    strncpy_s(entry.inputBuffer, sizeof(entry.inputBuffer), expression, _TRUNCATE);
    entry.color[0] = 0.25f;
    entry.color[1] = 0.50f;
    entry.color[2] = 0.75f;
    entry.color[3] = 1.0f;
    entry.visible = false;
    entry.zSlice = 1.25f;
    entry.parse();
    return entry;
}

static std::string makeProjectJson(const std::string& extraMembers = {}) {
    std::string json =
        "{ \"schemaVersion\": 1, \"fileType\": \"XpressFormulaProject\", \"formulas\": []";
    if (!extraMembers.empty()) {
        json += ", ";
        json += extraMembers;
    }
    json += " }";
    return json;
}

static std::string makeProjectJsonWithFormulas(const std::string& formulasJson) {
    return "{ \"schemaVersion\": 1, \"fileType\": \"XpressFormulaProject\", \"formulas\": " +
        formulasJson + " }";
}

static std::string makeProjectJsonWithCenterX(const char* numberText) {
    return makeProjectJson(std::string("\"view\": { \"centerX\": ") + numberText + " }");
}

static void assertClose(double expected, double actual, double tolerance = 0.0) {
    const double scale = (std::max)(1.0, (std::max)(std::fabs(expected), std::fabs(actual)));
    const double allowed = tolerance > 0.0
        ? tolerance
        : std::numeric_limits<double>::epsilon() * scale * 8.0;
    Assert::IsTrue(std::fabs(expected - actual) <= allowed);
}

static void assertParseFails(std::string_view json) {
    const ProjectSessionParseResult parsed = parseProjectSession(json);
    Assert::IsFalse(parsed.success);
}

TEST_CASE(ProjectSession_RoundTripPreservesCoreState) {
    std::vector<FormulaEntry> formulas = { makeFormula("sin(x)") };
    XFCore::ViewTransform view;
    view.centerX = 2.5;
    view.centerY = -1.25;
    view.scaleX = 80.0;
    view.scaleY = 90.0;

    PlotSettings plot;
    plot.xyRenderModePreference = XYRenderModePreference::Force3D;
    plot.hudMode = PlotHudMode::Detailed;
    plot.showGrid = false;
    plot.showCoordinates = false;
    plot.showAxisTriad = true;
    plot.azimuthDeg = 42.0f;
    plot.elevationDeg = -35.0f;
    plot.zScale = 2.0f;
    plot.surfaceResolution = 88;
    plot.wireOpacity = 0.4f;
    plot.wireStride = 4;

    const std::string json = serializeCurrentProjectSession(formulas, view, plot);
    const ProjectSessionParseResult parsed = parseProjectSession(json);

    Assert::IsTrue(parsed.success);
    Assert::AreEqual(1, static_cast<int>(parsed.session.formulas.size()));
    Assert::IsTrue(parsed.session.formulas[0].expression == "sin(x)");
    Assert::IsFalse(parsed.session.formulas[0].visible);
    Assert::AreEqual(2.5, parsed.session.view.centerX);
    Assert::AreEqual(-1.25, parsed.session.view.centerY);
    Assert::AreEqual(XYRenderModePreference::Force3D, parsed.session.plot.xyRenderModePreference);
    Assert::AreEqual(PlotHudMode::Detailed, parsed.session.plot.hudMode);
    Assert::IsTrue(parsed.session.plot.showAxisTriad);
    Assert::AreEqual(88, parsed.session.plot.surfaceResolution);
    Assert::AreEqual(4, parsed.session.plot.wireStride);
}

TEST_CASE(ProjectSession_HighPrecisionValuesRoundTrip) {
    ProjectSession session;
    session.formulas.push_back(ProjectFormulaRecord{});
    session.formulas[0].expression = "sin(x)";
    session.formulas[0].color = { 0.123456791f, 0.234567896f, 0.345678926f, 0.456789136f };
    session.formulas[0].zSlice = 0.123456791f;
    session.view.centerX = 0.12345678901234566;
    session.view.centerY = -98765.432109876536;
    session.view.scaleX = 60.123456789012344;
    session.view.scaleY = 0.33333333333333331;
    session.plot.azimuthDeg = 42.123455f;
    session.plot.elevationDeg = -37.987656f;
    session.plot.zScale = 1.23456788f;
    session.plot.surfaceOpacity = 0.876543224f;
    session.plot.wireOpacity = 0.345678926f;
    session.plot.wireThickness = 1.23456788f;
    session.plot.envelopeThickness = 2.34567881f;
    session.plot.heatmapOpacity = 0.654321074f;

    const ProjectSessionParseResult parsed = parseProjectSession(serializeProjectSession(session));

    Assert::IsTrue(parsed.success);
    Assert::IsTrue(session.view.centerX == parsed.session.view.centerX);
    Assert::IsTrue(session.view.centerY == parsed.session.view.centerY);
    Assert::IsTrue(session.view.scaleX == parsed.session.view.scaleX);
    Assert::IsTrue(session.view.scaleY == parsed.session.view.scaleY);
    Assert::AreEqual(session.formulas[0].zSlice, parsed.session.formulas[0].zSlice);
    Assert::AreEqual(session.plot.azimuthDeg, parsed.session.plot.azimuthDeg);
    Assert::AreEqual(session.plot.elevationDeg, parsed.session.plot.elevationDeg);
    Assert::AreEqual(session.plot.zScale, parsed.session.plot.zScale);
    Assert::AreEqual(session.plot.surfaceOpacity, parsed.session.plot.surfaceOpacity);
    Assert::AreEqual(session.plot.wireOpacity, parsed.session.plot.wireOpacity);
    Assert::AreEqual(session.plot.wireThickness, parsed.session.plot.wireThickness);
    Assert::AreEqual(session.plot.envelopeThickness, parsed.session.plot.envelopeThickness);
    Assert::AreEqual(session.plot.heatmapOpacity, parsed.session.plot.heatmapOpacity);
}

TEST_CASE(ProjectSession_RepeatedSerializeParseIsStable) {
    ProjectSession session;
    session.formulas.push_back(ProjectFormulaRecord{});
    session.formulas[0].expression = "z=sin(x)*cos(y)";
    session.formulas[0].zSlice = -0.987654328f;
    session.view.centerX = 1234.5678901234567;
    session.view.centerY = -0.00000012345678901234559;
    session.view.scaleX = 77.777777777777771;
    session.view.scaleY = 88.888888888888886;
    session.plot.xyRenderModePreference = XYRenderModePreference::Force3D;
    session.plot.hudMode = PlotHudMode::OnlyWhileInteracting;
    session.plot.azimuthDeg = 12.3456783f;
    session.plot.elevationDeg = -45.6789131f;
    session.plot.zScale = 3.33333325f;

    std::string json = serializeProjectSession(session);
    for (int i = 0; i < 5; ++i) {
        const ProjectSessionParseResult parsed = parseProjectSession(json);
        Assert::IsTrue(parsed.success);
        const std::string nextJson = serializeProjectSession(parsed.session);
        Assert::AreEqual(json, nextJson);
        json = nextJson;
    }
}

TEST_CASE(ProjectSession_SerializedSnapshotCharacterizesDirtyState) {
    ProjectSession session;
    session.formulas.push_back(ProjectFormulaRecord{});
    session.formulas[0].expression = "sin(sqrt(x^2+y^2))";
    session.formulas[0].visible = true;
    session.formulas[0].color = { 0.18f, 0.78f, 0.32f, 1.0f };
    session.view.scaleX = 60.0;
    session.view.scaleY = 60.0;

    const std::string cleanSnapshot = serializeProjectSession(session);

    ProjectSession edited = session;
    edited.formulas[0].expression = "sin(x)";
    Assert::IsFalse(serializeProjectSession(edited) == cleanSnapshot);

    const std::string savedSnapshot = serializeProjectSession(edited);
    Assert::AreEqual(savedSnapshot, serializeProjectSession(edited));

    edited.view.centerX = 2.0;
    Assert::IsFalse(serializeProjectSession(edited) == savedSnapshot);
}

TEST_CASE(ProjectSession_SerializesNonFiniteValuesAsSafeJsonNumbers) {
    ProjectSession session;
    session.formulas.push_back(ProjectFormulaRecord{});
    session.formulas[0].expression = "sin(x)";
    session.formulas[0].color[0] = std::numeric_limits<float>::quiet_NaN();
    session.formulas[0].zSlice = std::numeric_limits<float>::infinity();
    session.view.centerX = std::numeric_limits<double>::quiet_NaN();
    session.view.scaleX = std::numeric_limits<double>::infinity();
    session.plot.azimuthDeg = std::numeric_limits<float>::infinity();
    session.plot.wireOpacity = std::numeric_limits<float>::quiet_NaN();

    const std::string json = serializeProjectSession(session);
    Assert::IsTrue(json.find("nan") == std::string::npos);
    Assert::IsTrue(json.find("inf") == std::string::npos);

    const ProjectSessionParseResult parsed = parseProjectSession(json);
    Assert::IsTrue(parsed.success);
    Assert::AreEqual(0.0, parsed.session.view.centerX);
    Assert::AreEqual(60.0, parsed.session.view.scaleX);
    Assert::AreEqual(0.0f, parsed.session.formulas[0].zSlice);
    Assert::AreEqual(kDefaultAzimuthDeg, parsed.session.plot.azimuthDeg);
    Assert::AreEqual(kDefaultWireOpacity, parsed.session.plot.wireOpacity);
}

TEST_CASE(ProjectSession_ParsesUnicodeEscapesAsUtf8) {
    const std::string escaped = R"json(
        {
          "schemaVersion": 1,
          "fileType": "XpressFormulaProject",
          "formulas": [
            { "expression": "\u03C0 + \u221A(x)" },
            { "expression": "\u4F60\u597D" },
            { "expression": "\uD83D\uDE80" }
          ]
        }
    )json";

    const ProjectSessionParseResult parsed = parseProjectSession(escaped);
    Assert::IsTrue(parsed.success);
    Assert::AreEqual(3, static_cast<int>(parsed.session.formulas.size()));
    Assert::AreEqual(std::string("\xCF\x80 + \xE2\x88\x9A(x)"),
                     parsed.session.formulas[0].expression);
    Assert::AreEqual(std::string("\xE4\xBD\xA0\xE5\xA5\xBD"),
                     parsed.session.formulas[1].expression);
    Assert::AreEqual(std::string("\xF0\x9F\x9A\x80"),
                     parsed.session.formulas[2].expression);
}

TEST_CASE(ProjectSession_PreservesDirectUtf8Strings) {
    const std::string directExpression =
        std::string("\xCF\x80 + \xE2\x88\x9A(x) + \xF0\x9F\x9A\x80");
    const std::string json = makeProjectJsonWithFormulas(
        std::string("[{ \"expression\": \"") + directExpression + "\" }]");

    const ProjectSessionParseResult parsed = parseProjectSession(json);
    Assert::IsTrue(parsed.success);
    Assert::AreEqual(directExpression, parsed.session.formulas[0].expression);
}

TEST_CASE(ProjectSession_RejectsMalformedSurrogatePair) {
    const char* invalidJson[] = {
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "\uD83D" }] })json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "\uD83Dx" }] })json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "\uDE80" }] })json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "\uD83D\u0041" }] })json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "\u12" }] })json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "\u12G4" }] })json"
    };

    for (const char* json : invalidJson) {
        assertParseFails(json);
    }
}

TEST_CASE(ProjectSession_RejectsMalformedJson) {
    const char* malformed[] = {
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [] )json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [] } trailing)json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "sin(x) }] })json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [], "view": { "centerX": 1. } })json",
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": [{ "expression": "\q" }] })json",
        "{ \"schemaVersion\": 1, \"fileType\": \"XpressFormulaProject\", \"formulas\": "
            "[{ \"expression\": \"line\nbreak\" }] }"
    };

    for (const char* json : malformed) {
        assertParseFails(json);
    }
}

TEST_CASE(ProjectSession_RejectsWrongFileType) {
    assertParseFails(
        R"json({ "schemaVersion": 1, "fileType": "SomeOtherApplication", "formulas": [] })json");
    assertParseFails(
        R"json({ "schemaVersion": 1, "fileType": 123, "formulas": [] })json");
}

TEST_CASE(ProjectSession_RejectsMissingFormulasArray) {
    assertParseFails(R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject" })json");
    assertParseFails(
        R"json({ "schemaVersion": 1, "fileType": "XpressFormulaProject", "formulas": {} })json");
}

TEST_CASE(ProjectSession_RejectsMissingSchemaVersion) {
    assertParseFails(R"json({ "fileType": "XpressFormulaProject", "formulas": [] })json");
    assertParseFails(
        R"json({ "schemaVersion": "1", "fileType": "XpressFormulaProject", "formulas": [] })json");
}

TEST_CASE(ProjectSession_RejectsFractionalAndNegativeSchemaVersion) {
    assertParseFails(
        R"json({ "schemaVersion": 1.5, "fileType": "XpressFormulaProject", "formulas": [] })json");
    assertParseFails(
        R"json({ "schemaVersion": -1, "fileType": "XpressFormulaProject", "formulas": [] })json");
}

TEST_CASE(ProjectSession_RejectsUnsupportedSchemaVersion) {
    const std::string json =
        "{ \"schemaVersion\": 99, \"fileType\": \"XpressFormulaProject\", \"formulas\": [] }";
    const ProjectSessionParseResult parsed = parseProjectSession(json);

    Assert::IsFalse(parsed.success);
    Assert::IsTrue(parsed.error.find("Unsupported") != std::string::npos);
}

TEST_CASE(ProjectSession_IgnoresUnknownFields) {
    const ProjectSessionParseResult parsed = parseProjectSession(makeProjectJson(
        "\"futureTopLevel\": { \"nested\": [1, true, null] }, "
        "\"display\": { \"showGrid\": false, \"futureDisplay\": \"kept-for-later\" }"));

    Assert::IsTrue(parsed.success);
    Assert::IsFalse(parsed.session.plot.showGrid);
}

TEST_CASE(ProjectSession_EmptyProjectRoundTrips) {
    ProjectSession session;
    session.formulas.clear();

    const ProjectSessionParseResult parsed = parseProjectSession(serializeProjectSession(session));
    Assert::IsTrue(parsed.success);
    Assert::AreEqual(0, static_cast<int>(parsed.session.formulas.size()));

    std::vector<FormulaEntry> formulas;
    XFCore::ViewTransform view;
    PlotSettings plot;
    std::vector<std::string> warnings;
    applyProjectSession(parsed.session, formulas, view, plot, warnings);
    Assert::AreEqual(0, static_cast<int>(formulas.size()));
}

TEST_CASE(ProjectSession_MultipleFormulasPreserveOrderAndState) {
    ProjectSession session;
    for (int i = 0; i < 3; ++i) {
        ProjectFormulaRecord formula;
        formula.expression = i == 0 ? "sin(x)" : (i == 1 ? "x^2+y^2" : "x^2+y^2=1");
        formula.visible = i != 1;
        formula.color = {
            0.1f * static_cast<float>(i + 1),
            0.2f * static_cast<float>(i + 1),
            0.3f * static_cast<float>(i + 1),
            1.0f
        };
        formula.zSlice = static_cast<float>(i) - 1.0f;
        session.formulas.push_back(formula);
    }

    const ProjectSessionParseResult parsed = parseProjectSession(serializeProjectSession(session));
    Assert::IsTrue(parsed.success);
    Assert::AreEqual(3, static_cast<int>(parsed.session.formulas.size()));
    Assert::AreEqual(std::string("sin(x)"), parsed.session.formulas[0].expression);
    Assert::AreEqual(std::string("x^2+y^2"), parsed.session.formulas[1].expression);
    Assert::AreEqual(std::string("x^2+y^2=1"), parsed.session.formulas[2].expression);
    Assert::IsTrue(parsed.session.formulas[0].visible);
    Assert::IsFalse(parsed.session.formulas[1].visible);
    Assert::AreEqual(0.6f, parsed.session.formulas[1].color[2]);
    Assert::AreEqual(1.0f, parsed.session.formulas[2].zSlice);
}

TEST_CASE(ProjectSession_SkipsMalformedFormulaEntriesWithWarnings) {
    const ProjectSessionParseResult parsed = parseProjectSession(makeProjectJsonWithFormulas(
        R"json([42, { "visible": true }, { "expression": "x" }])json"));

    Assert::IsTrue(parsed.success);
    Assert::AreEqual(1, static_cast<int>(parsed.session.formulas.size()));
    Assert::IsTrue(parsed.warnings.size() >= 2);
}

TEST_CASE(ProjectSession_LongFormulaProducesTruncationWarning) {
    FormulaEntry probe;
    const std::size_t bufferSize = sizeof(probe.inputBuffer);

    ProjectSession session;
    ProjectFormulaRecord formula;
    formula.expression.assign(bufferSize + 40, 'x');
    session.formulas.push_back(std::move(formula));

    std::vector<FormulaEntry> formulas;
    XFCore::ViewTransform view;
    PlotSettings plot;
    std::vector<std::string> warnings;
    applyProjectSession(session, formulas, view, plot, warnings);

    Assert::AreEqual(1, static_cast<int>(formulas.size()));
    Assert::AreEqual(static_cast<int>(bufferSize - 1),
                     static_cast<int>(std::strlen(formulas[0].inputBuffer)));
    Assert::AreEqual('\0', formulas[0].inputBuffer[bufferSize - 1]);

    const bool hasTruncationWarning = std::any_of(
        warnings.begin(),
        warnings.end(),
        [](const std::string& warning) {
            return warning.find("truncated") != std::string::npos;
        });
    Assert::IsTrue(hasTruncationWarning);
}

TEST_CASE(ProjectSession_ApplyValidatesLoadedFormulas) {
    ProjectSession session;
    session.formulas.push_back(ProjectFormulaRecord{ "x = y = 1" });

    std::vector<FormulaEntry> formulas;
    XFCore::ViewTransform view;
    PlotSettings plot;
    std::vector<std::string> warnings;
    applyProjectSession(session, formulas, view, plot, warnings);

    Assert::AreEqual(1, static_cast<int>(formulas.size()));
    Assert::AreEqual(std::string("x = y = 1"), std::string(formulas[0].inputBuffer));
    Assert::IsFalse(formulas[0].isValid());
    Assert::IsFalse(warnings.empty());
}

TEST_CASE(ProjectSession_ParsesEnumCompatibilityAndWarnsOnUnknownValues) {
    const struct {
        const char* value;
        XYRenderModePreference expected;
    } renderModes[] = {
        { "auto", XYRenderModePreference::Auto },
        { "force2D", XYRenderModePreference::Force2D },
        { "force3D", XYRenderModePreference::Force3D }
    };

    for (const auto& mode : renderModes) {
        const ProjectSessionParseResult parsed = parseProjectSession(makeProjectJson(
            std::string("\"display\": { \"xyRenderModePreference\": \"") + mode.value + "\" }"));
        Assert::IsTrue(parsed.success);
        Assert::AreEqual(mode.expected, parsed.session.plot.xyRenderModePreference);
    }

    const struct {
        const char* value;
        PlotHudMode expected;
    } hudModes[] = {
        { "off", PlotHudMode::Off },
        { "minimal", PlotHudMode::Minimal },
        { "detailed", PlotHudMode::Detailed },
        { "onlyWhileInteracting", PlotHudMode::OnlyWhileInteracting }
    };

    for (const auto& mode : hudModes) {
        const ProjectSessionParseResult parsed = parseProjectSession(makeProjectJson(
            std::string("\"display\": { \"hudMode\": \"") + mode.value + "\" }"));
        Assert::IsTrue(parsed.success);
        Assert::AreEqual(mode.expected, parsed.session.plot.hudMode);
    }

    const ProjectSessionParseResult unknown = parseProjectSession(makeProjectJson(
        "\"display\": { \"xyRenderModePreference\": \"future\", \"hudMode\": \"verbose\" }"));
    Assert::IsTrue(unknown.success);
    Assert::AreEqual(XYRenderModePreference::Auto, unknown.session.plot.xyRenderModePreference);
    Assert::AreEqual(PlotHudMode::Minimal, unknown.session.plot.hudMode);
    Assert::AreEqual(2, static_cast<int>(unknown.warnings.size()));
}

TEST_CASE(ProjectSession_AppliesSafeClampsAndCoordinatePolicy) {
    ProjectSession session;
    session.view.centerX = std::numeric_limits<double>::quiet_NaN();
    session.view.centerY = std::numeric_limits<double>::infinity();
    session.view.scaleX = 0.0;
    session.view.scaleY = 500000.0;
    session.plot.surfaceResolution = 999;
    session.plot.implicitSurfaceResolution = -4;
    session.plot.surfaceOpacity = 8.0f;
    session.plot.wireOpacity = 2.0f;
    session.plot.wireThickness = std::numeric_limits<float>::quiet_NaN();
    session.plot.wireStride = 100;
    session.plot.envelopeThickness = std::numeric_limits<float>::infinity();
    session.plot.heatmapOpacity = -4.0f;
    session.plot.azimuthDeg = 999.0f;
    session.plot.elevationDeg = -999.0f;
    session.plot.zScale = 999.0f;
    session.plot.autoRotateSpeedDegPerSec = -10.0f;
    session.plot.showCoordinates = true;
    session.plot.showAxisTriad = true;

    std::vector<FormulaEntry> formulas;
    XFCore::ViewTransform view;
    PlotSettings plot;
    std::vector<std::string> warnings;
    applyProjectSession(session, formulas, view, plot, warnings);

    Assert::AreEqual(0.0, view.centerX);
    Assert::AreEqual(0.0, view.centerY);
    Assert::AreEqual(0.1, view.scaleX);
    Assert::AreEqual(100000.0, view.scaleY);
    Assert::AreEqual(256, plot.surfaceResolution);
    Assert::AreEqual(16, plot.implicitSurfaceResolution);
    Assert::AreEqual(1.0f, plot.surfaceOpacity);
    Assert::AreEqual(1.0f, plot.wireOpacity);
    Assert::AreEqual(kDefaultWireThickness, plot.wireThickness);
    Assert::AreEqual(16, plot.wireStride);
    Assert::AreEqual(kDefaultEnvelopeThickness, plot.envelopeThickness);
    Assert::AreEqual(0.0f, plot.heatmapOpacity);
    Assert::AreEqual(180.0f, plot.azimuthDeg);
    Assert::AreEqual(-85.0f, plot.elevationDeg);
    Assert::AreEqual(8.0f, plot.zScale);
    Assert::AreEqual(2.0f, plot.autoRotateSpeedDegPerSec);
    Assert::IsFalse(plot.showAxisTriad);
}

TEST_CASE(ProjectSession_AcceptsJsonNumberGrammar) {
    const ProjectSessionParseResult parsed = parseProjectSession(makeProjectJson(
        "\"view\": { \"centerX\": -12, \"centerY\": 12.34, "
        "\"scaleX\": 1e10, \"scaleY\": -4.2E-3 }"));

    Assert::IsTrue(parsed.success);
    Assert::AreEqual(-12.0, parsed.session.view.centerX);
    Assert::AreEqual(12.34, parsed.session.view.centerY);
    Assert::AreEqual(1e10, parsed.session.view.scaleX);
    assertClose(-4.2e-3, parsed.session.view.scaleY);
}

TEST_CASE(ProjectSession_RejectsInvalidJsonNumberForms) {
    const char* invalidNumbers[] = {
        "+1",
        "01",
        "1.",
        ".e2",
        "NaN",
        "Infinity",
        "1e999"
    };

    for (const char* numberText : invalidNumbers) {
        assertParseFails(makeProjectJsonWithCenterX(numberText));
    }
}

TEST_CASE(ProjectSession_BoundedNumberParserHonorsStringViewLength) {
    std::string backing = "12.34";
    std::string_view bounded(backing.data(), 2);

    ProjectSessionDetail::JsonValue value;
    ProjectSessionDetail::JsonParser parser(bounded);
    std::string error;
    Assert::IsTrue(parser.parse(value, error));
    Assert::AreEqual(ProjectSessionDetail::JsonValue::Type::Number, value.type);
    Assert::AreEqual(12.0, value.number);
}

} // namespace XpressFormulaTests
