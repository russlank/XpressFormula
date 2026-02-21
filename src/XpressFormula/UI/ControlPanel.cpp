// ControlPanel.cpp - Zoom / pan / reset controls implementation.
#include "ControlPanel.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>

namespace XpressFormula::UI {

void ControlPanel::render(Core::ViewTransform& vt) {
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
}

} // namespace XpressFormula::UI
