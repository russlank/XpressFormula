#include "CppUnitTest.h"

#include "../XpressFormula/Application/ApplicationState.h"

using Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace XFApp = XpressFormula::Application;
namespace XFModel = XpressFormula::Model;

TEST_CASE(ApplicationState_RenderPolicyWaitsOnlyWhenOptimizedAndIdle) {
    XFApp::RenderFramePolicyInput input;
    input.optimizeRendering = true;
    input.redrawRequested = false;
    input.autoRotate = false;
    input.sceneHasVisible3D = true;
    input.effectiveRenderMode = XFModel::XYRenderMode::Surface3D;

    Assert::IsFalse(XFApp::isContinuousRenderRequired(input));
    Assert::IsTrue(XFApp::shouldWaitForNextMessage(input));
    Assert::IsFalse(XFApp::redrawRequestedAfterPresentedFrame(input));
    Assert::AreEqual(30, XFApp::occludedSwapChainSleepMilliseconds(input));

    input.redrawRequested = true;
    Assert::IsFalse(XFApp::shouldWaitForNextMessage(input));

    input.optimizeRendering = false;
    input.redrawRequested = false;
    Assert::IsFalse(XFApp::shouldWaitForNextMessage(input));
    Assert::IsTrue(XFApp::redrawRequestedAfterPresentedFrame(input));
}

TEST_CASE(ApplicationState_RenderPolicyKeepsAutoRotateSurfaceContinuous) {
    XFApp::RenderFramePolicyInput input;
    input.optimizeRendering = true;
    input.redrawRequested = false;
    input.autoRotate = true;
    input.sceneHasVisible3D = true;
    input.effectiveRenderMode = XFModel::XYRenderMode::Surface3D;

    Assert::IsTrue(XFApp::isContinuousRenderRequired(input));
    Assert::IsFalse(XFApp::shouldWaitForNextMessage(input));
    Assert::IsTrue(XFApp::redrawRequestedAfterPresentedFrame(input));
    Assert::AreEqual(10, XFApp::occludedSwapChainSleepMilliseconds(input));

    input.effectiveRenderMode = XFModel::XYRenderMode::Heatmap2D;
    Assert::IsFalse(XFApp::isContinuousRenderRequired(input));
    Assert::IsTrue(XFApp::shouldWaitForNextMessage(input));
    Assert::AreEqual(30, XFApp::occludedSwapChainSleepMilliseconds(input));
}

TEST_CASE(ApplicationState_AutoRotationIsRuntimeOnlyAndResettable) {
    XFModel::Document document;
    XFModel::PlotSettings settings = document.plotSettings();
    settings.azimuthDeg = 170.0f;
    settings.autoRotate = true;
    settings.autoRotateSpeedDegPerSec = 25.0f;
    Assert::IsTrue(document.setPlotSettings(settings));
    document.markSaved();
    const XFModel::Document::Revision cleanRevision = document.revision();

    XFApp::PlotRuntimeState runtime;
    Assert::IsTrue(XFApp::advanceAutoRotation(runtime, document.plotSettings(), 1.0f));

    Assert::AreEqual(cleanRevision, document.revision());
    Assert::IsFalse(document.dirty());
    Assert::AreEqual(170.0f, document.plotSettings().azimuthDeg);
    Assert::AreEqual(-165.0f, XFApp::effectiveRuntimeAzimuthDeg(document.plotSettings(), runtime));

    runtime.resetAutoRotation();
    Assert::AreEqual(0.0f, runtime.autoRotationOffsetDeg);
    Assert::AreEqual(170.0f, XFApp::effectiveRuntimeAzimuthDeg(document.plotSettings(), runtime));
}

TEST_CASE(ApplicationState_AutoRotationDoesNotAdvanceWhenDisabled) {
    XFModel::PlotSettings settings;
    settings.autoRotate = false;
    settings.autoRotateSpeedDegPerSec = 90.0f;

    XFApp::PlotRuntimeState runtime;
    runtime.autoRotationOffsetDeg = 30.0f;

    Assert::IsFalse(XFApp::advanceAutoRotation(runtime, settings, 1.0f));
    Assert::AreEqual(30.0f, runtime.autoRotationOffsetDeg);
}
