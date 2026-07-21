// ProjectPersistenceSubsystemTests.cpp - Integration tests for project persistence boundaries.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/InputLimits.h"
#include "../XpressFormula/Infrastructure/Persistence/ProjectMapper.h"
#include "../XpressFormula/Infrastructure/Persistence/ProjectRepository.h"
#include "../XpressFormula/Infrastructure/Persistence/ProjectSerializer.h"
#include "../XpressFormula/Infrastructure/Persistence/RecentProjectsStore.h"
#include "../XpressFormula/Platform/Windows/Utf.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFPersist = XpressFormula::Infrastructure::Persistence;
namespace XFInputLimits = XpressFormula::Core::InputLimits;
namespace XFWUtf = XpressFormula::Platform::Windows;

namespace XpressFormulaTests {
namespace {

std::filesystem::path uniquePersistenceTestDirectory() {
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        (L"XpressFormulaPersistenceTests_" + std::to_wstring(static_cast<long long>(ticks)));
    std::filesystem::create_directories(path);
    return path;
}

void writeTextFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::string text;
    in.seekg(0, std::ios::end);
    text.resize(static_cast<std::size_t>(in.tellg()));
    in.seekg(0, std::ios::beg);
    in.read(text.data(), static_cast<std::streamsize>(text.size()));
    return text;
}

XFPersist::ProjectSession makeRepositorySession(const std::string& expression) {
    XFPersist::ProjectSession session;
    XFPersist::ProjectFormulaRecord formula;
    formula.expression = expression;
    formula.color = { 0.2f, 0.4f, 0.6f, 1.0f };
    formula.visible = false;
    formula.zSlice = 1.5f;
    session.formulas.push_back(formula);
    session.view.centerX = 12.5;
    session.view.scaleX = 80.0;
    session.plot.surfaceResolution = 64;
    return session;
}

} // namespace

TEST_CASE(ProjectRepository_SaveLoadAndAtomicOverwrite) {
    const std::filesystem::path dir = uniquePersistenceTestDirectory();
    const std::filesystem::path projectPath = dir / L"project.xfplot";
    XFPersist::ProjectRepository repository;

    writeTextFile(projectPath, "old");
    const XFPersist::ProjectSession saved = makeRepositorySession("sin(x)");

    const XFPersist::ProjectSaveResult saveResult = repository.save(projectPath, saved);
    Assert::IsTrue(saveResult.success);
    Assert::IsFalse(std::filesystem::exists(projectPath.wstring() + L".tmp"));

    const XFPersist::ProjectLoadResult loaded = repository.load(projectPath);
    Assert::IsTrue(loaded.success);
    Assert::AreEqual(1, static_cast<int>(loaded.session.formulas.size()));
    Assert::AreEqual(std::string("sin(x)"), loaded.session.formulas[0].expression);
    Assert::AreEqual(12.5, loaded.session.view.centerX);
    Assert::AreEqual(64, loaded.session.plot.surfaceResolution);

    const XFPersist::ProjectSession overwritten = makeRepositorySession("cos(x)");
    Assert::IsTrue(repository.save(projectPath, overwritten).success);
    Assert::IsTrue(readTextFile(projectPath).find("cos(x)") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectRepository_ReportsMalformedAndUnsupportedFiles) {
    const std::filesystem::path dir = uniquePersistenceTestDirectory();
    const std::filesystem::path malformed = dir / L"bad.xfplot";
    const std::filesystem::path unsupported = dir / L"future.xfplot";
    XFPersist::ProjectRepository repository;

    writeTextFile(malformed, R"({"schemaVersion":1,"fileType":"XpressFormulaProject","formulas":[)");
    writeTextFile(unsupported,
                  R"({"schemaVersion":99,"fileType":"XpressFormulaProject","formulas":[]})");

    const XFPersist::ProjectLoadResult malformedResult = repository.load(malformed);
    Assert::IsFalse(malformedResult.success);
    Assert::IsFalse(malformedResult.error.empty());

    const XFPersist::ProjectLoadResult unsupportedResult = repository.load(unsupported);
    Assert::IsFalse(unsupportedResult.success);
    Assert::IsTrue(unsupportedResult.error.find("Unsupported") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectRepository_RejectsOversizedFileBeforeParsing) {
    const std::filesystem::path dir = uniquePersistenceTestDirectory();
    const std::filesystem::path projectPath = dir / L"huge.xfplot";
    {
        std::ofstream out(projectPath, std::ios::binary | std::ios::trunc);
        out.seekp(static_cast<std::streamoff>(XFInputLimits::kMaxProjectFileBytes));
        out.put('\0');
    }

    XFPersist::ProjectRepository repository;
    const XFPersist::ProjectLoadResult result = repository.load(projectPath);

    Assert::IsFalse(result.success);
    Assert::IsTrue(result.error.find("larger than the supported limit") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(ProjectMapper_CompilesLoadedFormulasAndClampsSettings) {
    XFPersist::ProjectSession session;
    session.formulas.push_back(XFPersist::ProjectFormulaRecord{ "sin(x)" });
    session.formulas.push_back(XFPersist::ProjectFormulaRecord{ "x = y = 1" });
    session.view.scaleX = 0.0;
    session.view.scaleY = 500000.0;
    session.plot.surfaceResolution = 999;
    session.plot.showCoordinates = true;
    session.plot.showAxisTriad = true;

    const XFPersist::ProjectMapResult mapped =
        XFPersist::mapProjectSessionToDocument(session);

    Assert::AreEqual(2, static_cast<int>(mapped.document.formulas.size()));
    Assert::IsTrue(mapped.document.formulas[0].isValid());
    Assert::IsFalse(mapped.document.formulas[1].isValid());
    Assert::IsFalse(mapped.warnings.empty());
    Assert::AreEqual(0.1, mapped.document.view.state.scaleX);
    Assert::AreEqual(100000.0, mapped.document.view.state.scaleY);
    Assert::AreEqual(256, mapped.document.plot.surfaceResolution);
    Assert::IsFalse(mapped.document.plot.showAxisTriad);
}

TEST_CASE(RecentProjectsStore_DeduplicatesUnicodeAndPrunesMissingEntries) {
    const std::filesystem::path dir = uniquePersistenceTestDirectory();
    const std::filesystem::path storePath = dir / L"recent-projects.txt";
    const std::filesystem::path existing = dir / L"unicode_\x03C0.xfplot";
    const std::filesystem::path missing = dir / L"missing.xfplot";
    writeTextFile(existing, "{}");

    const std::string existingUtf8 = XFWUtf::utf16ToUtf8OrEmpty(existing.wstring());
    const std::string missingUtf8 = XFWUtf::utf16ToUtf8OrEmpty(missing.wstring());
    writeTextFile(storePath, existingUtf8 + "\n" + existingUtf8 + "\n" + missingUtf8 + "\n");

    XFPersist::RecentProjectsStore store(storePath, 8);
    const XFPersist::RecentProjectsLoadResult loaded = store.load();

    Assert::IsTrue(loaded.changed);
    Assert::AreEqual(1, static_cast<int>(loaded.paths.size()));
    Assert::AreEqual(existing.wstring(), loaded.paths[0]);
    Assert::IsTrue(readTextFile(storePath).find(missingUtf8) == std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(RecentProjectsStore_AddsMostRecentFirstAndBoundsCount) {
    const std::filesystem::path dir = uniquePersistenceTestDirectory();
    const std::filesystem::path storePath = dir / L"recent-projects.txt";
    const std::filesystem::path first = dir / L"first.xfplot";
    const std::filesystem::path second = dir / L"second.xfplot";
    const std::filesystem::path third = dir / L"third.xfplot";
    writeTextFile(first, "{}");
    writeTextFile(second, "{}");
    writeTextFile(third, "{}");

    XFPersist::RecentProjectsStore store(storePath, 2);
    std::vector<std::wstring> paths;
    store.add(paths, first.wstring());
    store.add(paths, second.wstring());
    store.add(paths, first.wstring());
    store.add(paths, third.wstring());

    Assert::AreEqual(2, static_cast<int>(paths.size()));
    Assert::AreEqual(third.wstring(), paths[0]);
    Assert::AreEqual(first.wstring(), paths[1]);

    const XFPersist::RecentProjectsLoadResult loaded = store.load();
    Assert::AreEqual(2, static_cast<int>(loaded.paths.size()));
    Assert::AreEqual(third.wstring(), loaded.paths[0]);
    Assert::AreEqual(first.wstring(), loaded.paths[1]);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

} // namespace XpressFormulaTests
