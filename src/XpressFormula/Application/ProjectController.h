// ProjectController.h - Non-UI project workflow controller.
#pragma once

#include "../Infrastructure/Persistence/ProjectRepository.h"
#include "../Infrastructure/Persistence/RecentProjectsStore.h"
#include "../Model/Document.h"
#include "../Platform/Windows/FileDialogService.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace XpressFormula::Application {

class IProjectFileDialog {
public:
    virtual ~IProjectFileDialog() = default;

    [[nodiscard]] virtual Platform::Windows::DialogResult openProject() = 0;
    [[nodiscard]] virtual Platform::Windows::DialogResult saveProject(
        std::wstring_view currentPath) = 0;
};

enum class ProjectAction {
    None,
    NewProject,
    OpenDialog,
    OpenRecent,
    CloseApp
};

enum class UnsavedProjectChoice {
    None,
    Save,
    Discard,
    Cancel
};

class ProjectController {
public:
    using DefaultDocumentFactory = std::function<Model::Document()>;

    ProjectController(Infrastructure::Persistence::ProjectRepository& repository,
                      Infrastructure::Persistence::RecentProjectsStore& recentProjects,
                      IProjectFileDialog& fileDialog,
                      DefaultDocumentFactory defaultDocumentFactory);

    [[nodiscard]] const std::wstring& currentPath() const noexcept;
    [[nodiscard]] std::string currentPathUtf8() const;
    [[nodiscard]] const std::string& status() const noexcept;
    [[nodiscard]] const std::vector<std::wstring>& recentProjectPaths() const noexcept;
    [[nodiscard]] std::string displayName(const Model::Document& document) const;

    void loadRecentProjectPaths();
    void saveRecentProjectPaths() const;

    void startNewCleanDocument(Model::Document& document);
    void requestNew(Model::Document& document);
    void requestOpenDialog(Model::Document& document);
    void requestOpenRecent(Model::Document& document, std::wstring path);
    void requestClose(Model::Document& document);

    [[nodiscard]] bool save(Model::Document& document);
    [[nodiscard]] bool saveAs(Model::Document& document);
    [[nodiscard]] bool saveToPath(Model::Document& document,
                                  const std::wstring& path,
                                  std::string& error);
    [[nodiscard]] bool openFromDialog(Model::Document& document);
    [[nodiscard]] bool openFromPath(Model::Document& document,
                                    const std::wstring& path,
                                    std::string& error);

    void handleUnsavedChoice(Model::Document& document, UnsavedProjectChoice choice);
    [[nodiscard]] bool consumeUnsavedPromptRequest() noexcept;
    [[nodiscard]] bool consumeCloseRequest() noexcept;
    [[nodiscard]] bool consumeDocumentReplaced() noexcept;

private:
    void requestDestructiveAction(Model::Document& document,
                                  ProjectAction action,
                                  std::wstring path = {});
    void executeAction(Model::Document& document,
                       ProjectAction action,
                       const std::wstring& path);
    void addRecentProjectPath(const std::wstring& path);
    void clearPendingAction();

    Infrastructure::Persistence::ProjectRepository& m_repository;
    Infrastructure::Persistence::RecentProjectsStore& m_recentProjects;
    IProjectFileDialog& m_fileDialog;
    DefaultDocumentFactory m_defaultDocumentFactory;

    std::wstring m_currentPath;
    std::string m_status;
    std::vector<std::wstring> m_recentProjectPaths;
    ProjectAction m_pendingAction = ProjectAction::None;
    std::wstring m_pendingPath;
    bool m_openUnsavedPromptNextFrame = false;
    bool m_closeRequested = false;
    bool m_documentReplaced = false;
};

} // namespace XpressFormula::Application
