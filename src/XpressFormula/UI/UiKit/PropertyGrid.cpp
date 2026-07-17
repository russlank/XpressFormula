// PropertyGrid.cpp - Reusable property-grid implementation.
#include "PropertyGrid.h"
#include "UiScopes.h"
#include "imgui.h"

namespace XpressFormula::UI::UiKit {

PropertyGrid::PropertyGrid(const char* id, const PropertyGridOptions& options)
    : m_id(id),
      m_options(options) {
}

PropertyGrid::~PropertyGrid() {
    if (m_open) {
        ImGui::EndTable();
    }
}

bool PropertyGrid::begin() {
    const int columnCount = m_options.showResetColumn ? 3 : 2;
    m_open = ImGui::BeginTable(m_id, columnCount,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings,
        ImVec2(0.0f, 0.0f));
    if (m_open) {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, m_options.labelWidth);
        ImGui::TableSetupColumn("Slider with value", ImGuiTableColumnFlags_WidthStretch);
        if (m_options.showResetColumn) {
            ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, m_options.resetWidth);
        }
    }
    return m_open;
}

void PropertyGrid::drawLabel(const char* label, const char* tooltip) {
    ImGui::TextUnformatted(label);
    if (tooltip && tooltip[0] != '\0') {
        ImGui::SetItemTooltip("%s", tooltip);
    }
}

bool PropertyGrid::resetButton(const char* label) {
    if (!m_options.showResetColumn) {
        return false;
    }

    const char* buttonText = "Reset";
    const float buttonWidth =
        ImGui::CalcTextSize(buttonText).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float available = ImGui::GetContentRegionAvail().x;
    if (available > buttonWidth) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - buttonWidth);
    }

    const bool clicked = ImGui::SmallButton(buttonText);
    ImGui::SetItemTooltip("Reset %s to its default value.", label);
    return clicked;
}

bool PropertyGrid::sliderFloat(const char* label,
                               float& value,
                               float minimum,
                               float maximum,
                               float defaultValue,
                               const char* format,
                               const char* tooltip) {
    if (!m_open) {
        return false;
    }

    bool changed = false;
    IdScope id(label);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    drawLabel(label, tooltip);

    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    changed = ImGui::SliderFloat("##Control", &value, minimum, maximum, format);

    if (m_options.showResetColumn) {
        ImGui::TableSetColumnIndex(2);
        if (resetButton(label)) {
            value = defaultValue;
            changed = true;
        }
    }
    return changed;
}

bool PropertyGrid::sliderInt(const char* label,
                             int& value,
                             int minimum,
                             int maximum,
                             int defaultValue,
                             const char* tooltip) {
    if (!m_open) {
        return false;
    }

    bool changed = false;
    IdScope id(label);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    drawLabel(label, tooltip);

    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    changed = ImGui::SliderInt("##Control", &value, minimum, maximum);

    if (m_options.showResetColumn) {
        ImGui::TableSetColumnIndex(2);
        if (resetButton(label)) {
            value = defaultValue;
            changed = true;
        }
    }
    return changed;
}

} // namespace XpressFormula::UI::UiKit
