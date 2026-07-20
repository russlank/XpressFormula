// DocumentTests.cpp - Revision and dirty-state tests for the document aggregate.
#include "CppUnitTest.h"
#include "../XpressFormula/Model/Document.h"

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

} // namespace XpressFormulaTests
