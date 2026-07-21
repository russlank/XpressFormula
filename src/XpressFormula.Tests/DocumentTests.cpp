// DocumentTests.cpp - Revision and dirty-state tests for the document aggregate.
#include "CppUnitTest.h"
#include "../XpressFormula/Model/Document.h"

#include <limits>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFCore = XpressFormula::Core;
namespace XFModel = XpressFormula::Model;

namespace XpressFormulaTests {
namespace {

XFModel::Formula makeFormula(const char* expression) {
    XFModel::Formula formula;
    formula.setExpression(expression ? expression : "");
    formula.compile();
    return formula;
}

} // namespace

TEST_CASE(Document_RevisionDirtyAndCleanTransitions) {
    XFModel::Document document;
    Assert::AreEqual(0ull, document.revision());
    Assert::IsFalse(document.dirty());

    document.addFormula(makeFormula("sin(x)"));
    Assert::AreEqual(1ull, document.revision());
    Assert::IsTrue(document.dirty());

    document.markSaved();
    Assert::AreEqual(document.revision(), document.savedRevision());
    Assert::IsFalse(document.dirty());
}

TEST_CASE(Document_NoOpMutationsDoNotIncrementRevision) {
    XFModel::Document document;
    const XFModel::Formula formula = makeFormula("sin(x)");
    const XFModel::FormulaId id = document.addFormula(formula);
    XFModel::ViewState view = document.view();
    XFModel::PlotSettings plot = document.plotSettings();
    document.markSaved();
    const XFModel::Document::Revision saved = document.revision();

    Assert::IsFalse(document.updateFormula(id, formula));
    Assert::IsFalse(document.setFormulaVisibility(id, formula.visible));
    Assert::IsFalse(document.setFormulaColor(id, formula.color));
    Assert::IsFalse(document.setFormulaZSlice(id, formula.zSlice));
    Assert::IsFalse(document.hideOtherFormulas(id));
    Assert::IsFalse(document.moveFormula(id, 0));
    Assert::IsFalse(document.setViewState(view));
    Assert::IsFalse(document.setPlotSettings(plot));

    {
        auto formulas = document.editFormulas();
        (void)formulas.get();
    }
    {
        auto editedView = document.editViewTransform();
        editedView.get().viewport.width += 100.0f;
    }
    {
        auto editedPlot = document.editPlotSettings();
        (void)editedPlot.get();
    }

    Assert::AreEqual(saved, document.revision());
    Assert::IsFalse(document.dirty());
}

TEST_CASE(Document_AddAndUpdateRevisionBehaviorIsTransactional) {
    XFModel::Document document;
    XFModel::Formula formula = makeFormula("sin(x)");

    const XFModel::Document::Revision beforeAdd = document.revision();
    const XFModel::FormulaId id = document.addFormula(formula);
    Assert::AreEqual(beforeAdd + 1, document.revision());
    Assert::AreEqual(1, static_cast<int>(document.formulas().size()));

    document.markSaved();
    const XFModel::Document::Revision beforeNoOp = document.revision();
    Assert::IsFalse(document.updateFormula(id, formula));
    Assert::AreEqual(beforeNoOp, document.revision());

    XFModel::Formula updated = formula;
    updated.setExpression("cos(x)");
    updated.compile();
    Assert::IsTrue(document.updateFormula(id, updated));
    Assert::AreEqual(beforeNoOp + 1, document.revision());
    Assert::AreEqual(id, document.formulas()[0].id);
    Assert::AreEqual(std::string("cos(x)"), document.formulas()[0].expression);
}

TEST_CASE(Document_UpdateMissingFormulaFailsWithoutInsertion) {
    XFModel::Document document;
    XFModel::Formula formula = makeFormula("sin(x)");
    document.addFormula(formula);
    document.markSaved();
    const XFModel::Document::Revision before = document.revision();

    XFModel::Formula updated = makeFormula("cos(x)");
    Assert::IsFalse(document.updateFormula(formula.id + 1000, updated));

    Assert::AreEqual(before, document.revision());
    Assert::AreEqual(1, static_cast<int>(document.formulas().size()));
    Assert::AreEqual(std::string("sin(x)"), document.formulas()[0].expression);
}

TEST_CASE(Document_UpdateFindsFormulaAfterReorderByStableId) {
    XFModel::Document document;
    const XFModel::FormulaId firstId = document.addFormula(makeFormula("sin(x)"));
    const XFModel::FormulaId secondId = document.addFormula(makeFormula("cos(x)"));
    document.markSaved();

    Assert::IsTrue(document.moveFormula(secondId, 0));
    document.markSaved();
    const XFModel::Document::Revision beforeUpdate = document.revision();

    XFModel::Formula updated = makeFormula("tan(x)");
    Assert::IsTrue(document.updateFormula(firstId, updated));

    Assert::AreEqual(beforeUpdate + 1, document.revision());
    Assert::AreEqual(secondId, document.formulas()[0].id);
    Assert::AreEqual(firstId, document.formulas()[1].id);
    Assert::AreEqual(std::string("tan(x)"), document.formulas()[1].expression);
}

TEST_CASE(Formula_SetExpressionInvalidatesStaleCompiledState) {
    XFModel::Formula formula = makeFormula("sin(x)");
    const auto oldAst = formula.compiled.ast;

    formula.setExpression("cos(x)");

    Assert::AreEqual(std::string("cos(x)"), formula.expression);
    Assert::IsFalse(formula.hasCompiledExpression);
    Assert::IsTrue(formula.lastCompiledExpression.empty());
    Assert::IsFalse(formula.compiled.valid());
    Assert::IsTrue(formula.compiled.ast == nullptr);
    Assert::IsTrue(oldAst != formula.compiled.ast);
}

TEST_CASE(Document_EditScopesTrackPersistentChanges) {
    XFModel::Document document;
    document.addFormula(makeFormula("sin(x)"));
    document.markSaved();
    const XFModel::FormulaId formulaId = document.formulas()[0].id;

    {
        auto formulas = document.editFormulas();
        formulas.get()[0].visible = false;
    }
    Assert::AreEqual(2ull, document.revision());
    Assert::IsTrue(document.dirty());

    document.markSaved();
    {
        auto view = document.editViewTransform();
        view.get().state.centerX = 10.0;
    }
    Assert::AreEqual(3ull, document.revision());
    Assert::IsTrue(document.dirty());

    document.markSaved();
    {
        auto plot = document.editPlotSettings();
        plot.get().showGrid = !plot.get().showGrid;
    }
    Assert::AreEqual(4ull, document.revision());
    Assert::IsTrue(document.dirty());

    document.markSaved();
    Assert::IsTrue(document.removeFormula(formulaId));
    Assert::AreEqual(5ull, document.revision());
    Assert::IsTrue(document.dirty());
}

TEST_CASE(Document_FormulaCommandsTrackOnlyRealChanges) {
    XFModel::Document document;
    const XFModel::FormulaId firstId = document.addFormula(makeFormula("sin(x)"));
    const XFModel::FormulaId secondId = document.addFormula(makeFormula("cos(x)"));
    document.markSaved();
    const XFModel::Document::Revision cleanRevision = document.revision();

    XFModel::ColorRgba color = document.formulas()[0].color;
    color[0] = 0.25f;

    Assert::IsTrue(document.setFormulaColor(firstId, color));
    Assert::AreEqual(cleanRevision + 1, document.revision());
    document.markSaved();

    Assert::IsTrue(document.setFormulaZSlice(firstId, 2.5));
    Assert::AreEqual(cleanRevision + 2, document.revision());
    document.markSaved();

    Assert::IsTrue(document.hideOtherFormulas(secondId));
    Assert::IsFalse(document.formulas()[0].visible);
    Assert::IsTrue(document.formulas()[1].visible);
    Assert::AreEqual(cleanRevision + 3, document.revision());
    document.markSaved();

    Assert::IsTrue(document.duplicateFormula(secondId));
    Assert::AreEqual(3, static_cast<int>(document.formulas().size()));
    Assert::IsTrue(document.formulas()[1].id != document.formulas()[2].id);
    Assert::AreEqual(std::string("cos(x)"), document.formulas()[2].expression);
    Assert::AreEqual(cleanRevision + 4, document.revision());
}

TEST_CASE(Document_ReplaceStateCanEstablishCleanLoadedDocument) {
    XFModel::Document document;
    document.addFormula(makeFormula("sin(x)"));
    document.markSaved();

    XFCore::ViewTransform view;
    view.state.centerY = 4.5;
    XFModel::PlotSettings plot;
    plot.showWires = false;

    std::vector<XFModel::Formula> formulas;
    formulas.push_back(makeFormula("cos(x)"));
    document.replaceState(std::move(formulas), view, plot, true);

    Assert::AreEqual(1, static_cast<int>(document.formulas().size()));
    Assert::AreEqual(4.5, document.view().centerY);
    Assert::IsFalse(document.plotSettings().showWires);
    Assert::IsFalse(document.dirty());

    Assert::IsTrue(document.setViewState({ 1.0, 2.0, 60.0, 60.0 }));
    Assert::IsTrue(document.dirty());
}

TEST_CASE(Document_PublicMutationsNormalizePersistentState) {
    XFModel::Document document;

    XFModel::Formula first = makeFormula("sin(x)");
    first.id = 42;
    const XFModel::FormulaId firstId = document.addFormula(first);

    XFModel::Formula duplicate = makeFormula("cos(x)");
    duplicate.id = firstId;
    duplicate.color[0] = -1.0f;
    duplicate.color[1] = 0.25f;
    duplicate.color[2] = std::numeric_limits<float>::infinity();
    duplicate.color[3] = std::numeric_limits<float>::quiet_NaN();
    duplicate.zSlice = std::numeric_limits<double>::infinity();
    const XFModel::FormulaId secondId = document.addFormula(duplicate);

    Assert::AreEqual(42ull, firstId);
    Assert::IsTrue(secondId != 0);
    Assert::IsTrue(secondId != firstId);
    Assert::AreEqual(0.0f, document.formulas()[1].color[0]);
    Assert::AreEqual(0.25f, document.formulas()[1].color[1]);
    Assert::AreEqual(1.0f, document.formulas()[1].color[2]);
    Assert::AreEqual(1.0f, document.formulas()[1].color[3]);
    Assert::AreEqual(0.0, document.formulas()[1].zSlice);

    XFModel::ColorRgba color;
    color[0] = -4.0f;
    color[1] = 0.5f;
    color[2] = 4.0f;
    color[3] = std::numeric_limits<float>::quiet_NaN();
    Assert::IsTrue(document.setFormulaColor(firstId, color));
    Assert::AreEqual(0.0f, document.formulas()[0].color[0]);
    Assert::AreEqual(0.5f, document.formulas()[0].color[1]);
    Assert::AreEqual(1.0f, document.formulas()[0].color[2]);
    Assert::AreEqual(1.0f, document.formulas()[0].color[3]);

    Assert::IsTrue(document.setFormulaZSlice(firstId, 3.0));
    Assert::IsTrue(document.setFormulaZSlice(firstId, std::numeric_limits<double>::infinity()));
    Assert::AreEqual(0.0, document.formulas()[0].zSlice);

    Assert::IsTrue(document.setViewState({
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        0.0,
        500000.0
    }));
    Assert::AreEqual(0.0, document.view().centerX);
    Assert::AreEqual(0.0, document.view().centerY);
    Assert::AreEqual(0.1, document.view().scaleX);
    Assert::AreEqual(100000.0, document.view().scaleY);

    XFModel::PlotSettings plot;
    plot.surfaceResolution = 999;
    plot.wireThickness = std::numeric_limits<float>::quiet_NaN();
    plot.showCoordinates = true;
    plot.showAxisTriad = true;
    Assert::IsTrue(document.setPlotSettings(plot));
    Assert::AreEqual(256, document.plotSettings().surfaceResolution);
    Assert::AreEqual(XFModel::kDefaultWireThickness, document.plotSettings().wireThickness);
    Assert::IsFalse(document.plotSettings().showAxisTriad);
}

TEST_CASE(Document_EditScopesNormalizeImportedStateAndTrackIdChanges) {
    XFModel::Document document;
    const XFModel::FormulaId firstId = document.addFormula(makeFormula("sin(x)"));
    const XFModel::FormulaId secondId = document.addFormula(makeFormula("cos(x)"));
    document.markSaved();

    {
        auto edit = document.editFormulas();
        edit.get()[1].id = firstId;
        edit.get()[1].color[0] = std::numeric_limits<float>::quiet_NaN();
    }

    Assert::IsTrue(document.dirty());
    Assert::IsTrue(document.formulas()[1].id != 0);
    Assert::IsTrue(document.formulas()[1].id != firstId);
    Assert::IsTrue(document.formulas()[1].id != secondId);
    Assert::AreEqual(1.0f, document.formulas()[1].color[0]);
}

} // namespace XpressFormulaTests
