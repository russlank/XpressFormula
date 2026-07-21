// Projection3D.cpp - Shared world-origin anchored 3D plot projection.
#include "Projection3D.h"

#include <algorithm>
#include <cmath>

namespace XpressFormula::Plotting {

namespace {

constexpr double kPi = 3.14159265358979323846;

double degreesToRadians(double degrees) noexcept {
    return degrees * kPi / 180.0;
}

} // namespace

Projection3D::Projection3D(const Camera3D& camera) noexcept
    : m_camera(camera) {
    const double azimuth = degreesToRadians(static_cast<double>(camera.azimuthDeg));
    const double elevation = degreesToRadians(static_cast<double>(camera.elevationDeg));
    m_cosA = std::cos(azimuth);
    m_sinA = std::sin(azimuth);
    m_cosE = std::cos(elevation);
    m_sinE = std::sin(elevation);
}

ProjectedPoint3D Projection3D::project(const Geometry::Vec3& point) const noexcept {
    const double zWorld = point.z * static_cast<double>(m_camera.zScale);
    const double xYaw = m_cosA * point.x - m_sinA * point.y;
    const double yYaw = m_sinA * point.x + m_cosA * point.y;

    return ProjectedPoint3D{
        xYaw,
        m_cosE * yYaw - m_sinE * zWorld,
        m_sinE * yYaw + m_cosE * zWorld
    };
}

ViewVector3D Projection3D::rotateScaledVectorToView(const Geometry::Vec3& vector) const noexcept {
    const double xYaw = m_cosA * vector.x - m_sinA * vector.y;
    const double yYaw = m_sinA * vector.x + m_cosA * vector.y;

    return ViewVector3D{
        xYaw,
        m_cosE * yYaw - m_sinE * vector.z,
        m_sinE * yYaw + m_cosE * vector.z
    };
}

bool Projection3D::projectDirection2D(const Geometry::Vec3& direction,
                                      Geometry::Vec2& outDirection) const noexcept {
    const ProjectedPoint3D projected = project(direction);
    const double dx = projected.x;
    const double dy = -projected.y;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (!(len > 1e-4) || !std::isfinite(len)) {
        outDirection = Geometry::Vec2{};
        return false;
    }

    outDirection = Geometry::Vec2{ dx / len, dy / len };
    return true;
}

ScreenPoint3D Projection3D::toScreen(const ProjectedPoint3D& point,
                                     const ProjectionScreenAnchor& anchor) const noexcept {
    return ScreenPoint3D{
        Geometry::Vec2{
            anchor.originScreen.x + point.x * anchor.scale,
            anchor.originScreen.y - point.y * anchor.scale
        },
        point.depth
    };
}

ScreenPoint3D Projection3D::projectToScreen(const Geometry::Vec3& point,
                                            const ProjectionScreenAnchor& anchor) const noexcept {
    return toScreen(project(point), anchor);
}

ProjectionScreenAnchor projectionScreenAnchorFor(
    const Core::ViewTransform& viewTransform) noexcept {
    const Core::Vec2 origin = viewTransform.worldToScreen(0.0, 0.0);
    return ProjectionScreenAnchor{
        Geometry::Vec2{ origin.x, origin.y },
        std::max(1e-6, std::min(viewTransform.state.scaleX, viewTransform.state.scaleY))
    };
}

} // namespace XpressFormula::Plotting
