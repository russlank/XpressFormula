// PlotPanel.cpp - Interactive plot panel implementation.
#include "PlotPanel.h"
#include "../Plotting/PlotRenderer.h"
#include "imgui.h"

namespace XpressFormula::UI {

void PlotPanel::render(std::vector<FormulaEntry>& formulas,
                       Core::ViewTransform& vt,
                       PlotSettings& settings) {
    // Update the viewport transform from the ImGui window
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 1.0f) size.x = 1.0f;
    if (size.y < 1.0f) size.y = 1.0f;

    vt.screenOriginX = pos.x;
    vt.screenOriginY = pos.y;
    vt.screenWidth   = size.x;
    vt.screenHeight  = size.y;

    // Reserve the plot area as an invisible button so we capture mouse events
    ImGui::InvisibleButton("##plot_area", size,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonRight);
    bool isHovered = ImGui::IsItemHovered();
    bool isActive  = ImGui::IsItemActive();

    // Draw background
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      IM_COL32(25, 25, 30, 255));

    // Grid, axes, labels
    Plotting::PlotRenderer::drawGrid(dl, vt);
    Plotting::PlotRenderer::drawAxes(dl, vt);
    Plotting::PlotRenderer::drawAxisLabels(dl, vt);

    bool hasSurface = false;
    for (const auto& formula : formulas) {
        if (formula.visible && formula.isValid() &&
            formula.renderKind == FormulaRenderKind::Surface3D) {
            hasSurface = true;
            break;
        }
    }

    if (hasSurface && settings.autoRotate) {
        settings.azimuthDeg += ImGui::GetIO().DeltaTime * settings.autoRotateSpeedDegPerSec;
        if (settings.azimuthDeg > 180.0f) {
            settings.azimuthDeg -= 360.0f;
        }
    }

    // Draw each formula
    for (auto& f : formulas) {
        if (!f.visible || !f.isValid()) continue;
        switch (f.renderKind) {
            case FormulaRenderKind::Curve2D:
                Plotting::PlotRenderer::drawCurve2D(dl, vt, f.ast, f.color);
                break;
            case FormulaRenderKind::Surface3D:
                if (settings.xyRenderMode == XYRenderMode::Surface3D) {
                    Plotting::PlotRenderer::Surface3DOptions options;
                    options.azimuthDeg = settings.azimuthDeg;
                    options.elevationDeg = settings.elevationDeg;
                    options.zScale = settings.zScale;
                    options.resolution = settings.surfaceResolution;
                    options.opacity = settings.surfaceOpacity;
                    options.wireThickness = settings.wireThickness;
                    options.showEnvelope = settings.showSurfaceEnvelope;
                    options.envelopeThickness = settings.envelopeThickness;
                    options.showDimensionArrows = settings.showDimensionArrows;
                    Plotting::PlotRenderer::drawSurface3D(dl, vt, f.ast, f.color, options);
                } else {
                    Plotting::PlotRenderer::drawHeatmap(
                        dl, vt, f.ast, f.color, settings.heatmapOpacity);
                }
                break;
            case FormulaRenderKind::Implicit2D:
                Plotting::PlotRenderer::drawImplicitContour2D(dl, vt, f.ast, f.color, 2.0f);
                break;
            case FormulaRenderKind::ScalarField3D:
                Plotting::PlotRenderer::drawCrossSection(
                    dl, vt, f.ast, f.zSlice, f.color, settings.heatmapOpacity);
                break;
            default:
                break;
        }
    }

    // Border
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                IM_COL32(100, 100, 100, 255));

    // --- Mouse interaction ---
    if (isHovered) {
        // Pan with left drag
        if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            vt.panPixels(delta.x, delta.y);
        }

        // Zoom with scroll wheel
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            double factor = (wheel > 0) ? 1.15 : (1.0 / 1.15);

            // Zoom toward cursor position
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            double wxBefore, wyBefore;
            vt.screenToWorld(mousePos.x, mousePos.y, wxBefore, wyBefore);

            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyShift) {
                vt.zoomX(factor);      // Shift+wheel = zoom X only
            } else if (io.KeyCtrl) {
                vt.zoomY(factor);      // Ctrl+wheel  = zoom Y only
            } else {
                vt.zoomAll(factor);    // plain wheel  = zoom both
            }

            // Adjust center so cursor stays over the same world point
            double wxAfter, wyAfter;
            vt.screenToWorld(mousePos.x, mousePos.y, wxAfter, wyAfter);
            vt.centerX += (wxBefore - wxAfter);
            vt.centerY += (wyBefore - wyAfter);
        }
    }

    // Tooltip showing world coordinates under cursor
    if (isHovered) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        double wx, wy;
        vt.screenToWorld(mousePos.x, mousePos.y, wx, wy);
        if (hasSurface && settings.xyRenderMode == XYRenderMode::Surface3D) {
            ImGui::SetTooltip("x = %.4g\ny = %.4g\n3D camera: az %.1f, el %.1f",
                              wx, wy, settings.azimuthDeg, settings.elevationDeg);
        } else {
            ImGui::SetTooltip("x = %.4g\ny = %.4g", wx, wy);
        }
    }
}

} // namespace XpressFormula::UI
