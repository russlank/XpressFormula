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
    return ImGui::BeginTable(id, 4,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings,
        ImVec2(0.0f, 0.0f));
}

void setupPropertyColumns() {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 112.0f);
    ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 52.0f);
}

void drawPropertyLabel(const char* label, const char* tooltip) {
    ImGui::TextUnformatted(label);
    if (tooltip && tooltip[0] != '\0') {
        ImGui::SetItemTooltip("%s", tooltip);
    }
}

bool sliderFloatProperty(const char* label,
                         float& value,
                         float minValue,
                         float maxValue,
                         float resetValue,
                         const char* sliderFormat,
                         const char* valueFormat,
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
    ImGui::Text(valueFormat, value);

    ImGui::TableSetColumnIndex(3);
    if (ImGui::SmallButton("Reset")) {
        value = resetValue;
        changed = true;
    }
    ImGui::SetItemTooltip("Reset %s to its default value.", label);
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
    ImGui::Text("%d", value);

    ImGui::TableSetColumnIndex(3);
    if (ImGui::SmallButton("Reset")) {
        value = resetValue;
        changed = true;
    }
    ImGui::SetItemTooltip("Reset %s to its default value.", label);
    ImGui::PopID();
    return changed;
}

} // namespace

ControlPanelActions ControlPanel::render(Core::ViewTransform& vt, PlotSettings& settings,
                                         bool has2DFormula,
                                         bool hasSurfaceFormula,
                                         const std::string& exportStatus) {
    ControlPanelActions actions;

    ImGui::TextUnformatted("View Controls");
    ImGui::Separator();

    // --- Zoom all ---
    float zoomAllLog = static_cast<float>(std::log(std::max(vt.scaleX, 1e-6)) / std::log(2.0));
    if (ImGui::SliderFloat("Zoom##all", &zoomAllLog, -3.0f, 14.0f, "2^%.1f")) {
        double newScale = std::clamp(std::pow(2.0, static_cast<double>(zoomAllLog)), 0.1, 100000.0);
        vt.scaleX = newScale;
        vt.scaleY = newScale;
    }

    if (ImGui::Button("Zoom In##all")) { vt.zoomAll(1.25); }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out##all")) { vt.zoomAll(0.8); }

    ImGui::Spacing();

    // --- Zoom X only ---
    if (ImGui::Button("Zoom X+")) { vt.zoomX(1.25); }
    ImGui::SameLine();
    if (ImGui::Button("Zoom X-")) { vt.zoomX(0.8); }

    // --- Zoom Y only ---
    if (ImGui::Button("Zoom Y+")) { vt.zoomY(1.25); }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Y-")) { vt.zoomY(0.8); }

    ImGui::Spacing();

    // --- Pan ---
    float panStep = 1.0f;
    ImGui::TextUnformatted("Pan:");
    if (ImGui::Button("Left"))  { vt.pan(-panStep, 0); }
    ImGui::SameLine();
    if (ImGui::Button("Right")) { vt.pan( panStep, 0); }
    ImGui::SameLine();
    if (ImGui::Button("Up"))    { vt.pan(0,  panStep); }
    ImGui::SameLine();
    if (ImGui::Button("Down"))  { vt.pan(0, -panStep); }

    ImGui::Spacing();
    ImGui::Separator();

    // --- Reset ---
    if (ImGui::Button("Reset View", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        vt.reset();
    }

    ImGui::Spacing();

    // --- Current view range info ---
    ImGui::TextUnformatted("View Range:");
    char buf[128];
    std::snprintf(buf, sizeof(buf), "  X: [%.4g, %.4g]", vt.worldXMin(), vt.worldXMax());
    ImGui::TextUnformatted(buf);
    std::snprintf(buf, sizeof(buf), "  Y: [%.4g, %.4g]", vt.worldYMin(), vt.worldYMax());
    ImGui::TextUnformatted(buf);
    std::snprintf(buf, sizeof(buf), "  Scale: %.1f x %.1f px/unit", vt.scaleX, vt.scaleY);
    ImGui::TextUnformatted(buf);

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
        ImGui::Checkbox("Show Coordinates", &settings.showCoordinates);
        ImGui::Checkbox("Show Wires", &settings.showWires);

        if (effectiveRenderMode == XYRenderMode::Surface3D) {
            ImGui::Separator();
            ImGui::TextDisabled("3D Display");
            if (beginPropertyTable("Display3DProperties")) {
                setupPropertyColumns();
                ImGui::BeginDisabled(!settings.showWires);
                sliderFloatProperty("Wire",
                                    settings.wireThickness,
                                    0.0f,
                                    2.5f,
                                    2.0f,
                                    "%.2f",
                                    "%.2f",
                                    "Line thickness for 3D mesh/wire overlays.");
                ImGui::EndDisabled();

                if (settings.showSurfaceEnvelope) {
                    sliderFloatProperty("Envelope",
                                        settings.envelopeThickness,
                                        0.5f,
                                        3.0f,
                                        2.0f,
                                        "%.2f",
                                        "%.2f",
                                        "Thickness for the 3D bounding envelope.");
                }

                if (settings.autoRotate) {
                    sliderFloatProperty("Rotate",
                                        settings.autoRotateSpeedDegPerSec,
                                        2.0f,
                                        90.0f,
                                        20.0f,
                                        "%.1f deg/s",
                                        "%.1f",
                                        "Automatic camera rotation speed in degrees per second.");
                }
                ImGui::EndTable();
            }

            ImGui::Checkbox("Show Envelope Box", &settings.showSurfaceEnvelope);
            ImGui::Checkbox("Show Axis Triad (X/Y/Z)", &settings.showAxisTriad);
            if (settings.showCoordinates && settings.showAxisTriad) {
                ImGui::TextDisabled("Axis triad is hidden while coordinates are enabled.");
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
                                30.0f,
                                "%.1f deg",
                                "%.1f",
                                "Horizontal camera angle for 3D surfaces.");
            sliderFloatProperty("Elevation",
                                settings.elevationDeg,
                                -85.0f,
                                85.0f,
                                -60.0f,
                                "%.1f deg",
                                "%.1f",
                                "Vertical camera angle for 3D surfaces.");
            sliderFloatProperty("Z scale",
                                settings.zScale,
                                0.1f,
                                8.0f,
                                1.5f,
                                "%.2f",
                                "%.2f",
                                "Vertical exaggeration applied to 3D geometry.");
            sliderIntProperty("Surface density",
                              settings.surfaceResolution,
                              12,
                              96,
                              50,
                              "Sampling density for explicit z=f(x,y) surfaces.");
            sliderIntProperty("Implicit res.",
                              settings.implicitSurfaceResolution,
                              16,
                              96,
                              64,
                              "Grid resolution for implicit F(x,y,z)=0 surfaces.");
            sliderFloatProperty("Opacity",
                                settings.surfaceOpacity,
                                0.25f,
                                1.0f,
                                0.80f,
                                "%.2f",
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
            sliderFloatProperty("Heatmap",
                                settings.heatmapOpacity,
                                0.1f,
                                1.0f,
                                0.62f,
                                "%.2f",
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
