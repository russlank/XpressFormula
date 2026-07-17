// UiMetrics.h - Shared UI dimensions and responsive breakpoints.
#pragma once

namespace XpressFormula::UI::UiKit {

struct UiMetrics {
    // Spacing values are in ImGui screen pixels.
    float compactSpacing = 4.0f;
    float normalSpacing = 8.0f;
    float sectionSpacing = 10.0f;

    // Padding values are in ImGui screen pixels.
    float panelPaddingX = 8.0f;
    float panelPaddingY = 6.0f;
    float cardPaddingX = 8.0f;
    float cardPaddingY = 6.0f;

    // Main window split dimensions, in pixels.
    float minimumSidebarWidth = 280.0f;
    float defaultSidebarWidth = 360.0f;
    float maximumSidebarWidth = 600.0f;
    float minimumPlotWidth = 360.0f;
    float splitterWidth = 8.0f;

    // Plot toolbar responsive thresholds, in pixels of available toolbar width/height.
    float toolbarCompactWidth3D = 700.0f;
    float toolbarCompactWidth2D = 600.0f;
    float toolbarExtraCompactWidth = 520.0f;
    float toolbarOneRowWidth3D = 1220.0f;
    float toolbarOneRowWidth2D = 860.0f;
    float toolbarShortHeight = 260.0f;

    // Formula-card responsive thresholds, in pixels of available card width.
    float formulaCardCompactWidth = 420.0f;
    float formulaCardExtraCompactWidth = 320.0f;

    // Tooltip wrap width expressed in multiples of the current font size.
    float tooltipWrapEm = 42.0f;
};

const UiMetrics& metrics();

} // namespace XpressFormula::UI::UiKit
