// ImGuiPlotBackend.h - Dear ImGui drawing backend for plotting geometry outputs.
#pragma once

#include "../Geometry/Polyline.h"
#include "../Sampling/ScalarGridSampler.h"
#include "../../Core/ViewTransform.h"

#include <vector>

struct ImDrawList;

namespace XpressFormula::Plotting::Rendering {

class ImGuiPlotBackend {
public:
    static unsigned int colorU32(const float color[4]);
    static unsigned int heatColor(double value,
                                  double lo,
                                  double hi,
                                  const float tint[4],
                                  float alpha);

    static void drawPolylines(ImDrawList* dl,
                              const Core::ViewTransform& vt,
                              const std::vector<Geometry::Polyline>& polylines,
                              const float color[4],
                              float thickness);

    static void drawLineSegments(ImDrawList* dl,
                                 const Core::ViewTransform& vt,
                                 const std::vector<Geometry::LineSegment2D>& segments,
                                 const float color[4],
                                 float thickness);

    static void drawScalarCellGrid(ImDrawList* dl,
                                   const Core::ViewTransform& vt,
                                   const Sampling::ScalarCellGrid& grid,
                                   const float tint[4],
                                   float alpha);
};

} // namespace XpressFormula::Plotting::Rendering
