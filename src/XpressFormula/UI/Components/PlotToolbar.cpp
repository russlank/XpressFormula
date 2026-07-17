// PlotToolbar.cpp - Plot toolbar component implementation.
#include "PlotToolbar.h"
#include "../UiKit/ResponsiveRows.h"
#include "../UiKit/UiScopes.h"

#include <cmath>

namespace XpressFormula::UI::Components {

namespace {

struct CameraPreset {
    const char* label;
    float azimuthDeg;
    float elevationDeg;
    const char* tooltip;
};

const CameraPreset kCameraPresets[] = {
    { "Front", 0.0f, -85.0f, "View from the front, showing X and Z." },
    { "Back", 180.0f, -85.0f, "View from the back, showing X and Z reversed." },
    { "Left", -90.0f, -85.0f, "View from the left, showing Y and Z." },
    { "Right", 90.0f, -85.0f, "View from the right, showing Y and Z." },
    { "Top", 0.0f, 0.0f, "Top-down X/Y view." },
    { "Bottom", 180.0f, 0.0f, "Bottom-oriented X/Y view." },
    { "Isometric", kDefaultAzimuthDeg, kDefaultElevationDeg,
      "Restore the default isometric camera." },
};

const char* renderModePreferenceLabel(XYRenderModePreference preference) {
    switch (preference) {
        case XYRenderModePreference::Force3D: return "Force 3D";
        case XYRenderModePreference::Force2D: return "Force 2D";
        case XYRenderModePreference::Auto:
        default:
            return "Auto";
    }
}

const char* effectiveRenderModeLabel(XYRenderMode mode) {
    return mode == XYRenderMode::Surface3D ? "3D" : "2D";
}

bool isMatchingPreset(const PlotSettings& settings, const CameraPreset& preset) {
    return std::abs(settings.azimuthDeg - preset.azimuthDeg) < 0.01f &&
           std::abs(settings.elevationDeg - preset.elevationDeg) < 0.01f;
}

void nextGroup() {
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
}

} // namespace

PlotToolbarActions renderPlotToolbar(PlotSettings& settings,
                                     const PlotToolbarContext& context) {
    PlotToolbarActions actions;
    const UiKit::PlotToolbarLayoutPlan plan = UiKit::planPlotToolbar({
        context.availableSize.x,
        context.availableSize.y,
        context.is3DMode
    });

    UiKit::ResponsiveRows rows("##PlotToolbar", plan.rowCount);
    if (!rows.begin()) {
        return actions;
    }

    auto requestCameraPreset = [&](const CameraPreset& preset) {
        actions.applyCameraPreset = true;
        actions.cameraAzimuthDeg = preset.azimuthDeg;
        actions.cameraElevationDeg = preset.elevationDeg;
        actions.requestRedraw = true;
    };

    auto drawActions = [&]() {
        if (ImGui::Button("Fit##PlotToolbar")) {
            actions.requestFit = true;
        }
        ImGui::SetItemTooltip("Fit the plot to the standard [-10, 10] math domain. Shortcut: F.");
        ImGui::SameLine();

        if (ImGui::Button("Reset##PlotToolbar")) {
            actions.requestReset = true;
        }
        ImGui::SetItemTooltip("Reset view scale, center, and 3D camera. Shortcut: Home.");
        ImGui::SameLine();

        if (ImGui::Button("Export##PlotToolbar")) {
            actions.requestExport = true;
        }
        ImGui::SetItemTooltip("Open the export dialog. Shortcut: E.");
    };

    auto drawModeControls = [&](bool showEffectiveMode) {
        ImGui::TextUnformatted("Mode");
        ImGui::SameLine();
        const bool compactModeWidth =
            plan.layout == UiKit::PlotToolbarLayout::Compact ||
            plan.layout == UiKit::PlotToolbarLayout::ExtraCompact;
        ImGui::SetNextItemWidth(compactModeWidth ? 132.0f : 104.0f);
        if (ImGui::BeginCombo("##PlotRenderModePreference",
                              renderModePreferenceLabel(settings.xyRenderModePreference))) {
            const XYRenderModePreference preferences[] = {
                XYRenderModePreference::Auto,
                XYRenderModePreference::Force3D,
                XYRenderModePreference::Force2D
            };
            for (XYRenderModePreference preference : preferences) {
                const bool selected = (settings.xyRenderModePreference == preference);
                if (ImGui::Selectable(renderModePreferenceLabel(preference), selected)) {
                    settings.xyRenderModePreference = preference;
                    actions.requestRedraw = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Select automatic mode, force 3D surfaces, or force 2D heatmaps/cross-sections.");

        if (showEffectiveMode) {
            ImGui::SameLine();
            ImGui::TextDisabled("Effective %s", effectiveRenderModeLabel(context.effectiveRenderMode));
        }
    };

    auto drawDisplayToggles = [&]() {
        if (ImGui::Checkbox("Grid##PlotToolbar", &settings.showGrid)) {
            actions.requestRedraw = true;
        }
        ImGui::SetItemTooltip("Show or hide the grid. Shortcut: G.");
        ImGui::SameLine();
        if (ImGui::Checkbox("Wires##PlotToolbar", &settings.showWires)) {
            actions.requestRedraw = true;
        }
        ImGui::SetItemTooltip("Show or hide 3D wire overlays. Shortcut: W.");
        ImGui::SameLine();
        if (ImGui::Checkbox("Coordinates##PlotToolbar", &settings.showCoordinates)) {
            settings.applyCoordinateOverlayPolicy();
            actions.requestRedraw = true;
        }
        ImGui::SetItemTooltip("Show or hide coordinate axes and labels.");
    };

    auto drawCameraPresetMenuItems = [&]() {
        ImGui::TextDisabled("Camera");
        for (const CameraPreset& preset : kCameraPresets) {
            const bool selected = isMatchingPreset(settings, preset);
            if (ImGui::Selectable(preset.label, selected)) {
                requestCameraPreset(preset);
            }
            ImGui::SetItemTooltip("%s", preset.tooltip);
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
    };

    auto drawMoreMenu = [&](bool includeCameraPresets) {
        if (ImGui::Button("More...##PlotToolbar")) {
            ImGui::OpenPopup("##PlotToolbarMore");
        }
        ImGui::SetItemTooltip("Show display toggles and additional 3D overlay controls.");
        if (ImGui::BeginPopup("##PlotToolbarMore")) {
            if (ImGui::Checkbox("Grid", &settings.showGrid)) {
                actions.requestRedraw = true;
            }
            ImGui::SetItemTooltip("Show or hide the grid. Shortcut: G.");
            if (ImGui::Checkbox("Wires", &settings.showWires)) {
                actions.requestRedraw = true;
            }
            ImGui::SetItemTooltip("Show or hide 3D wire overlays. Shortcut: W.");
            if (ImGui::Checkbox("Coordinates", &settings.showCoordinates)) {
                settings.applyCoordinateOverlayPolicy();
                actions.requestRedraw = true;
            }
            ImGui::SetItemTooltip("Show or hide coordinate axes and labels.");
            if (context.is3DMode) {
                ImGui::Separator();
                {
                    UiKit::DisabledScope disabled(settings.showCoordinates);
                    if (ImGui::Checkbox("Axis Triad", &settings.showAxisTriad)) {
                        settings.applyCoordinateOverlayPolicy();
                        actions.requestRedraw = true;
                    }
                }
                if (settings.showCoordinates &&
                    ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Axis triad is disabled while coordinates are enabled.");
                }
                if (includeCameraPresets) {
                    ImGui::Separator();
                    drawCameraPresetMenuItems();
                }
            }
            ImGui::EndPopup();
        }
    };

    auto matchingCameraLabel = [&]() {
        for (const CameraPreset& preset : kCameraPresets) {
            if (isMatchingPreset(settings, preset)) {
                return preset.label;
            }
        }
        return "Custom";
    };

    auto drawCameraButtons = [&]() {
        ImGui::TextUnformatted("Camera");
        ImGui::SameLine();
        const size_t presetCount = sizeof(kCameraPresets) / sizeof(kCameraPresets[0]);
        for (size_t i = 0; i < presetCount; ++i) {
            const CameraPreset& preset = kCameraPresets[i];
            if (ImGui::Button(preset.label)) {
                requestCameraPreset(preset);
            }
            ImGui::SetItemTooltip("%s", preset.tooltip);
            if (i + 1 < presetCount) {
                ImGui::SameLine();
            }
        }
    };

    auto drawCameraCombo = [&]() {
        ImGui::TextUnformatted("Camera");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(154.0f);
        if (ImGui::BeginCombo("##PlotToolbarCameraPreset", matchingCameraLabel())) {
            for (const CameraPreset& preset : kCameraPresets) {
                const bool selected = isMatchingPreset(settings, preset);
                if (ImGui::Selectable(preset.label, selected)) {
                    requestCameraPreset(preset);
                }
                ImGui::SetItemTooltip("%s", preset.tooltip);
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Apply a deterministic 3D camera preset.");
    };

    switch (plan.layout) {
        case UiKit::PlotToolbarLayout::OneRow:
            rows.beginRow(0);
            drawActions();
            nextGroup();
            drawModeControls(plan.showEffectiveMode);
            nextGroup();
            drawDisplayToggles();
            if (plan.showCameraButtons) {
                nextGroup();
                drawCameraButtons();
            }
            break;
        case UiKit::PlotToolbarLayout::TwoRow:
            rows.beginRow(0);
            drawActions();
            nextGroup();
            drawModeControls(plan.showEffectiveMode);
            if (context.is3DMode) {
                nextGroup();
                drawDisplayToggles();
                rows.beginRow(1);
                drawCameraButtons();
            } else {
                rows.beginRow(1);
                drawDisplayToggles();
            }
            break;
        case UiKit::PlotToolbarLayout::Compact:
            rows.beginRow(0);
            drawActions();
            ImGui::SameLine();
            drawMoreMenu(false);
            rows.beginRow(1);
            drawModeControls(plan.showEffectiveMode);
            if (plan.showCameraCombo) {
                rows.beginRow(2);
                drawCameraCombo();
            }
            break;
        case UiKit::PlotToolbarLayout::ExtraCompact:
            rows.beginRow(0);
            drawActions();
            ImGui::SameLine();
            drawMoreMenu(plan.putCameraInMoreMenu);
            rows.beginRow(1);
            drawModeControls(plan.showEffectiveMode);
            break;
    }

    return actions;
}

} // namespace XpressFormula::UI::Components
