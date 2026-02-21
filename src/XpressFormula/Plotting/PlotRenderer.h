// PlotRenderer.h - Renders grid, axes, and formula plots onto an ImGui DrawList.
#pragma once

#include "../Core/ViewTransform.h"
#include "../Core/ASTNode.h"

struct ImDrawList;

namespace XpressFormula::Plotting {

class PlotRenderer {
public:
    /// Draw grid lines (major and minor).
    static void drawGrid(ImDrawList* dl, const Core::ViewTransform& vt);

    /// Draw X and Y axes through the origin.
    static void drawAxes(ImDrawList* dl, const Core::ViewTransform& vt);

    /// Draw tick labels along the axes.
    static void drawAxisLabels(ImDrawList* dl, const Core::ViewTransform& vt);

    /// Plot a 2D curve f(x).
    static void drawCurve2D(ImDrawList* dl, const Core::ViewTransform& vt,
                            const Core::ASTNodePtr& ast,
                            const float color[4], float thickness = 2.0f);

    /// Plot a heat-map for f(x,y).
    static void drawHeatmap(ImDrawList* dl, const Core::ViewTransform& vt,
                            const Core::ASTNodePtr& ast, float alpha = 0.6f);

    /// Plot a heat-map cross-section for f(x,y,z) at a given z slice.
    static void drawCrossSection(ImDrawList* dl, const Core::ViewTransform& vt,
                                 const Core::ASTNodePtr& ast,
                                 float zSlice, float alpha = 0.6f);

private:
    static unsigned int heatColor(double value, double lo, double hi, float alpha);
    static unsigned int colorU32(const float c[4]);
    static void         formatLabel(char* buf, size_t len, double v);
};

} // namespace XpressFormula::Plotting
