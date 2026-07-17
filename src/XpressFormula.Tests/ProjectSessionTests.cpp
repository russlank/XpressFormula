// ProjectSessionTests.cpp - Unit tests for versioned .xfplot persistence.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/ProjectSession.h"

#include <cstring>
#include <string>
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

TEST_CASE(ProjectSession_ApplyValidatesLoadedFormulas) {
    ProjectSession session;
    session.formulas.push_back(ProjectFormulaRecord{ "x = y = 1" });

    std::vector<FormulaEntry> formulas;
    XFCore::ViewTransform view;
    PlotSettings plot;
    std::vector<std::string> warnings;
    applyProjectSession(session, formulas, view, plot, warnings);

    Assert::AreEqual(1, static_cast<int>(formulas.size()));
    Assert::IsFalse(formulas[0].isValid());
    Assert::IsFalse(warnings.empty());
}

TEST_CASE(ProjectSession_RejectsUnsupportedSchemaVersion) {
    const std::string json =
        "{ \"schemaVersion\": 99, \"fileType\": \"XpressFormulaProject\", \"formulas\": [] }";
    const ProjectSessionParseResult parsed = parseProjectSession(json);

    Assert::IsFalse(parsed.success);
    Assert::IsTrue(parsed.error.find("Unsupported") != std::string::npos);
}

TEST_CASE(ProjectSession_AppliesSafeClampsAndCoordinatePolicy) {
    ProjectSession session;
    session.view.scaleX = 0.0;
    session.view.scaleY = 500000.0;
    session.plot.surfaceResolution = 999;
    session.plot.implicitSurfaceResolution = -4;
    session.plot.wireOpacity = 2.0f;
    session.plot.wireStride = 100;
    session.plot.showCoordinates = true;
    session.plot.showAxisTriad = true;

    std::vector<FormulaEntry> formulas;
    XFCore::ViewTransform view;
    PlotSettings plot;
    std::vector<std::string> warnings;
    applyProjectSession(session, formulas, view, plot, warnings);

    Assert::AreEqual(0.1, view.scaleX);
    Assert::AreEqual(100000.0, view.scaleY);
    Assert::AreEqual(256, plot.surfaceResolution);
    Assert::AreEqual(16, plot.implicitSurfaceResolution);
    Assert::AreEqual(1.0f, plot.wireOpacity);
    Assert::AreEqual(16, plot.wireStride);
    Assert::IsFalse(plot.showAxisTriad);
}

} // namespace XpressFormulaTests
