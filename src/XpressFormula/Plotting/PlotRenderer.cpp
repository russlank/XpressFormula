// PlotRenderer.cpp - Rendering implementation for grids, axes, and curves.
#include "PlotRenderer.h"
#include "../Core/Evaluator.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace XpressFormula::Plotting {

// ---- helpers ----------------------------------------------------------------

unsigned int PlotRenderer::colorU32(const float c[4]) {
    return IM_COL32(
        std::clamp(static_cast<int>(c[0] * 255), 0, 255),
        std::clamp(static_cast<int>(c[1] * 255), 0, 255),
        std::clamp(static_cast<int>(c[2] * 255), 0, 255),
        std::clamp(static_cast<int>(c[3] * 255), 0, 255));
}

unsigned int PlotRenderer::heatColor(double value, double lo, double hi,
                                     const float tint[4], float alpha) {
    if (std::isnan(value) || std::isinf(value)) {
        return IM_COL32(30, 30, 35, static_cast<int>(alpha * 255));
    }

    double range = hi - lo;
    if (range == 0.0) {
        range = 1.0;
    }

    const double t = std::clamp((value - lo) / range, 0.0, 1.0);

    // Cool-warm divergent palette blended with the formula tint.
    float baseR;
    float baseG;
    float baseB;
    if (t < 0.5) {
        const float s = static_cast<float>(t * 2.0);
        baseR = 0.15f + 0.30f * s;
        baseG = 0.30f + 0.40f * s;
        baseB = 0.95f;
    } else {
        const float s = static_cast<float>((t - 0.5) * 2.0);
        baseR = 0.95f;
        baseG = 0.70f - 0.45f * s;
        baseB = 0.45f - 0.25f * s;
    }

    const float r = std::clamp(baseR * 0.70f + tint[0] * 0.30f, 0.0f, 1.0f);
    const float g = std::clamp(baseG * 0.70f + tint[1] * 0.30f, 0.0f, 1.0f);
    const float b = std::clamp(baseB * 0.70f + tint[2] * 0.30f, 0.0f, 1.0f);

    return IM_COL32(
        static_cast<int>(r * 255),
        static_cast<int>(g * 255),
        static_cast<int>(b * 255),
        static_cast<int>(alpha * 255));
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

namespace {

void drawViewportDimensionArrows3D(ImDrawList* dl,
                                   const XpressFormula::Core::ViewTransform& vt,
                                   const XpressFormula::Plotting::PlotRenderer::Surface3DOptions& options) {
    const double azimuth = static_cast<double>(options.azimuthDeg) * 3.14159265358979323846 / 180.0;
    const double elevation = static_cast<double>(options.elevationDeg) * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(azimuth);
    const double sinA = std::sin(azimuth);
    const double cosE = std::cos(elevation);
    const double sinE = std::sin(elevation);

    auto projectDirection = [&](double wx, double wy, double wz, ImVec2& outDir) -> bool {
        const double zWorld = wz * options.zScale;
        const double xYaw = cosA * wx - sinA * wy;
        const double yYaw = sinA * wx + cosA * wy;
        const double xProj = xYaw;
        const double yProj = cosE * yYaw - sinE * zWorld;
        const float dx = static_cast<float>(xProj);
        const float dy = static_cast<float>(-yProj); // screen Y grows downward
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-4f) {
            outDir = ImVec2(0.0f, 0.0f);
            return false;
        }
        outDir = ImVec2(dx / len, dy / len);
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
        vt.screenOriginX + 22.0f - minDx,
        vt.screenOriginY + vt.screenHeight - 22.0f - maxDy);

    const float xMaxAllowed = vt.screenOriginX + vt.screenWidth - 26.0f;
    const float yMinAllowed = vt.screenOriginY + 26.0f;
    if (origin.x + maxDx > xMaxAllowed) {
        origin.x -= (origin.x + maxDx - xMaxAllowed);
    }
    if (origin.y + minDy < yMinAllowed) {
        origin.y += (yMinAllowed - (origin.y + minDy));
    }

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
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

    const double azimuth = static_cast<double>(options.azimuthDeg) * 3.14159265358979323846 / 180.0;
    const double elevation = static_cast<double>(options.elevationDeg) * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(azimuth);
    const double sinA = std::sin(azimuth);
    const double cosE = std::cos(elevation);
    const double sinE = std::sin(elevation);
    const double scale = std::max(1e-6, std::min(vt.scaleX, vt.scaleY));
    const Core::Vec2 origin = vt.worldToScreen(0.0, 0.0);

    auto projectPoint = [&](double wx, double wy, double wz) -> Core::Vec2 {
        const double zWorld = wz * options.zScale;
        const double xYaw = cosA * wx - sinA * wy;
        const double yYaw = sinA * wx + cosA * wy;
        const double xProj = xYaw;
        const double yProj = cosE * yYaw - sinE * zWorld;
        return Core::Vec2(
            origin.x + static_cast<float>(xProj * scale),
            origin.y - static_cast<float>(yProj * scale));
    };

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
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

    const double azimuth = static_cast<double>(options.azimuthDeg) * 3.14159265358979323846 / 180.0;
    const double elevation = static_cast<double>(options.elevationDeg) * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(azimuth);
    const double sinA = std::sin(azimuth);
    const double cosE = std::cos(elevation);
    const double sinE = std::sin(elevation);
    const double scale = std::max(1e-6, std::min(vt.scaleX, vt.scaleY));
    const Core::Vec2 originScreen = vt.worldToScreen(0.0, 0.0);

    auto projectPoint = [&](double wx, double wy, double wz) -> Core::Vec2 {
        const double zWorld = wz * options.zScale;
        const double xYaw = cosA * wx - sinA * wy;
        const double yYaw = sinA * wx + cosA * wy;
        const double xProj = xYaw;
        const double yProj = cosE * yYaw - sinE * zWorld;
        return Core::Vec2(
            originScreen.x + static_cast<float>(xProj * scale),
            originScreen.y - static_cast<float>(yProj * scale));
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

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
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
        const float yLo = vt.screenOriginY;
        const float yHi = vt.screenOriginY + vt.screenHeight - 16.0f;
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
    if (!ast) {
        return;
    }

    Core::Evaluator::Variables vars;
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const int numSamples = static_cast<int>(vt.screenWidth) * 2;
    const double dx = (xMax - xMin) / numSamples;
    const ImU32 col = colorU32(color);

    // Clipping rectangle for the plot area
    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    struct Point {
        float x;
        float y;
        bool valid;
    };
    std::vector<Point> points;
    points.reserve(numSamples + 1);

    for (int i = 0; i <= numSamples; ++i) {
        const double wx = xMin + i * dx;
        vars["x"] = wx;
        const double wy = Core::Evaluator::evaluate(ast, vars);

        if (std::isfinite(wy)) {
            Core::Vec2 sp = vt.worldToScreen(wx, wy);
            points.push_back({ sp.x, sp.y, true });
        } else {
            points.push_back({ 0.0f, 0.0f, false });
        }
    }

    // Draw connected segments, breaking at NaN/Inf and large jumps
    const float maxPixelJump = vt.screenHeight * 2.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        if (!points[i].valid || !points[i - 1].valid) {
            continue;
        }
        const float dy = std::abs(points[i].y - points[i - 1].y);
        if (dy > maxPixelJump) {
            continue; // likely a discontinuity
        }
        dl->AddLine(ImVec2(points[i - 1].x, points[i - 1].y),
                    ImVec2(points[i].x, points[i].y),
                    col, thickness);
    }

    dl->PopClipRect();
}

// ---- heat-map for f(x,y) ---------------------------------------------------

void PlotRenderer::drawHeatmap(ImDrawList* dl, const Core::ViewTransform& vt,
                               const Core::ASTNodePtr& ast,
                               const float tint[4], float alpha) {
    if (!ast) {
        return;
    }

    const int resX = 200;
    const int resY = 150;
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();
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
            const double value = Core::Evaluator::evaluate(ast, vars);
            values[iy * resX + ix] = value;
            if (std::isfinite(value)) {
                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
        }
    }
    if (lo >= hi) {
        lo = -1.0;
        hi = 1.0;
    }

    // Clip
    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    // Second pass: draw rectangles
    for (int iy = 0; iy < resY; ++iy) {
        const double wy = yMin + iy * dy;
        for (int ix = 0; ix < resX; ++ix) {
            const double wx = xMin + ix * dx;
            const double value = values[iy * resX + ix];

            Core::Vec2 tl = vt.worldToScreen(wx, wy + dy);
            Core::Vec2 br = vt.worldToScreen(wx + dx, wy);
            const ImU32 color = heatColor(value, lo, hi, tint, alpha);
            dl->AddRectFilled(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y), color);
        }
    }

    dl->PopClipRect();
}

// ---- cross-section for f(x,y,z) at fixed z ---------------------------------

void PlotRenderer::drawCrossSection(ImDrawList* dl, const Core::ViewTransform& vt,
                                    const Core::ASTNodePtr& ast,
                                    float zSlice,
                                    const float tint[4], float alpha) {
    if (!ast) {
        return;
    }

    const int resX = 200;
    const int resY = 150;
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();
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
            const double value = Core::Evaluator::evaluate(ast, vars);
            values[iy * resX + ix] = value;
            if (std::isfinite(value)) {
                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
        }
    }
    if (lo >= hi) {
        lo = -1.0;
        hi = 1.0;
    }

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    for (int iy = 0; iy < resY; ++iy) {
        const double wy = yMin + iy * dy;
        for (int ix = 0; ix < resX; ++ix) {
            const double wx = xMin + ix * dx;
            const double value = values[iy * resX + ix];
            Core::Vec2 tl = vt.worldToScreen(wx, wy + dy);
            Core::Vec2 br = vt.worldToScreen(wx + dx, wy);
            dl->AddRectFilled(ImVec2(tl.x, tl.y), ImVec2(br.x, br.y),
                              heatColor(value, lo, hi, tint, alpha));
        }
    }

    dl->PopClipRect();
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
    struct ClipVertex {
        float x;
        float y;
        double depth;
        double value;
    };

    const int resolution = std::clamp(options.resolution, 12, 96);
    const int nx = resolution;
    const int ny = resolution;
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();
    const double dx = (xMax - xMin) / nx;
    const double dy = (yMax - yMin) / ny;
    std::vector<double> values((nx + 1) * (ny + 1), std::numeric_limits<double>::quiet_NaN());
    double zMin = std::numeric_limits<double>::max();
    double zMax = std::numeric_limits<double>::lowest();

    Core::Evaluator::Variables vars;
    for (int iy = 0; iy <= ny; ++iy) {
        const double wy = yMin + iy * dy;
        vars["y"] = wy;
        for (int ix = 0; ix <= nx; ++ix) {
            const double wx = xMin + ix * dx;
            vars["x"] = wx;
            const double z = Core::Evaluator::evaluate(ast, vars);
            values[iy * (nx + 1) + ix] = z;
            if (std::isfinite(z)) {
                zMin = std::min(zMin, z);
                zMax = std::max(zMax, z);
            }
        }
    }

    if (zMin >= zMax) {
        zMin = -1.0;
        zMax = 1.0;
    }

    const double azimuth = static_cast<double>(options.azimuthDeg) * 3.14159265358979323846 / 180.0;
    const double elevation = static_cast<double>(options.elevationDeg) * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(azimuth);
    const double sinA = std::sin(azimuth);
    const double cosE = std::cos(elevation);
    const double sinE = std::sin(elevation);

    std::vector<Vertex> projected((nx + 1) * (ny + 1));
    int validPointCount = 0;

    for (int iy = 0; iy <= ny; ++iy) {
        const double wy = yMin + iy * dy;
        for (int ix = 0; ix <= nx; ++ix) {
            const double wx = xMin + ix * dx;
            const double z = values[iy * (nx + 1) + ix];
            Vertex& v = projected[iy * (nx + 1) + ix];

            if (!std::isfinite(z)) {
                v.valid = false;
                continue;
            }

            // Keep X/Y in world coordinates so the projected 3D geometry remains anchored to the
            // same origin used by the 2D grid/axes (ViewTransform). Subtracting the current view
            // center here would re-center the mesh every frame and cause visible "swimming".
            const double x = wx;
            const double y = wy;
            const double zWorld = z * options.zScale;

            const double xYaw = cosA * x - sinA * y;
            const double yYaw = sinA * x + cosA * y;

            v.xProj = xYaw;
            v.yProj = cosE * yYaw - sinE * zWorld;
            v.depth = sinE * yYaw + cosE * zWorld;
            v.value = z;
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
    const double scale = std::max(1e-6, std::min(vt.scaleX, vt.scaleY));
    const float sxCenter = vt.worldToScreen(0.0, 0.0).x;
    const float syCenter = vt.worldToScreen(0.0, 0.0).y;

    std::vector<ScreenVertex> screenVerts((nx + 1) * (ny + 1));
    for (size_t i = 0; i < projected.size(); ++i) {
        const Vertex& v = projected[i];
        ScreenVertex& s = screenVerts[i];
        if (!v.valid) {
            s.valid = false;
            continue;
        }
        s.x = sxCenter + static_cast<float>(v.xProj * scale);
        s.y = syCenter - static_cast<float>(v.yProj * scale);
        s.depth = v.depth;
        s.value = v.value;
        s.valid = true;
    }

    const bool usePlaneSplitPass = (options.planePass != SurfacePlanePass3D::All);
    const double planeZ = options.gridPlaneZ;
    std::vector<Face> faces;
    faces.reserve(static_cast<size_t>(nx * ny * (usePlaneSplitPass ? 4 : 2)));

    auto pushFaceRaw = [&](const ClipVertex& a,
                           const ClipVertex& b,
                           const ClipVertex& c) {
        faces.push_back({
            ImVec2(a.x, a.y),
            ImVec2(b.x, b.y),
            ImVec2(c.x, c.y),
            (a.depth + b.depth + c.depth) / 3.0,
            (a.value + b.value + c.value) / 3.0
        });
    };

    auto clipIntersect = [&](const ClipVertex& a, const ClipVertex& b) -> ClipVertex {
        const double denom = (b.value - a.value);
        double t = 0.0;
        if (std::abs(denom) > 1e-12) {
            t = (planeZ - a.value) / denom;
        }
        t = std::clamp(t, 0.0, 1.0);
        return ClipVertex{
            a.x + static_cast<float>((b.x - a.x) * t),
            a.y + static_cast<float>((b.y - a.y) * t),
            a.depth + (b.depth - a.depth) * t,
            a.value + (b.value - a.value) * t
        };
    };

    auto pushFace = [&](const ScreenVertex& a,
                        const ScreenVertex& b,
                        const ScreenVertex& c) {
        if (!a.valid || !b.valid || !c.valid) {
            return;
        }

        if (!usePlaneSplitPass) {
            pushFaceRaw(
                ClipVertex{ a.x, a.y, a.depth, a.value },
                ClipVertex{ b.x, b.y, b.depth, b.value },
                ClipVertex{ c.x, c.y, c.depth, c.value });
            return;
        }

        const auto isInside = [&](const ClipVertex& v) {
            if (options.planePass == SurfacePlanePass3D::BelowGridPlane) {
                return v.value <= planeZ;
            }
            return v.value >= planeZ;
        };

        ClipVertex input[4] = {
            { a.x, a.y, a.depth, a.value },
            { b.x, b.y, b.depth, b.value },
            { c.x, c.y, c.depth, c.value },
            {}
        };
        int inputCount = 3;
        ClipVertex output[8] = {};
        int outputCount = 0;

        for (int i = 0; i < inputCount; ++i) {
            const ClipVertex& curr = input[i];
            const ClipVertex& prev = input[(i + inputCount - 1) % inputCount];
            const bool currInside = isInside(curr);
            const bool prevInside = isInside(prev);

            if (currInside) {
                if (!prevInside) {
                    output[outputCount++] = clipIntersect(prev, curr);
                }
                output[outputCount++] = curr;
            } else if (prevInside) {
                output[outputCount++] = clipIntersect(prev, curr);
            }
        }

        if (outputCount < 3) {
            return;
        }

        for (int i = 1; i + 1 < outputCount; ++i) {
            pushFaceRaw(output[0], output[i], output[i + 1]);
        }
    };

    for (int iy = 0; iy < ny; ++iy) {
        for (int ix = 0; ix < nx; ++ix) {
            const ScreenVertex& v00 = screenVerts[iy * (nx + 1) + ix];
            const ScreenVertex& v10 = screenVerts[iy * (nx + 1) + (ix + 1)];
            const ScreenVertex& v01 = screenVerts[(iy + 1) * (nx + 1) + ix];
            const ScreenVertex& v11 = screenVerts[(iy + 1) * (nx + 1) + (ix + 1)];

            pushFace(v00, v10, v11);
            pushFace(v00, v11, v01);
        }
    }

    std::sort(faces.begin(), faces.end(),
              [](const Face& a, const Face& b) { return a.depth < b.depth; });

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
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

        const ImU32 edge = IM_COL32(
            static_cast<int>(std::clamp(r * 0.6f, 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(g * 0.6f, 0.0f, 1.0f) * 255),
            static_cast<int>(std::clamp(b * 0.6f, 0.0f, 1.0f) * 255),
            200);

        dl->AddTriangleFilled(face.p0, face.p1, face.p2, fill);
        if (options.wireThickness > 0.0f) {
            dl->AddLine(face.p0, face.p1, edge, options.wireThickness);
            dl->AddLine(face.p1, face.p2, edge, options.wireThickness);
            dl->AddLine(face.p2, face.p0, edge, options.wireThickness);
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
            const double x = wx;
            const double y = wy;
            const double zWorld = wz * options.zScale;

            const double xYaw = cosA * x - sinA * y;
            const double yYaw = sinA * x + cosA * y;

            const double xProj = xYaw;
            const double yProj = cosE * yYaw - sinE * zWorld;
            const double depth = sinE * yYaw + cosE * zWorld;

            const float sx = sxCenter + static_cast<float>(xProj * scale);
            const float sy = syCenter - static_cast<float>(yProj * scale);
            return EnvelopePoint{ ImVec2(sx, sy), depth };
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
            const float lineThickness = std::clamp(options.envelopeThickness, 0.2f, 4.0f);

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
    if (options.showDimensionArrows) {
        drawViewportDimensionArrows3D(dl, vt, options);
    }
}

// ---- implicit 3D surface F(x,y,z)=0 ----------------------------------------

void PlotRenderer::drawImplicitSurface3D(ImDrawList* dl, const Core::ViewTransform& vt,
                                         const Core::ASTNodePtr& ast,
                                         const float color[4],
                                         const Surface3DOptions& options) {
    if (!ast) {
        return;
    }

    struct Point3 {
        double x;
        double y;
        double z;
    };
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
    };
    struct ClipProjectedVertex {
        double xProj;
        double yProj;
        double depth;
        double wz;
    };
    // Stored in world coordinates so we can reuse the extracted mesh across camera changes
    // (azimuth/elevation/zScale/opacity/wireframe) and only re-project when needed.
    struct WorldFace {
        Point3 p0;
        Point3 p1;
        Point3 p2;
    };
    // Cache invalidation is intentionally tied to AST identity + sampling domain + grid size.
    // Camera and visual styling are excluded because they only affect projection/shading.
    struct MeshCacheKey {
        const void* astPtr;
        int gridRes;
        double xMin;
        double xMax;
        double yMin;
        double yMax;
        double zCenter;
        double zMinDomain;
        double zMaxDomain;
    };
    struct MeshCacheData {
        MeshCacheKey key{};
        std::vector<WorldFace> faces;
        // Bounds of the extracted surface (not the whole sampling box). Used for envelope box
        // and to stabilize z-based coloring without rescanning all triangles every frame.
        double surfXMin = 0.0;
        double surfXMax = 0.0;
        double surfYMin = 0.0;
        double surfYMax = 0.0;
        double surfZMin = 0.0;
        double surfZMax = 0.0;
        bool valid = false;
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
    const int gridRes = std::clamp(requestedImplicitRes, 16, 96);
    const int nx = gridRes;
    const int ny = gridRes;
    const int nz = gridRes;

    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();
    const double dx = std::max(1e-6, (xMax - xMin) / nx);
    const double dy = std::max(1e-6, (yMax - yMin) / ny);

    // The implicit surface is sampled only inside the current view domain.
    // This means a valid shape (e.g. a sphere) can appear "cut open" if the current x/y range
    // clips it; the mesh is built only for the sampled box.
    const double xySpan = std::max(std::max(1e-6, xMax - xMin), std::max(1e-6, yMax - yMin));
    const double zCenter = static_cast<double>(options.implicitZCenter);
    const double zHalfSpan = std::max(1.0, xySpan * 0.5);
    const double zMinDomain = zCenter - zHalfSpan;
    const double zMaxDomain = zCenter + zHalfSpan;
    const double dz = std::max(1e-6, (zMaxDomain - zMinDomain) / nz);

    auto gridIndex = [&](int ix, int iy, int iz) -> size_t {
        return static_cast<size_t>(((iz * (ny + 1)) + iy) * (nx + 1) + ix);
    };
    // Rebuild the implicit mesh only when the sampled field/domain changes.
    const MeshCacheKey cacheKey{
        ast.get(), gridRes,
        xMin, xMax, yMin, yMax, zCenter, zMinDomain, zMaxDomain
    };
    static MeshCacheData s_meshCache;
    const bool cacheHit = s_meshCache.valid &&
        s_meshCache.key.astPtr == cacheKey.astPtr &&
        s_meshCache.key.gridRes == cacheKey.gridRes &&
        s_meshCache.key.xMin == cacheKey.xMin &&
        s_meshCache.key.xMax == cacheKey.xMax &&
        s_meshCache.key.yMin == cacheKey.yMin &&
        s_meshCache.key.yMax == cacheKey.yMax &&
        s_meshCache.key.zCenter == cacheKey.zCenter &&
        s_meshCache.key.zMinDomain == cacheKey.zMinDomain &&
        s_meshCache.key.zMaxDomain == cacheKey.zMaxDomain;

    const double azimuth = static_cast<double>(options.azimuthDeg) * 3.14159265358979323846 / 180.0;
    const double elevation = static_cast<double>(options.elevationDeg) * 3.14159265358979323846 / 180.0;
    const double cosA = std::cos(azimuth);
    const double sinA = std::sin(azimuth);
    const double cosE = std::cos(elevation);
    const double sinE = std::sin(elevation);

    // Shared 3D->2D projection used for mesh triangles and the optional envelope/arrows.
    // The projection is world-origin anchored (no per-frame centering by sampled-box center)
    // so implicit meshes remain aligned with the 2D grid/axes during panning and zooming.
    auto projectPoint = [&](double wx, double wy, double wz,
                            double& outXProj, double& outYProj, double& outDepth) {
        // This helper projects a world-space point using the current 3D camera but keeps the same
        // world origin reference as the 2D axes/grid. It is shared by triangles, envelope edges,
        // and XYZ dimension arrows so those overlays remain aligned.
        const double x = wx;
        const double y = wy;
        const double zWorld = wz * options.zScale;

        const double xYaw = cosA * x - sinA * y;
        const double yYaw = sinA * x + cosA * y;

        outXProj = xYaw;
        outYProj = cosE * yYaw - sinE * zWorld;
        outDepth = sinE * yYaw + cosE * zWorld;
    };

    // Standard cube corner indexing (voxel cell corners) and edge topology.
    static const int kCubeOffsets[8][3] = {
        { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 },
        { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 }
    };
    static const int kCubeEdges[12][2] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };
    struct CellVertex {
        Point3 p;
        bool active;
    };

    std::vector<WorldFace> worldFaces;
    std::vector<ProjectedFace> projectedFaces;
    worldFaces.reserve(static_cast<size_t>(nx) * static_cast<size_t>(ny) *
                       static_cast<size_t>(nz) * 2u);
    const std::vector<WorldFace>* meshFaces = nullptr;

    double surfXMin = std::numeric_limits<double>::max();
    double surfXMax = std::numeric_limits<double>::lowest();
    double surfYMin = std::numeric_limits<double>::max();
    double surfYMax = std::numeric_limits<double>::lowest();
    double surfZMin = std::numeric_limits<double>::max();
    double surfZMax = std::numeric_limits<double>::lowest();

    // Zero-crossing test for an edge endpoint pair. `true` means the isosurface may cross
    // the segment, including the degenerate case where one endpoint is exactly zero.
    const auto signsCrossZero = [](double a, double b) -> bool {
        if (!std::isfinite(a) || !std::isfinite(b)) {
            return false;
        }
        if (a == 0.0 || b == 0.0) {
            return true;
        }
        return (a < 0.0 && b > 0.0) || (a > 0.0 && b < 0.0);
    };
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
    // Linear interpolation of the F=0 crossing along a grid edge. This is the basic primitive
    // used by both the cell vertex generation (surface nets) and surface location estimation.
    const auto interpolateIso = [&](const Point3& a, double va,
                                    const Point3& b, double vb,
                                    Point3& out) -> bool {
        if (!signsCrossZero(va, vb)) {
            return false;
        }
        double t = 0.5;
        const double denom = va - vb;
        if (std::isfinite(denom) && std::abs(denom) > 1e-12) {
            t = std::clamp(va / denom, 0.0, 1.0);
        }
        out = Point3{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
        return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
    };

    const double sampleStepWorld = std::max({ dx, dy, dz });
    const double areaTolSq = std::max(1e-16, sampleStepWorld * sampleStepWorld * 1e-10);

    // Store triangles in world space only. Projection and shading are deferred so cached meshes
    // can be reused when the camera changes.
    auto pushWorldTriangle = [&](const Point3& a, const Point3& b, const Point3& c) {
        const Point3 ab = sub3(b, a);
        const Point3 ac = sub3(c, a);
        const Point3 normal = cross3(ab, ac);
        const double nLenSq = lenSq3(normal);
        if (!(nLenSq > areaTolSq) || !std::isfinite(nLenSq)) {
            return;
        }

        worldFaces.push_back(WorldFace{ a, b, c });
        const Point3* verts[3] = { &a, &b, &c };
        for (const Point3* v : verts) {
            surfXMin = std::min(surfXMin, v->x);
            surfXMax = std::max(surfXMax, v->x);
            surfYMin = std::min(surfYMin, v->y);
            surfYMax = std::max(surfYMax, v->y);
            surfZMin = std::min(surfZMin, v->z);
            surfZMax = std::max(surfZMax, v->z);
        }
    };

    if (cacheHit) {
        // Fast path: reuse previously extracted mesh and its bounds. This avoids re-evaluating
        // the scalar field on the 3D grid and re-running the surface extraction.
        meshFaces = &s_meshCache.faces;
        surfXMin = s_meshCache.surfXMin;
        surfXMax = s_meshCache.surfXMax;
        surfYMin = s_meshCache.surfYMin;
        surfYMax = s_meshCache.surfYMax;
        surfZMin = s_meshCache.surfZMin;
        surfZMax = s_meshCache.surfZMax;
    } else {
        // Slow path: sample F(x,y,z) over the current 3D grid. This is the dominant cost and is
        // intentionally skipped on cache hits (camera/style changes only).
        std::vector<double> values(static_cast<size_t>(nx + 1) * (ny + 1) * (nz + 1),
                                   std::numeric_limits<double>::quiet_NaN());
        Core::Evaluator::Variables vars;
        for (int iz = 0; iz <= nz; ++iz) {
            vars["z"] = zMinDomain + iz * dz;
            for (int iy = 0; iy <= ny; ++iy) {
                vars["y"] = yMin + iy * dy;
                for (int ix = 0; ix <= nx; ++ix) {
                    vars["x"] = xMin + ix * dx;
                    values[gridIndex(ix, iy, iz)] = Core::Evaluator::evaluate(ast, vars);
                }
            }
        }

        std::vector<CellVertex> cellVertices(static_cast<size_t>(nx) * ny * nz,
                                             CellVertex{ Point3{ 0.0, 0.0, 0.0 }, false });
        auto cellIndex = [&](int ix, int iy, int iz) -> size_t {
            return static_cast<size_t>(((iz * ny) + iy) * nx + ix);
        };
        auto emitQuad = [&](const Point3& p00, const Point3& p10,
                            const Point3& p11, const Point3& p01) {
            // Choose the shorter diagonal to reduce sliver triangles on curved surfaces.
            const double d02 = lenSq3(sub3(p11, p00));
            const double d13 = lenSq3(sub3(p01, p10));
            if (d02 <= d13) {
                pushWorldTriangle(p00, p10, p11);
                pushWorldTriangle(p00, p11, p01);
            } else {
                pushWorldTriangle(p00, p10, p01);
                pushWorldTriangle(p10, p11, p01);
            }
        };
        // Around a sign-changing grid edge there are four neighboring cells. If all four cells
        // have a surface-nets vertex, they form one quad on the extracted surface.
        auto tryEmitQuadFromCells = [&](int ax, int ay, int az,
                                        int bx, int by, int bz,
                                        int cx, int cy, int cz,
                                        int dxCell, int dyCell, int dzCell) {
            if (ax < 0 || ax >= nx || ay < 0 || ay >= ny || az < 0 || az >= nz ||
                bx < 0 || bx >= nx || by < 0 || by >= ny || bz < 0 || bz >= nz ||
                cx < 0 || cx >= nx || cy < 0 || cy >= ny || cz < 0 || cz >= nz ||
                dxCell < 0 || dxCell >= nx || dyCell < 0 || dyCell >= ny || dzCell < 0 || dzCell >= nz) {
                return;
            }

            const CellVertex& ca = cellVertices[cellIndex(ax, ay, az)];
            const CellVertex& cb = cellVertices[cellIndex(bx, by, bz)];
            const CellVertex& cc = cellVertices[cellIndex(cx, cy, cz)];
            const CellVertex& cd = cellVertices[cellIndex(dxCell, dyCell, dzCell)];
            if (!ca.active || !cb.active || !cc.active || !cd.active) {
                return;
            }

            emitQuad(ca.p, cb.p, cc.p, cd.p);
        };

        // Pass 1 (surface nets):
        // For each voxel cell that contains a sign change, compute one representative vertex
        // by averaging all F=0 edge intersections in that cell. This creates a more uniform
        // vertex distribution than tetrahedra-based triangulation for smooth shapes.
        for (int iz = 0; iz < nz; ++iz) {
            for (int iy = 0; iy < ny; ++iy) {
                for (int ix = 0; ix < nx; ++ix) {
                    Point3 corners[8];
                    double cornerValues[8];
                    bool hasFinite = false;
                    double cellLo = std::numeric_limits<double>::max();
                    double cellHi = std::numeric_limits<double>::lowest();

                    for (int c = 0; c < 8; ++c) {
                        const int gx = ix + kCubeOffsets[c][0];
                        const int gy = iy + kCubeOffsets[c][1];
                        const int gz = iz + kCubeOffsets[c][2];
                        corners[c] = Point3{
                            xMin + gx * dx,
                            yMin + gy * dy,
                            zMinDomain + gz * dz
                        };
                        const double v = values[gridIndex(gx, gy, gz)];
                        cornerValues[c] = v;
                        if (std::isfinite(v)) {
                            hasFinite = true;
                            cellLo = std::min(cellLo, v);
                            cellHi = std::max(cellHi, v);
                        }
                    }

                    if (!hasFinite || cellLo > 0.0 || cellHi < 0.0) {
                        continue;
                    }

                    Point3 sum{ 0.0, 0.0, 0.0 };
                    int intersectionCount = 0;
                    for (const auto& edge : kCubeEdges) {
                        Point3 ip{};
                        if (!interpolateIso(corners[edge[0]], cornerValues[edge[0]],
                                            corners[edge[1]], cornerValues[edge[1]], ip)) {
                            continue;
                        }
                        sum.x += ip.x;
                        sum.y += ip.y;
                        sum.z += ip.z;
                        ++intersectionCount;
                    }

                    if (intersectionCount < 3) {
                        continue;
                    }

                    CellVertex& cv = cellVertices[cellIndex(ix, iy, iz)];
                    const double inv = 1.0 / static_cast<double>(intersectionCount);
                    cv.p = Point3{ sum.x * inv, sum.y * inv, sum.z * inv };
                    cv.active = true;
                }
            }
        }

        // Pass 2:
        // Stitch the per-cell vertices into quads around sign-changing grid edges, then split
        // each quad into two triangles. This yields far fewer triangles than tetrahedra output.
        // X-directed grid edges => quads in the YZ neighborhood.
        for (int iz = 1; iz < nz; ++iz) {
            for (int iy = 1; iy < ny; ++iy) {
                for (int ix = 0; ix < nx; ++ix) {
                    const double va = values[gridIndex(ix, iy, iz)];
                    const double vb = values[gridIndex(ix + 1, iy, iz)];
                    if (!signsCrossZero(va, vb)) {
                        continue;
                    }
                    tryEmitQuadFromCells(ix, iy - 1, iz - 1,
                                         ix, iy,     iz - 1,
                                         ix, iy,     iz,
                                         ix, iy - 1, iz);
                }
            }
        }

        // Y-directed grid edges => quads in the XZ neighborhood.
        for (int iz = 1; iz < nz; ++iz) {
            for (int iy = 0; iy < ny; ++iy) {
                for (int ix = 1; ix < nx; ++ix) {
                    const double va = values[gridIndex(ix, iy, iz)];
                    const double vb = values[gridIndex(ix, iy + 1, iz)];
                    if (!signsCrossZero(va, vb)) {
                        continue;
                    }
                    tryEmitQuadFromCells(ix - 1, iy, iz - 1,
                                         ix,     iy, iz - 1,
                                         ix,     iy, iz,
                                         ix - 1, iy, iz);
                }
            }
        }

        // Z-directed grid edges => quads in the XY neighborhood.
        for (int iz = 0; iz < nz; ++iz) {
            for (int iy = 1; iy < ny; ++iy) {
                for (int ix = 1; ix < nx; ++ix) {
                    const double va = values[gridIndex(ix, iy, iz)];
                    const double vb = values[gridIndex(ix, iy, iz + 1)];
                    if (!signsCrossZero(va, vb)) {
                        continue;
                    }
                    tryEmitQuadFromCells(ix - 1, iy - 1, iz,
                                         ix,     iy - 1, iz,
                                         ix,     iy,     iz,
                                         ix - 1, iy,     iz);
                }
            }
        }

        // Publish cache only after a full successful extraction.
        s_meshCache.key = cacheKey;
        s_meshCache.faces = worldFaces;
        s_meshCache.surfXMin = surfXMin;
        s_meshCache.surfXMax = surfXMax;
        s_meshCache.surfYMin = surfYMin;
        s_meshCache.surfYMax = surfYMax;
        s_meshCache.surfZMin = surfZMin;
        s_meshCache.surfZMax = surfZMax;
        s_meshCache.valid = true;
        meshFaces = &s_meshCache.faces;
    }

    if (meshFaces == nullptr) {
        meshFaces = &worldFaces;
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
        projectPoint(wf.p0.x, wf.p0.y, wf.p0.z, face.v0.xProj, face.v0.yProj, face.v0.depth);
        projectPoint(wf.p1.x, wf.p1.y, wf.p1.z, face.v1.xProj, face.v1.yProj, face.v1.depth);
        projectPoint(wf.p2.x, wf.p2.y, wf.p2.z, face.v2.xProj, face.v2.yProj, face.v2.depth);
        face.depth = (face.v0.depth + face.v1.depth + face.v2.depth) / 3.0;
        face.zAvg = (wf.p0.z + wf.p1.z + wf.p2.z) / 3.0;

        const double nxYaw = cosA * normal.x - sinA * normal.y;
        const double nyYaw = sinA * normal.x + cosA * normal.y;
        const double nViewX = nxYaw;
        const double nViewY = cosE * nyYaw - sinE * normal.z;
        const double nViewZ = sinE * nyYaw + cosE * normal.z;
        const double nvLen = std::sqrt(nViewX * nViewX + nViewY * nViewY + nViewZ * nViewZ);
        const double invNvLen = (nvLen > 1e-12) ? (1.0 / nvLen) : 1.0;
        const double ndotl = (nViewX * lightX + nViewY * lightY + nViewZ * lightZ) * invNvLen;
        face.shade = static_cast<float>(std::clamp(0.28 + 0.72 * std::abs(ndotl), 0.18, 1.0));

        projectedFaces.push_back(face);

    }

    if (projectedFaces.empty()) {
        return;
    }

    // Anchor implicit 3D projection to world origin for stable alignment with the 2D grid/axes.
    const double scale = std::max(1e-6, std::min(vt.scaleX, vt.scaleY));
    const float sxCenter = vt.worldToScreen(0.0, 0.0).x;
    const float syCenter = vt.worldToScreen(0.0, 0.0).y;

    const bool usePlaneSplitPass = (options.planePass != SurfacePlanePass3D::All);
    const double planeZ = options.gridPlaneZ;
    std::vector<ScreenFace> screenFaces;
    screenFaces.reserve(projectedFaces.size() * (usePlaneSplitPass ? 2u : 1u));

    auto pushScreenFaceRaw = [&](const ClipProjectedVertex& a,
                                 const ClipProjectedVertex& b,
                                 const ClipProjectedVertex& c,
                                 float shade) {
        screenFaces.push_back({
            ImVec2(sxCenter + static_cast<float>(a.xProj * scale),
                   syCenter - static_cast<float>(a.yProj * scale)),
            ImVec2(sxCenter + static_cast<float>(b.xProj * scale),
                   syCenter - static_cast<float>(b.yProj * scale)),
            ImVec2(sxCenter + static_cast<float>(c.xProj * scale),
                   syCenter - static_cast<float>(c.yProj * scale)),
            (a.depth + b.depth + c.depth) / 3.0,
            (a.wz + b.wz + c.wz) / 3.0,
            shade
        });
    };

    auto clipIntersect = [&](const ClipProjectedVertex& a,
                             const ClipProjectedVertex& b) -> ClipProjectedVertex {
        const double denom = (b.wz - a.wz);
        double t = 0.0;
        if (std::abs(denom) > 1e-12) {
            t = (planeZ - a.wz) / denom;
        }
        t = std::clamp(t, 0.0, 1.0);
        return ClipProjectedVertex{
            a.xProj + (b.xProj - a.xProj) * t,
            a.yProj + (b.yProj - a.yProj) * t,
            a.depth + (b.depth - a.depth) * t,
            a.wz + (b.wz - a.wz) * t
        };
    };

    auto pushProjectedFace = [&](const ProjectedFace& f) {
        if (!usePlaneSplitPass) {
            pushScreenFaceRaw(
                ClipProjectedVertex{ f.v0.xProj, f.v0.yProj, f.v0.depth, f.v0.wz },
                ClipProjectedVertex{ f.v1.xProj, f.v1.yProj, f.v1.depth, f.v1.wz },
                ClipProjectedVertex{ f.v2.xProj, f.v2.yProj, f.v2.depth, f.v2.wz },
                f.shade);
            return;
        }

        const auto isInside = [&](const ClipProjectedVertex& v) {
            if (options.planePass == SurfacePlanePass3D::BelowGridPlane) {
                return v.wz <= planeZ;
            }
            return v.wz >= planeZ;
        };

        ClipProjectedVertex input[4] = {
            { f.v0.xProj, f.v0.yProj, f.v0.depth, f.v0.wz },
            { f.v1.xProj, f.v1.yProj, f.v1.depth, f.v1.wz },
            { f.v2.xProj, f.v2.yProj, f.v2.depth, f.v2.wz },
            {}
        };
        int inputCount = 3;
        ClipProjectedVertex output[8] = {};
        int outputCount = 0;

        for (int i = 0; i < inputCount; ++i) {
            const ClipProjectedVertex& curr = input[i];
            const ClipProjectedVertex& prev = input[(i + inputCount - 1) % inputCount];
            const bool currInside = isInside(curr);
            const bool prevInside = isInside(prev);

            if (currInside) {
                if (!prevInside) {
                    output[outputCount++] = clipIntersect(prev, curr);
                }
                output[outputCount++] = curr;
            } else if (prevInside) {
                output[outputCount++] = clipIntersect(prev, curr);
            }
        }

        if (outputCount < 3) {
            return;
        }

        for (int i = 1; i + 1 < outputCount; ++i) {
            pushScreenFaceRaw(output[0], output[i], output[i + 1], f.shade);
        }
    };

    for (const ProjectedFace& f : projectedFaces) {
        pushProjectedFace(f);
    }

    // ImGui draw lists have no depth buffer, so we painter-sort triangles back-to-front.
    std::sort(screenFaces.begin(), screenFaces.end(),
              [](const ScreenFace& a, const ScreenFace& b) {
                  return a.depth < b.depth;
              });

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    if (!(surfZMin < surfZMax)) {
        surfZMin = zMinDomain;
        surfZMax = zMaxDomain;
    }
    const double zRange = std::max(1e-6, surfZMax - surfZMin);
    const float baseOpacity = std::clamp(options.opacity, 0.12f, 1.0f);
    const float edgeThickness = std::clamp(options.wireThickness, 0.0f, 4.0f);

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
            200);

        dl->AddTriangleFilled(face.p0, face.p1, face.p2, fill);
        if (edgeThickness > 0.0f) {
            dl->AddLine(face.p0, face.p1, edge, edgeThickness);
            dl->AddLine(face.p1, face.p2, edge, edgeThickness);
            dl->AddLine(face.p2, face.p0, edge, edgeThickness);
        }
    }

    if (options.showEnvelope) {
        // Envelope/arrows are drawn from extracted surface bounds (not full sample box) to give
        // a tighter visual wrapper around the actual shape.
        if (!(surfXMin < surfXMax)) { surfXMin = xMin; surfXMax = xMax; }
        if (!(surfYMin < surfYMax)) { surfYMin = yMin; surfYMax = yMax; }
        if (!(surfZMin < surfZMax)) { surfZMin = zMinDomain; surfZMax = zMaxDomain; }

        auto projectEnvelopePoint = [&](double wx, double wy, double wz) {
            double xp, yp, depth;
            projectPoint(wx, wy, wz, xp, yp, depth);
            const float sx = sxCenter + static_cast<float>(xp * scale);
            const float sy = syCenter - static_cast<float>(yp * scale);
            return EnvelopePoint{ ImVec2(sx, sy), depth };
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
            const float lineThickness = std::clamp(options.envelopeThickness, 0.2f, 4.0f);

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
    if (options.showDimensionArrows) {
        drawViewportDimensionArrows3D(dl, vt, options);
    }
}

// ---- implicit contour F(x,y)=0 ---------------------------------------------

void PlotRenderer::drawImplicitContour2D(ImDrawList* dl, const Core::ViewTransform& vt,
                                         const Core::ASTNodePtr& ast,
                                         const float color[4], float thickness) {
    if (!ast) {
        return;
    }

    const int resX = 180;
    const int resY = 140;
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();
    const double dx = (xMax - xMin) / resX;
    const double dy = (yMax - yMin) / resY;

    auto indexOf = [&](int ix, int iy) {
        return iy * (resX + 1) + ix;
    };

    std::vector<double> values((resX + 1) * (resY + 1), std::numeric_limits<double>::quiet_NaN());
    Core::Evaluator::Variables vars;

    for (int iy = 0; iy <= resY; ++iy) {
        vars["y"] = yMin + iy * dy;
        for (int ix = 0; ix <= resX; ++ix) {
            vars["x"] = xMin + ix * dx;
            values[indexOf(ix, iy)] = Core::Evaluator::evaluate(ast, vars);
        }
    }

    // Interpolate along a cell edge to find the zero-crossing between two sample values.
    // Returns true (and fills 'out' with the screen-space point) when the edge crosses zero.
    auto interpolate = [&](double x0, double y0, double v0,
                           double x1, double y1, double v1,
                           Core::Vec2& out) -> bool {
        if (!std::isfinite(v0) || !std::isfinite(v1)) {
            return false;
        }
        // Both values on the same side of zero — no crossing on this edge.
        if ((v0 > 0.0 && v1 > 0.0) || (v0 < 0.0 && v1 < 0.0)) {
            return false;
        }

        double t = 0.5;
        const double denom = v0 - v1;
        if (std::abs(denom) > 1e-12) {
            t = std::clamp(v0 / denom, 0.0, 1.0);
        } else if (v0 == 0.0 && v1 == 0.0) {
            // Both endpoints lie exactly on the contour — use the midpoint so the
            // edge still contributes a visible line segment.
            t = 0.5;
        }

        const double wx = x0 + (x1 - x0) * t;
        const double wy = y0 + (y1 - y0) * t;
        out = vt.worldToScreen(wx, wy);
        return true;
    };

    const ImU32 contourColor = colorU32(color);

    ImVec2 clipMin(vt.screenOriginX, vt.screenOriginY);
    ImVec2 clipMax(vt.screenOriginX + vt.screenWidth,
                   vt.screenOriginY + vt.screenHeight);
    dl->PushClipRect(clipMin, clipMax, true);

    for (int iy = 0; iy < resY; ++iy) {
        const double y0 = yMin + iy * dy;
        const double y1 = y0 + dy;
        for (int ix = 0; ix < resX; ++ix) {
            const double x0 = xMin + ix * dx;
            const double x1 = x0 + dx;

            const double v0 = values[indexOf(ix, iy)];
            const double v1 = values[indexOf(ix + 1, iy)];
            const double v2 = values[indexOf(ix + 1, iy + 1)];
            const double v3 = values[indexOf(ix, iy + 1)];

            Core::Vec2 intersections[4];
            int count = 0;

            if (interpolate(x0, y0, v0, x1, y0, v1, intersections[count])) {
                ++count;
            }
            if (interpolate(x1, y0, v1, x1, y1, v2, intersections[count])) {
                ++count;
            }
            if (interpolate(x1, y1, v2, x0, y1, v3, intersections[count])) {
                ++count;
            }
            if (interpolate(x0, y1, v3, x0, y0, v0, intersections[count])) {
                ++count;
            }

            if (count == 2) {
                dl->AddLine(ImVec2(intersections[0].x, intersections[0].y),
                            ImVec2(intersections[1].x, intersections[1].y),
                            contourColor, thickness);
            } else if (count == 4) {
                dl->AddLine(ImVec2(intersections[0].x, intersections[0].y),
                            ImVec2(intersections[1].x, intersections[1].y),
                            contourColor, thickness);
                dl->AddLine(ImVec2(intersections[2].x, intersections[2].y),
                            ImVec2(intersections[3].x, intersections[3].y),
                            contourColor, thickness);
            }
        }
    }

    dl->PopClipRect();
}

} // namespace XpressFormula::Plotting
