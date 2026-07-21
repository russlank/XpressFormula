// FormulaEditorStateTests.cpp - Tests for dynamic formula editor state.
#include "CppUnitTest.h"
#include "../XpressFormula/UI/FormulaEditorState.h"
#include "../XpressFormula/UI/FormulaListActions.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFModel = XpressFormula::Model;
namespace XFUI = XpressFormula::UI;

namespace XpressFormulaTests {

static XFModel::Formula makeFormula(const char* expression) {
    XFModel::Formula formula;
    formula.setExpression(expression ? expression : "");
    formula.compile();
    return formula;
}

TEST_CASE(FormulaEditorState_OpenTracksFormulaIdAndText) {
    XFModel::Formula formula = makeFormula("sin(x)");
    XFUI::FormulaEditorState editor;

    editor.open(formula);

    Assert::IsTrue(editor.targetId.has_value());
    Assert::AreEqual(formula.id, *editor.targetId);
    Assert::AreEqual(std::string("sin(x)"), editor.text);
    Assert::IsFalse(editor.previewAvailable);
}

TEST_CASE(FormulaEditorState_OpenAddDoesNotTargetExistingFormula) {
    XFModel::Formula draft = makeFormula("");
    const XFModel::FormulaId draftId = draft.id;
    XFUI::FormulaEditorState editor;

    editor.openAdd(draft);

    Assert::IsTrue(editor.active());
    Assert::IsTrue(editor.adding());
    Assert::IsFalse(editor.targetId.has_value());
    Assert::AreEqual(draftId, editor.draft.id);
    Assert::AreEqual(std::string(""), editor.text);
}

TEST_CASE(FormulaEditorState_LongTextAppliesWithoutTruncation) {
    XFModel::Formula formula = makeFormula("sin(x)");
    XFUI::FormulaEditorState editor;
    std::string longExpression = "x";
    longExpression.append(3000, ' ');

    editor.open(formula);
    editor.loadText(longExpression);
    editor.refreshPreview();
    editor.applyTo(formula);

    Assert::AreEqual(longExpression, formula.expression);
    Assert::IsTrue(formula.isValid());
    Assert::AreEqual(longExpression, formula.lastCompiledExpression);
}

TEST_CASE(FormulaEditorState_AddApplyBuildsOneValidDraftFormula) {
    XFModel::Formula draft;
    draft.color[0] = 0.25f;
    XFUI::FormulaEditorState editor;

    editor.openAdd(draft);
    editor.loadText("sin(x)");
    editor.refreshPreview();
    XFModel::Formula applied = editor.buildAppliedFormula();

    Assert::AreEqual(draft.id, applied.id);
    Assert::AreEqual(std::string("sin(x)"), applied.expression);
    Assert::AreEqual(0.25f, applied.color[0]);
    Assert::IsTrue(applied.isValid());
}

TEST_CASE(FormulaEditorState_WrongArityPreviewIsInvalidWithoutThrowing) {
    XFUI::FormulaEditorState editor;
    editor.loadText("sin()");

    bool threw = false;
    try {
        editor.refreshPreview();
    } catch (...) {
        threw = true;
    }

    Assert::IsFalse(threw);
    Assert::IsTrue(editor.previewAvailable);
    Assert::IsFalse(editor.preview.isValid());
    Assert::IsTrue(editor.preview.diagnosticMessage().find("expects") != std::string::npos);
}

TEST_CASE(FormulaEditorState_ZeroScientificPreviewAcceptedAndUnderflowRejected) {
    XFUI::FormulaEditorState editor;

    editor.loadText("0e999");
    Assert::IsTrue(editor.refreshPreview());
    Assert::IsTrue(editor.previewAvailable);
    Assert::IsTrue(editor.preview.isValid());

    editor.loadText("1e-9999");
    bool threw = false;
    try {
        Assert::IsTrue(editor.refreshPreview());
    } catch (...) {
        threw = true;
    }

    Assert::IsFalse(threw);
    Assert::IsTrue(editor.previewAvailable);
    Assert::IsFalse(editor.preview.isValid());
    Assert::IsTrue(
        editor.preview.diagnosticMessage().find("outside the supported range") != std::string::npos);
}

TEST_CASE(FormulaEditorState_PreviewRecompilesOnlyWhenTextChanges) {
    XFUI::FormulaEditorState editor;
    editor.loadText("sin(x)");

    const bool firstRefresh = editor.refreshPreview();
    const std::uint64_t firstRevision = editor.preview.compilationRevision;
    const auto firstAst = editor.preview.compiled.ast;
    const bool secondRefresh = editor.refreshPreview();

    Assert::IsTrue(firstRefresh);
    Assert::IsFalse(secondRefresh);
    Assert::AreEqual(firstRevision, editor.preview.compilationRevision);
    Assert::IsTrue(firstAst == editor.preview.compiled.ast);

    editor.loadText("x^2 + y^2");
    const bool thirdRefresh = editor.refreshPreview();
    Assert::IsTrue(thirdRefresh);
    Assert::IsTrue(editor.preview.isValid());
    Assert::IsTrue(firstAst != editor.preview.compiled.ast);
}

TEST_CASE(FormulaEditorState_ApplyTargetsFormulaFoundById) {
    std::vector<XFModel::Formula> formulas = {
        makeFormula("sin(x)"),
        makeFormula("cos(x)")
    };
    XFUI::FormulaEditorState editor;
    editor.open(formulas[1]);
    editor.loadText("tan(x)");

    const std::optional<std::size_t> target =
        XFUI::FormulaListActions::findFormulaIndex(formulas, *editor.targetId);
    Assert::IsTrue(target.has_value());
    editor.applyTo(formulas[*target]);

    Assert::AreEqual(std::string("sin(x)"), formulas[0].expression);
    Assert::AreEqual(std::string("tan(x)"), formulas[1].expression);
    Assert::IsTrue(formulas[1].isValid());
}

TEST_CASE(FormulaEditorState_CloseDoesNotMutateFormula) {
    XFModel::Formula formula = makeFormula("sin(x)");
    XFUI::FormulaEditorState editor;

    editor.open(formula);
    editor.loadText("cos(x)");
    editor.close();

    Assert::IsFalse(editor.targetId.has_value());
    Assert::AreEqual(std::string("sin(x)"), formula.expression);
}

TEST_CASE(FormulaEditorState_MultipleAddCancelCyclesLeaveNoTarget) {
    XFUI::FormulaEditorState editor;

    for (int i = 0; i < 3; ++i) {
        XFModel::Formula draft;
        editor.openAdd(draft);
        editor.loadText("sin(x)");
        editor.close();
        Assert::IsFalse(editor.active());
        Assert::IsFalse(editor.targetId.has_value());
        Assert::AreEqual(std::string(""), editor.text);
    }
}

} // namespace XpressFormulaTests
