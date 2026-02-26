// PlotPanel.cpp - Interactive plot panel implementation.
#include "PlotPanel.h"
#include "../Plotting/PlotRenderer.h"
#include "imgui.h"

namespace XpressFormula::UI {

void PlotPanel::render(std::vector<FormulaEntry>& formulas,
                       Core::ViewTransform& vt,
                       PlotSettings& settings,
                       const PlotRenderOverrides* overrides) {
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
    const bool useOverrides = (overrides && overrides->active);
    const bool showGrid = useOverrides ? overrides->showGrid : settings.showGrid;
    const bool showCoordinates = useOverrides ? overrides->showCoordinates : settings.showCoordinates;
    const bool showWires = useOverrides ? overrides->showWires : settings.showWires;
    const bool showEnvelope = useOverrides ? overrides->showEnvelope : settings.showSurfaceEnvelope;
    const float effectiveWireThickness =
        (showWires && settings.wireThickness > 0.01f) ? settings.wireThickness : 0.0f;
    const std::array<float, 4> bg = useOverrides ? overrides->backgroundColor
                                                  : std::array<float, 4>{0.098f, 0.098f, 0.118f, 1.0f};
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      ImGui::ColorConvertFloat4ToU32(ImVec4(bg[0], bg[1], bg[2], bg[3])));

    bool hasSurface = false;
    bool has2DFormula = false;
    for (const auto& formula : formulas) {
        if (!formula.visible || !formula.isValid()) {
            continue;
        }

        if (formula.renderKind == FormulaRenderKind::Surface3D ||
            (formula.renderKind == FormulaRenderKind::ScalarField3D && formula.isEquation)) {
            hasSurface = true;
        }

        switch (formula.renderKind) {
            case FormulaRenderKind::Curve2D:
            case FormulaRenderKind::Implicit2D:
                has2DFormula = true;
                break;
            case FormulaRenderKind::ScalarField3D:
                if (!formula.isEquation) {
                    has2DFormula = true;
                }
                break;
            default:
                break;
        }

        if (hasSurface && has2DFormula) {
            break;
        }
    }

    const XYRenderMode effectiveRenderMode =
        settings.resolveXYRenderMode(has2DFormula, hasSurface);
    const bool is3DMode = (effectiveRenderMode == XYRenderMode::Surface3D);
    const bool use3DGridPlaneInterleave = is3DMode && showGrid;

    // Apply auto-rotation BEFORE any 3D drawing so the grid, axes, and surfaces
    // all use the same azimuth for this frame (avoids a 1-frame visual tear).
    if (hasSurface && is3DMode && settings.autoRotate) {
        settings.azimuthDeg += ImGui::GetIO().DeltaTime * settings.autoRotateSpeedDegPerSec;
        if (settings.azimuthDeg > 180.0f) {
            settings.azimuthDeg -= 360.0f;
        }
    }

    // 2D grid/axes/labels are drawn up-front. In 3D mode the projected grid can be interleaved
    // between below-plane and above-plane geometry later to preserve XY-plane depth ordering.
    if (!is3DMode) {
        if (showGrid) {
            Plotting::PlotRenderer::drawGrid(dl, vt);
        }
        if (showCoordinates) {
            Plotting::PlotRenderer::drawAxes(dl, vt);
            Plotting::PlotRenderer::drawAxisLabels(dl, vt);
        }
    }

    const bool isDraggingLeft = isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    const bool isZoomingView = isHovered && (ImGui::GetIO().MouseWheel != 0.0f);
    const bool useInteractive3DThrottle =
        settings.optimizeRendering && hasSurface && is3DMode && (isDraggingLeft || isZoomingView);

    // Panning/zooming implicit F(x,y,z)=0 changes the sampled domain, which invalidates the mesh cache
    // and can force a full O(N^3) remesh every mouse move. Temporarily lowering mesh density (and
    // suppressing wireframe lines) keeps interaction responsive, then full quality returns on release.
    int interactiveSurfaceResolution = settings.surfaceResolution;
    if (useInteractive3DThrottle) {
        if (interactiveSurfaceResolution > 24) {
            interactiveSurfaceResolution = (interactiveSurfaceResolution * 2) / 3;
        }
        if (interactiveSurfaceResolution < 24) {
            interactiveSurfaceResolution = 24;
        }
    }

    int interactiveImplicitResolution = settings.implicitSurfaceResolution;
    if (useInteractive3DThrottle) {
        if (interactiveImplicitResolution > 20) {
            interactiveImplicitResolution /= 2;
        }
        if (interactiveImplicitResolution > 40) {
            interactiveImplicitResolution = 40;
        }
        if (interactiveImplicitResolution < 20) {
            interactiveImplicitResolution = 20;
        }
    }

    const float interactionWireThickness = useInteractive3DThrottle ? 0.0f : effectiveWireThickness;

    // Helper to build Surface3DOptions from the current settings (avoids duplicating
    // the same field list for Surface3D and ScalarField3D render kinds).
    auto make3DOptions = [&]() {
        Plotting::PlotRenderer::Surface3DOptions options;
        options.azimuthDeg = settings.azimuthDeg;
        options.elevationDeg = settings.elevationDeg;
        options.zScale = settings.zScale;
        options.resolution = interactiveSurfaceResolution;
        options.implicitResolution = interactiveImplicitResolution;
        options.opacity = settings.surfaceOpacity;
        options.wireThickness = interactionWireThickness;
        options.showEnvelope = showEnvelope;
        options.envelopeThickness = settings.envelopeThickness;
        // Dimension gizmo is an alternative to coordinate overlays in 3D mode, so keep them
        // mutually exclusive to avoid redundant on-screen guidance.
        options.showDimensionArrows = settings.showDimensionArrows && !showCoordinates;
        return options;
    };

    auto drawFormulas = [&](Plotting::PlotRenderer::SurfacePlanePass3D planePass,
                            bool enable3DOverlays) {
        for (auto& f : formulas) {
            if (!f.visible || !f.isValid()) continue;
            switch (f.renderKind) {
                case FormulaRenderKind::Curve2D:
                    if (!is3DMode) {
                        Plotting::PlotRenderer::drawCurve2D(dl, vt, f.ast, f.color);
                    }
                    break;
                case FormulaRenderKind::Surface3D:
                    if (is3DMode) {
                        auto options = make3DOptions();
                        options.planePass = planePass;
                        if (!enable3DOverlays) {
                            options.showEnvelope = false;
                            options.showDimensionArrows = false;
                        }
                        Plotting::PlotRenderer::drawSurface3D(dl, vt, f.ast, f.color, options);
                    } else {
                        Plotting::PlotRenderer::drawHeatmap(
                            dl, vt, f.ast, f.color, settings.heatmapOpacity);
                    }
                    break;
                case FormulaRenderKind::Implicit2D:
                    if (!is3DMode) {
                        Plotting::PlotRenderer::drawImplicitContour2D(dl, vt, f.ast, f.color, 2.0f);
                    }
                    break;
                case FormulaRenderKind::ScalarField3D:
                    if (f.isEquation && is3DMode) {
                        auto options = make3DOptions();
                        options.implicitZCenter = f.zSlice;
                        options.planePass = planePass;
                        if (!enable3DOverlays) {
                            options.showEnvelope = false;
                            options.showDimensionArrows = false;
                        }
                        Plotting::PlotRenderer::drawImplicitSurface3D(dl, vt, f.ast, f.color, options);
                    } else if (!is3DMode) {
                        Plotting::PlotRenderer::drawCrossSection(
                            dl, vt, f.ast, f.zSlice, f.color, settings.heatmapOpacity);
                    }
                    break;
                default:
                    break;
            }
        }
    };

    if (is3DMode) {
        Plotting::PlotRenderer::Surface3DOptions reference3D;
        reference3D.azimuthDeg = settings.azimuthDeg;
        reference3D.elevationDeg = settings.elevationDeg;
        reference3D.zScale = settings.zScale;

        if (use3DGridPlaneInterleave) {
            drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::BelowGridPlane, false);
            Plotting::PlotRenderer::drawGrid3D(dl, vt, reference3D);
            drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::AboveGridPlane, true);
        } else {
            drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::All, true);
            if (showGrid) {
                Plotting::PlotRenderer::drawGrid3D(dl, vt, reference3D);
            }
        }

        if (showCoordinates) {
            Plotting::PlotRenderer::drawAxes3D(dl, vt, reference3D);
        }
    } else {
        drawFormulas(Plotting::PlotRenderer::SurfacePlanePass3D::All, true);
    }

    // Border
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                IM_COL32(100, 100, 100, 255));

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
            vt.centerX += (wxBefore - wxAfter);
            vt.centerY += (wyBefore - wyAfter);
        }
    }

    // Tooltip showing 2D world coordinates or 3D view-plane coordinates (plus world domain coords).
    if (isHovered) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        double wx, wy;
        vt.screenToWorld(mousePos.x, mousePos.y, wx, wy);
        if (is3DMode) {
            const double projectionScale = (vt.scaleX < vt.scaleY) ? vt.scaleX : vt.scaleY;
            const double safeProjectionScale = (projectionScale > 1e-9) ? projectionScale : 1e-9;
            const Core::Vec2 originScreen = vt.worldToScreen(0.0, 0.0);
            const double viewU = (mousePos.x - originScreen.x) / safeProjectionScale;
            const double viewV = (originScreen.y - mousePos.y) / safeProjectionScale;

            ImGui::SetTooltip(
                "view u = %.4g\nview v = %.4g\nworld x = %.4g\nworld y = %.4g\n3D camera: az %.1f, el %.1f",
                viewU, viewV, wx, wy, settings.azimuthDeg, settings.elevationDeg);
        } else {
            ImGui::SetTooltip("x = %.4g\ny = %.4g", wx, wy);
        }
    }
}

} // namespace XpressFormula::UI
