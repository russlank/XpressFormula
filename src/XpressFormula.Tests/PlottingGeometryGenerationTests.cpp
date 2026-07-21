// PlottingGeometryGenerationTests.cpp - Unit tests for pure plotting geometry generation.
#include "CppUnitTest.h"
#include "../XpressFormula/Model/Formula.h"
#include "../XpressFormula/Plotting/Geometry/PlaneClipping.h"
#include "../XpressFormula/Plotting/Meshing/ExplicitSurfaceMesh.h"
#include "../XpressFormula/Plotting/Meshing/MarchingSquares.h"
#include "../XpressFormula/Plotting/Meshing/SurfaceNets.h"
#include "../XpressFormula/Plotting/Sampling/CurveSampler.h"
#include "../XpressFormula/Plotting/Sampling/ScalarGridSampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <utility>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFCore = XpressFormula::Core;
namespace XFGeometry = XpressFormula::Plotting::Geometry;
namespace XFMesh = XpressFormula::Plotting::Meshing;
namespace XFSampling = XpressFormula::Plotting::Sampling;

namespace XpressFormulaTests {

static XFCore::ASTNodePtr astFor(std::string expression) {
    XpressFormula::Model::Formula formula;
    formula.setExpression(std::move(expression));
    formula.compile(true);
    Assert::IsTrue(formula.valid(), L"Expected test formula to compile.");
    return formula.compiled.ast;
}

static void assertClose(double expected, double actual, double tolerance = 1e-9) {
    Assert::IsTrue(std::abs(expected - actual) <= tolerance);
}

static double segmentLengthSquared(const XFGeometry::LineSegment2D& segment) {
    const double dx = segment.a.x - segment.b.x;
    const double dy = segment.a.y - segment.b.y;
    return dx * dx + dy * dy;
}

static void assertVertexInside(const XFGeometry::PlaneClipVertex2D& vertex,
                               double plane,
                               XFGeometry::PlaneClipSide side) {
    const double tolerance = 1e-9;
    if (side == XFGeometry::PlaneClipSide::Below) {
        Assert::IsTrue(vertex.value <= plane + tolerance);
    } else {
        Assert::IsTrue(vertex.value >= plane - tolerance);
    }
}

static std::string roundedVertexKey(const XFGeometry::Vec3& point) {
    const auto rounded = [](double value) -> long long {
        return static_cast<long long>(std::llround(value * 1000000000.0));
    };
    return std::to_string(rounded(point.x)) + "," +
        std::to_string(rounded(point.y)) + "," +
        std::to_string(rounded(point.z));
}

static std::string triangleKey(const XFMesh::ImplicitMeshTriangle& triangle) {
    std::array<std::string, 3> vertices{
        roundedVertexKey(triangle.p0),
        roundedVertexKey(triangle.p1),
        roundedVertexKey(triangle.p2)
    };
    std::sort(vertices.begin(), vertices.end());
    return vertices[0] + "|" + vertices[1] + "|" + vertices[2];
}

TEST_CASE(CurveSampler_SimpleLineProducesOnePolyline) {
    const std::vector<XFGeometry::Polyline> polylines =
        XFSampling::sampleCurve2D(
            astFor("x"),
            XFSampling::CurveSampleOptions{ -1.0, 1.0, 4, 100.0 });

    Assert::AreEqual(static_cast<size_t>(1), polylines.size());
    Assert::AreEqual(static_cast<size_t>(5), polylines[0].points.size());
    assertClose(-1.0, polylines[0].points.front().x);
    assertClose(-1.0, polylines[0].points.front().y);
    assertClose(1.0, polylines[0].points.back().x);
    assertClose(1.0, polylines[0].points.back().y);
}

TEST_CASE(CurveSampler_BreaksAtUndefinedDiscontinuity) {
    const std::vector<XFGeometry::Polyline> polylines =
        XFSampling::sampleCurve2D(
            astFor("1/x"),
            XFSampling::CurveSampleOptions{ -1.0, 1.0, 4, 1000.0 });

    Assert::AreEqual(static_cast<size_t>(2), polylines.size());
    Assert::AreEqual(static_cast<size_t>(2), polylines[0].points.size());
    Assert::AreEqual(static_cast<size_t>(2), polylines[1].points.size());
    Assert::IsTrue(polylines[0].points.back().x < 0.0);
    Assert::IsTrue(polylines[1].points.front().x > 0.0);
}

TEST_CASE(ScalarGridSampler_HeatmapDimensionsAndRangeAreStable) {
    const XFSampling::ScalarCellGrid grid =
        XFSampling::sampleScalarCellGrid(
            astFor("x+y"),
            XFSampling::ScalarGridOptions{
                XFGeometry::Bounds2D{ 0.0, 2.0, 0.0, 3.0 },
                4,
                3
            });

    Assert::AreEqual(4, grid.columns);
    Assert::AreEqual(3, grid.rows);
    Assert::AreEqual(static_cast<size_t>(12), grid.values.size());
    Assert::IsTrue(grid.hasFiniteValue);
    assertClose(0.75, grid.minValue);
    assertClose(4.25, grid.maxValue);
}

TEST_CASE(MarchingSquares_CircleContourProducesWorldSegments) {
    const XFSampling::ScalarLattice lattice =
        XFSampling::sampleScalarLattice(
            astFor("x^2+y^2-1"),
            XFSampling::ScalarGridOptions{
                XFGeometry::Bounds2D{ -2.0, 2.0, -2.0, 2.0 },
                24,
                24
            });
    const std::vector<XFGeometry::LineSegment2D> segments =
        XFMesh::buildContourSegments(lattice);

    Assert::IsFalse(segments.empty());
    for (const XFGeometry::LineSegment2D& segment : segments) {
        const double ra = std::sqrt(segment.a.x * segment.a.x + segment.a.y * segment.a.y);
        const double rb = std::sqrt(segment.b.x * segment.b.x + segment.b.y * segment.b.y);
        Assert::IsTrue(ra > 0.75 && ra < 1.25);
        Assert::IsTrue(rb > 0.75 && rb < 1.25);
    }
}

TEST_CASE(MarchingSquares_SaddleCaseProducesTwoNonDegenerateSegments) {
    XFSampling::ScalarLattice lattice;
    lattice.bounds = XFGeometry::Bounds2D{ 0.0, 1.0, 0.0, 1.0 };
    lattice.cellsX = 1;
    lattice.cellsY = 1;
    lattice.values = {
        -2.0, 1.0,
         1.0, -2.0
    };

    const std::vector<XFGeometry::LineSegment2D> segments =
        XFMesh::buildContourSegments(lattice);

    Assert::AreEqual(static_cast<size_t>(2), segments.size());
    for (const XFGeometry::LineSegment2D& segment : segments) {
        Assert::IsTrue(segmentLengthSquared(segment) > 1e-24);
    }
}

TEST_CASE(MarchingSquares_AllZeroAndNaNCellsDoNotEmitDegenerateSegments) {
    XFSampling::ScalarLattice zeroLattice;
    zeroLattice.bounds = XFGeometry::Bounds2D{ 0.0, 1.0, 0.0, 1.0 };
    zeroLattice.cellsX = 1;
    zeroLattice.cellsY = 1;
    zeroLattice.values = { 0.0, 0.0, 0.0, 0.0 };

    XFSampling::ScalarLattice nanLattice = zeroLattice;
    nanLattice.values = {
        -1.0,
        std::numeric_limits<double>::quiet_NaN(),
        1.0,
        -1.0
    };

    Assert::IsTrue(XFMesh::buildContourSegments(zeroLattice).empty());
    for (const XFGeometry::LineSegment2D& segment : XFMesh::buildContourSegments(nanLattice)) {
        Assert::IsTrue(std::isfinite(segment.a.x));
        Assert::IsTrue(std::isfinite(segment.a.y));
        Assert::IsTrue(std::isfinite(segment.b.x));
        Assert::IsTrue(std::isfinite(segment.b.y));
        Assert::IsTrue(segmentLengthSquared(segment) > 1e-24);
    }
}

TEST_CASE(ExplicitSurfaceMesh_PlaneSamplesWorldSpaceVertices) {
    const XFMesh::ExplicitSurfaceMesh mesh =
        XFMesh::sampleExplicitSurface(
            astFor("x+y"),
            XFMesh::ExplicitSurfaceOptions{
                XFGeometry::Bounds2D{ -1.0, 1.0, -1.0, 1.0 },
                16
            });

    Assert::AreEqual(16, mesh.cellsX);
    Assert::AreEqual(16, mesh.cellsY);
    Assert::AreEqual(static_cast<size_t>(17 * 17), mesh.vertices.size());
    Assert::AreEqual(17 * 17, mesh.validVertexCount);
    assertClose(-2.0, mesh.vertexAt(0, 0).position.z);
    assertClose(0.0, mesh.vertexAt(8, 8).position.z);
    assertClose(2.0, mesh.vertexAt(16, 16).position.z);
}

TEST_CASE(SurfaceNets_SphereProducesNonEmptyBoundedMesh) {
    const XFMesh::ImplicitMeshEntry mesh =
        XFMesh::buildSurfaceNetsMesh(
            astFor("x^2+y^2+z^2-1"),
            XFMesh::SurfaceNetsOptions{
                XFGeometry::Bounds3D{ -1.5, 1.5, -1.5, 1.5, -1.5, 1.5 },
                16
            });

    Assert::IsFalse(mesh.faces.empty());
    Assert::IsTrue(mesh.surfaceBounds.valid());
    Assert::IsTrue(mesh.surfaceBounds.xMin < -0.75);
    Assert::IsTrue(mesh.surfaceBounds.xMax > 0.75);
    Assert::IsTrue(mesh.surfaceBounds.yMin < -0.75);
    Assert::IsTrue(mesh.surfaceBounds.yMax > 0.75);
    Assert::IsTrue(mesh.surfaceBounds.zMin < -0.75);
    Assert::IsTrue(mesh.surfaceBounds.zMax > 0.75);
}

TEST_CASE(SurfaceNets_BoundsContainGeneratedTriangles) {
    const XFMesh::ImplicitMeshEntry mesh =
        XFMesh::buildSurfaceNetsMesh(
            astFor("x^2+y^2+z^2-1"),
            XFMesh::SurfaceNetsOptions{
                XFGeometry::Bounds3D{ -1.5, 1.5, -1.5, 1.5, -1.5, 1.5 },
                16
            });

    Assert::IsFalse(mesh.faces.empty());
    for (const XFMesh::ImplicitMeshTriangle& face : mesh.faces) {
        const XFGeometry::Vec3 vertices[3] = { face.p0, face.p1, face.p2 };
        for (const XFGeometry::Vec3& vertex : vertices) {
            Assert::IsTrue(vertex.x >= mesh.surfaceBounds.xMin - 1e-9);
            Assert::IsTrue(vertex.x <= mesh.surfaceBounds.xMax + 1e-9);
            Assert::IsTrue(vertex.y >= mesh.surfaceBounds.yMin - 1e-9);
            Assert::IsTrue(vertex.y <= mesh.surfaceBounds.yMax + 1e-9);
            Assert::IsTrue(vertex.z >= mesh.surfaceBounds.zMin - 1e-9);
            Assert::IsTrue(vertex.z <= mesh.surfaceBounds.zMax + 1e-9);
        }
    }
}

TEST_CASE(SurfaceNets_AllZeroFieldDoesNotEmitDegenerateSurface) {
    const XFMesh::ImplicitMeshEntry mesh =
        XFMesh::buildSurfaceNetsMesh(
            astFor("0"),
            XFMesh::SurfaceNetsOptions{
                XFGeometry::Bounds3D{ -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 },
                16
            });

    Assert::IsTrue(mesh.faces.empty());
    Assert::IsFalse(mesh.surfaceBounds.valid());
}

TEST_CASE(SurfaceNets_ExactZeroPlaneOnGridProducesFiniteNonDegenerateTriangles) {
    const XFMesh::ImplicitMeshEntry mesh =
        XFMesh::buildSurfaceNetsMesh(
            astFor("z"),
            XFMesh::SurfaceNetsOptions{
                XFGeometry::Bounds3D{ -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 },
                16
            });

    Assert::IsFalse(mesh.faces.empty());
    for (const XFMesh::ImplicitMeshTriangle& face : mesh.faces) {
        Assert::IsTrue(XFGeometry::triangleAreaValid(face.p0, face.p1, face.p2, 1e-16));
        Assert::IsTrue(std::isfinite(face.p0.z));
        Assert::IsTrue(std::isfinite(face.p1.z));
        Assert::IsTrue(std::isfinite(face.p2.z));
    }
}

TEST_CASE(SurfaceNets_IsolatedExactZeroGridContactDoesNotEmitSpuriousPatch) {
    const XFMesh::SurfaceNetsOptions options{
        XFGeometry::Bounds3D{ -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 },
        16
    };

    const XFMesh::ImplicitMeshEntry positiveContact =
        XFMesh::buildSurfaceNetsMesh(astFor("x^2+y^2+z^2"), options);
    const XFMesh::ImplicitMeshEntry negativeContact =
        XFMesh::buildSurfaceNetsMesh(astFor("-(x^2+y^2+z^2)"), options);

    Assert::IsTrue(positiveContact.faces.empty());
    Assert::IsFalse(positiveContact.surfaceBounds.valid());
    Assert::IsTrue(negativeContact.faces.empty());
    Assert::IsFalse(negativeContact.surfaceBounds.valid());
}

TEST_CASE(SurfaceNets_DoesNotEmitDuplicateTrianglesForDeterministicPlane) {
    const XFMesh::ImplicitMeshEntry mesh =
        XFMesh::buildSurfaceNetsMesh(
            astFor("z"),
            XFMesh::SurfaceNetsOptions{
                XFGeometry::Bounds3D{ -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 },
                16
            });

    std::set<std::string> seen;
    for (const XFMesh::ImplicitMeshTriangle& face : mesh.faces) {
        Assert::IsTrue(seen.insert(triangleKey(face)).second);
    }
}

TEST_CASE(PlaneClipping_ClipsTriangleAboveAndBelowPlane) {
    const XFGeometry::PlaneClipVertex2D a{ XFGeometry::Vec2{ 0.0, 0.0 }, 0.0, -1.0 };
    const XFGeometry::PlaneClipVertex2D b{ XFGeometry::Vec2{ 2.0, 0.0 }, 0.0, 1.0 };
    const XFGeometry::PlaneClipVertex2D c{ XFGeometry::Vec2{ 0.0, 2.0 }, 0.0, 1.0 };

    const std::vector<XFGeometry::PlaneClipVertex2D> below =
        XFGeometry::clipTriangleByValue(a, b, c, 0.0, XFGeometry::PlaneClipSide::Below);
    const std::vector<XFGeometry::PlaneClipVertex2D> above =
        XFGeometry::clipTriangleByValue(a, b, c, 0.0, XFGeometry::PlaneClipSide::Above);

    Assert::AreEqual(static_cast<size_t>(3), below.size());
    Assert::AreEqual(static_cast<size_t>(4), above.size());
    for (const XFGeometry::PlaneClipVertex2D& vertex : below) {
        assertVertexInside(vertex, 0.0, XFGeometry::PlaneClipSide::Below);
    }
    for (const XFGeometry::PlaneClipVertex2D& vertex : above) {
        assertVertexInside(vertex, 0.0, XFGeometry::PlaneClipSide::Above);
    }
}

TEST_CASE(PlaneClipping_RejectsDegenerateTriangles) {
    Assert::IsFalse(
        XFGeometry::triangleAreaValid(
            XFGeometry::Vec3{ 0.0, 0.0, 0.0 },
            XFGeometry::Vec3{ 1.0, 1.0, 1.0 },
            XFGeometry::Vec3{ 2.0, 2.0, 2.0 },
            1e-16));
    Assert::IsTrue(
        XFGeometry::triangleAreaValid(
            XFGeometry::Vec3{ 0.0, 0.0, 0.0 },
            XFGeometry::Vec3{ 1.0, 0.0, 0.0 },
            XFGeometry::Vec3{ 0.0, 1.0, 0.0 },
            1e-16));
}

TEST_CASE(SurfaceNets_FixedInputIsStable) {
    const XFCore::ASTNodePtr ast = astFor("x^2+y^2+z^2-1");
    const XFMesh::SurfaceNetsOptions options{
        XFGeometry::Bounds3D{ -1.5, 1.5, -1.5, 1.5, -1.5, 1.5 },
        16
    };
    const XFMesh::ImplicitMeshEntry first = XFMesh::buildSurfaceNetsMesh(ast, options);
    const XFMesh::ImplicitMeshEntry second = XFMesh::buildSurfaceNetsMesh(ast, options);

    Assert::AreEqual(first.faces.size(), second.faces.size());
    Assert::IsFalse(first.faces.empty());
    assertClose(first.surfaceBounds.xMin, second.surfaceBounds.xMin);
    assertClose(first.surfaceBounds.xMax, second.surfaceBounds.xMax);
    assertClose(first.surfaceBounds.yMin, second.surfaceBounds.yMin);
    assertClose(first.surfaceBounds.yMax, second.surfaceBounds.yMax);
    assertClose(first.surfaceBounds.zMin, second.surfaceBounds.zMin);
    assertClose(first.surfaceBounds.zMax, second.surfaceBounds.zMax);

    const size_t comparedFaces = std::min<size_t>(first.faces.size(), 8);
    for (size_t i = 0; i < comparedFaces; ++i) {
        assertClose(first.faces[i].p0.x, second.faces[i].p0.x);
        assertClose(first.faces[i].p0.y, second.faces[i].p0.y);
        assertClose(first.faces[i].p0.z, second.faces[i].p0.z);
        assertClose(first.faces[i].p1.x, second.faces[i].p1.x);
        assertClose(first.faces[i].p1.y, second.faces[i].p1.y);
        assertClose(first.faces[i].p1.z, second.faces[i].p1.z);
        assertClose(first.faces[i].p2.x, second.faces[i].p2.x);
        assertClose(first.faces[i].p2.y, second.faces[i].p2.y);
        assertClose(first.faces[i].p2.z, second.faces[i].p2.z);
    }
}

} // namespace XpressFormulaTests
