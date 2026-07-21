// Projection3D.h - Shared world-origin anchored 3D plot projection.
#pragma once

#include "Camera3D.h"
#include "Geometry/Vec2.h"
#include "Geometry/Vec3.h"
#include "../Core/ViewTransform.h"

namespace XpressFormula::Plotting {

struct ProjectedPoint3D {
    double x = 0.0;
    double y = 0.0;
    double depth = 0.0;
};

struct ViewVector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct ProjectionScreenAnchor {
    Geometry::Vec2 originScreen;
    double scale = 1.0;
};

struct ScreenPoint3D {
    Geometry::Vec2 screen;
    double depth = 0.0;
};

class Projection3D {
public:
    explicit Projection3D(const Camera3D& camera) noexcept;

    [[nodiscard]] ProjectedPoint3D project(const Geometry::Vec3& point) const noexcept;
    [[nodiscard]] ViewVector3D rotateScaledVectorToView(const Geometry::Vec3& vector) const noexcept;
    [[nodiscard]] bool projectDirection2D(const Geometry::Vec3& direction,
                                          Geometry::Vec2& outDirection) const noexcept;
    [[nodiscard]] ScreenPoint3D toScreen(const ProjectedPoint3D& point,
                                         const ProjectionScreenAnchor& anchor) const noexcept;
    [[nodiscard]] ScreenPoint3D projectToScreen(const Geometry::Vec3& point,
                                                const ProjectionScreenAnchor& anchor) const noexcept;

private:
    Camera3D m_camera;
    double m_cosA = 1.0;
    double m_sinA = 0.0;
    double m_cosE = 1.0;
    double m_sinE = 0.0;
};

[[nodiscard]] ProjectionScreenAnchor projectionScreenAnchorFor(
    const Core::ViewTransform& viewTransform) noexcept;

} // namespace XpressFormula::Plotting
