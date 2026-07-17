// UiScopes.h - Small RAII guards for common Dear ImGui scopes.
#pragma once

#include "imgui.h"

namespace XpressFormula::UI::UiKit {

class IdScope {
public:
    explicit IdScope(int id) { ImGui::PushID(id); }
    explicit IdScope(const char* id) { ImGui::PushID(id); }
    ~IdScope() { ImGui::PopID(); }

    IdScope(const IdScope&) = delete;
    IdScope& operator=(const IdScope&) = delete;
    IdScope(IdScope&&) = delete;
    IdScope& operator=(IdScope&&) = delete;
};

class DisabledScope {
public:
    explicit DisabledScope(bool disabled = true)
        : m_active(disabled) {
        if (m_active) {
            ImGui::BeginDisabled();
        }
    }

    ~DisabledScope() {
        if (m_active) {
            ImGui::EndDisabled();
        }
    }

    bool active() const { return m_active; }

    DisabledScope(const DisabledScope&) = delete;
    DisabledScope& operator=(const DisabledScope&) = delete;
    DisabledScope(DisabledScope&&) = delete;
    DisabledScope& operator=(DisabledScope&&) = delete;

private:
    bool m_active = false;
};

class StyleVarScope {
public:
    StyleVarScope(ImGuiStyleVar index, float value) { ImGui::PushStyleVar(index, value); }
    StyleVarScope(ImGuiStyleVar index, ImVec2 value) { ImGui::PushStyleVar(index, value); }
    ~StyleVarScope() { ImGui::PopStyleVar(); }

    StyleVarScope(const StyleVarScope&) = delete;
    StyleVarScope& operator=(const StyleVarScope&) = delete;
    StyleVarScope(StyleVarScope&&) = delete;
    StyleVarScope& operator=(StyleVarScope&&) = delete;
};

class StyleColorScope {
public:
    StyleColorScope(ImGuiCol index, ImVec4 value) { ImGui::PushStyleColor(index, value); }
    StyleColorScope(ImGuiCol index, ImU32 value) { ImGui::PushStyleColor(index, value); }
    ~StyleColorScope() { ImGui::PopStyleColor(); }

    StyleColorScope(const StyleColorScope&) = delete;
    StyleColorScope& operator=(const StyleColorScope&) = delete;
    StyleColorScope(StyleColorScope&&) = delete;
    StyleColorScope& operator=(StyleColorScope&&) = delete;
};

class TextWrapScope {
public:
    explicit TextWrapScope(float wrapPosition) { ImGui::PushTextWrapPos(wrapPosition); }
    ~TextWrapScope() { ImGui::PopTextWrapPos(); }

    TextWrapScope(const TextWrapScope&) = delete;
    TextWrapScope& operator=(const TextWrapScope&) = delete;
    TextWrapScope(TextWrapScope&&) = delete;
    TextWrapScope& operator=(TextWrapScope&&) = delete;
};

} // namespace XpressFormula::UI::UiKit
