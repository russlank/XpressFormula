// ProjectControls.cpp - Command-oriented project controls component.
#include "ProjectControls.h"

#include "imgui.h"

namespace XpressFormula::UI::Components {
namespace {

constexpr const char* kUnsavedProjectPopupId = "Unsaved Project Changes";

} // namespace

ProjectControlsAction renderProjectControls(const ProjectControlsContext& context) {
    ProjectControlsAction action;

    ImGui::TextUnformatted("Project");
    ImGui::Separator();
    ImGui::TextWrapped("%s", context.displayName.c_str());
    if (!context.fullPath.empty()) {
        ImGui::SetItemTooltip("%s", context.fullPath.c_str());
    }

    const float buttonWidth =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("New", ImVec2(buttonWidth, 0.0f))) {
        action.command = ProjectControlCommand::NewProject;
    }
    ImGui::SameLine();
    if (ImGui::Button("Open...", ImVec2(buttonWidth, 0.0f))) {
        action.command = ProjectControlCommand::OpenProject;
    }
    if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f))) {
        action.command = ProjectControlCommand::Save;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As...", ImVec2(buttonWidth, 0.0f))) {
        action.command = ProjectControlCommand::SaveAs;
    }

    if (!context.status.empty()) {
        ImGui::TextWrapped("%s", context.status.c_str());
    }

    const std::vector<RecentProjectItem>* recentProjects = context.recentProjects;
    if (recentProjects && !recentProjects->empty() &&
        ImGui::CollapsingHeader("Recent Projects", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < static_cast<int>(recentProjects->size()); ++i) {
            ImGui::PushID(i);
            const RecentProjectItem& item = (*recentProjects)[static_cast<std::size_t>(i)];
            if (!item.exists) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton(item.label.c_str()) && item.exists) {
                action.command = ProjectControlCommand::OpenRecent;
                action.recentPath = item.path;
            }
            if (!item.exists) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (item.exists) {
                    ImGui::SetTooltip("%s", item.fullPath.c_str());
                } else {
                    ImGui::SetTooltip("Missing: %s", item.fullPath.c_str());
                }
            }
            ImGui::PopID();
        }
    }

    return action;
}

UnsavedProjectDialogChoice renderUnsavedProjectDialog(bool openNextFrame) {
    if (openNextFrame) {
        ImGui::OpenPopup(kUnsavedProjectPopupId);
    }

    UnsavedProjectDialogChoice choice = UnsavedProjectDialogChoice::None;
    if (ImGui::BeginPopupModal(kUnsavedProjectPopupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("The current project has unsaved changes.");
        ImGui::TextWrapped("Save before continuing, discard the changes, or cancel.");
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(96.0f, 0.0f))) {
            choice = UnsavedProjectDialogChoice::Save;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(96.0f, 0.0f))) {
            choice = UnsavedProjectDialogChoice::Discard;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(96.0f, 0.0f))) {
            choice = UnsavedProjectDialogChoice::Cancel;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    return choice;
}

} // namespace XpressFormula::UI::Components
