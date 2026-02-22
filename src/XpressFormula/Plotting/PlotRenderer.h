// PlotRenderer.h - Renders grid, axes, and formula plots onto an ImGui DrawList.
#pragma once

#include "../Core/ViewTransform.h"
#include "../Core/ASTNode.h"

struct ImDrawList;

namespace XpressFormula::Plotting {

class PlotRenderer {
public:
    struct Surface3DOptions {
        float azimuthDeg = 40.0f;
        float elevationDeg = 30.0f;
        float zScale = 1.0f;
        int   resolution = 36;
        // Used by implicit F(x,y,z)=0 extraction. Kept separate because implicit meshing
        // is O(N^3) and usually needs a different quality/perf tradeoff than z=f(x,y).
        int   implicitResolution = 64;
        float opacity = 0.82f;
        float wireThickness = 1.0f;
        bool  showEnvelope = true;
        float envelopeThickness = 1.25f;
        bool  showDimensionArrows = true;
        // Center of the implicit z sampling window. The sampled z range is derived from the
        // current x/y view span around this center (so panning/zooming the view changes mesh).
        float implicitZCenter = 0.0f;
    };

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
                            const Core::ASTNodePtr& ast,
                            const float tint[4], float alpha = 0.6f);

    /// Plot a heat-map cross-section for f(x,y,z) at a given z slice.
    static void drawCrossSection(ImDrawList* dl, const Core::ViewTransform& vt,
                                 const Core::ASTNodePtr& ast,
                                 float zSlice,
                                 const float tint[4], float alpha = 0.6f);

    /// Plot a 3D z=f(x,y) surface using an isometric-style projection.
    static void drawSurface3D(ImDrawList* dl, const Core::ViewTransform& vt,
                              const Core::ASTNodePtr& ast, const float color[4],
                              const Surface3DOptions& options);

    /// Plot the implicit 3D surface F(x,y,z)=0 using a cached surface-nets style mesh,
    /// then project/draw it as depth-sorted triangles in ImGui.
    static void drawImplicitSurface3D(ImDrawList* dl, const Core::ViewTransform& vt,
                                      const Core::ASTNodePtr& ast, const float color[4],
                                      const Surface3DOptions& options);

    /// Plot the zero contour F(x,y)=0 for implicit equations.
    static void drawImplicitContour2D(ImDrawList* dl, const Core::ViewTransform& vt,
                                      const Core::ASTNodePtr& ast,
                                      const float color[4], float thickness = 2.0f);

private:
    static unsigned int heatColor(double value, double lo, double hi,
                                  const float tint[4], float alpha);
    static unsigned int colorU32(const float c[4]);
    static void         formatLabel(char* buf, size_t len, double v);
};

} // namespace XpressFormula::Plotting
