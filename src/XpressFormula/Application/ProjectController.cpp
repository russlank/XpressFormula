// ProjectController.cpp - Non-UI project workflow controller.
#include "ProjectController.h"
#include "../Infrastructure/Persistence/ProjectMapper.h"
#include "../Platform/Windows/Utf.h"

#include <filesystem>
#include <sstream>
#include <system_error>
#include <utility>

namespace XpressFormula::Application {

namespace XFPersistence = Infrastructure::Persistence;
namespace XFWin = Platform::Windows;
namespace XFWUtf = Platform::Windows;

ProjectController::ProjectController(XFPersistence::ProjectRepository& repository,
                                     XFPersistence::RecentProjectsStore& recentProjects,
                                     IProjectFileDialog& fileDialog,
                                     DefaultDocumentFactory defaultDocumentFactory)
    : m_repository(repository),
      m_recentProjects(recentProjects),
      m_fileDialog(fileDialog),
      m_defaultDocumentFactory(std::move(defaultDocumentFactory)) {
}

const std::wstring& ProjectController::currentPath() const noexcept {
    return m_currentPath;
}

std::string ProjectController::currentPathUtf8() const {
    return XFWUtf::utf16ToUtf8OrEmpty(m_currentPath);
}

const std::string& ProjectController::status() const noexcept {
    return m_status;
}

const std::vector<std::wstring>& ProjectController::recentProjectPaths() const noexcept {
    return m_recentProjectPaths;
}

std::string ProjectController::displayName(const Model::Document& document) const {
    std::string name = m_currentPath.empty()
        ? std::string("Untitled.xfplot")
        : XFWUtf::utf16ToUtf8OrEmpty(std::filesystem::path(m_currentPath).filename().wstring());
    if (name.empty()) {
        name = "Untitled.xfplot";
    }
    if (document.dirty()) {
        name += " *";
    }
    return name;
}

void ProjectController::loadRecentProjectPaths() {
    m_recentProjectPaths = m_recentProjects.load().paths;
}

void ProjectController::saveRecentProjectPaths() const {
    m_recentProjects.save(m_recentProjectPaths);
}

void ProjectController::startNewCleanDocument(Model::Document& document) {
    Model::Document next = m_defaultDocumentFactory ? m_defaultDocumentFactory() : Model::Document{};
    next.markSaved();
    document = std::move(next);
    m_currentPath.clear();
    m_documentReplaced = true;
}

void ProjectController::requestNew(Model::Document& document) {
    requestDestructiveAction(document, ProjectAction::NewProject);
}

void ProjectController::requestOpenDialog(Model::Document& document) {
    requestDestructiveAction(document, ProjectAction::OpenDialog);
}

void ProjectController::requestOpenRecent(Model::Document& document, std::wstring path) {
    requestDestructiveAction(document, ProjectAction::OpenRecent, std::move(path));
}

void ProjectController::requestClose(Model::Document& document) {
    requestDestructiveAction(document, ProjectAction::CloseApp);
}

bool ProjectController::saveToPath(Model::Document& document,
                                   const std::wstring& path,
                                   std::string& error) {
    error.clear();
    const std::filesystem::path projectPath(path);
    const XFPersistence::ProjectSession session = XFPersistence::makeProjectSession(
        document.formulas(),
        document.viewTransform(),
        document.plotSettings());
    const XFPersistence::ProjectSaveResult saveResult =
        m_repository.save(projectPath, session);
    if (!saveResult) {
        error = "Could not save project file: " + saveResult.error;
        return false;
    }

    m_currentPath = projectPath.wstring();
    addRecentProjectPath(m_currentPath);
    document.markSaved();
    m_status = "Saved project: " + XFWUtf::utf16ToUtf8OrEmpty(m_currentPath);
    return true;
}

bool ProjectController::saveAs(Model::Document& document) {
    const XFWin::DialogResult dialog = m_fileDialog.saveProject(m_currentPath);
    if (dialog.cancelled()) {
        m_status = "Save project canceled.";
        return false;
    }
    if (!dialog.selected()) {
        m_status = "Save project failed: " + dialog.error;
        return false;
    }

    std::string error;
    if (!saveToPath(document, dialog.path, error)) {
        m_status = "Save project failed: " + error;
        return false;
    }
    return true;
}

bool ProjectController::save(Model::Document& document) {
    if (m_currentPath.empty()) {
        return saveAs(document);
    }

    std::string error;
    if (!saveToPath(document, m_currentPath, error)) {
        m_status = "Save project failed: " + error;
        return false;
    }
    return true;
}

bool ProjectController::openFromPath(Model::Document& document,
                                     const std::wstring& path,
                                     std::string& error) {
    error.clear();
    const XFPersistence::ProjectLoadResult loaded =
        m_repository.load(std::filesystem::path(path));
    if (!loaded) {
        error = loaded.error;
        return false;
    }

    XFPersistence::ProjectMapResult mapped =
        XFPersistence::mapProjectSessionToDocument(loaded.session);
    std::vector<std::string> warnings = loaded.warnings;
    warnings.insert(warnings.end(), mapped.warnings.begin(), mapped.warnings.end());

    document.replaceState(
        std::move(mapped.document.formulas),
        mapped.document.view,
        mapped.document.plot,
        true);
    m_currentPath = std::filesystem::absolute(std::filesystem::path(path)).wstring();
    addRecentProjectPath(m_currentPath);
    m_documentReplaced = true;

    std::ostringstream status;
    status << "Opened project: " << XFWUtf::utf16ToUtf8OrEmpty(m_currentPath);
    if (!warnings.empty()) {
        status << " (" << warnings.size() << " warning";
        if (warnings.size() != 1) {
            status << "s";
        }
        status << ": " << warnings.front() << ")";
    }
    m_status = status.str();
    return true;
}

bool ProjectController::openFromDialog(Model::Document& document) {
    const XFWin::DialogResult dialog = m_fileDialog.openProject();
    if (dialog.cancelled()) {
        m_status = "Open project canceled.";
        return false;
    }
    if (!dialog.selected()) {
        m_status = "Open project failed: " + dialog.error;
        return false;
    }

    std::string error;
    if (!openFromPath(document, dialog.path, error)) {
        m_status = "Open project failed: " + error;
        return false;
    }
    return true;
}

void ProjectController::handleUnsavedChoice(Model::Document& document,
                                            UnsavedProjectChoice choice) {
    if (choice == UnsavedProjectChoice::None) {
        return;
    }
    if (choice == UnsavedProjectChoice::Cancel) {
        clearPendingAction();
        return;
    }

    const ProjectAction action = m_pendingAction;
    const std::wstring path = m_pendingPath;
    if (action == ProjectAction::None) {
        return;
    }

    if (choice == UnsavedProjectChoice::Save && !save(document)) {
        m_openUnsavedPromptNextFrame = true;
        return;
    }

    clearPendingAction();
    executeAction(document, action, path);
}

bool ProjectController::consumeUnsavedPromptRequest() noexcept {
    const bool requested = m_openUnsavedPromptNextFrame;
    m_openUnsavedPromptNextFrame = false;
    return requested;
}

bool ProjectController::consumeCloseRequest() noexcept {
    const bool requested = m_closeRequested;
    m_closeRequested = false;
    return requested;
}

bool ProjectController::consumeDocumentReplaced() noexcept {
    const bool replaced = m_documentReplaced;
    m_documentReplaced = false;
    return replaced;
}

void ProjectController::requestDestructiveAction(Model::Document& document,
                                                 ProjectAction action,
                                                 std::wstring path) {
    if (document.dirty()) {
        m_pendingAction = action;
        m_pendingPath = std::move(path);
        m_openUnsavedPromptNextFrame = true;
        return;
    }

    executeAction(document, action, path);
}

void ProjectController::executeAction(Model::Document& document,
                                      ProjectAction action,
                                      const std::wstring& path) {
    switch (action) {
        case ProjectAction::NewProject:
            startNewCleanDocument(document);
            m_status = "Started a new project.";
            break;
        case ProjectAction::OpenDialog:
            (void)openFromDialog(document);
            break;
        case ProjectAction::OpenRecent: {
            std::string error;
            if (!openFromPath(document, path, error)) {
                m_status = "Open recent project failed: " + error;
                std::error_code pathError;
                if (!std::filesystem::exists(std::filesystem::path(path), pathError)) {
                    m_recentProjects.remove(m_recentProjectPaths, path);
                }
            }
            break;
        }
        case ProjectAction::CloseApp:
            m_closeRequested = true;
            break;
        case ProjectAction::None:
        default:
            break;
    }
}

void ProjectController::addRecentProjectPath(const std::wstring& path) {
    m_recentProjects.add(m_recentProjectPaths, path);
}

void ProjectController::clearPendingAction() {
    m_pendingAction = ProjectAction::None;
    m_pendingPath.clear();
}

} // namespace XpressFormula::Application
