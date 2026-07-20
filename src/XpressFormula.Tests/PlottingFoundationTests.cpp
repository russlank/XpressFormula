// PlottingFoundationTests.cpp - Unit tests for projection, render planning, quality, and caches.
#include "CppUnitTest.h"
#include "../XpressFormula/Plotting/Meshing/ImplicitMeshCache.h"
#include "../XpressFormula/Plotting/PlotQualityPolicy.h"
#include "../XpressFormula/Plotting/PlotRenderPlan.h"
#include "../XpressFormula/Plotting/PlotRenderer.h"
#include "../XpressFormula/Plotting/Projection3D.h"

#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFCore = XpressFormula::Core;
namespace XFModel = XpressFormula::Model;
namespace XFPlot = XpressFormula::Plotting;
namespace XFMesh = XpressFormula::Plotting::Meshing;

namespace XpressFormulaTests {

static void assertClose(double expected, double actual, double tolerance = 1e-9) {
    Assert::IsTrue(std::abs(expected - actual) <= tolerance);
}

static XFModel::SceneSummary sceneWith(bool has2D, bool has3D) {
    XFModel::SceneSummary scene;
    scene.hasVisibleCurve2D = has2D;
    scene.hasVisibleExplicitSurface3D = has3D;
    return scene;
}

static XFModel::Formula formula(std::string expression) {
    XFModel::Formula result;
    result.setExpression(std::move(expression));
    result.compile();
    return result;
}

static XFMesh::ImplicitMeshKey meshKey(XFModel::FormulaId id,
                                       std::uint64_t revision,
                                       int resolution) {
    return XFMesh::ImplicitMeshKey{
        id,
        revision,
        reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x1000u + id + revision)),
        resolution,
        -1.0,
        1.0,
        -2.0,
        2.0,
        0.0,
        -3.0,
        3.0
    };
}

static XFMesh::ImplicitMeshEntry meshEntry(double offset) {
    XFMesh::ImplicitMeshEntry entry;
    entry.faces.push_back(XFMesh::ImplicitMeshTriangle{
        XFPlot::Geometry::Vec3{ offset, 0.0, 0.0 },
        XFPlot::Geometry::Vec3{ offset + 1.0, 0.0, 0.0 },
        XFPlot::Geometry::Vec3{ offset, 1.0, 0.0 }
    });
    entry.surfaceBounds.include(entry.faces.front().p0);
    entry.surfaceBounds.include(entry.faces.front().p1);
    entry.surfaceBounds.include(entry.faces.front().p2);
    return entry;
}

TEST_CASE(Projection3D_KnownPointsUseCurrentVisualMath) {
    const XFPlot::Projection3D projection(XFPlot::Camera3D{ 0.0f, 0.0f, 1.0f });

    const XFPlot::ProjectedPoint3D point =
        projection.project(XFPlot::Geometry::Vec3{ 1.0, 2.0, 3.0 });

    assertClose(1.0, point.x);
    assertClose(2.0, point.y);
    assertClose(3.0, point.depth);
}

TEST_CASE(Projection3D_AppliesZScaleToDepthAndScreenY) {
    const XFPlot::Projection3D projection(XFPlot::Camera3D{ 0.0f, 30.0f, 2.0f });

    const XFPlot::ProjectedPoint3D point =
        projection.project(XFPlot::Geometry::Vec3{ 0.0, 0.0, 3.0 });

    assertClose(-3.0, point.y);
    assertClose(3.0 * std::sqrt(3.0), point.depth);
}

TEST_CASE(Projection3D_ToScreenIsAnchoredAtWorldOrigin) {
    XFCore::ViewTransform view;
    view.viewport.originX = 100.0f;
    view.viewport.originY = 40.0f;
    view.viewport.width = 800.0f;
    view.viewport.height = 600.0f;
    view.state.centerX = 2.0;
    view.state.centerY = -1.0;
    view.state.scaleX = 50.0;
    view.state.scaleY = 75.0;

    const XFPlot::Projection3D projection(XFPlot::Camera3D{ 30.0f, -60.0f, 1.5f });
    const XFPlot::ProjectionScreenAnchor anchor = XFPlot::projectionScreenAnchorFor(view);
    const XFPlot::ScreenPoint3D origin =
        projection.projectToScreen(XFPlot::Geometry::Vec3{}, anchor);
    const XFCore::Vec2 expected = view.worldToScreen(0.0, 0.0);

    assertClose(expected.x, origin.screen.x);
    assertClose(expected.y, origin.screen.y);
    assertClose(50.0, anchor.scale);
}

TEST_CASE(PlotRenderPlan_SurfaceModeInterleavesGridPlane) {
    XFModel::PlotSettings settings;
    settings.showGrid = true;
    XFModel::Formula surface = formula("x + y");
    const std::vector<XFModel::Formula> formulas{ surface };

    const XFPlot::PlotRenderPlan plan = XFPlot::buildPlotRenderPlan(
        XFPlot::PlotRenderPlanInput{
            std::span<const XFModel::Formula>(formulas.data(), formulas.size()),
            settings,
            sceneWith(false, true),
            XFPlot::PlotInteractionState{}
        });

    Assert::IsTrue(plan.is3DMode);
    Assert::IsTrue(plan.gridPlaneInterleave);
    Assert::AreEqual(XFPlot::PlotRenderPassKind::Surface3DBelowGrid, plan.passes[0]);
    Assert::AreEqual(XFPlot::PlotRenderPassKind::Grid3D, plan.passes[1]);
    Assert::AreEqual(XFPlot::PlotFormulaDispatchKind::ExplicitSurface3D,
                     plan.formulas[0].kind);
}

TEST_CASE(PlotRenderPlan_Force2DDispatchesSurfaceAsHeatmap) {
    XFModel::PlotSettings settings;
    settings.xyRenderModePreference = XFModel::XYRenderModePreference::Force2D;
    XFModel::Formula surface = formula("x + y");
    const std::vector<XFModel::Formula> formulas{ surface };

    const XFPlot::PlotRenderPlan plan = XFPlot::buildPlotRenderPlan(
        XFPlot::PlotRenderPlanInput{
            std::span<const XFModel::Formula>(formulas.data(), formulas.size()),
            settings,
            sceneWith(false, true),
            XFPlot::PlotInteractionState{}
        });

    Assert::IsFalse(plan.is3DMode);
    Assert::AreEqual(XFModel::XYRenderMode::Heatmap2D, plan.effective.renderMode);
    Assert::AreEqual(XFPlot::PlotFormulaDispatchKind::Heatmap2D, plan.formulas[0].kind);
}

TEST_CASE(PlotRenderPlan_RuntimeAzimuthAffectsCameraOnly) {
    XFModel::PlotSettings settings;
    settings.azimuthDeg = 30.0f;
    XFModel::Formula surface = formula("x + y");
    const std::vector<XFModel::Formula> formulas{ surface };

    const XFPlot::PlotRenderPlan interactivePlan = XFPlot::buildPlotRenderPlan(
        XFPlot::PlotRenderPlanInput{
            std::span<const XFModel::Formula>(formulas.data(), formulas.size()),
            settings,
            sceneWith(false, true),
            XFPlot::PlotInteractionState{},
            nullptr,
            nullptr,
            XFPlot::PlotQualityPurpose::Interactive,
            75.0f
        });
    const XFPlot::PlotRenderPlan exportPlan = XFPlot::buildPlotRenderPlan(
        XFPlot::PlotRenderPlanInput{
            std::span<const XFModel::Formula>(formulas.data(), formulas.size()),
            settings,
            sceneWith(false, true),
            XFPlot::PlotInteractionState{}
        });

    Assert::AreEqual(75.0f, interactivePlan.camera.azimuthDeg);
    Assert::AreEqual(30.0f, interactivePlan.effective.azimuthDeg);
    Assert::AreEqual(30.0f, exportPlan.camera.azimuthDeg);
}

TEST_CASE(PlotRenderer_SurfaceOptionsUseEffectivePolicyValues) {
    XFModel::PlotSettings settings;
    settings.wireThickness = XFModel::plotLimits().wireThickness.max;
    settings.envelopeThickness = XFModel::plotLimits().envelopeThickness.max;
    XFModel::Formula surface = formula("x + y");
    const std::vector<XFModel::Formula> formulas{ surface };

    const XFPlot::PlotRenderPlan plan = XFPlot::buildPlotRenderPlan(
        XFPlot::PlotRenderPlanInput{
            std::span<const XFModel::Formula>(formulas.data(), formulas.size()),
            settings,
            sceneWith(false, true),
            XFPlot::PlotInteractionState{}
        });
    const XFPlot::PlotRenderer::Surface3DOptions options =
        XFPlot::PlotRenderer::makeSurface3DOptions(plan.effective, plan.camera);

    Assert::AreEqual(XFModel::plotLimits().wireThickness.max, options.wireThickness);
    Assert::AreEqual(XFModel::plotLimits().envelopeThickness.max, options.envelopeThickness);
    Assert::AreEqual(XFModel::kDefaultWireThickness,
                     XFPlot::PlotRenderer::Surface3DOptions{}.wireThickness);
}

TEST_CASE(PlotQualityPolicy_InteractionThrottleReducesExpensiveSettings) {
    XFModel::PlotSettings settings;
    settings.surfaceResolution = 90;
    settings.implicitSurfaceResolution = 100;
    settings.wireThickness = 1.5f;

    const XFPlot::PlotQualityPlan plan = XFPlot::PlotQualityPolicy::resolve(
        XFPlot::PlotQualityPolicyInput{
            settings,
            sceneWith(false, true),
            XFModel::XYRenderMode::Surface3D,
            XFPlot::PlotInteractionState{ true, false }
        });

    Assert::IsTrue(plan.decision.interactiveThrottle);
    Assert::AreEqual(60, plan.effective.surfaceResolution);
    Assert::AreEqual(40, plan.effective.implicitSurfaceResolution);
    Assert::AreEqual(0.0f, plan.effective.wireThickness);
}

TEST_CASE(PlotQualityPolicy_PreviewUsesBoundedOverrideWhenNoExportQualityProvided) {
    XFModel::PlotSettings settings;
    settings.surfaceResolution = 120;
    settings.implicitSurfaceResolution = 128;

    const XFPlot::PlotQualityPlan plan = XFPlot::PlotQualityPolicy::resolve(
        XFPlot::PlotQualityPolicyInput{
            settings,
            sceneWith(false, true),
            XFModel::XYRenderMode::Surface3D,
            XFPlot::PlotInteractionState{},
            XFPlot::PlotQualityPurpose::Preview
        });

    Assert::IsTrue(plan.decision.overrideQuality);
    Assert::AreEqual(40, plan.effective.surfaceResolution);
    Assert::AreEqual(48, plan.effective.implicitSurfaceResolution);
    Assert::IsTrue(plan.effective.wireThickness < settings.wireThickness);
}

TEST_CASE(ImplicitMeshCache_HitMissAndFieldKeysAreStable) {
    XFMesh::ImplicitMeshCache cache(4);
    const XFMesh::ImplicitMeshKey key = meshKey(101, 3, 32);
    (void)cache.store(key, meshEntry(0.0));

    Assert::IsTrue(cache.find(key) != nullptr);

    XFMesh::ImplicitMeshKey changedView = key;
    changedView.xMin = -2.0;
    Assert::IsTrue(cache.find(changedView) == nullptr);

    XFMesh::ImplicitMeshKey changedResolution = key;
    changedResolution.gridResolution = 48;
    Assert::IsTrue(cache.find(changedResolution) == nullptr);

    XFMesh::ImplicitMeshKey changedRevision = key;
    changedRevision.compilationRevision = 4;
    Assert::IsTrue(cache.find(changedRevision) == nullptr);
}

TEST_CASE(ImplicitMeshCache_MultipleFormulaKeysDoNotEvictEachOther) {
    XFMesh::ImplicitMeshCache cache(4);
    const XFMesh::ImplicitMeshKey first = meshKey(201, 1, 32);
    const XFMesh::ImplicitMeshKey second = meshKey(202, 1, 32);

    (void)cache.store(first, meshEntry(0.0));
    (void)cache.store(second, meshEntry(10.0));

    Assert::AreEqual(static_cast<std::size_t>(2), cache.size());
    Assert::IsTrue(cache.find(first) != nullptr);
    Assert::IsTrue(cache.find(second) != nullptr);
}

TEST_CASE(ImplicitMeshCache_InvalidatesFormulaAndBoundsCapacity) {
    XFMesh::ImplicitMeshCache cache(2);
    const XFMesh::ImplicitMeshKey first = meshKey(301, 1, 32);
    const XFMesh::ImplicitMeshKey second = meshKey(302, 1, 32);
    const XFMesh::ImplicitMeshKey third = meshKey(303, 1, 32);

    (void)cache.store(first, meshEntry(0.0));
    (void)cache.store(second, meshEntry(10.0));
    Assert::IsTrue(cache.find(first) != nullptr);
    (void)cache.store(third, meshEntry(20.0));

    Assert::AreEqual(static_cast<std::size_t>(2), cache.size());
    Assert::IsTrue(cache.find(second) == nullptr);
    Assert::IsTrue(cache.find(first) != nullptr);
    Assert::IsTrue(cache.find(third) != nullptr);

    cache.invalidateFormula(first.formulaId);
    Assert::IsTrue(cache.find(first) == nullptr);
}

} // namespace XpressFormulaTests
