// SurfaceNets.cpp - Pure implicit scalar-field sampling and surface-nets mesh generation.
#include "SurfaceNets.h"

#include "../Geometry/PlaneClipping.h"
#include "../../Core/Evaluator.h"
#include "../../Model/PlotPolicy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

namespace XpressFormula::Plotting::Meshing {

namespace {

using Point3 = Geometry::Vec3;
using PointKey = std::array<double, 3>;
using TriangleKey = std::array<PointKey, 3>;

struct CellVertex {
    Point3 p;
    bool active = false;
};

[[nodiscard]] bool signsCrossZero(double a, double b) noexcept {
    if (!std::isfinite(a) || !std::isfinite(b)) {
        return false;
    }
    if (a == 0.0 && b == 0.0) {
        return false;
    }
    if (a == 0.0 || b == 0.0) {
        return true;
    }
    return (a < 0.0 && b > 0.0) || (a > 0.0 && b < 0.0);
}

[[nodiscard]] Point3 sub3(const Point3& a, const Point3& b) noexcept {
    return Point3{ a.x - b.x, a.y - b.y, a.z - b.z };
}

[[nodiscard]] double lenSq3(const Point3& v) noexcept {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

[[nodiscard]] bool pointFinite(const Point3& point) noexcept {
    return std::isfinite(point.x) &&
           std::isfinite(point.y) &&
           std::isfinite(point.z);
}

[[nodiscard]] PointKey makePointKey(const Point3& point) noexcept {
    return PointKey{ point.x, point.y, point.z };
}

[[nodiscard]] TriangleKey makeTriangleKey(const Point3& a,
                                          const Point3& b,
                                          const Point3& c) noexcept {
    TriangleKey key{ makePointKey(a), makePointKey(b), makePointKey(c) };
    std::sort(key.begin(), key.end());
    return key;
}

bool appendUniqueIntersection(std::array<Point3, 12>& points,
                              std::size_t& count,
                              const Point3& point,
                              double toleranceSquared) noexcept {
    if (!pointFinite(point) || count >= points.size()) {
        return false;
    }

    for (std::size_t i = 0; i < count; ++i) {
        if (lenSq3(sub3(points[i], point)) <= toleranceSquared) {
            return false;
        }
    }

    points[count] = point;
    ++count;
    return true;
}

[[nodiscard]] bool interpolateIso(const Point3& a,
                                  double va,
                                  const Point3& b,
                                  double vb,
                                  Point3& out) noexcept {
    if (!signsCrossZero(va, vb)) {
        return false;
    }
    if (va == 0.0) {
        out = a;
        return true;
    }
    if (vb == 0.0) {
        out = b;
        return true;
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
}

} // namespace

ImplicitMeshEntry buildSurfaceNetsMesh(const Core::ASTNodePtr& ast,
                                       const SurfaceNetsOptions& options) {
    ImplicitMeshEntry entry;
    if (!ast || !options.bounds.valid()) {
        return entry;
    }

    const int gridRes = XpressFormula::Model::clampImplicitSurfaceResolution(options.resolution);
    const int nx = gridRes;
    const int ny = gridRes;
    const int nz = gridRes;
    const double dx = std::max(1e-6, (options.bounds.xMax - options.bounds.xMin) / nx);
    const double dy = std::max(1e-6, (options.bounds.yMax - options.bounds.yMin) / ny);
    const double dz = std::max(1e-6, (options.bounds.zMax - options.bounds.zMin) / nz);

    auto gridIndex = [&](int ix, int iy, int iz) -> size_t {
        return static_cast<size_t>(((iz * (ny + 1)) + iy) * (nx + 1) + ix);
    };
    auto cellIndex = [&](int ix, int iy, int iz) -> size_t {
        return static_cast<size_t>(((iz * ny) + iy) * nx + ix);
    };

    static const int kCubeOffsets[8][3] = {
        { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 },
        { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 }
    };
    static const int kCubeEdges[12][2] = {
        { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    std::vector<double> values(static_cast<size_t>(nx + 1) *
                                   static_cast<size_t>(ny + 1) *
                                   static_cast<size_t>(nz + 1),
                               std::numeric_limits<double>::quiet_NaN());
    Core::EvaluationContext context;
    for (int iz = 0; iz <= nz; ++iz) {
        context.z = options.bounds.zMin + static_cast<double>(iz) * dz;
        for (int iy = 0; iy <= ny; ++iy) {
            context.y = options.bounds.yMin + static_cast<double>(iy) * dy;
            for (int ix = 0; ix <= nx; ++ix) {
                context.x = options.bounds.xMin + static_cast<double>(ix) * dx;
                values[gridIndex(ix, iy, iz)] = Core::Evaluator::evaluate(ast, context);
            }
        }
    }

    std::vector<CellVertex> cellVertices(static_cast<size_t>(nx) *
                                             static_cast<size_t>(ny) *
                                             static_cast<size_t>(nz),
                                         CellVertex{});
    const double sampleStepWorld = std::max({ dx, dy, dz });
    const double areaTolSq = std::max(1e-16, sampleStepWorld * sampleStepWorld * 1e-10);
    const double intersectionTolSq =
        std::max(1e-24, sampleStepWorld * sampleStepWorld * 1e-18);
    std::set<TriangleKey> emittedTriangles;

    auto pushWorldTriangle = [&](const Point3& a, const Point3& b, const Point3& c) {
        if (!pointFinite(a) || !pointFinite(b) || !pointFinite(c)) {
            return;
        }
        if (!Geometry::triangleAreaValid(a, b, c, areaTolSq)) {
            return;
        }
        if (!emittedTriangles.insert(makeTriangleKey(a, b, c)).second) {
            return;
        }

        entry.faces.push_back(ImplicitMeshTriangle{ a, b, c });
        entry.surfaceBounds.include(a);
        entry.surfaceBounds.include(b);
        entry.surfaceBounds.include(c);
    };

    auto emitQuad = [&](const Point3& p00, const Point3& p10,
                        const Point3& p11, const Point3& p01) {
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

    auto tryEmitQuadFromCells = [&](int ax, int ay, int az,
                                    int bx, int by, int bz,
                                    int cx, int cy, int cz,
                                    int dxCell, int dyCell, int dzCell) {
        if (ax < 0 || ax >= nx || ay < 0 || ay >= ny || az < 0 || az >= nz ||
            bx < 0 || bx >= nx || by < 0 || by >= ny || bz < 0 || bz >= nz ||
            cx < 0 || cx >= nx || cy < 0 || cy >= ny || cz < 0 || cz >= nz ||
            dxCell < 0 || dxCell >= nx || dyCell < 0 || dyCell >= ny ||
            dzCell < 0 || dzCell >= nz) {
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
                        options.bounds.xMin + static_cast<double>(gx) * dx,
                        options.bounds.yMin + static_cast<double>(gy) * dy,
                        options.bounds.zMin + static_cast<double>(gz) * dz
                    };
                    const double value = values[gridIndex(gx, gy, gz)];
                    cornerValues[c] = value;
                    if (std::isfinite(value)) {
                        hasFinite = true;
                        cellLo = std::min(cellLo, value);
                        cellHi = std::max(cellHi, value);
                    }
                }

                if (!hasFinite || cellLo > 0.0 || cellHi < 0.0) {
                    continue;
                }

                std::array<Point3, 12> intersections{};
                std::size_t intersectionCount = 0;
                for (const auto& edge : kCubeEdges) {
                    Point3 ip{};
                    if (!interpolateIso(corners[edge[0]], cornerValues[edge[0]],
                                        corners[edge[1]], cornerValues[edge[1]], ip)) {
                        continue;
                    }
                    (void)appendUniqueIntersection(
                        intersections,
                        intersectionCount,
                        ip,
                        intersectionTolSq);
                }

                if (intersectionCount < 3) {
                    continue;
                }

                Point3 sum{ 0.0, 0.0, 0.0 };
                for (std::size_t i = 0; i < intersectionCount; ++i) {
                    sum.x += intersections[i].x;
                    sum.y += intersections[i].y;
                    sum.z += intersections[i].z;
                }

                CellVertex& cv = cellVertices[cellIndex(ix, iy, iz)];
                const double inv = 1.0 / static_cast<double>(intersectionCount);
                cv.p = Point3{ sum.x * inv, sum.y * inv, sum.z * inv };
                cv.active = true;
            }
        }
    }

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

    return entry;
}

} // namespace XpressFormula::Plotting::Meshing
