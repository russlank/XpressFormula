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
