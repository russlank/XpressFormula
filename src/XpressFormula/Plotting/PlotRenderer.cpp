// PlotRenderer.cpp - Rendering implementation for grids, axes, and curves.
#include "PlotRenderer.h"
#include "../Core/Evaluator.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <limits>
#include <vector>

namespace XpressFormula::Plotting {

// ---- helpers ----------------------------------------------------------------

unsigned int PlotRenderer::colorU32(const float c[4]) {
    return IM_COL32(
        static_cast<int>(c[0] * 255),
        static_cast<int>(c[1] * 255),
        static_cast<int>(c[2] * 255),
        static_cast<int>(c[3] * 255));
}

unsigned int PlotRenderer::heatColor(double value, double lo, double hi, float alpha) {
    if (std::isnan(value) || std::isinf(value))
        return IM_COL32(40, 40, 40, static_cast<int>(alpha * 255));
    double range = hi - lo;
    if (range == 0.0) range = 1.0;
    double t = std::clamp((value - lo) / range, 0.0, 1.0);

    // Cool-warm divergent colormap: blue(0) -> white(0.5) -> red(1)
    float r, g, b;
    if (t < 0.5) {
        float s = static_cast<float>(t * 2.0);
        r = s;  g = s;  b = 1.0f;
    } else {
        float s = static_cast<float>((t - 0.5) * 2.0);
        r = 1.0f;  g = 1.0f - s;  b = 1.0f - s;
    }
    return IM_COL32(
        static_cast<int>(r * 255), static_cast<int>(g * 255),
        static_cast<int>(b * 255), static_cast<int>(alpha * 255));
}

void PlotRenderer::formatLabel(char* buf, size_t len, double v) {
    if (v == 0.0) { std::snprintf(buf, len, "0"); return; }
    if (std::abs(v) >= 10000.0 || std::abs(v) < 0.01)
        std::snprintf(buf, len, "%.2g", v);
    else
        std::snprintf(buf, len, "%.4g", v);
}

// ---- grid -------------------------------------------------------------------

void PlotRenderer::drawGrid(ImDrawList* dl, const Core::ViewTransform& vt) {
    const ImU32 colMinor = IM_COL32(60, 60, 60, 255);
    const ImU32 colMajor = IM_COL32(90, 90, 90, 255);
    const double gx = vt.gridSpacingX();
    const double gy = vt.gridSpacingY();

    // Vertical grid lines
    double xStart = std::floor(vt.worldXMin() / gx) * gx;
    for (double wx = xStart; wx <= vt.worldXMax(); wx += gx) {
        Core::Vec2 top = vt.worldToScreen(wx, vt.worldYMax());
        Core::Vec2 bot = vt.worldToScreen(wx, vt.worldYMin());
        bool major = (std::fmod(std::abs(wx), gx * 5.0) < gx * 0.1);
        dl->AddLine(ImVec2(top.x, top.y), ImVec2(bot.x, bot.y),
                    major ? colMajor : colMinor, major ? 1.0f : 0.5f);
    }

    // Horizontal grid lines
    double yStart = std::floor(vt.worldYMin() / gy) * gy;
    for (double wy = yStart; wy <= vt.worldYMax(); wy += gy) {
        Core::Vec2 left  = vt.worldToScreen(vt.worldXMin(), wy);
        Core::Vec2 right = vt.worldToScreen(vt.worldXMax(), wy);
        bool major = (std::fmod(std::abs(wy), gy * 5.0) < gy * 0.1);
        dl->AddLine(ImVec2(left.x, left.y), ImVec2(right.x, right.y),
                    major ? colMajor : colMinor, major ? 1.0f : 0.5f);
    }
}

// ---- axes -------------------------------------------------------------------

void PlotRenderer::drawAxes(ImDrawList* dl, const Core::ViewTransform& vt) {
    const ImU32 colAxis = IM_COL32(200, 200, 200, 255);

    // Y axis (vertical line at x = 0)
    Core::Vec2 yTop = vt.worldToScreen(0, vt.worldYMax());
    Core::Vec2 yBot = vt.worldToScreen(0, vt.worldYMin());
    dl->AddLine(ImVec2(yTop.x, yTop.y), ImVec2(yBot.x, yBot.y), colAxis, 1.5f);

    // X axis (horizontal line at y = 0)
    Core::Vec2 xLeft  = vt.worldToScreen(vt.worldXMin(), 0);
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

// ---- tick labels ------------------------------------------------------------

void PlotRenderer::drawAxisLabels(ImDrawList* dl, const Core::ViewTransform& vt) {
    const ImU32 colText = IM_COL32(180, 180, 180, 255);
    const double gx = vt.gridSpacingX();
    const double gy = vt.gridSpacingY();
    char buf[32];

    // Get the screen position of the origin to position labels near axes
    Core::Vec2 origin = vt.worldToScreen(0, 0);

    // X axis labels
    double xStart = std::floor(vt.worldXMin() / gx) * gx;
    for (double wx = xStart; wx <= vt.worldXMax(); wx += gx) {
        if (std::abs(wx) < gx * 0.01) continue; // skip zero
        Core::Vec2 p = vt.worldToScreen(wx, 0);
        // Keep labels within the plot area vertically
        const float yLo = vt.screenOriginY;
        const float yHi = vt.screenOriginY + vt.screenHeight - 16.0f;
        const float ly = std::clamp(origin.y + 4.0f,
                                    std::min(yLo, yHi),
                                    std::max(yLo, yHi));
        formatLabel(buf, sizeof(buf), wx);
        dl->AddText(ImVec2(p.x + 2.0f, ly), colText, buf);
    }

    // Y axis labels
    double yStart = std::floor(vt.worldYMin() / gy) * gy;
    for (double wy = yStart; wy <= vt.worldYMax(); wy += gy) {
        if (std::abs(wy) < gy * 0.01) continue;
        Core::Vec2 p = vt.worldToScreen(0, wy);
        const float xLo = vt.screenOriginX;
        const float xHi = vt.screenOriginX + vt.screenWidth - 48.0f;
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
    if (!ast) return;

    Core::Evaluator::Variables vars;
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const int    numSamples = static_cast<int>(vt.screenWidth) * 2;
    const double dx = (xMax - xMin) / numSamples;
    const ImU32  col = colorU32(color);

    // Clipping rectangle for the plot area
    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    struct Point { float x, y; bool valid; };
    std::vector<Point> points;
    points.reserve(numSamples + 1);

    for (int i = 0; i <= numSamples; ++i) {
        double wx = xMin + i * dx;
        vars["x"] = wx;
        double wy = Core::Evaluator::evaluate(ast, vars);

        bool valid = std::isfinite(wy);
        if (valid) {
            Core::Vec2 sp = vt.worldToScreen(wx, wy);
            points.push_back({ sp.x, sp.y, true });
        } else {
            points.push_back({ 0, 0, false });
        }
    }

    // Draw connected segments, breaking at NaN/Inf and large jumps
    const float maxPixelJump = vt.screenHeight * 2.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        if (!points[i].valid || !points[i - 1].valid) continue;
        float dy = std::abs(points[i].y - points[i - 1].y);
        if (dy > maxPixelJump) continue; // likely a discontinuity
        dl->AddLine(ImVec2(points[i - 1].x, points[i - 1].y),
                    ImVec2(points[i].x,     points[i].y),
                    col, thickness);
    }

    dl->PopClipRect();
}

// ---- heat-map for f(x,y) ---------------------------------------------------

void PlotRenderer::drawHeatmap(ImDrawList* dl, const Core::ViewTransform& vt,
                                const Core::ASTNodePtr& ast, float alpha) {
    if (!ast) return;

    const int resX = 200, resY = 150;
    const double xMin = vt.worldXMin(), xMax = vt.worldXMax();
    const double yMin = vt.worldYMin(), yMax = vt.worldYMax();
    const double dx = (xMax - xMin) / resX;
    const double dy = (yMax - yMin) / resY;

    // First pass: evaluate and find range
    std::vector<double> values(resX * resY);
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    Core::Evaluator::Variables vars;

    for (int iy = 0; iy < resY; ++iy) {
        vars["y"] = yMin + (iy + 0.5) * dy;
        for (int ix = 0; ix < resX; ++ix) {
            vars["x"] = xMin + (ix + 0.5) * dx;
            double v = Core::Evaluator::evaluate(ast, vars);
            values[iy * resX + ix] = v;
            if (std::isfinite(v)) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
        }
    }
    if (lo >= hi) { lo = -1.0; hi = 1.0; }

    // Clip
    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    // Second pass: draw rectangles
    for (int iy = 0; iy < resY; ++iy) {
        double wy = yMin + iy * dy;
        for (int ix = 0; ix < resX; ++ix) {
            double wx = xMin + ix * dx;
            double v  = values[iy * resX + ix];

            Core::Vec2 tl = vt.worldToScreen(wx, wy + dy);
            Core::Vec2 br = vt.worldToScreen(wx + dx, wy);
            ImU32 col = heatColor(v, lo, hi, alpha);
            dl->AddRectFilled(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y), col);
        }
    }

    dl->PopClipRect();
}

// ---- cross-section for f(x,y,z) at fixed z ---------------------------------

void PlotRenderer::drawCrossSection(ImDrawList* dl, const Core::ViewTransform& vt,
                                     const Core::ASTNodePtr& ast,
                                     float zSlice, float alpha) {
    if (!ast) return;

    const int resX = 200, resY = 150;
    const double xMin = vt.worldXMin(), xMax = vt.worldXMax();
    const double yMin = vt.worldYMin(), yMax = vt.worldYMax();
    const double dx = (xMax - xMin) / resX;
    const double dy = (yMax - yMin) / resY;

    std::vector<double> values(resX * resY);
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    Core::Evaluator::Variables vars;
    vars["z"] = static_cast<double>(zSlice);

    for (int iy = 0; iy < resY; ++iy) {
        vars["y"] = yMin + (iy + 0.5) * dy;
        for (int ix = 0; ix < resX; ++ix) {
            vars["x"] = xMin + (ix + 0.5) * dx;
            double v = Core::Evaluator::evaluate(ast, vars);
            values[iy * resX + ix] = v;
            if (std::isfinite(v)) { lo = std::min(lo, v); hi = std::max(hi, v); }
        }
    }
    if (lo >= hi) { lo = -1.0; hi = 1.0; }

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    for (int iy = 0; iy < resY; ++iy) {
        double wy = yMin + iy * dy;
        for (int ix = 0; ix < resX; ++ix) {
            double wx = xMin + ix * dx;
            double v  = values[iy * resX + ix];
            Core::Vec2 tl = vt.worldToScreen(wx, wy + dy);
            Core::Vec2 br = vt.worldToScreen(wx + dx, wy);
            dl->AddRectFilled(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y),
                              heatColor(v, lo, hi, alpha));
        }
    }

    dl->PopClipRect();
}

} // namespace XpressFormula::Plotting
