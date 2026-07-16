// ResponsiveRows.cpp - ImGui row container implementation.
#include "ResponsiveRows.h"

#include <algorithm>
#include <cassert>

namespace XpressFormula::UI::UiKit {

ResponsiveRows::ResponsiveRows(const char* id,
                               int rowCount,
                               const ResponsiveRowsOptions& options)
    : m_id(id),
      m_rowCount(std::max(1, rowCount)),
      m_padding(options.padding),
      m_bordered(options.bordered),
      m_noScrollbar(options.noScrollbar) {
    const ImGuiStyle& style = ImGui::GetStyle();
    m_rowStride = ImGui::GetFrameHeight() + style.ItemSpacing.y;
    m_height =
        m_padding.y * 2.0f +
        ImGui::GetFrameHeight() * static_cast<float>(m_rowCount) +
        style.ItemSpacing.y * static_cast<float>(std::max(0, m_rowCount - 1)) +
        options.extraBottomPadding;
}

ResponsiveRows::~ResponsiveRows() {
    if (m_begun) {
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
}

bool ResponsiveRows::begin() {
    assert(!m_begun);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, m_padding);
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    if (m_noScrollbar) {
        flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }
    m_open = ImGui::BeginChild(m_id ? m_id : "##ResponsiveRows",
                               ImVec2(0.0f, m_height),
                               m_bordered,
                               flags);
    m_begun = true;
    return m_open;
}

void ResponsiveRows::beginRow(int rowIndex) {
    assert(m_begun);
    assert(rowIndex >= 0 && rowIndex < m_rowCount);
    const int safeRow = std::clamp(rowIndex, 0, m_rowCount - 1);
    ImGui::SetCursorPosY(m_padding.y + static_cast<float>(safeRow) * m_rowStride);
    ImGui::SetCursorPosX(m_padding.x);
}

} // namespace XpressFormula::UI::UiKit
