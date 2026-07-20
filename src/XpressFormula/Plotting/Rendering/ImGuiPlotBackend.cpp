// ImGuiPlotBackend.cpp - Dear ImGui drawing backend for plotting geometry outputs.
#include "ImGuiPlotBackend.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace XpressFormula::Plotting::Rendering {

namespace {

void pushPlotClip(ImDrawList* dl, const Core::ViewTransform& vt) {
    dl->PushClipRect(
        ImVec2(vt.viewport.originX, vt.viewport.originY),
        ImVec2(vt.viewport.originX + vt.viewport.width,
               vt.viewport.originY + vt.viewport.height),
        true);
}

} // namespace

unsigned int ImGuiPlotBackend::colorU32(const float color[4]) {
    return IM_COL32(
        std::clamp(static_cast<int>(color[0] * 255), 0, 255),
        std::clamp(static_cast<int>(color[1] * 255), 0, 255),
        std::clamp(static_cast<int>(color[2] * 255), 0, 255),
        std::clamp(static_cast<int>(color[3] * 255), 0, 255));
}

unsigned int ImGuiPlotBackend::heatColor(double value,
                                         double lo,
                                         double hi,
                                         const float tint[4],
                                         float alpha) {
    if (std::isnan(value) || std::isinf(value)) {
        return IM_COL32(30, 30, 35, static_cast<int>(alpha * 255));
    }

    double range = hi - lo;
    if (range == 0.0) {
        range = 1.0;
    }

    const double t = std::clamp((value - lo) / range, 0.0, 1.0);

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

void ImGuiPlotBackend::drawPolylines(ImDrawList* dl,
                                     const Core::ViewTransform& vt,
                                     const std::vector<Geometry::Polyline>& polylines,
                                     const float color[4],
                                     float thickness) {
    if (dl == nullptr || polylines.empty()) {
        return;
    }

    const ImU32 drawColor = colorU32(color);
    pushPlotClip(dl, vt);

    for (const Geometry::Polyline& polyline : polylines) {
        if (polyline.points.size() < 2) {
            continue;
        }

        for (size_t i = 1; i < polyline.points.size(); ++i) {
            const Core::Vec2 a = vt.worldToScreen(polyline.points[i - 1].x,
                                                  polyline.points[i - 1].y);
            const Core::Vec2 b = vt.worldToScreen(polyline.points[i].x,
                                                  polyline.points[i].y);
            dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), drawColor, thickness);
        }

        if (polyline.closed && polyline.points.size() > 2) {
            const Core::Vec2 a = vt.worldToScreen(polyline.points.back().x,
                                                  polyline.points.back().y);
            const Core::Vec2 b = vt.worldToScreen(polyline.points.front().x,
                                                  polyline.points.front().y);
            dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), drawColor, thickness);
        }
    }

    dl->PopClipRect();
}

void ImGuiPlotBackend::drawLineSegments(ImDrawList* dl,
                                        const Core::ViewTransform& vt,
                                        const std::vector<Geometry::LineSegment2D>& segments,
                                        const float color[4],
                                        float thickness) {
    if (dl == nullptr || segments.empty()) {
        return;
    }

    const ImU32 drawColor = colorU32(color);
    pushPlotClip(dl, vt);

    for (const Geometry::LineSegment2D& segment : segments) {
        const Core::Vec2 a = vt.worldToScreen(segment.a.x, segment.a.y);
        const Core::Vec2 b = vt.worldToScreen(segment.b.x, segment.b.y);
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), drawColor, thickness);
    }

    dl->PopClipRect();
}

void ImGuiPlotBackend::drawScalarCellGrid(ImDrawList* dl,
                                          const Core::ViewTransform& vt,
                                          const Sampling::ScalarCellGrid& grid,
                                          const float tint[4],
                                          float alpha) {
    if (dl == nullptr || grid.empty() || !grid.bounds.valid()) {
        return;
    }

    const double dx = grid.cellWidth();
    const double dy = grid.cellHeight();
    pushPlotClip(dl, vt);

    for (int iy = 0; iy < grid.rows; ++iy) {
        const double wy = grid.bounds.yMin + static_cast<double>(iy) * dy;
        for (int ix = 0; ix < grid.columns; ++ix) {
            const double wx = grid.bounds.xMin + static_cast<double>(ix) * dx;
            const Core::Vec2 tl = vt.worldToScreen(wx, wy + dy);
            const Core::Vec2 br = vt.worldToScreen(wx + dx, wy);
            dl->AddRectFilled(
                ImVec2(tl.x, tl.y),
                ImVec2(br.x, br.y),
                heatColor(grid.valueAt(ix, iy), grid.minValue, grid.maxValue, tint, alpha));
        }
    }

    dl->PopClipRect();
}

} // namespace XpressFormula::Plotting::Rendering
