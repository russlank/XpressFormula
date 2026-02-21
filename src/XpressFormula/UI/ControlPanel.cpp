// ControlPanel.cpp - Zoom / pan / reset controls implementation.
#include "ControlPanel.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>

namespace XpressFormula::UI {

void ControlPanel::render(Core::ViewTransform& vt, PlotSettings& settings,
                          bool hasSurfaceFormula) {
    ImGui::TextUnformatted("View Controls");
    ImGui::Separator();

    // --- Zoom all ---
    float zoomAllLog = static_cast<float>(std::log(vt.scaleX) / std::log(2.0));
    if (ImGui::SliderFloat("Zoom##all", &zoomAllLog, -3.0f, 14.0f, "2^%.1f")) {
        double newScale = std::pow(2.0, static_cast<double>(zoomAllLog));
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
    ImGui::TextUnformatted("X/Y Formula Rendering");

    int renderMode = (settings.xyRenderMode == XYRenderMode::Surface3D) ? 0 : 1;
    if (ImGui::RadioButton("3D Surface z=f(x,y)", renderMode == 0)) {
        settings.xyRenderMode = XYRenderMode::Surface3D;
    }
    if (ImGui::RadioButton("2D Heatmap", renderMode == 1)) {
        settings.xyRenderMode = XYRenderMode::Heatmap2D;
    }

    if (settings.xyRenderMode == XYRenderMode::Surface3D) {
        ImGui::Spacing();
        ImGui::TextUnformatted("3D Camera");

        ImGui::SliderFloat("Azimuth", &settings.azimuthDeg, -180.0f, 180.0f, "%.1f deg");
        ImGui::SliderFloat("Elevation", &settings.elevationDeg, -85.0f, 85.0f, "%.1f deg");
        ImGui::SliderFloat("Z Scale", &settings.zScale, 0.1f, 8.0f, "%.2f");
        ImGui::SliderInt("Surface Density", &settings.surfaceResolution, 12, 80);
        ImGui::SliderFloat("Surface Opacity", &settings.surfaceOpacity, 0.25f, 1.0f, "%.2f");
        ImGui::SliderFloat("Wire Thickness", &settings.wireThickness, 0.25f, 2.5f, "%.2f");
        ImGui::Checkbox("Show Envelope Box", &settings.showSurfaceEnvelope);
        if (settings.showSurfaceEnvelope) {
            ImGui::SliderFloat("Envelope Thickness", &settings.envelopeThickness,
                               0.5f, 3.0f, "%.2f");
        }

        ImGui::Checkbox("Auto Rotate", &settings.autoRotate);
        if (settings.autoRotate) {
            ImGui::SliderFloat("Rotate Speed", &settings.autoRotateSpeedDegPerSec,
                               2.0f, 90.0f, "%.1f deg/s");
        }

        if (hasSurfaceFormula) {
            ImGui::TextWrapped("Tip: Drag in the plot to pan X/Y domain and use wheel to zoom.");
        } else {
            ImGui::TextWrapped("No z=f(x,y) formulas are currently visible.");
        }
    } else {
        ImGui::SliderFloat("Heatmap Opacity", &settings.heatmapOpacity, 0.1f, 1.0f, "%.2f");
    }
}

} // namespace XpressFormula::UI
