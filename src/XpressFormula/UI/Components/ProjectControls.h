// ProjectControls.h - Command-oriented project controls component.
#pragma once

#include <string>
#include <vector>

namespace XpressFormula::UI::Components {

enum class ProjectControlCommand {
    None,
    NewProject,
    OpenProject,
    Save,
    SaveAs,
    OpenRecent
};

enum class UnsavedProjectDialogChoice {
    None,
    Save,
    Discard,
    Cancel
};

struct RecentProjectItem {
    std::wstring path;
    std::string label;
    std::string fullPath;
    bool exists = true;
};

struct ProjectControlsContext {
    std::string displayName;
    std::string fullPath;
    std::string status;
    const std::vector<RecentProjectItem>* recentProjects = nullptr;
};

struct ProjectControlsAction {
    ProjectControlCommand command = ProjectControlCommand::None;
    std::wstring recentPath;
};

[[nodiscard]] ProjectControlsAction renderProjectControls(const ProjectControlsContext& context);
[[nodiscard]] UnsavedProjectDialogChoice renderUnsavedProjectDialog(bool openNextFrame);

} // namespace XpressFormula::UI::Components
