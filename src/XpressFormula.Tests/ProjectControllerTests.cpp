// ProjectControllerTests.cpp - Project workflow tests without ImGui.
#include "CppUnitTest.h"
#include "../XpressFormula/Application/ProjectController.h"
#include "../XpressFormula/Infrastructure/Persistence/ProjectMapper.h"
#include "../XpressFormula/Infrastructure/Persistence/ProjectRepository.h"
#include "../XpressFormula/Infrastructure/Persistence/RecentProjectsStore.h"
#include "../XpressFormula/Model/Document.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFApp = XpressFormula::Application;
namespace XFPersist = XpressFormula::Infrastructure::Persistence;
namespace XFModel = XpressFormula::Model;
namespace XFWin = XpressFormula::Platform::Windows;

namespace XpressFormulaTests {
namespace {

std::filesystem::path uniqueProjectControllerTestDirectory() {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        (L"XpressFormulaProjectControllerTests_" + std::to_wstring(static_cast<long long>(ticks)));
    std::filesystem::create_directories(path);
    return path;
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

XFModel::Formula makeFormula(const char* expression) {
    XFModel::Formula formula;
    formula.setExpression(expression ? expression : "");
    formula.compile();
    return formula;
}

XFModel::Document makeDocument(const char* expression = "sin(x)") {
    XFModel::Document document;
    document.addFormula(makeFormula(expression));
    document.markSaved();
    return document;
}

XFModel::Document makeDefaultDocument() {
    return makeDocument("sin(sqrt(x^2+y^2))");
}

class FakeProjectFileDialog final : public XFApp::IProjectFileDialog {
public:
    XFWin::DialogResult openResult;
    XFWin::DialogResult saveResult;
    int openCalls = 0;
    int saveCalls = 0;

    [[nodiscard]] XFWin::DialogResult openProject() override {
        ++openCalls;
        return openResult;
    }

    [[nodiscard]] XFWin::DialogResult saveProject(std::wstring_view) override {
        ++saveCalls;
        return saveResult;
    }
};

struct ControllerHarness {
    explicit ControllerHarness(const std::filesystem::path& dir)
        : recentStore(dir / L"recent-projects.txt", 8),
          controller(repository, recentStore, dialog, [] { return makeDefaultDocument(); }) {
    }

    FakeProjectFileDialog dialog;
    XFPersist::ProjectRepository repository;
    XFPersist::RecentProjectsStore recentStore;
    XFApp::ProjectController controller;
};

} // namespace

TEST_CASE(ProjectController_SaveAsCanceledKeepsDocumentDirty) {
    const std::filesystem::path dir = uniqueProjectControllerTestDirectory();
    ControllerHarness harness(dir);
    XFModel::Document document = makeDocument();
    document.addFormula(makeFormula("cos(x)"));

    harness.dialog.saveResult.status = XFWin::DialogStatus::Cancelled;

    Assert::IsFalse(harness.controller.saveAs(document));
    Assert::IsTrue(document.dirty());
    Assert::AreEqual(1, harness.dialog.saveCalls);
    Assert::AreEqual(std::string("Save project canceled."), harness.controller.status());

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectController_SaveSuccessMarksDocumentCleanAndAddsRecent) {
    const std::filesystem::path dir = uniqueProjectControllerTestDirectory();
    const std::filesystem::path projectPath = dir / L"saved.xfplot";
    ControllerHarness harness(dir);
    XFModel::Document document = makeDocument();
    document.addFormula(makeFormula("cos(x)"));
    harness.dialog.saveResult.status = XFWin::DialogStatus::Selected;
    harness.dialog.saveResult.path = projectPath.wstring();

    Assert::IsTrue(harness.controller.saveAs(document));

    Assert::IsFalse(document.dirty());
    Assert::AreEqual(projectPath.wstring(), harness.controller.currentPath());
    Assert::AreEqual(1, static_cast<int>(harness.controller.recentProjectPaths().size()));
    Assert::IsTrue(std::filesystem::exists(projectPath));

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectController_FailedSaveDoesNotMarkDocumentClean) {
    const std::filesystem::path dir = uniqueProjectControllerTestDirectory();
    ControllerHarness harness(dir);
    XFModel::Document document = makeDocument();
    document.addFormula(makeFormula("cos(x)"));

    std::string error;
    Assert::IsFalse(harness.controller.saveToPath(document, dir.wstring(), error));
    Assert::IsTrue(document.dirty());
    Assert::IsFalse(error.empty());

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectController_OpenReplacesDocumentOnlyAfterValidParse) {
    const std::filesystem::path dir = uniqueProjectControllerTestDirectory();
    const std::filesystem::path validPath = dir / L"valid.xfplot";
    const std::filesystem::path invalidPath = dir / L"invalid.xfplot";
    ControllerHarness harness(dir);
    XFModel::Document document = makeDocument("sin(x)");

    writeTextFile(invalidPath, R"({"schemaVersion":1,"fileType":"XpressFormulaProject","formulas":[)");
    std::string error;
    Assert::IsFalse(harness.controller.openFromPath(document, invalidPath.wstring(), error));
    Assert::AreEqual(std::string("sin(x)"), document.formulas()[0].expression);
    Assert::IsFalse(harness.controller.consumeDocumentReplaced());

    XFModel::Document saved = makeDocument("cos(x)");
    Assert::IsTrue(harness.repository.save(
        validPath,
        XFPersist::makeProjectSession(saved.formulas(), saved.viewTransform(), saved.plotSettings())).success);

    Assert::IsTrue(harness.controller.openFromPath(document, validPath.wstring(), error));
    Assert::AreEqual(std::string("cos(x)"), document.formulas()[0].expression);
    Assert::IsFalse(document.dirty());
    Assert::IsTrue(harness.controller.consumeDocumentReplaced());

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectController_NewOpenCloseUnsavedChoices) {
    const std::filesystem::path dir = uniqueProjectControllerTestDirectory();
    const std::filesystem::path savePath = dir / L"before-new.xfplot";
    ControllerHarness harness(dir);
    XFModel::Document document = makeDocument("sin(x)");
    document.addFormula(makeFormula("cos(x)"));

    harness.controller.requestNew(document);
    Assert::IsTrue(harness.controller.consumeUnsavedPromptRequest());
    harness.controller.handleUnsavedChoice(document, XFApp::UnsavedProjectChoice::Cancel);
    Assert::AreEqual(2, static_cast<int>(document.formulas().size()));
    Assert::IsFalse(harness.controller.consumeDocumentReplaced());

    harness.controller.requestNew(document);
    Assert::IsTrue(harness.controller.consumeUnsavedPromptRequest());
    harness.dialog.saveResult.status = XFWin::DialogStatus::Selected;
    harness.dialog.saveResult.path = savePath.wstring();
    harness.controller.handleUnsavedChoice(document, XFApp::UnsavedProjectChoice::Save);
    Assert::IsTrue(std::filesystem::exists(savePath));
    Assert::AreEqual(std::string("sin(sqrt(x^2+y^2))"), document.formulas()[0].expression);
    Assert::IsFalse(document.dirty());
    Assert::IsTrue(harness.controller.consumeDocumentReplaced());

    document.addFormula(makeFormula("tan(x)"));
    harness.controller.requestClose(document);
    Assert::IsTrue(harness.controller.consumeUnsavedPromptRequest());
    harness.controller.handleUnsavedChoice(document, XFApp::UnsavedProjectChoice::Discard);
    Assert::IsTrue(harness.controller.consumeCloseRequest());

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectController_MissingRecentProjectIsRemovedAfterOpenFailure) {
    const std::filesystem::path dir = uniqueProjectControllerTestDirectory();
    const std::filesystem::path projectPath = dir / L"recent.xfplot";
    ControllerHarness harness(dir);
    XFModel::Document document = makeDocument();
    std::string error;
    Assert::IsTrue(harness.controller.saveToPath(document, projectPath.wstring(), error));
    Assert::AreEqual(1, static_cast<int>(harness.controller.recentProjectPaths().size()));

    std::error_code ignored;
    std::filesystem::remove(projectPath, ignored);
    harness.controller.requestOpenRecent(document, projectPath.wstring());

    Assert::AreEqual(0, static_cast<int>(harness.controller.recentProjectPaths().size()));
    Assert::IsTrue(harness.controller.status().find("Open recent project failed") != std::string::npos);

    std::filesystem::remove_all(dir, ignored);
}

} // namespace XpressFormulaTests
