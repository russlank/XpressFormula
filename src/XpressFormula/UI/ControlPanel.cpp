// SPDX-License-Identifier: MIT
// ControlPanel.cpp - Sidebar view/render/export controls implementation.
#include "ControlPanel.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace XpressFormula::UI {

namespace {

bool beginPropertyTable(const char* id) {
    return ImGui::BeginTable(id, 3,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings,
        ImVec2(0.0f, 0.0f));
}

void setupPropertyColumns() {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 122.0f);
    ImGui::TableSetupColumn("Slider with value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 58.0f);
}

void drawPropertyLabel(const char* label, const char* tooltip) {
    ImGui::TextUnformatted(label);
    if (tooltip && tooltip[0] != '\0') {
        ImGui::SetItemTooltip("%s", tooltip);
    }
}

bool resetPropertyButton(const char* label) {
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

bool sliderFloatProperty(const char* label,
                         float& value,
                         float minValue,
                         float maxValue,
                         float resetValue,
                         const char* sliderFormat,
                         const char* tooltip = nullptr) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    drawPropertyLabel(label, tooltip);

    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    changed = ImGui::SliderFloat("##Control", &value, minValue, maxValue, sliderFormat);

    ImGui::TableSetColumnIndex(2);
    if (resetPropertyButton(label)) {
        value = resetValue;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

bool sliderIntProperty(const char* label,
                       int& value,
                       int minValue,
                       int maxValue,
                       int resetValue,
                       const char* tooltip = nullptr) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    drawPropertyLabel(label, tooltip);

    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    changed = ImGui::SliderInt("##Control", &value, minValue, maxValue);

    ImGui::TableSetColumnIndex(2);
    if (resetPropertyButton(label)) {
        value = resetValue;
        changed = true;
    }
    ImGui::PopID();
    return changed;
}

} // namespace

ControlPanelActions ControlPanel::render(Core::ViewTransform& vt, PlotSettings& settings,
                                         bool has2DFormula,
                                         bool hasSurfaceFormula,
                                         const std::string& exportStatus) {
    ControlPanelActions actions;

    ImGui::TextUnformatted("View");
    ImGui::Separator();

    float viewScale = static_cast<float>(std::clamp(
        std::sqrt(std::max(1e-6, vt.scaleX * vt.scaleY)),
        0.1,
        100000.0));
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("Zoom / scale##all",
                           &viewScale,
                           0.1f,
                           100000.0f,
                           "%.1f px/unit",
                           ImGuiSliderFlags_Logarithmic)) {
        const double newScale = std::clamp(static_cast<double>(viewScale), 0.1, 100000.0);
        vt.scaleX = newScale;
        vt.scaleY = newScale;
    }
    ImGui::SetItemTooltip("Adjust X and Y scale together in pixels per world unit.");

    char buf[128];
    std::snprintf(buf, sizeof(buf), "X: [%.4g, %.4g]", vt.worldXMin(), vt.worldXMax());
    ImGui::TextUnformatted(buf);
    std::snprintf(buf, sizeof(buf), "Y: [%.4g, %.4g]", vt.worldYMin(), vt.worldYMax());
    ImGui::TextUnformatted(buf);
    std::snprintf(buf, sizeof(buf), "Scale: %.1f x %.1f px/unit", vt.scaleX, vt.scaleY);
    ImGui::TextUnformatted(buf);
    ImGui::TextWrapped("Mouse: drag to pan; wheel to zoom; Shift/Ctrl wheel constrains X/Y.");

    ImGui::SetNextItemOpen(m_advancedViewExpanded, ImGuiCond_Always);
    m_advancedViewExpanded = ImGui::CollapsingHeader(
        "Advanced View Controls##AdvancedViewControls", ImGuiTreeNodeFlags_SpanAvailWidth);
    if (m_advancedViewExpanded) {
        if (ImGui::Button("Zoom X+")) { vt.zoomX(1.25); }
        ImGui::SameLine();
        if (ImGui::Button("Zoom X-")) { vt.zoomX(0.8); }

        if (ImGui::Button("Zoom Y+")) { vt.zoomY(1.25); }
        ImGui::SameLine();
        if (ImGui::Button("Zoom Y-")) { vt.zoomY(0.8); }

        const float panStep = 1.0f;
        ImGui::TextUnformatted("Pan");
        if (ImGui::Button("Left"))  { vt.pan(-panStep, 0); }
        ImGui::SameLine();
        if (ImGui::Button("Right")) { vt.pan( panStep, 0); }
        ImGui::SameLine();
        if (ImGui::Button("Up"))    { vt.pan(0,  panStep); }
        ImGui::SameLine();
        if (ImGui::Button("Down"))  { vt.pan(0, -panStep); }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Performance");
    ImGui::Checkbox("Optimize Rendering", &settings.optimizeRendering);
    ImGui::TextWrapped("When enabled, the app stops redrawing while idle and temporarily lowers 3D quality while dragging/zooming to keep interaction responsive.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("2D / 3D Formula Rendering");

    int renderPreference = static_cast<int>(settings.xyRenderModePreference);
    if (ImGui::RadioButton("Auto (2D for mixed content; 3D when only 3D formulas are visible)",
                           renderPreference == static_cast<int>(XYRenderModePreference::Auto))) {
        settings.xyRenderModePreference = XYRenderModePreference::Auto;
    }
    if (ImGui::RadioButton("Force 3D Surfaces / Implicit", renderPreference == static_cast<int>(XYRenderModePreference::Force3D))) {
        settings.xyRenderModePreference = XYRenderModePreference::Force3D;
    }
    if (ImGui::RadioButton("Force 2D Heatmap / Cross-Section", renderPreference == static_cast<int>(XYRenderModePreference::Force2D))) {
        settings.xyRenderModePreference = XYRenderModePreference::Force2D;
    }

    const XYRenderMode effectiveRenderMode =
        settings.resolveXYRenderMode(has2DFormula, hasSurfaceFormula);
    ImGui::TextDisabled("Effective mode: %s",
                        (effectiveRenderMode == XYRenderMode::Surface3D)
                            ? "3D"
                            : "2D");
    if (settings.xyRenderModePreference == XYRenderModePreference::Auto) {
        if (hasSurfaceFormula && has2DFormula) {
            ImGui::TextWrapped("Auto is using 2D because both 2D and 3D formulas are visible.");
        } else if (hasSurfaceFormula) {
            ImGui::TextWrapped("Auto is using 3D because only 3D-capable formulas are visible.");
        } else {
            ImGui::TextWrapped("Auto is using 2D (no visible 3D-capable formulas).");
        }
    } else if (settings.xyRenderModePreference == XYRenderModePreference::Force3D) {
        ImGui::TextWrapped("Force 3D hides 2D-only curves/contours while 3D mode is active.");
    } else {
        ImGui::TextWrapped("Force 2D shows heatmaps/cross-sections for 3D formulas.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SetNextItemOpen(m_displaySectionExpanded, ImGuiCond_Always);
    m_displaySectionExpanded = ImGui::CollapsingHeader(
        "Display##DisplaySectionToggle", ImGuiTreeNodeFlags_SpanAvailWidth);
    if (m_displaySectionExpanded) {
        ImGui::Spacing();
        ImGui::Checkbox("Show Grid", &settings.showGrid);
        if (ImGui::Checkbox("Show Coordinates", &settings.showCoordinates)) {
            settings.applyCoordinateOverlayPolicy();
        }
        ImGui::Checkbox("Show Wires", &settings.showWires);
        if (ImGui::BeginCombo("HUD", plotHudModeLabel(settings.hudMode))) {
            const PlotHudMode modes[] = {
                PlotHudMode::Off,
                PlotHudMode::Minimal,
                PlotHudMode::Detailed,
                PlotHudMode::OnlyWhileInteracting
            };
            for (PlotHudMode mode : modes) {
                const bool selected = (settings.hudMode == mode);
                if (ImGui::Selectable(plotHudModeLabel(mode), selected)) {
                    settings.hudMode = mode;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Choose the stable corner readout shown over the plot.");

        if (effectiveRenderMode == XYRenderMode::Surface3D) {
            ImGui::Separator();
            ImGui::TextDisabled("3D Display");
            if (beginPropertyTable("Display3DProperties")) {
                setupPropertyColumns();
                ImGui::BeginDisabled(!settings.showWires);
                sliderFloatProperty("Wire Opacity",
                                    settings.wireOpacity,
                                    0.0f,
                                    1.0f,
                                    kDefaultWireOpacity,
                                    "%.2f",
                                    "Opacity for 3D mesh/wire overlays.");
                sliderFloatProperty("Wire Thickness",
                                    settings.wireThickness,
                                    0.0f,
                                    2.5f,
                                    kDefaultWireThickness,
                                    "%.2f",
                                    "Line thickness for 3D mesh/wire overlays.");
                sliderIntProperty("Wire Stride",
                                  settings.wireStride,
                                  1,
                                  8,
                                  kDefaultWireStride,
                                  "Draw every Nth wire row/column without changing surface sampling.");
                ImGui::EndDisabled();
                settings.wireOpacity = clampWireOpacity(settings.wireOpacity);
                settings.wireStride = clampWireStride(settings.wireStride);

                if (settings.showSurfaceEnvelope) {
                    sliderFloatProperty("Envelope Thickness",
                                        settings.envelopeThickness,
                                        0.5f,
                                        3.0f,
                                        kDefaultEnvelopeThickness,
                                        "%.2f",
                                        "Thickness for the 3D bounding envelope.");
                }

                if (settings.autoRotate) {
                    sliderFloatProperty("Rotation Speed",
                                        settings.autoRotateSpeedDegPerSec,
                                        2.0f,
                                        90.0f,
                                        kDefaultAutoRotateSpeedDegPerSec,
                                        "%.1f deg/s",
                                        "Automatic camera rotation speed in degrees per second.");
                }
                ImGui::EndTable();
            }
            ImGui::TextWrapped("Surface Density changes mesh sampling; Wire Stride only changes displayed wire density.");

            ImGui::Checkbox("Show Envelope Box", &settings.showSurfaceEnvelope);
            ImGui::BeginDisabled(settings.showCoordinates);
            if (ImGui::Checkbox("Show Axis Triad (X/Y/Z)", &settings.showAxisTriad)) {
                settings.applyCoordinateOverlayPolicy();
            }
            ImGui::EndDisabled();
            if (settings.showCoordinates && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Axis triad is disabled while coordinates are enabled.");
            }
            ImGui::Checkbox("Auto Rotate", &settings.autoRotate);
        } else {
            ImGui::TextDisabled("3D display overlays are available when the effective mode is 3D.");
        }
    }

    if (effectiveRenderMode == XYRenderMode::Surface3D) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("3D Camera");

        if (beginPropertyTable("Camera3DProperties")) {
            setupPropertyColumns();
            sliderFloatProperty("Azimuth",
                                settings.azimuthDeg,
                                -180.0f,
                                180.0f,
                                kDefaultAzimuthDeg,
                                "%.1f deg",
                                "Horizontal camera angle for 3D surfaces.");
            sliderFloatProperty("Elevation",
                                settings.elevationDeg,
                                -85.0f,
                                85.0f,
                                kDefaultElevationDeg,
                                "%.1f deg",
                                "Vertical camera angle for 3D surfaces.");
            sliderFloatProperty("Z Scale",
                                settings.zScale,
                                0.1f,
                                8.0f,
                                kDefaultZScale,
                                "%.2f",
                                "Vertical exaggeration applied to 3D geometry.");
            sliderIntProperty("Surface Density",
                              settings.surfaceResolution,
                              12,
                              96,
                              kDefaultSurfaceResolution,
                              "Sampling density for explicit z=f(x,y) surfaces.");
            sliderIntProperty("Implicit Resolution",
                              settings.implicitSurfaceResolution,
                              16,
                              96,
                              kDefaultImplicitSurfaceResolution,
                              "Grid resolution for implicit F(x,y,z)=0 surfaces.");
            sliderFloatProperty("Surface Opacity",
                                settings.surfaceOpacity,
                                0.25f,
                                1.0f,
                                kDefaultSurfaceOpacity,
                                "%.2f",
                                "Surface fill opacity.");
            ImGui::EndTable();
        }

        if (hasSurfaceFormula) {
            ImGui::TextWrapped("Tip: Drag in the plot to pan X/Y domain and use wheel to zoom.");
        } else {
            ImGui::TextWrapped("No 3D-capable formulas are currently visible (z=f(x,y) or F(x,y,z)=0).");
        }
    } else {
        if (beginPropertyTable("HeatmapProperties")) {
            setupPropertyColumns();
            sliderFloatProperty("Heatmap Opacity",
                                settings.heatmapOpacity,
                                0.1f,
                                1.0f,
                                kDefaultHeatmapOpacity,
                                "%.2f",
                                "Opacity for heatmap and scalar-field cross-section fills.");
            ImGui::EndTable();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextUnformatted("Export");
    if (ImGui::Button("Open Export Dialog...", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        actions.requestOpenExportDialog = true;
    }

    if (!exportStatus.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", exportStatus.c_str());
    }

    return actions;
}

} // namespace XpressFormula::UI
