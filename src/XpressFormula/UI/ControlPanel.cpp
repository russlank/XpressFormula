// SPDX-License-Identifier: MIT
// ControlPanel.cpp - Sidebar view/render/export controls implementation.
#include "ControlPanel.h"
#include "UiKit/PropertyGrid.h"
#include "UiKit/UiScopes.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace XpressFormula::UI {

ControlPanelActions ControlPanel::render(Core::ViewTransform& vt, PlotSettings& settings,
                                         const Model::SceneSummary& scene,
                                         const std::string& exportStatus) {
    ControlPanelActions actions;
    const PlotLimits& limits = plotLimits();

    ImGui::TextUnformatted("View");
    ImGui::Separator();

    float viewScale = static_cast<float>(std::clamp(
        std::sqrt(std::max(1e-6, vt.state.scaleX * vt.state.scaleY)),
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
        vt.state.scaleX = newScale;
        vt.state.scaleY = newScale;
    }
    ImGui::SetItemTooltip("Adjust X and Y scale together in pixels per world unit.");

    char buf[128];
    std::snprintf(buf, sizeof(buf), "X: [%.4g, %.4g]", vt.worldXMin(), vt.worldXMax());
    ImGui::TextUnformatted(buf);
    std::snprintf(buf, sizeof(buf), "Y: [%.4g, %.4g]", vt.worldYMin(), vt.worldYMax());
    ImGui::TextUnformatted(buf);
    std::snprintf(buf, sizeof(buf), "Scale: %.1f x %.1f px/unit", vt.state.scaleX, vt.state.scaleY);
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

    const XYRenderMode effectiveRenderMode = settings.resolveXYRenderMode(scene);
    ImGui::TextDisabled("Effective mode: %s",
                        (effectiveRenderMode == XYRenderMode::Surface3D)
                            ? "3D"
                            : "2D");
    if (settings.xyRenderModePreference == XYRenderModePreference::Auto) {
        if (scene.hasVisible3D() && scene.hasVisible2D()) {
            ImGui::TextWrapped("Auto is using 2D because both 2D and 3D formulas are visible.");
        } else if (scene.hasVisible3D()) {
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
            {
                UiKit::PropertyGrid displayGrid("Display3DProperties");
                if (displayGrid.begin()) {
                    {
                        UiKit::DisabledScope disabled(!settings.showWires);
                        displayGrid.sliderFloat("Wire Opacity",
                                                settings.wireOpacity,
                                                limits.wireOpacity.min,
                                                limits.wireOpacity.max,
                                                kDefaultWireOpacity,
                                                "%.2f",
                                                "Opacity for 3D mesh/wire overlays.");
                        displayGrid.sliderFloat("Wire Thickness",
                                                settings.wireThickness,
                                                limits.wireThickness.min,
                                                limits.wireThickness.max,
                                                kDefaultWireThickness,
                                                "%.2f",
                                                "Line thickness for 3D mesh/wire overlays.");
                        displayGrid.sliderInt("Wire Stride",
                                              settings.wireStride,
                                              limits.wireStride.min,
                                              limits.wireStride.max,
                                              kDefaultWireStride,
                                              "Draw every Nth wire row/column without changing surface sampling.");
                    }
                    settings.wireOpacity = clampWireOpacity(settings.wireOpacity);
                    settings.wireStride = clampWireStride(settings.wireStride);

                    if (settings.showSurfaceEnvelope) {
                        displayGrid.sliderFloat("Envelope Thickness",
                                                settings.envelopeThickness,
                                                limits.envelopeThickness.min,
                                                limits.envelopeThickness.max,
                                                kDefaultEnvelopeThickness,
                                                "%.2f",
                                                "Thickness for the 3D bounding envelope.");
                    }

                    if (settings.autoRotate) {
                        displayGrid.sliderFloat("Rotation Speed",
                                                settings.autoRotateSpeedDegPerSec,
                                                limits.autoRotateSpeedDegPerSec.min,
                                                limits.autoRotateSpeedDegPerSec.max,
                                                kDefaultAutoRotateSpeedDegPerSec,
                                                "%.1f deg/s",
                                                "Automatic camera rotation speed in degrees per second.");
                    }
                }
            }
            ImGui::TextWrapped("Surface Density changes mesh sampling; Wire Stride only changes displayed wire density.");

            ImGui::Checkbox("Show Envelope Box", &settings.showSurfaceEnvelope);
            {
                UiKit::DisabledScope disabled(settings.showCoordinates);
                if (ImGui::Checkbox("Show Axis Triad (X/Y/Z)", &settings.showAxisTriad)) {
                    settings.applyCoordinateOverlayPolicy();
                }
            }
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

        {
            UiKit::PropertyGrid cameraGrid("Camera3DProperties");
            if (cameraGrid.begin()) {
                cameraGrid.sliderFloat("Azimuth",
                                       settings.azimuthDeg,
                                       limits.azimuthDeg.min,
                                       limits.azimuthDeg.max,
                                       kDefaultAzimuthDeg,
                                       "%.1f deg",
                                       "Horizontal camera angle for 3D surfaces.");
                cameraGrid.sliderFloat("Elevation",
                                       settings.elevationDeg,
                                       limits.elevationDeg.min,
                                       limits.elevationDeg.max,
                                       kDefaultElevationDeg,
                                       "%.1f deg",
                                       "Vertical camera angle for 3D surfaces.");
                cameraGrid.sliderFloat("Z Scale",
                                       settings.zScale,
                                       limits.zScale.min,
                                       limits.zScale.max,
                                       kDefaultZScale,
                                       "%.2f",
                                       "Vertical exaggeration applied to 3D geometry.");
                cameraGrid.sliderInt("Surface Density",
                                     settings.surfaceResolution,
                                     limits.surfaceResolution.min,
                                     limits.surfaceResolution.max,
                                     kDefaultSurfaceResolution,
                                     "Sampling density for explicit z=f(x,y) surfaces.");
                cameraGrid.sliderInt("Implicit Resolution",
                                     settings.implicitSurfaceResolution,
                                     limits.implicitSurfaceResolution.min,
                                     limits.implicitSurfaceResolution.max,
                                     kDefaultImplicitSurfaceResolution,
                                     "Grid resolution for implicit F(x,y,z)=0 surfaces.");
                cameraGrid.sliderFloat("Surface Opacity",
                                       settings.surfaceOpacity,
                                       limits.surfaceOpacity.min,
                                       limits.surfaceOpacity.max,
                                       kDefaultSurfaceOpacity,
                                       "%.2f",
                                       "Surface fill opacity.");
            }
        }

        if (scene.hasVisible3D()) {
            ImGui::TextWrapped("Tip: Drag in the plot to pan X/Y domain and use wheel to zoom.");
        } else {
            ImGui::TextWrapped("No 3D-capable formulas are currently visible (z=f(x,y) or F(x,y,z)=0).");
        }
    } else {
        {
            UiKit::PropertyGrid heatmapGrid("HeatmapProperties");
            if (heatmapGrid.begin()) {
                heatmapGrid.sliderFloat("Heatmap Opacity",
                                        settings.heatmapOpacity,
                                        limits.heatmapOpacity.min,
                                        limits.heatmapOpacity.max,
                                        kDefaultHeatmapOpacity,
                                        "%.2f",
                                        "Opacity for heatmap and scalar-field cross-section fills.");
            }
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

    normalizePlotSettings(settings);
    return actions;
}

} // namespace XpressFormula::UI
