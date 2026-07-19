// SceneSummaryTests.cpp - Unit tests for visible scene capability analysis.
#include "CppUnitTest.h"
#include "../XpressFormula/Model/SceneSummary.h"

#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFModel = XpressFormula::Model;

namespace XpressFormulaTests {

static XFModel::Formula makeFormula(const char* expression, bool visible = true) {
    XFModel::Formula formula;
    formula.setExpression(expression ? expression : "");
    formula.visible = visible;
    formula.compile();
    return formula;
}

static XFModel::SceneSummary analyze(const std::vector<XFModel::Formula>& formulas) {
    return XFModel::analyzeScene(std::span<const XFModel::Formula>(
        formulas.data(), formulas.size()));
}

TEST_CASE(SceneSummary_NoFormulasIsEmpty) {
    const XFModel::SceneSummary scene = analyze({});

    Assert::IsTrue(scene.empty());
    Assert::IsFalse(scene.hasVisible2D());
    Assert::IsFalse(scene.hasVisible3D());
}

TEST_CASE(SceneSummary_InvalidOnlyIsEmpty) {
    const XFModel::SceneSummary scene = analyze({ makeFormula("x = y = 1") });

    Assert::IsTrue(scene.empty());
    Assert::IsFalse(scene.hasVisible2D());
    Assert::IsFalse(scene.hasVisible3D());
}

TEST_CASE(SceneSummary_HiddenFormulasDoNotContribute) {
    const XFModel::SceneSummary scene = analyze({ makeFormula("z = x^2 + y^2", false) });

    Assert::IsTrue(scene.empty());
    Assert::IsFalse(scene.hasVisibleExplicitSurface3D);
}

TEST_CASE(SceneSummary_CurvesOnlyAre2D) {
    const XFModel::SceneSummary scene = analyze({ makeFormula("sin(x)") });

    Assert::IsTrue(scene.hasVisibleCurve2D);
    Assert::IsTrue(scene.hasVisible2D());
    Assert::IsFalse(scene.hasVisible3D());
}

TEST_CASE(SceneSummary_ExplicitSurfaceOnlyIs3D) {
    const XFModel::SceneSummary scene = analyze({ makeFormula("z = x^2 + y^2") });

    Assert::IsTrue(scene.hasVisibleExplicitSurface3D);
    Assert::IsFalse(scene.hasVisible2D());
    Assert::IsTrue(scene.hasVisible3D());
}

TEST_CASE(SceneSummary_ImplicitSurfaceOnlyIs3D) {
    const XFModel::SceneSummary scene = analyze({ makeFormula("x^2 + y^2 + z^2 = 1") });

    Assert::IsTrue(scene.hasVisibleImplicitSurface3D);
    Assert::IsFalse(scene.hasVisible2D());
    Assert::IsTrue(scene.hasVisible3D());
}

TEST_CASE(SceneSummary_ScalarFieldExpressionIs2DContent) {
    const XFModel::SceneSummary scene = analyze({ makeFormula("x + y + z") });

    Assert::IsTrue(scene.hasVisibleScalarField);
    Assert::IsTrue(scene.hasVisible2D());
    Assert::IsFalse(scene.hasVisible3D());
}

TEST_CASE(SceneSummary_Mixed2DAnd3DTracksBothCapabilities) {
    const XFModel::SceneSummary scene = analyze({
        makeFormula("sin(x)"),
        makeFormula("z = x^2 + y^2")
    });

    Assert::IsTrue(scene.hasVisible2D());
    Assert::IsTrue(scene.hasVisible3D());
    Assert::IsFalse(scene.empty());
}

} // namespace XpressFormulaTests
