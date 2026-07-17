// Splitter.h - Reusable ImGui splitter mechanics.
#pragma once

#include "imgui.h"

namespace XpressFormula::UI::UiKit {

bool drawVerticalSplitter(const char* id,
                          float height,
                          float& firstPaneWidth,
                          float minimumFirst,
                          float maximumFirst,
                          float defaultFirst,
                          float splitterWidth);

} // namespace XpressFormula::UI::UiKit
