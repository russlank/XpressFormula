// PlotPanel.cpp - Interactive plot panel implementation.
#include "PlotPanel.h"
#include "../Plotting/PlotRenderer.h"
#include "FormulaPresentation.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace XpressFormula::UI {

namespace {

void drawCornerHud(ImDrawList* dl,
                   const ImVec2& plotPos,
                   const ImVec2& plotSize,
                   const std::vector<std::string>& lines,
                   float alpha) {
    if (!dl || lines.empty() || alpha <= 0.0f || plotSize.x <= 32.0f || plotSize.y <= 32.0f) {
        return;
    }

    const float margin = 10.0f;
    const float paddingX = 8.0f;
    const float paddingY = 6.0f;
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    float contentWidth = 0.0f;
    for (const std::string& line : lines) {
        contentWidth = (std::max)(contentWidth, ImGui::CalcTextSize(line.c_str()).x);
    }

    const float maxWidth = (std::max)(140.0f, (std::min)(340.0f, plotSize.x - margin * 2.0f));
    const float width = (std::min)(contentWidth + paddingX * 2.0f, maxWidth);
    const float height = paddingY * 2.0f + lineHeight * static_cast<float>(lines.size());
    if (height >= plotSize.y - margin * 2.0f) {
        return;
    }

    const ImVec2 min(plotPos.x + plotSize.x - width - margin,
                     plotPos.y + margin);
    const ImVec2 max(min.x + width, min.y + height);
    const ImU32 bg = IM_COL32(18, 20, 24, static_cast<int>(std::clamp(alpha * 178.0f, 0.0f, 210.0f)));
    const ImU32 border = IM_COL32(160, 166, 180, static_cast<int>(std::clamp(alpha * 80.0f, 0.0f, 120.0f)));
    const ImU32 text = IM_COL32(236, 238, 244, static_cast<int>(std::clamp(alpha * 235.0f, 0.0f, 255.0f)));

    dl->AddRectFilled(min, max, bg, 6.0f);
    dl->AddRect(min, max, border, 6.0f);
    float y = min.y + paddingY;
    for (const std::string& line : lines) {
        dl->AddText(ImVec2(min.x + paddingX, y), text, line.c_str());
        y += lineHeight;
    }
}

} // namespace

void PlotPanel::render(std::vector<Model::Formula>& formulas,
                       Core::ViewTransform& vt,
                       PlotSettings& settings,
                       const Model::SceneSummary& scene,
                       const PlotRenderOverrides* overrides,
                       const PlotQualityDecision* qualityDecision) {
    normalizePlotSettings(settings);

    // Update the viewport transform from the ImGui window
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 1.0f) size.x = 1.0f;
    if (size.y < 1.0f) size.y = 1.0f;

    vt.viewport.originX = pos.x;
    vt.viewport.originY = pos.y;
    vt.viewport.width   = size.x;
    vt.viewport.height  = size.y;

    // Reserve the plot area as an invisible button so we capture mouse events
    ImGui::InvisibleButton("##plot_area", size,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonRight);
    bool isHovered = ImGui::IsItemHovered();
    bool isActive  = ImGui::IsItemActive();

    const XYRenderMode requestedRenderMode = settings.resolveXYRenderMode(scene);
    const bool requested3DMode = (requestedRenderMode == XYRenderMode::Surface3D);

    // Apply auto-rotation BEFORE any 3D drawing so the grid, axes, and surfaces
    // all use the same azimuth for this frame (avoids a 1-frame visual tear).
    if (scene.hasVisible3D() && requested3DMode && settings.autoRotate) {
        settings.azimuthDeg += ImGui::GetIO().DeltaTime * settings.autoRotateSpeedDegPerSec;
        if (settings.azimuthDeg > 180.0f) {
            settings.azimuthDeg -= 360.0f;
        }
    }

    const bool isDraggingLeft = isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    const bool isZoomingView = isHovered && (ImGui::GetIO().MouseWheel != 0.0f);
    const bool useInteractive3DThrottle =
        settings.optimizeRendering && scene.hasVisible3D() && requested3DMode &&
        (isDraggingLeft || isZoomingView);

    PlotRenderOverrides renderOverrides = overrides ? *overrides : PlotRenderOverrides{};
    PlotQualityDecision quality = qualityDecision ? *qualityDecision : PlotQualityDecision{};
    quality.interactiveThrottle = quality.interactiveThrottle || useInteractive3DThrottle;
    const EffectivePlotSettings effective =
        resolveEffectivePlotSettings(settings, renderOverrides, quality, scene);

    // Draw background
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const std::array<float, 4>& bg = effective.backgroundColor;
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      ImGui::ColorConvertFloat4ToU32(ImVec4(bg[0], bg[1], bg[2], bg[3])));

    const XYRenderMode effectiveRenderMode = effective.renderMode;
    const bool is3DMode = effective.is3DMode;
    const bool use3DGridPlaneInterleave = is3DMode && effective.showGrid;

    // 2D grid/axes/labels are drawn up-front. In 3D mode the projected grid can be interleaved
    // between below-plane and above-plane geometry later to preserve XY-plane depth ordering.
    if (!is3DMode) {
        if (effective.showGrid) {
            Plotting::PlotRenderer::drawGrid(dl, vt);
        }
        if (effective.showCoordinates) {
            Plotting::PlotRenderer::drawAxes(dl, vt);
            Plotting::PlotRenderer::drawAxisLabels(dl, vt);
        }
    }

    // Helper to build Surface3DOptions from the current settings (avoids duplicating
    // the same field list for Surface3D and ScalarField3D render kinds).
    auto make3DOptions = [&]() {
        Plotting::PlotRenderer::Surface3DOptions options;
        options.azimuthDeg = effective.azimuthDeg;
        options.elevationDeg = effective.elevationDeg;
        options.zScale = effective.zScale;
        options.resolution = effective.surfaceResolution;
        options.implicitResolution = effective.implicitSurfaceResolution;
        options.opacity = effective.surfaceOpacity;
        options.wireOpacity = effective.wireOpacity;
        options.wireThickness = effective.wireThickness;
        options.wireStride = effective.wireStride;
        options.showEnvelope = effective.showEnvelope;
        options.envelopeThickness = effective.envelopeThickness;
        // Axis triad is an alternative to coordinate overlays in 3D mode, so keep them
        // mutually exclusive to avoid redundant on-screen guidance.
        options.showAxisTriad = effective.showAxisTriad;
        return options;
    };

    auto drawFormulas = [&](Plotting::PlotRenderer::SurfacePlanePass3D planePass,
                            bool enable3DOverlays) {
        for (auto& f : formulas) {
            if (!f.visible || !f.isValid()) continue;
            switch (formulaRenderKindFor(f.compiled.kind)) {
                case FormulaRenderKind::Curve2D:
                    if (!is3DMode) {
                        Plotting::PlotRenderer::drawCurve2D(dl, vt, f.compiled.ast, f.color.data());
                    }
                    break;
                case FormulaRenderKind::Surface3D:
                    if (is3DMode) {
                        auto options = make3DOptions();
                        options.planePass = planePass;
                        if (!enable3DOverlays) {
                            options.showEnvelope = false;
                            options.showAxisTriad = false;
                        }
                        Plotting::PlotRenderer::drawSurface3D(
                            dl, vt, f.compiled.ast, f.color.data(), options);
                    } else {
                        Plotting::PlotRenderer::drawHeatmap(
                            dl, vt, f.compiled.ast, f.color.data(), effective.heatmapOpacity);
                    }
                    break;
                case FormulaRenderKind::Implicit2D:
                    if (!is3DMode) {
                        Plotting::PlotRenderer::drawImplicitContour2D(
                            dl, vt, f.compiled.ast, f.color.data(), 2.0f);
                    }
                    break;
                case FormulaRenderKind::ScalarField3D:
                    if (f.compiled.equation && is3DMode) {
                        auto options = make3DOptions();
                        options.implicitZCenter = static_cast<float>(f.zSlice);
                        options.planePass = planePass;
                        if (!enable3DOverlays) {
                            options.showEnvelope = false;
                            options.showAxisTriad = false;
                        }
                        Plotting::PlotRenderer::drawImplicitSurface3D(
                            dl, vt, f.compiled.ast, f.color.data(), options);
                    } else if (!is3DMode) {
                        Plotting::PlotRenderer::drawCrossSection(
                            dl,
                            vt,
                            f.compiled.ast,
                            static_cast<float>(f.zSlice),
                            f.color.data(),
                            effective.heatmapOpacity);
                    }
                    break;
                default:
                    break;
            }
        }
    };

    if (is3DMode) {
        Plotting::PlotRenderer::Surface3DOptions reference3D;
        reference3D.azimuthDeg = effective.azimuthDeg;
        reference3D.elevationDeg = effective.elevationDeg;
        reference3D.zScale = effective.zScale;

        if (use3DGridPlaneInterleave) {
            drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::BelowGridPlane, false);
            Plotting::PlotRenderer::drawGrid3D(dl, vt, reference3D);
            drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::AboveGridPlane, true);
        } else {
            drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::All, true);
            if (effective.showGrid) {
                Plotting::PlotRenderer::drawGrid3D(dl, vt, reference3D);
            }
        }

        if (effective.showCoordinates) {
            Plotting::PlotRenderer::drawAxes3D(dl, vt, reference3D);
        }
    } else {
        drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::All, true);
    }

    if (effective.showCanvasBorder) {
        dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(100, 100, 100, 255));
    }

    // --- Mouse interaction ---
    if (isHovered) {
        // Pan with left drag.
        // In 3D surface mode this still pans the X/Y sampling domain (not the camera orbit), so
        // implicit F(x,y,z)=0 meshes may need to be rebuilt if the visible domain changes.
        if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            vt.panPixels(delta.x, delta.y);
        }

        // Zoom with scroll wheel
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            double factor = (wheel > 0) ? 1.15 : (1.0 / 1.15);

            // Zoom toward cursor position in world coordinates so the point under the cursor stays
            // visually stable. This is shared by 2D and 3D modes because 3D plots still use the
            // 2D X/Y view domain as the sampling box for explicit/implicit surfaces.
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
            vt.state.centerX += (wxBefore - wxAfter);
            vt.state.centerY += (wyBefore - wyAfter);
        }
    }

    const bool hudInteraction =
        isHovered &&
        (isActive || isZoomingView ||
         ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
         ImGui::IsMouseDragging(ImGuiMouseButton_Right));
    const double now = ImGui::GetTime();
    if (hudInteraction) {
        m_lastHudInteractionTime = now;
    }

    float hudAlpha = 0.0f;
    PlotHudMode hudMode = settings.hudMode;
    if (effective.showHud && hudMode != PlotHudMode::Off) {
        if (hudMode == PlotHudMode::OnlyWhileInteracting) {
            const double elapsed = now - m_lastHudInteractionTime;
            if (elapsed <= 0.90) {
                hudAlpha = 1.0f;
            } else if (elapsed <= 1.50) {
                hudAlpha = static_cast<float>((1.50 - elapsed) / 0.60);
            }
        } else {
            hudAlpha = 1.0f;
        }
    }

    if (hudAlpha > 0.0f) {
        char line[128];
        std::vector<std::string> hudLines;
        const ImVec2 samplePos = isHovered
            ? ImGui::GetIO().MousePos
            : ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        double wx = 0.0;
        double wy = 0.0;
        vt.screenToWorld(samplePos.x, samplePos.y, wx, wy);
        const char* sampleLabel = isHovered ? "Mouse" : "Center";

        if (hudMode == PlotHudMode::Detailed) {
            std::snprintf(line, sizeof(line), "%s | %s",
                          is3DMode ? "3D" : "2D",
                          effectiveRenderMode == XYRenderMode::Surface3D ? "surface" : "heatmap");
            hudLines.emplace_back(line);
            std::snprintf(line, sizeof(line), "%s x %.4g  y %.4g", sampleLabel, wx, wy);
            hudLines.emplace_back(line);
            std::snprintf(line, sizeof(line), "X [%.4g, %.4g]", vt.worldXMin(), vt.worldXMax());
            hudLines.emplace_back(line);
            std::snprintf(line, sizeof(line), "Y [%.4g, %.4g]", vt.worldYMin(), vt.worldYMax());
            hudLines.emplace_back(line);
            std::snprintf(line, sizeof(line), "Scale %.1f x %.1f px/unit", vt.state.scaleX, vt.state.scaleY);
            hudLines.emplace_back(line);
            if (is3DMode) {
                std::snprintf(line, sizeof(line), "Camera az %.1f  el %.1f  z %.2f",
                              effective.azimuthDeg, effective.elevationDeg, effective.zScale);
                hudLines.emplace_back(line);
                std::snprintf(line, sizeof(line), "Wires %.2f opacity  stride %d",
                              effective.wireOpacity, effective.wireStride);
                hudLines.emplace_back(line);
            }
        } else if (is3DMode) {
            std::snprintf(line, sizeof(line), "%s x %.4g  y %.4g", sampleLabel, wx, wy);
            hudLines.emplace_back(line);
            std::snprintf(line, sizeof(line), "Camera az %.1f  el %.1f",
                          effective.azimuthDeg, effective.elevationDeg);
            hudLines.emplace_back(line);
        } else {
            std::snprintf(line, sizeof(line), "%s x %.4g  y %.4g", sampleLabel, wx, wy);
            hudLines.emplace_back(line);
            std::snprintf(line, sizeof(line), "Scale %.1f px/unit",
                          std::sqrt(std::max(1e-6, vt.state.scaleX * vt.state.scaleY)));
            hudLines.emplace_back(line);
        }

        drawCornerHud(dl, pos, size, hudLines, hudAlpha);
    }
}

} // namespace XpressFormula::UI
