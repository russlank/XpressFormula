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
        static_cast<int>(c[0] * 255),
        static_cast<int>(c[1] * 255),
        static_cast<int>(c[2] * 255),
        static_cast<int>(c[3] * 255));
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

    const int resolution = std::clamp(options.resolution, 12, 96);
    const int nx = resolution;
    const int ny = resolution;
    const double xMin = vt.worldXMin();
    const double xMax = vt.worldXMax();
    const double yMin = vt.worldYMin();
    const double yMax = vt.worldYMax();
    const double dx = (xMax - xMin) / nx;
    const double dy = (yMax - yMin) / ny;
    const double xCenter = (xMin + xMax) * 0.5;
    const double yCenter = (yMin + yMax) * 0.5;

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
    double pxMin = std::numeric_limits<double>::max();
    double pxMax = std::numeric_limits<double>::lowest();
    double pyMin = std::numeric_limits<double>::max();
    double pyMax = std::numeric_limits<double>::lowest();
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

            const double x = wx - xCenter;
            const double y = wy - yCenter;
            const double zWorld = z * options.zScale;

            const double xYaw = cosA * x - sinA * y;
            const double yYaw = sinA * x + cosA * y;

            v.xProj = xYaw;
            v.yProj = cosE * yYaw - sinE * zWorld;
            v.depth = sinE * yYaw + cosE * zWorld;
            v.value = z;
            v.valid = true;
            validPointCount++;

            pxMin = std::min(pxMin, v.xProj);
            pxMax = std::max(pxMax, v.xProj);
            pyMin = std::min(pyMin, v.yProj);
            pyMax = std::max(pyMax, v.yProj);
        }
    }

    if (validPointCount == 0) {
        return;
    }

    const double spanX = std::max(1e-6, pxMax - pxMin);
    const double spanY = std::max(1e-6, pyMax - pyMin);
    const double margin = 24.0;
    const double plotW = std::max(40.0, static_cast<double>(vt.screenWidth) - margin * 2.0);
    const double plotH = std::max(40.0, static_cast<double>(vt.screenHeight) - margin * 2.0);
    const double scale = std::min(plotW / spanX, plotH / spanY);

    const double pxCenter = (pxMin + pxMax) * 0.5;
    const double pyCenter = (pyMin + pyMax) * 0.5;
    const float sxCenter = vt.screenOriginX + vt.screenWidth * 0.5f;
    const float syCenter = vt.screenOriginY + vt.screenHeight * 0.5f;

    std::vector<ScreenVertex> screenVerts((nx + 1) * (ny + 1));
    for (size_t i = 0; i < projected.size(); ++i) {
        const Vertex& v = projected[i];
        ScreenVertex& s = screenVerts[i];
        if (!v.valid) {
            s.valid = false;
            continue;
        }
        s.x = sxCenter + static_cast<float>((v.xProj - pxCenter) * scale);
        s.y = syCenter - static_cast<float>((v.yProj - pyCenter) * scale);
        s.depth = v.depth;
        s.value = v.value;
        s.valid = true;
    }

    std::vector<Face> faces;
    faces.reserve(static_cast<size_t>(nx * ny * 2));

    auto pushFace = [&](const ScreenVertex& a,
                        const ScreenVertex& b,
                        const ScreenVertex& c) {
        if (!a.valid || !b.valid || !c.valid) {
            return;
        }
        faces.push_back({
            ImVec2(a.x, a.y),
            ImVec2(b.x, b.y),
            ImVec2(c.x, c.y),
            (a.depth + b.depth + c.depth) / 3.0,
            (a.value + b.value + c.value) / 3.0
        });
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

    if (options.showEnvelope || options.showDimensionArrows) {
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
            const double x = wx - xCenter;
            const double y = wy - yCenter;
            const double zWorld = wz * options.zScale;

            const double xYaw = cosA * x - sinA * y;
            const double yYaw = sinA * x + cosA * y;

            const double xProj = xYaw;
            const double yProj = cosE * yYaw - sinE * zWorld;
            const double depth = sinE * yYaw + cosE * zWorld;

            const float sx = sxCenter + static_cast<float>((xProj - pxCenter) * scale);
            const float sy = syCenter - static_cast<float>((yProj - pyCenter) * scale);
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

        if (options.showDimensionArrows) {
            auto drawArrow = [&](const ImVec2& from, const ImVec2& to,
                                 const char* label, ImU32 color) {
                const float dx = to.x - from.x;
                const float dy = to.y - from.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length < 1.0f) {
                    return;
                }

                const float ux = dx / length;
                const float uy = dy / length;
                const float px = -uy;
                const float py = ux;
                const float headLength = std::clamp(length * 0.12f, 8.0f, 18.0f);
                const float headWidth = headLength * 0.5f;
                const float thickness = std::clamp(options.envelopeThickness * 1.2f, 0.8f, 4.0f);

                const ImVec2 shaftEnd(to.x - ux * headLength, to.y - uy * headLength);
                dl->AddLine(from, shaftEnd, color, thickness);
                dl->AddTriangleFilled(
                    to,
                    ImVec2(to.x - ux * headLength + px * headWidth,
                           to.y - uy * headLength + py * headWidth),
                    ImVec2(to.x - ux * headLength - px * headWidth,
                           to.y - uy * headLength - py * headWidth),
                    color);

                dl->AddText(ImVec2(to.x + px * 4.0f, to.y + py * 4.0f), color, label);
            };

            const ImU32 colorX = IM_COL32(240, 95, 95, 245);
            const ImU32 colorY = IM_COL32(95, 225, 120, 245);
            const ImU32 colorZ = IM_COL32(110, 165, 250, 245);
            const ImVec2 origin = corners[0].screen; // (xMin, yMin, zMin)

            drawArrow(origin, corners[1].screen, "X", colorX); // +X direction
            drawArrow(origin, corners[3].screen, "Y", colorY); // +Y direction
            drawArrow(origin, corners[4].screen, "Z", colorZ); // +Z direction
        }
    }

    dl->PopClipRect();
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

    auto interpolate = [&](double x0, double y0, double v0,
                           double x1, double y1, double v1,
                           Core::Vec2& out) -> bool {
        if (!std::isfinite(v0) || !std::isfinite(v1)) {
            return false;
        }
        if ((v0 > 0.0 && v1 > 0.0) || (v0 < 0.0 && v1 < 0.0)) {
            return false;
        }

        double t = 0.5;
        const double denom = v0 - v1;
        if (std::abs(denom) > 1e-12) {
            t = std::clamp(v0 / denom, 0.0, 1.0);
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
