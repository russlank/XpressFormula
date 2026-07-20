// PlotRenderer.cpp - Rendering implementation for grids, axes, and curves.
#include "PlotRenderer.h"
#include "Geometry/PlaneClipping.h"
#include "Meshing/ExplicitSurfaceMesh.h"
#include "Meshing/MarchingSquares.h"
#include "Meshing/SurfaceNets.h"
#include "Projection3D.h"
#include "Rendering/ImGuiPlotBackend.h"
#include "Sampling/CurveSampler.h"
#include "Sampling/ScalarGridSampler.h"
#include "../Model/PlotPolicy.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace XpressFormula::Plotting {

// ---- helpers ----------------------------------------------------------------

unsigned int PlotRenderer::colorU32(const float c[4]) {
    return Rendering::ImGuiPlotBackend::colorU32(c);
}

unsigned int PlotRenderer::heatColor(double value, double lo, double hi,
                                     const float tint[4], float alpha) {
    return Rendering::ImGuiPlotBackend::heatColor(value, lo, hi, tint, alpha);
}

void PlotRenderer::formatLabel(char* buf, size_t len, double v) {
    if (v == 0.0) {
        std::snprintf(buf, len, "0");
        return;
    }
    if (std::abs(v) >= 10000.0 || std::abs(v) < 0.01) {
        std::snprintf(buf, len, "%.2g", v);
    } else {
        std::snprintf(buf, len, "%.4g", v);
    }
}

PlotRenderer::Surface3DOptions PlotRenderer::makeSurface3DOptions(
    const Model::EffectivePlotSettings& effective,
    const Camera3D& camera,
    SurfacePlanePass3D planePass,
    float implicitZCenter,
    double gridPlaneZ) {
    Surface3DOptions options;
    options.azimuthDeg = camera.azimuthDeg;
    options.elevationDeg = camera.elevationDeg;
    options.zScale = camera.zScale;
    options.resolution = effective.surfaceResolution;
    options.implicitResolution = effective.implicitSurfaceResolution;
    options.opacity = effective.surfaceOpacity;
    options.wireOpacity = effective.wireOpacity;
    options.wireThickness = effective.wireThickness;
    options.wireStride = effective.wireStride;
    options.showEnvelope = effective.showEnvelope;
    options.envelopeThickness = effective.envelopeThickness;
    options.showAxisTriad = effective.showAxisTriad;
    options.implicitZCenter = implicitZCenter;
    options.planePass = planePass;
    options.gridPlaneZ = gridPlaneZ;
    return options;
}

namespace {

Camera3D cameraFromOptions(const PlotRenderer::Surface3DOptions& options) noexcept {
    return Camera3D{ options.azimuthDeg, options.elevationDeg, options.zScale };
}

void drawViewportAxisTriad3D(ImDrawList* dl,
                             const XpressFormula::Core::ViewTransform& vt,
                             const XpressFormula::Plotting::PlotRenderer::Surface3DOptions& options) {
    const Projection3D projection(cameraFromOptions(options));

    auto projectDirection = [&](double wx, double wy, double wz, ImVec2& outDir) -> bool {
        Geometry::Vec2 direction;
        if (!projection.projectDirection2D(Geometry::Vec3{ wx, wy, wz }, direction)) {
            outDir = ImVec2(0.0f, 0.0f);
            return false;
        }
        outDir = ImVec2(static_cast<float>(direction.x), static_cast<float>(direction.y));
        return true;
    };

    auto drawArrow = [&](const ImVec2& from, const ImVec2& to,
                         const char* label, ImU32 arrowColor) {
        const float dxs = to.x - from.x;
        const float dys = to.y - from.y;
        const float length = std::sqrt(dxs * dxs + dys * dys);
        if (length < 1.0f) {
            return;
        }

        const float ux = dxs / length;
        const float uy = dys / length;
        const float px = -uy;
        const float py = ux;
        const float headLength = std::clamp(length * 0.22f, 8.0f, 14.0f);
        const float headWidth = headLength * 0.55f;
        const float thickness = 2.2f;

        const ImVec2 shaftEnd(to.x - ux * headLength, to.y - uy * headLength);
        dl->AddLine(from, shaftEnd, arrowColor, thickness);
        dl->AddTriangleFilled(
            to,
            ImVec2(to.x - ux * headLength + px * headWidth,
                   to.y - uy * headLength + py * headWidth),
            ImVec2(to.x - ux * headLength - px * headWidth,
                   to.y - uy * headLength - py * headWidth),
            arrowColor);
        dl->AddText(ImVec2(to.x + px * 4.0f, to.y + py * 4.0f), arrowColor, label);
    };

    ImVec2 dirX, dirY, dirZ;
    const bool okX = projectDirection(1.0, 0.0, 0.0, dirX);
    const bool okY = projectDirection(0.0, 1.0, 0.0, dirY);
    const bool okZ = projectDirection(0.0, 0.0, 1.0, dirZ);
    if (!okX && !okY && !okZ) {
        return;
    }
    if (!okZ) {
        dirZ = ImVec2(0.0f, -1.0f);
    }

    const float axisLen = 44.0f;
    ImVec2 tipX = ImVec2(dirX.x * axisLen, dirX.y * axisLen);
    ImVec2 tipY = ImVec2(dirY.x * axisLen, dirY.y * axisLen);
    ImVec2 tipZ = ImVec2(dirZ.x * axisLen, dirZ.y * axisLen);

    float minDx = std::min({ 0.0f, tipX.x, tipY.x, tipZ.x });
    float maxDx = std::max({ 0.0f, tipX.x, tipY.x, tipZ.x });
    float minDy = std::min({ 0.0f, tipX.y, tipY.y, tipZ.y });
    float maxDy = std::max({ 0.0f, tipX.y, tipY.y, tipZ.y });

    ImVec2 origin(
        vt.viewport.originX + 22.0f - minDx,
        vt.viewport.originY + vt.viewport.height - 22.0f - maxDy);

    const float xMaxAllowed = vt.viewport.originX + vt.viewport.width - 26.0f;
    const float yMinAllowed = vt.viewport.originY + 26.0f;
    if (origin.x + maxDx > xMaxAllowed) {
        origin.x -= (origin.x + maxDx - xMaxAllowed);
    }
    if (origin.y + minDy < yMinAllowed) {
        origin.y += (yMinAllowed - (origin.y + minDy));
    }

    ImVec2 clipMin(vt.viewport.originX, vt.viewport.originY);
    ImVec2 clipMax(vt.viewport.originX + vt.viewport.width,
                   vt.viewport.originY + vt.viewport.height);
    dl->PushClipRect(clipMin, clipMax, true);

    const ImU32 back = IM_COL32(18, 20, 24, 120);
    dl->AddCircleFilled(origin, 11.0f, back, 18);

    const ImU32 colorX = IM_COL32(240, 95, 95, 245);
    const ImU32 colorY = IM_COL32(95, 225, 120, 245);
    const ImU32 colorZ = IM_COL32(110, 165, 250, 245);
    if (okX) drawArrow(origin, ImVec2(origin.x + tipX.x, origin.y + tipX.y), "X", colorX);
    if (okY) drawArrow(origin, ImVec2(origin.x + tipY.x, origin.y + tipY.y), "Y", colorY);
    drawArrow(origin, ImVec2(origin.x + tipZ.x, origin.y + tipZ.y), "Z", colorZ);

    dl->PopClipRect();
}

} // namespace

// ---- grid -------------------------------------------------------------------

void PlotRenderer::drawGrid(ImDrawList* dl, const Core::ViewTransform& vt) {
    const ImU32 colMinor = IM_COL32(60, 60, 60, 255);
    const ImU32 colMajor = IM_COL32(90, 90, 90, 255);
    const double gx = vt.gridSpacingX();
    const double gy = vt.gridSpacingY();

    // Vertical grid lines
    const double xStart = std::floor(vt.worldXMin() / gx) * gx;
    for (double wx = xStart; wx <= vt.worldXMax(); wx += gx) {
        Core::Vec2 top = vt.worldToScreen(wx, vt.worldYMax());
        Core::Vec2 bot = vt.worldToScreen(wx, vt.worldYMin());
        const bool major = (std::fmod(std::abs(wx), gx * 5.0) < gx * 0.1);
        dl->AddLine(ImVec2(top.x, top.y), ImVec2(bot.x, bot.y),
                    major ? colMajor : colMinor, major ? 1.0f : 0.5f);
    }

    // Horizontal grid lines
    const double yStart = std::floor(vt.worldYMin() / gy) * gy;
    for (double wy = yStart; wy <= vt.worldYMax(); wy += gy) {
        Core::Vec2 left = vt.worldToScreen(vt.worldXMin(), wy);
        Core::Vec2 right = vt.worldToScreen(vt.worldXMax(), wy);
        const bool major = (std::fmod(std::abs(wy), gy * 5.0) < gy * 0.1);
        dl->AddLine(ImVec2(left.x, left.y), ImVec2(right.x, right.y),
                    major ? colMajor : colMinor, major ? 1.0f : 0.5f);
    }
}

void PlotRenderer::drawGrid3D(ImDrawList* dl, const Core::ViewTransform& vt,
                              const Surface3DOptions& options) {
    const ImU32 colPlaneFill = IM_COL32(110, 120, 132, 52);
    const ImU32 colMinor = IM_COL32(72, 76, 82, 190);
    const ImU32 colMajor = IM_COL32(112, 118, 126, 220);
    const ImU32 colFrame = IM_COL32(180, 188, 200, 235);
    const double gx = vt.gridSpacingX();
    const double gy = vt.gridSpacingY();
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();

    const Projection3D projection(cameraFromOptions(options));
    const ProjectionScreenAnchor anchor = projectionScreenAnchorFor(vt);

    auto projectPoint = [&](double wx, double wy, double wz) -> Core::Vec2 {
        const ScreenPoint3D screen =
            projection.projectToScreen(Geometry::Vec3{ wx, wy, wz }, anchor);
        return Core::Vec2(
            static_cast<float>(screen.screen.x),
            static_cast<float>(screen.screen.y));
    };

    ImVec2 clipMin(vt.viewport.originX, vt.viewport.originY);
    ImVec2 clipMax(vt.viewport.originX + vt.viewport.width,
                   vt.viewport.originY + vt.viewport.height);
    dl->PushClipRect(clipMin, clipMax, true);

    // Project the XY plane frame (z = 0) and draw a subtle translucent fill so the grid plane
    // is visually legible even when surfaces don't cross it.
    const Core::Vec2 p00 = projectPoint(xMin, yMin, 0.0);
    const Core::Vec2 p10 = projectPoint(xMax, yMin, 0.0);
    const Core::Vec2 p11 = projectPoint(xMax, yMax, 0.0);
    const Core::Vec2 p01 = projectPoint(xMin, yMax, 0.0);

    dl->AddQuadFilled(
        ImVec2(p00.x, p00.y), ImVec2(p10.x, p10.y),
        ImVec2(p11.x, p11.y), ImVec2(p01.x, p01.y),
        colPlaneFill);

    // Draw grid lines strictly inside the frame. Using ceil() avoids extra off-plane lines that
    // can appear beyond the projected frame when xMin/yMin are not aligned to grid spacing.
    const double xEps = std::max(1e-9, std::abs(gx) * 1e-6);
    const double yEps = std::max(1e-9, std::abs(gy) * 1e-6);
    const double xStart = std::ceil((xMin - xEps) / gx) * gx;
    for (double wx = xStart; wx <= xMax + xEps; wx += gx) {
        if (wx <= xMin + xEps || wx >= xMax - xEps) {
            continue; // frame edges are drawn separately with a thicker outline
        }
        const bool major = (std::fmod(std::abs(wx), gx * 5.0) < gx * 0.1);
        const Core::Vec2 a = projectPoint(wx, yMin, 0.0);
        const Core::Vec2 b = projectPoint(wx, yMax, 0.0);
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y),
                    major ? colMajor : colMinor, major ? 1.0f : 0.5f);
    }

    const double yStart = std::ceil((yMin - yEps) / gy) * gy;
    for (double wy = yStart; wy <= yMax + yEps; wy += gy) {
        if (wy <= yMin + yEps || wy >= yMax - yEps) {
            continue; // frame edges are drawn separately with a thicker outline
        }
        const bool major = (std::fmod(std::abs(wy), gy * 5.0) < gy * 0.1);
        const Core::Vec2 a = projectPoint(xMin, wy, 0.0);
        const Core::Vec2 b = projectPoint(xMax, wy, 0.0);
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y),
                    major ? colMajor : colMinor, major ? 1.0f : 0.5f);
    }

    // Thicker projected frame around the grid plane.
    const float frameThickness = 2.25f;
    dl->AddLine(ImVec2(p00.x, p00.y), ImVec2(p10.x, p10.y), colFrame, frameThickness);
    dl->AddLine(ImVec2(p10.x, p10.y), ImVec2(p11.x, p11.y), colFrame, frameThickness);
    dl->AddLine(ImVec2(p11.x, p11.y), ImVec2(p01.x, p01.y), colFrame, frameThickness);
    dl->AddLine(ImVec2(p01.x, p01.y), ImVec2(p00.x, p00.y), colFrame, frameThickness);

    dl->PopClipRect();
}

// ---- axes -------------------------------------------------------------------

void PlotRenderer::drawAxes(ImDrawList* dl, const Core::ViewTransform& vt) {
    const ImU32 colAxis = IM_COL32(200, 200, 200, 255);

    // Y axis (vertical line at x = 0)
    Core::Vec2 yTop = vt.worldToScreen(0, vt.worldYMax());
    Core::Vec2 yBot = vt.worldToScreen(0, vt.worldYMin());
    dl->AddLine(ImVec2(yTop.x, yTop.y), ImVec2(yBot.x, yBot.y), colAxis, 1.5f);

    // X axis (horizontal line at y = 0)
    Core::Vec2 xLeft = vt.worldToScreen(vt.worldXMin(), 0);
    Core::Vec2 xRight = vt.worldToScreen(vt.worldXMax(), 0);
    dl->AddLine(ImVec2(xLeft.x, xLeft.y), ImVec2(xRight.x, xRight.y), colAxis, 1.5f);

    // Arrow tips on axes
    const float arrowSz = 8.0f;
    // X axis right arrow
    dl->AddTriangleFilled(
        ImVec2(xRight.x, xRight.y),
        ImVec2(xRight.x - arrowSz, xRight.y - arrowSz * 0.5f),
        ImVec2(xRight.x - arrowSz, xRight.y + arrowSz * 0.5f), colAxis);
    // Y axis up arrow
    dl->AddTriangleFilled(
        ImVec2(yTop.x, yTop.y),
        ImVec2(yTop.x - arrowSz * 0.5f, yTop.y + arrowSz),
        ImVec2(yTop.x + arrowSz * 0.5f, yTop.y + arrowSz), colAxis);
}

void PlotRenderer::drawAxes3D(ImDrawList* dl, const Core::ViewTransform& vt,
                              const Surface3DOptions& options) {
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();
    const double xSpan = std::max(1e-6, xMax - xMin);
    const double ySpan = std::max(1e-6, yMax - yMin);
    const double zSpan = std::max(xSpan, ySpan) * 0.35;

    const Projection3D projection(cameraFromOptions(options));
    const ProjectionScreenAnchor anchor = projectionScreenAnchorFor(vt);
    const Core::Vec2 originScreen(
        static_cast<float>(anchor.originScreen.x),
        static_cast<float>(anchor.originScreen.y));

    auto projectPoint = [&](double wx, double wy, double wz) -> Core::Vec2 {
        const ScreenPoint3D screen =
            projection.projectToScreen(Geometry::Vec3{ wx, wy, wz }, anchor);
        return Core::Vec2(
            static_cast<float>(screen.screen.x),
            static_cast<float>(screen.screen.y));
    };

    auto drawArrow = [&](const Core::Vec2& from, const Core::Vec2& to, ImU32 col, float thickness) {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0f) {
            return;
        }
        const float ux = dx / len;
        const float uy = dy / len;
        const float px = -uy;
        const float py = ux;
        const float headLength = std::clamp(len * 0.10f, 7.0f, 16.0f);
        const float headWidth = headLength * 0.45f;
        const ImVec2 tip(to.x, to.y);
        const ImVec2 base(to.x - ux * headLength, to.y - uy * headLength);

        dl->AddLine(ImVec2(from.x, from.y), base, col, thickness);
        dl->AddTriangleFilled(
            tip,
            ImVec2(base.x + px * headWidth, base.y + py * headWidth),
            ImVec2(base.x - px * headWidth, base.y - py * headWidth),
            col);
    };

    ImVec2 clipMin(vt.viewport.originX, vt.viewport.originY);
    ImVec2 clipMax(vt.viewport.originX + vt.viewport.width,
                   vt.viewport.originY + vt.viewport.height);
    dl->PushClipRect(clipMin, clipMax, true);

    const Core::Vec2 xNeg = projectPoint(xMin, 0.0, 0.0);
    const Core::Vec2 xPos = projectPoint(xMax, 0.0, 0.0);
    const Core::Vec2 yNeg = projectPoint(0.0, yMin, 0.0);
    const Core::Vec2 yPos = projectPoint(0.0, yMax, 0.0);
    const Core::Vec2 zPos = projectPoint(0.0, 0.0, zSpan);

    const ImU32 colX = IM_COL32(240, 95, 95, 235);
    const ImU32 colY = IM_COL32(95, 225, 120, 235);
    const ImU32 colZ = IM_COL32(110, 165, 250, 235);

    dl->AddLine(ImVec2(xNeg.x, xNeg.y), ImVec2(xPos.x, xPos.y), colX, 1.5f);
    dl->AddLine(ImVec2(yNeg.x, yNeg.y), ImVec2(yPos.x, yPos.y), colY, 1.5f);
    drawArrow(originScreen, xPos, colX, 2.0f);
    drawArrow(originScreen, yPos, colY, 2.0f);
    drawArrow(originScreen, zPos, colZ, 2.0f);

    dl->PopClipRect();
}

// ---- tick labels ------------------------------------------------------------

void PlotRenderer::drawAxisLabels(ImDrawList* dl, const Core::ViewTransform& vt) {
    const ImU32 colText = IM_COL32(180, 180, 180, 255);
    const double gx = vt.gridSpacingX();
    const double gy = vt.gridSpacingY();
    char buf[32];

    // Get the screen position of the origin to position labels near axes
    Core::Vec2 origin = vt.worldToScreen(0, 0);

    // X axis labels
    const double xStart = std::floor(vt.worldXMin() / gx) * gx;
    for (double wx = xStart; wx <= vt.worldXMax(); wx += gx) {
        if (std::abs(wx) < gx * 0.01) {
            continue;
        }
        Core::Vec2 p = vt.worldToScreen(wx, 0);
        // Keep labels within the plot area vertically
        const float yLo = vt.viewport.originY;
        const float yHi = vt.viewport.originY + vt.viewport.height - 16.0f;
        const float ly = std::clamp(origin.y + 4.0f,
                                    std::min(yLo, yHi),
                                    std::max(yLo, yHi));
        formatLabel(buf, sizeof(buf), wx);
        dl->AddText(ImVec2(p.x + 2.0f, ly), colText, buf);
    }

    // Y axis labels
    const double yStart = std::floor(vt.worldYMin() / gy) * gy;
    for (double wy = yStart; wy <= vt.worldYMax(); wy += gy) {
        if (std::abs(wy) < gy * 0.01) {
            continue;
        }
        Core::Vec2 p = vt.worldToScreen(0, wy);
        const float xLo = vt.viewport.originX;
        const float xHi = vt.viewport.originX + vt.viewport.width - 48.0f;
        const float lx = std::clamp(origin.x + 4.0f,
                                    std::min(xLo, xHi),
                                    std::max(xLo, xHi));
        formatLabel(buf, sizeof(buf), wy);
        dl->AddText(ImVec2(lx, p.y - 6.0f), colText, buf);
    }

    // Origin label
    dl->AddText(ImVec2(origin.x + 4.0f, origin.y + 4.0f), colText, "0");
}

// ---- 2D curve ---------------------------------------------------------------

void PlotRenderer::drawCurve2D(ImDrawList* dl, const Core::ViewTransform& vt,
                               const Core::ASTNodePtr& ast,
                               const float color[4], float thickness) {
    if (!ast) {
        return;
    }

    const int numSamples = std::max(1, static_cast<int>(vt.viewport.width) * 2);
    const double scaleY = std::max(1e-6, std::abs(vt.state.scaleY));
    const double maxWorldYJump = static_cast<double>(vt.viewport.height) * 2.0 / scaleY;
    const std::vector<Geometry::Polyline> polylines =
        Sampling::sampleCurve2D(
            ast,
            Sampling::CurveSampleOptions{
                vt.worldXMin(),
                vt.worldXMax(),
                numSamples,
                maxWorldYJump
            });

    Rendering::ImGuiPlotBackend::drawPolylines(dl, vt, polylines, color, thickness);
}

// ---- heat-map for f(x,y) ---------------------------------------------------

void PlotRenderer::drawHeatmap(ImDrawList* dl, const Core::ViewTransform& vt,
                               const Core::ASTNodePtr& ast,
                               const float tint[4], float alpha) {
    if (!ast) {
        return;
    }

    const Sampling::ScalarCellGrid grid =
        Sampling::sampleScalarCellGrid(
            ast,
            Sampling::ScalarGridOptions{
                Geometry::Bounds2D{
                    vt.worldXMin(),
                    vt.worldXMax(),
                    vt.worldYMin(),
                    vt.worldYMax()
                },
                200,
                150
            });

    Rendering::ImGuiPlotBackend::drawScalarCellGrid(dl, vt, grid, tint, alpha);
}

// ---- cross-section for f(x,y,z) at fixed z ---------------------------------

void PlotRenderer::drawCrossSection(ImDrawList* dl, const Core::ViewTransform& vt,
                                    const Core::ASTNodePtr& ast,
                                    float zSlice,
                                    const float tint[4], float alpha) {
    if (!ast) {
        return;
    }

    const Sampling::ScalarCellGrid grid =
        Sampling::sampleScalarCellGrid(
            ast,
            Sampling::ScalarGridOptions{
                Geometry::Bounds2D{
                    vt.worldXMin(),
                    vt.worldXMax(),
                    vt.worldYMin(),
                    vt.worldYMax()
                },
                200,
                150
            },
            static_cast<double>(zSlice));

    Rendering::ImGuiPlotBackend::drawScalarCellGrid(dl, vt, grid, tint, alpha);
}

// ---- 3D surface for z=f(x,y) -----------------------------------------------

void PlotRenderer::drawSurface3D(ImDrawList* dl, const Core::ViewTransform& vt,
                                 const Core::ASTNodePtr& ast,
                                 const float color[4],
                                 const Surface3DOptions& options) {
    if (!ast) {
        return;
    }

    struct Vertex {
        double xProj;
        double yProj;
        double depth;
        double value;
        bool valid;
    };
    struct ScreenVertex {
        float x;
        float y;
        double depth;
        double value;
        bool valid;
    };
    struct Face {
        ImVec2 p0;
        ImVec2 p1;
        ImVec2 p2;
        double depth;
        double value;
    };

    const Meshing::ExplicitSurfaceMesh mesh =
        Meshing::sampleExplicitSurface(
            ast,
            Meshing::ExplicitSurfaceOptions{
                Geometry::Bounds2D{
                    vt.worldXMin(),
                    vt.worldXMax(),
                    vt.worldYMin(),
                    vt.worldYMax()
                },
                options.resolution
            });
    if (mesh.empty() || mesh.validVertexCount == 0) {
        return;
    }

    const int nx = mesh.cellsX;
    const int ny = mesh.cellsY;
    const double xMin = mesh.bounds2D.xMin;
    const double xMax = mesh.bounds2D.xMax;
    const double yMin = mesh.bounds2D.yMin;
    const double yMax = mesh.bounds2D.yMax;
    const double zMin = mesh.scalarMin;
    const double zMax = mesh.scalarMax;

    const Projection3D projection(cameraFromOptions(options));
    const ProjectionScreenAnchor anchor = projectionScreenAnchorFor(vt);

    std::vector<Vertex> projected(mesh.vertices.size());
    int validPointCount = 0;

    for (int iy = 0; iy <= ny; ++iy) {
        for (int ix = 0; ix <= nx; ++ix) {
            const Meshing::SurfaceVertex& source = mesh.vertexAt(ix, iy);
            Vertex& v = projected[mesh.indexOf(ix, iy)];

            if (!source.valid) {
                v.valid = false;
                continue;
            }

            // Keep X/Y in world coordinates so the projected 3D geometry remains anchored to the
            // same origin used by the 2D grid/axes (ViewTransform). Subtracting the current view
            // center here would re-center the mesh every frame and cause visible "swimming".
            const ProjectedPoint3D projectedPoint = projection.project(source.position);
            v.xProj = projectedPoint.x;
            v.yProj = projectedPoint.y;
            v.depth = projectedPoint.depth;
            v.value = source.scalar;
            v.valid = true;
            validPointCount++;
        }
    }

    if (validPointCount == 0) {
        return;
    }

    // Anchor 3D projection to the world origin so the 3D scene stays aligned with the 2D axes/grid
    // while panning/zooming. Previously this auto-centered to the visible surface bounds, which made
    // the 3D scene appear to "swim" relative to the 2D coordinates.
    // We also reuse the 2D pixel/unit scale (ViewTransform) so moving the view changes both the
    // overlays and the 3D geometry consistently.
    std::vector<ScreenVertex> screenVerts((nx + 1) * (ny + 1));
    for (size_t i = 0; i < projected.size(); ++i) {
        const Vertex& v = projected[i];
        ScreenVertex& s = screenVerts[i];
        if (!v.valid) {
            s.valid = false;
            continue;
        }
        const ScreenPoint3D screen =
            projection.toScreen(ProjectedPoint3D{ v.xProj, v.yProj, v.depth }, anchor);
        s.x = static_cast<float>(screen.screen.x);
        s.y = static_cast<float>(screen.screen.y);
        s.depth = v.depth;
        s.value = v.value;
        s.valid = true;
    }

    const bool usePlaneSplitPass = (options.planePass != SurfacePlanePass3D::All);
    const double planeZ = options.gridPlaneZ;
    std::vector<Face> faces;
    faces.reserve(static_cast<size_t>(nx * ny * (usePlaneSplitPass ? 4 : 2)));

    auto toClipVertex = [](const ScreenVertex& vertex) {
        return Geometry::PlaneClipVertex2D{
            Geometry::Vec2{ vertex.x, vertex.y },
            vertex.depth,
            vertex.value
        };
    };
    const Geometry::PlaneClipSide clipSide =
        options.planePass == SurfacePlanePass3D::BelowGridPlane
            ? Geometry::PlaneClipSide::Below
            : Geometry::PlaneClipSide::Above;

    auto pushFaceRaw = [&](const Geometry::PlaneClipVertex2D& a,
                           const Geometry::PlaneClipVertex2D& b,
                           const Geometry::PlaneClipVertex2D& c) {
        faces.push_back({
            ImVec2(static_cast<float>(a.point.x), static_cast<float>(a.point.y)),
            ImVec2(static_cast<float>(b.point.x), static_cast<float>(b.point.y)),
            ImVec2(static_cast<float>(c.point.x), static_cast<float>(c.point.y)),
            (a.depth + b.depth + c.depth) / 3.0,
            (a.value + b.value + c.value) / 3.0
        });
    };

    auto pushFace = [&](const ScreenVertex& a,
                        const ScreenVertex& b,
                        const ScreenVertex& c) {
        if (!a.valid || !b.valid || !c.valid) {
            return;
        }

        if (!usePlaneSplitPass) {
            pushFaceRaw(toClipVertex(a), toClipVertex(b), toClipVertex(c));
            return;
        }

        const std::vector<Geometry::PlaneClipVertex2D> clipped =
            Geometry::clipTriangleByValue(
                toClipVertex(a),
                toClipVertex(b),
                toClipVertex(c),
                planeZ,
                clipSide);
        if (clipped.size() < 3) {
            return;
        }

        for (size_t i = 1; i + 1 < clipped.size(); ++i) {
            pushFaceRaw(clipped[0], clipped[i], clipped[i + 1]);
        }
    };

    for (int iy = 0; iy < ny; ++iy) {
        for (int ix = 0; ix < nx; ++ix) {
            const ScreenVertex& v00 = screenVerts[mesh.indexOf(ix, iy)];
            const ScreenVertex& v10 = screenVerts[mesh.indexOf(ix + 1, iy)];
            const ScreenVertex& v01 = screenVerts[mesh.indexOf(ix, iy + 1)];
            const ScreenVertex& v11 = screenVerts[mesh.indexOf(ix + 1, iy + 1)];

            pushFace(v00, v10, v11);
            pushFace(v00, v11, v01);
        }
    }

    std::sort(faces.begin(), faces.end(),
              [](const Face& a, const Face& b) { return a.depth < b.depth; });

    ImVec2 clipMin(vt.viewport.originX, vt.viewport.originY);
    ImVec2 clipMax(vt.viewport.originX + vt.viewport.width,
                   vt.viewport.originY + vt.viewport.height);
    dl->PushClipRect(clipMin, clipMax, true);

    for (const Face& face : faces) {
        const double t = std::clamp((face.value - zMin) / (zMax - zMin), 0.0, 1.0);

        // Blend formula tint with altitude-based warm/cool variation.
        const float gradientR = static_cast<float>(0.15 + 0.80 * t);
        const float gradientG = static_cast<float>(0.30 + 0.50 * (1.0 - std::abs(2.0 * t - 1.0)));
        const float gradientB = static_cast<float>(0.95 - 0.75 * t);

        const float r = std::clamp(0.55f * color[0] + 0.45f * gradientR, 0.0f, 1.0f);
        const float g = std::clamp(0.55f * color[1] + 0.45f * gradientG, 0.0f, 1.0f);
        const float b = std::clamp(0.55f * color[2] + 0.45f * gradientB, 0.0f, 1.0f);

        const ImU32 fill = IM_COL32(
            static_cast<int>(r * 255),
            static_cast<int>(g * 255),
            static_cast<int>(b * 255),
            static_cast<int>(std::clamp(options.opacity, 0.1f, 1.0f) * 255));

        dl->AddTriangleFilled(face.p0, face.p1, face.p2, fill);
    }

    const Model::PlotLimits& limits = Model::plotLimits();
    const float edgeThickness = options.wireThickness <= 0.0f
        ? 0.0f
        : std::clamp(
            options.wireThickness,
            limits.wireThickness.min,
            limits.wireThickness.max);
    const float wireOpacity = std::clamp(
        options.wireOpacity,
        limits.wireOpacity.min,
        limits.wireOpacity.max);
    const int wireStride = std::clamp(
        options.wireStride,
        limits.wireStride.min,
        limits.wireStride.max);
    if (edgeThickness > 0.0f && wireOpacity > 0.0f) {
        const ImU32 edge = IM_COL32(
            static_cast<int>(std::clamp(color[0] * 0.45f + 0.05f, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color[1] * 0.45f + 0.05f, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(color[2] * 0.45f + 0.05f, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(wireOpacity * 255.0f));

        auto drawWireSegment = [&](const ScreenVertex& a, const ScreenVertex& b) {
            if (!a.valid || !b.valid) {
                return;
            }

            Geometry::PlaneClipVertex2D pa = toClipVertex(a);
            Geometry::PlaneClipVertex2D pb = toClipVertex(b);
            if (usePlaneSplitPass) {
                const std::optional<Geometry::PlaneClipSegment2D> clipped =
                    Geometry::clipSegmentByValue(pa, pb, planeZ, clipSide);
                if (!clipped.has_value()) {
                    return;
                }
                pa = clipped->a;
                pb = clipped->b;
            }

            dl->AddLine(
                ImVec2(static_cast<float>(pa.point.x), static_cast<float>(pa.point.y)),
                ImVec2(static_cast<float>(pb.point.x), static_cast<float>(pb.point.y)),
                edge,
                edgeThickness);
        };

        auto includeWireIndex = [wireStride](int value, int maxValue) {
            return value == 0 || value == maxValue || (value % wireStride) == 0;
        };

        for (int iy = 0; iy <= ny; ++iy) {
            if (!includeWireIndex(iy, ny)) {
                continue;
            }
            for (int ix = 0; ix < nx; ++ix) {
                drawWireSegment(screenVerts[mesh.indexOf(ix, iy)],
                                screenVerts[mesh.indexOf(ix + 1, iy)]);
            }
        }

        for (int ix = 0; ix <= nx; ++ix) {
            if (!includeWireIndex(ix, nx)) {
                continue;
            }
            for (int iy = 0; iy < ny; ++iy) {
                drawWireSegment(screenVerts[mesh.indexOf(ix, iy)],
                                screenVerts[mesh.indexOf(ix, iy + 1)]);
            }
        }
    }

    if (options.showEnvelope) {
        struct EnvelopePoint {
            ImVec2 screen;
            double depth;
        };
        struct EnvelopeEdge {
            int a;
            int b;
            double depth;
        };

        auto projectEnvelopePoint = [&](double wx, double wy, double wz) {
            const ScreenPoint3D screen =
                projection.projectToScreen(Geometry::Vec3{ wx, wy, wz }, anchor);
            return EnvelopePoint{
                ImVec2(static_cast<float>(screen.screen.x), static_cast<float>(screen.screen.y)),
                screen.depth
            };
        };

        EnvelopePoint corners[8] = {
            projectEnvelopePoint(xMin, yMin, zMin),
            projectEnvelopePoint(xMax, yMin, zMin),
            projectEnvelopePoint(xMax, yMax, zMin),
            projectEnvelopePoint(xMin, yMax, zMin),
            projectEnvelopePoint(xMin, yMin, zMax),
            projectEnvelopePoint(xMax, yMin, zMax),
            projectEnvelopePoint(xMax, yMax, zMax),
            projectEnvelopePoint(xMin, yMax, zMax)
        };

        static const int kEdgeIndex[12][2] = {
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
            { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
        };

        std::vector<EnvelopeEdge> edges;
        edges.reserve(12);
        double edgeDepthMin = std::numeric_limits<double>::max();
        double edgeDepthMax = std::numeric_limits<double>::lowest();
        for (const auto& idx : kEdgeIndex) {
            const double depth = (corners[idx[0]].depth + corners[idx[1]].depth) * 0.5;
            edges.push_back({ idx[0], idx[1], depth });
            edgeDepthMin = std::min(edgeDepthMin, depth);
            edgeDepthMax = std::max(edgeDepthMax, depth);
        }
        std::sort(edges.begin(), edges.end(),
                  [](const EnvelopeEdge& lhs, const EnvelopeEdge& rhs) {
                      return lhs.depth < rhs.depth;
                  });

        if (options.showEnvelope) {
            const float baseR = std::clamp(color[0] * 0.55f + 0.45f, 0.0f, 1.0f);
            const float baseG = std::clamp(color[1] * 0.55f + 0.45f, 0.0f, 1.0f);
            const float baseB = std::clamp(color[2] * 0.55f + 0.45f, 0.0f, 1.0f);
            const double edgeRange = std::max(1e-6, edgeDepthMax - edgeDepthMin);
            const float lineThickness = std::clamp(
                options.envelopeThickness,
                limits.envelopeThickness.min,
                limits.envelopeThickness.max);

            for (const EnvelopeEdge& edge : edges) {
                const double depthNorm = (edge.depth - edgeDepthMin) / edgeRange;
                const float alpha = static_cast<float>(80.0 + depthNorm * 150.0);
                const ImU32 lineColor = IM_COL32(
                    static_cast<int>(baseR * 255),
                    static_cast<int>(baseG * 255),
                    static_cast<int>(baseB * 255),
                    static_cast<int>(std::clamp(alpha, 40.0f, 255.0f)));
                dl->AddLine(corners[edge.a].screen, corners[edge.b].screen,
                            lineColor, lineThickness);
            }
        }

    }

    dl->PopClipRect();
    if (options.showAxisTriad) {
        drawViewportAxisTriad3D(dl, vt, options);
    }
}

// ---- implicit 3D surface F(x,y,z)=0 ----------------------------------------

void PlotRenderer::drawImplicitSurface3D(ImDrawList* dl, const Core::ViewTransform& vt,
                                         const Core::ASTNodePtr& ast,
                                         const float color[4],
                                         const Surface3DOptions& options,
                                         Meshing::ImplicitMeshCache& meshCache,
                                         Model::FormulaId formulaId,
                                         std::uint64_t compilationRevision) {
    if (!ast) {
        return;
    }

    using Point3 = Geometry::Vec3;
    using WorldFace = Meshing::ImplicitMeshTriangle;

    struct ProjectedVertex {
        double wx;
        double wy;
        double wz;
        double xProj;
        double yProj;
        double depth;
    };
    struct ProjectedFace {
        ProjectedVertex v0;
        ProjectedVertex v1;
        ProjectedVertex v2;
        double depth;
        double zAvg;
        float shade;
    };
    struct ScreenFace {
        ImVec2 p0;
        ImVec2 p1;
        ImVec2 p2;
        double depth;
        double zAvg;
        float shade;
        size_t wireOrdinal;
    };
    struct EnvelopePoint {
        ImVec2 screen;
        double depth;
    };
    struct EnvelopeEdge {
        int a;
        int b;
        double depth;
    };

    const int requestedImplicitRes = (options.implicitResolution > 0)
        ? options.implicitResolution
        : (options.resolution / 2 + 8);
    const int gridRes = XpressFormula::Model::clampImplicitSurfaceResolution(requestedImplicitRes);

    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();

    // The implicit surface is sampled only inside the current view domain.
    // This means a valid shape (e.g. a sphere) can appear "cut open" if the current x/y range
    // clips it; the mesh is built only for the sampled box.
    const double xySpan = std::max(std::max(1e-6, xMax - xMin), std::max(1e-6, yMax - yMin));
    const double zCenter = static_cast<double>(options.implicitZCenter);
    const double zHalfSpan = std::max(1.0, xySpan * 0.5);
    const double zMinDomain = zCenter - zHalfSpan;
    const double zMaxDomain = zCenter + zHalfSpan;

    // Rebuild the implicit mesh only when the sampled field/domain changes.
    // Camera and visual styling are excluded because they only affect projection/shading.
    const Meshing::ImplicitMeshKey cacheKey{
        formulaId,
        compilationRevision,
        ast.get(),
        gridRes,
        xMin,
        xMax,
        yMin,
        yMax,
        zCenter,
        zMinDomain,
        zMaxDomain
    };
    const Meshing::ImplicitMeshEntry* cachedMesh = meshCache.find(cacheKey);

    const Projection3D projection(cameraFromOptions(options));
    const ProjectionScreenAnchor anchor = projectionScreenAnchorFor(vt);

    std::vector<ProjectedFace> projectedFaces;
    const std::vector<WorldFace>* meshFaces = nullptr;

    Geometry::Bounds3D surfaceBounds;

    const auto dot3 = [](const Point3& a, const Point3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const auto cross3 = [](const Point3& a, const Point3& b) {
        return Point3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    };
    const auto sub3 = [](const Point3& a, const Point3& b) {
        return Point3{ a.x - b.x, a.y - b.y, a.z - b.z };
    };
    const auto lenSq3 = [&](const Point3& v) {
        return dot3(v, v);
    };
    const auto normalize3 = [&](Point3& v) -> bool {
        const double ls = lenSq3(v);
        if (!(ls > 1e-18) || !std::isfinite(ls)) {
            return false;
        }
        const double inv = 1.0 / std::sqrt(ls);
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
        return true;
    };

    if (cachedMesh != nullptr) {
        // Fast path: reuse previously extracted mesh and its bounds. This avoids re-evaluating
        // the scalar field on the 3D grid and re-running the surface extraction.
        meshFaces = &cachedMesh->faces;
        surfaceBounds = cachedMesh->surfaceBounds;
    } else {
        // Slow path: sample F(x,y,z) and build the surface in pure meshing code.
        Meshing::ImplicitMeshEntry entry =
            Meshing::buildSurfaceNetsMesh(
                ast,
                Meshing::SurfaceNetsOptions{
                    Geometry::Bounds3D{
                        xMin,
                        xMax,
                        yMin,
                        yMax,
                        zMinDomain,
                        zMaxDomain
                    },
                    gridRes
                });
        const Meshing::ImplicitMeshEntry& storedEntry =
            meshCache.store(cacheKey, std::move(entry));
        meshFaces = &storedEntry.faces;
        surfaceBounds = storedEntry.surfaceBounds;
    }
    if (meshFaces->empty()) {
        return;
    }

    // Lighting is evaluated after projection from cached world triangles so visual changes
    // (camera angle / zScale) update correctly without rebuilding the mesh.
    const double lx = -0.35;
    const double ly = -0.45;
    const double lz = 0.82;
    const double lLen = std::sqrt(lx * lx + ly * ly + lz * lz);
    const double lightX = lx / lLen;
    const double lightY = ly / lLen;
    const double lightZ = lz / lLen;

    // Camera-dependent projection + shading pass. This is much cheaper than field sampling and
    // surface extraction, and can run every frame while rotating.
    projectedFaces.reserve(meshFaces->size());
    for (const WorldFace& wf : *meshFaces) {
        Point3 ab = sub3(wf.p1, wf.p0);
        Point3 ac = sub3(wf.p2, wf.p0);
        ab.z *= options.zScale;
        ac.z *= options.zScale;
        Point3 normal = cross3(ab, ac);
        if (!normalize3(normal)) {
            continue;
        }

        ProjectedFace face{};
        face.v0.wx = wf.p0.x; face.v0.wy = wf.p0.y; face.v0.wz = wf.p0.z;
        face.v1.wx = wf.p1.x; face.v1.wy = wf.p1.y; face.v1.wz = wf.p1.z;
        face.v2.wx = wf.p2.x; face.v2.wy = wf.p2.y; face.v2.wz = wf.p2.z;
        const ProjectedPoint3D p0 = projection.project(wf.p0);
        const ProjectedPoint3D p1 = projection.project(wf.p1);
        const ProjectedPoint3D p2 = projection.project(wf.p2);
        face.v0.xProj = p0.x; face.v0.yProj = p0.y; face.v0.depth = p0.depth;
        face.v1.xProj = p1.x; face.v1.yProj = p1.y; face.v1.depth = p1.depth;
        face.v2.xProj = p2.x; face.v2.yProj = p2.y; face.v2.depth = p2.depth;
        face.depth = (face.v0.depth + face.v1.depth + face.v2.depth) / 3.0;
        face.zAvg = (wf.p0.z + wf.p1.z + wf.p2.z) / 3.0;

        const ViewVector3D normalView = projection.rotateScaledVectorToView(normal);
        const double nViewX = normalView.x;
        const double nViewY = normalView.y;
        const double nViewZ = normalView.z;
        const double nvLen = std::sqrt(nViewX * nViewX + nViewY * nViewY + nViewZ * nViewZ);
        const double invNvLen = (nvLen > 1e-12) ? (1.0 / nvLen) : 1.0;
        const double ndotl = (nViewX * lightX + nViewY * lightY + nViewZ * lightZ) * invNvLen;
        face.shade = static_cast<float>(std::clamp(0.28 + 0.72 * std::abs(ndotl), 0.18, 1.0));

        projectedFaces.push_back(face);

    }

    if (projectedFaces.empty()) {
        return;
    }

    const bool usePlaneSplitPass = (options.planePass != SurfacePlanePass3D::All);
    const double planeZ = options.gridPlaneZ;
    std::vector<ScreenFace> screenFaces;
    screenFaces.reserve(projectedFaces.size() * (usePlaneSplitPass ? 2u : 1u));

    auto toClipVertex = [](const ProjectedVertex& vertex) {
        return Geometry::PlaneClipVertex2D{
            Geometry::Vec2{ vertex.xProj, vertex.yProj },
            vertex.depth,
            vertex.wz
        };
    };
    const Geometry::PlaneClipSide clipSide =
        options.planePass == SurfacePlanePass3D::BelowGridPlane
            ? Geometry::PlaneClipSide::Below
            : Geometry::PlaneClipSide::Above;

    auto pushScreenFaceRaw = [&](const Geometry::PlaneClipVertex2D& a,
                                 const Geometry::PlaneClipVertex2D& b,
                                 const Geometry::PlaneClipVertex2D& c,
                                 float shade,
                                 size_t wireOrdinal) {
        const ScreenPoint3D aScreen =
            projection.toScreen(ProjectedPoint3D{ a.point.x, a.point.y, a.depth }, anchor);
        const ScreenPoint3D bScreen =
            projection.toScreen(ProjectedPoint3D{ b.point.x, b.point.y, b.depth }, anchor);
        const ScreenPoint3D cScreen =
            projection.toScreen(ProjectedPoint3D{ c.point.x, c.point.y, c.depth }, anchor);
        screenFaces.push_back({
            ImVec2(static_cast<float>(aScreen.screen.x), static_cast<float>(aScreen.screen.y)),
            ImVec2(static_cast<float>(bScreen.screen.x), static_cast<float>(bScreen.screen.y)),
            ImVec2(static_cast<float>(cScreen.screen.x), static_cast<float>(cScreen.screen.y)),
            (a.depth + b.depth + c.depth) / 3.0,
            (a.value + b.value + c.value) / 3.0,
            shade,
            wireOrdinal
        });
    };

    auto pushProjectedFace = [&](const ProjectedFace& f, size_t wireOrdinal) {
        if (!usePlaneSplitPass) {
            pushScreenFaceRaw(
                toClipVertex(f.v0),
                toClipVertex(f.v1),
                toClipVertex(f.v2),
                f.shade,
                wireOrdinal);
            return;
        }

        const std::vector<Geometry::PlaneClipVertex2D> clipped =
            Geometry::clipTriangleByValue(
                toClipVertex(f.v0),
                toClipVertex(f.v1),
                toClipVertex(f.v2),
                planeZ,
                clipSide);
        if (clipped.size() < 3) {
            return;
        }

        for (size_t i = 1; i + 1 < clipped.size(); ++i) {
            pushScreenFaceRaw(clipped[0], clipped[i], clipped[i + 1], f.shade, wireOrdinal);
        }
    };

    for (size_t i = 0; i < projectedFaces.size(); ++i) {
        pushProjectedFace(projectedFaces[i], i);
    }

    // ImGui draw lists have no depth buffer, so we painter-sort triangles back-to-front.
    std::sort(screenFaces.begin(), screenFaces.end(),
              [](const ScreenFace& a, const ScreenFace& b) {
                  return a.depth < b.depth;
              });

    ImVec2 clipMin(vt.viewport.originX, vt.viewport.originY);
    ImVec2 clipMax(vt.viewport.originX + vt.viewport.width,
                   vt.viewport.originY + vt.viewport.height);
    dl->PushClipRect(clipMin, clipMax, true);

    double surfXMin = surfaceBounds.xMin;
    double surfXMax = surfaceBounds.xMax;
    double surfYMin = surfaceBounds.yMin;
    double surfYMax = surfaceBounds.yMax;
    double surfZMin = surfaceBounds.zMin;
    double surfZMax = surfaceBounds.zMax;
    if (!(surfZMin < surfZMax)) {
        surfZMin = zMinDomain;
        surfZMax = zMaxDomain;
    }
    const double zRange = std::max(1e-6, surfZMax - surfZMin);
    const float baseOpacity = std::clamp(options.opacity, 0.12f, 1.0f);
    const Model::PlotLimits& limits = Model::plotLimits();
    const float edgeThickness = options.wireThickness <= 0.0f
        ? 0.0f
        : std::clamp(
            options.wireThickness,
            limits.wireThickness.min,
            limits.wireThickness.max);
    const float wireOpacity = std::clamp(
        options.wireOpacity,
        limits.wireOpacity.min,
        limits.wireOpacity.max);
    const int wireStride = std::clamp(
        options.wireStride,
        limits.wireStride.min,
        limits.wireStride.max);

    for (const ScreenFace& face : screenFaces) {
        const double t = std::clamp((face.zAvg - surfZMin) / zRange, 0.0, 1.0);
        const float gradientR = static_cast<float>(0.18 + 0.76 * t);
        const float gradientG = static_cast<float>(0.28 + 0.48 * (1.0 - std::abs(2.0 * t - 1.0)));
        const float gradientB = static_cast<float>(0.95 - 0.72 * t);

        const float baseR = std::clamp(0.58f * color[0] + 0.42f * gradientR, 0.0f, 1.0f);
        const float baseG = std::clamp(0.58f * color[1] + 0.42f * gradientG, 0.0f, 1.0f);
        const float baseB = std::clamp(0.58f * color[2] + 0.42f * gradientB, 0.0f, 1.0f);
        const float shadeMix = std::clamp(0.52f + 0.48f * face.shade, 0.0f, 1.0f);
        const float r = std::clamp(baseR * shadeMix, 0.0f, 1.0f);
        const float g = std::clamp(baseG * shadeMix, 0.0f, 1.0f);
        const float b = std::clamp(baseB * shadeMix, 0.0f, 1.0f);

        const ImU32 fill = IM_COL32(
            static_cast<int>(r * 255.0f),
            static_cast<int>(g * 255.0f),
            static_cast<int>(b * 255.0f),
            static_cast<int>(baseOpacity * 255.0f));

        const ImU32 edge = IM_COL32(
            static_cast<int>(std::clamp(r * 0.55f, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(g * 0.55f, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(std::clamp(b * 0.55f, 0.0f, 1.0f) * 255.0f),
            static_cast<int>(wireOpacity * 255.0f));

        dl->AddTriangleFilled(face.p0, face.p1, face.p2, fill);
        if (edgeThickness > 0.0f &&
            wireOpacity > 0.0f &&
            (face.wireOrdinal % static_cast<size_t>(wireStride)) == 0u) {
            dl->AddLine(face.p0, face.p1, edge, edgeThickness);
            dl->AddLine(face.p1, face.p2, edge, edgeThickness);
            dl->AddLine(face.p2, face.p0, edge, edgeThickness);
        }
    }

    if (options.showEnvelope) {
        // Envelope/axis-triad overlays are drawn from extracted surface bounds (not full sample box) to give
        // a tighter visual wrapper around the actual shape.
        if (!(surfXMin < surfXMax)) { surfXMin = xMin; surfXMax = xMax; }
        if (!(surfYMin < surfYMax)) { surfYMin = yMin; surfYMax = yMax; }
        if (!(surfZMin < surfZMax)) { surfZMin = zMinDomain; surfZMax = zMaxDomain; }

        auto projectEnvelopePoint = [&](double wx, double wy, double wz) {
            const ScreenPoint3D screen =
                projection.projectToScreen(Geometry::Vec3{ wx, wy, wz }, anchor);
            return EnvelopePoint{
                ImVec2(static_cast<float>(screen.screen.x), static_cast<float>(screen.screen.y)),
                screen.depth
            };
        };

        EnvelopePoint corners[8] = {
            projectEnvelopePoint(surfXMin, surfYMin, surfZMin),
            projectEnvelopePoint(surfXMax, surfYMin, surfZMin),
            projectEnvelopePoint(surfXMax, surfYMax, surfZMin),
            projectEnvelopePoint(surfXMin, surfYMax, surfZMin),
            projectEnvelopePoint(surfXMin, surfYMin, surfZMax),
            projectEnvelopePoint(surfXMax, surfYMin, surfZMax),
            projectEnvelopePoint(surfXMax, surfYMax, surfZMax),
            projectEnvelopePoint(surfXMin, surfYMax, surfZMax)
        };

        static const int kEdgeIndex[12][2] = {
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
            { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
        };

        std::vector<EnvelopeEdge> edges;
        edges.reserve(12);
        double edgeDepthMin = std::numeric_limits<double>::max();
        double edgeDepthMax = std::numeric_limits<double>::lowest();
        for (const auto& idx : kEdgeIndex) {
            const double depth = (corners[idx[0]].depth + corners[idx[1]].depth) * 0.5;
            edges.push_back({ idx[0], idx[1], depth });
            edgeDepthMin = std::min(edgeDepthMin, depth);
            edgeDepthMax = std::max(edgeDepthMax, depth);
        }
        std::sort(edges.begin(), edges.end(),
                  [](const EnvelopeEdge& lhs, const EnvelopeEdge& rhs) {
                      return lhs.depth < rhs.depth;
                  });

        if (options.showEnvelope) {
            const float baseR = std::clamp(color[0] * 0.55f + 0.45f, 0.0f, 1.0f);
            const float baseG = std::clamp(color[1] * 0.55f + 0.45f, 0.0f, 1.0f);
            const float baseB = std::clamp(color[2] * 0.55f + 0.45f, 0.0f, 1.0f);
            const double edgeRange = std::max(1e-6, edgeDepthMax - edgeDepthMin);
            const float lineThickness = std::clamp(
                options.envelopeThickness,
                limits.envelopeThickness.min,
                limits.envelopeThickness.max);

            for (const EnvelopeEdge& edge : edges) {
                const double depthNorm = (edge.depth - edgeDepthMin) / edgeRange;
                const float alpha = static_cast<float>(80.0 + depthNorm * 150.0);
                const ImU32 lineColor = IM_COL32(
                    static_cast<int>(baseR * 255),
                    static_cast<int>(baseG * 255),
                    static_cast<int>(baseB * 255),
                    static_cast<int>(std::clamp(alpha, 40.0f, 255.0f)));
                dl->AddLine(corners[edge.a].screen, corners[edge.b].screen,
                            lineColor, lineThickness);
            }
        }

    }

    dl->PopClipRect();
    if (options.showAxisTriad) {
        drawViewportAxisTriad3D(dl, vt, options);
    }
}

// ---- implicit contour F(x,y)=0 ---------------------------------------------

void PlotRenderer::drawImplicitContour2D(ImDrawList* dl, const Core::ViewTransform& vt,
                                         const Core::ASTNodePtr& ast,
                                         const float color[4], float thickness) {
    if (!ast) {
        return;
    }

    const Sampling::ScalarLattice lattice =
        Sampling::sampleScalarLattice(
            ast,
            Sampling::ScalarGridOptions{
                Geometry::Bounds2D{
                    vt.worldXMin(),
                    vt.worldXMax(),
                    vt.worldYMin(),
                    vt.worldYMax()
                },
                180,
                140
            });
    const std::vector<Geometry::LineSegment2D> segments =
        Meshing::buildContourSegments(lattice);

    Rendering::ImGuiPlotBackend::drawLineSegments(dl, vt, segments, color, thickness);
}

} // namespace XpressFormula::Plotting
