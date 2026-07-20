// ProjectControls.cpp - Command-oriented project controls component.
#include "ProjectControls.h"
#include "../../Platform/Windows/Utf.h"

#include "imgui.h"

#include <filesystem>
#include <system_error>

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

    const std::vector<std::wstring>* recentPaths = context.recentProjectPaths;
    if (recentPaths && !recentPaths->empty() &&
        ImGui::CollapsingHeader("Recent Projects", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < static_cast<int>(recentPaths->size()); ++i) {
            ImGui::PushID(i);
            const std::wstring& path = (*recentPaths)[static_cast<std::size_t>(i)];
            std::string label = Platform::Windows::utf16ToUtf8OrEmpty(
                std::filesystem::path(path).filename().wstring());
            if (label.empty()) {
                label = Platform::Windows::utf16ToUtf8OrEmpty(path);
            }
            std::error_code pathError;
            const bool pathExists = std::filesystem::exists(std::filesystem::path(path), pathError);
            if (!pathExists) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton(label.c_str()) && pathExists) {
                action.command = ProjectControlCommand::OpenRecent;
                action.recentPath = path;
            }
            if (!pathExists) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                const std::string pathText = Platform::Windows::utf16ToUtf8OrEmpty(path);
                if (pathExists) {
                    ImGui::SetTooltip("%s", pathText.c_str());
                } else {
                    ImGui::SetTooltip("Missing: %s", pathText.c_str());
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
