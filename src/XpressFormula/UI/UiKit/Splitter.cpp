// Splitter.cpp - Reusable ImGui splitter mechanics implementation.
#include "Splitter.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace XpressFormula::UI::UiKit {

bool drawVerticalSplitter(const char* id,
                          float height,
                          float& firstPaneWidth,
                          float minimumFirst,
                          float maximumFirst,
                          float defaultFirst,
                          float splitterWidth) {
    assert(std::isfinite(height));
    assert(std::isfinite(firstPaneWidth));
    assert(std::isfinite(minimumFirst));
    assert(std::isfinite(maximumFirst));
    assert(std::isfinite(defaultFirst));
    assert(std::isfinite(splitterWidth));

    const float safeHeight = std::max(1.0f, height);
    const float safeWidth = std::max(1.0f, splitterWidth);
    const float lower = std::max(0.0f, std::min(minimumFirst, maximumFirst));
    const float upper = std::max(lower, maximumFirst);
    bool changed = false;

    ImGui::InvisibleButton(id ? id : "##VerticalSplitter",
                           ImVec2(safeWidth, safeHeight),
                           ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered || active) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    if (active) {
        const float nextWidth =
            std::clamp(firstPaneWidth + ImGui::GetIO().MouseDelta.x, lower, upper);
        changed = nextWidth != firstPaneWidth;
        firstPaneWidth = nextWidth;
    }

    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const float nextWidth = std::clamp(defaultFirst, lower, upper);
        changed = nextWidth != firstPaneWidth;
        firstPaneWidth = nextWidth;
    }

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2((min.x + max.x) * 0.5f, min.y + 4.0f),
        ImVec2((min.x + max.x) * 0.5f, max.y - 4.0f),
        IM_COL32(120, 120, 128, hovered ? 220 : 140),
        active ? 2.0f : 1.0f);

    return changed;
}

} // namespace XpressFormula::UI::UiKit
