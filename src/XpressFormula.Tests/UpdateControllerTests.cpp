#include "CppUnitTest.h"

#include "../XpressFormula/Application/UpdateController.h"

#include <chrono>
#include <future>
#include <memory>
#include <string>

using Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace XFApp = XpressFormula::Application;

namespace {

XFApp::UpdateCheckResult successfulUpdate(bool manualRequest,
                                          std::string tag = "9.9.9",
                                          bool updateAvailable = true) {
    XFApp::UpdateCheckResult result;
    result.requestSucceeded = true;
    result.updateAvailable = updateAvailable;
    result.manualRequest = manualRequest;
    result.latestTag = std::move(tag);
    result.releaseUrl = "https://example.test/releases/latest";
    result.statusMessage = result.updateAvailable
        ? "Update available."
        : "You are running the latest version.";
    return result;
}

} // namespace

TEST_CASE(UpdateController_DelayedStartupCheckWaitsForPolicyWindow) {
    int calls = 0;
    XFApp::UpdateController controller([&](bool manualRequest) {
        ++calls;
        return successfulUpdate(manualRequest, "2.0.0", true);
    });

    const XFApp::UpdateController::Clock::time_point start{};
    controller.resetStartupDelay(start);

    Assert::IsFalse(controller.tick(start + std::chrono::seconds(59)));
    Assert::AreEqual(0, calls);
    Assert::IsFalse(controller.startupCheckDone());

    Assert::IsTrue(controller.tick(start + std::chrono::seconds(60)));
    Assert::IsTrue(controller.startupCheckDone());

    Assert::IsTrue(controller.waitForPendingCheck());
    Assert::AreEqual(1, calls);
    const XFApp::UpdateNotificationState state = controller.notificationState();
    Assert::IsTrue(state.updateAvailable);
    Assert::AreEqual(std::string("2.0.0"), state.latestTag);
}

TEST_CASE(UpdateController_ManualRequestReportsSuccessAndDismissesNotice) {
    bool manualSeen = false;
    XFApp::UpdateController controller([&](bool manualRequest) {
        manualSeen = manualRequest;
        return successfulUpdate(manualRequest, "3.4.5", true);
    });

    Assert::IsTrue(controller.requestManualCheck());
    Assert::IsTrue(controller.checkInProgress());
    Assert::AreEqual(std::string("Checking GitHub releases..."), controller.status());

    Assert::IsTrue(controller.waitForPendingCheck());
    Assert::IsTrue(manualSeen);
    Assert::IsFalse(controller.checkInProgress());
    Assert::IsTrue(controller.updateAvailable());
    Assert::AreEqual(std::string("3.4.5"), controller.latestTag());
    Assert::IsFalse(controller.noticeDismissed());

    Assert::IsTrue(controller.dismissNotice());
    Assert::IsTrue(controller.noticeDismissed());
    Assert::IsFalse(controller.dismissNotice());
}

TEST_CASE(UpdateController_FailureKeepsNotificationNonAvailable) {
    XFApp::UpdateController controller([](bool manualRequest) {
        XFApp::UpdateCheckResult result;
        result.manualRequest = manualRequest;
        result.releaseUrl = "https://example.test/releases";
        result.statusMessage = "Update check failed: offline.";
        return result;
    });

    Assert::IsTrue(controller.requestManualCheck());
    Assert::IsTrue(controller.waitForPendingCheck());

    Assert::IsFalse(controller.updateAvailable());
    Assert::IsFalse(controller.noticeDismissed());
    Assert::AreEqual(std::string("Update check failed: offline."), controller.status());
    Assert::AreEqual(std::string("https://example.test/releases"), controller.releaseUrl());
}

TEST_CASE(UpdateController_InProgressManualRequestDoesNotLaunchSecondWorker) {
    int calls = 0;
    auto gate = std::make_shared<std::promise<void>>();
    std::shared_future<void> releaseGate = gate->get_future().share();
    XFApp::UpdateController controller([&](bool manualRequest) {
        ++calls;
        releaseGate.wait();
        return successfulUpdate(manualRequest, "4.0.0", false);
    });

    Assert::IsTrue(controller.requestManualCheck());
    Assert::IsTrue(controller.checkInProgress());
    Assert::IsTrue(controller.requestManualCheck());
    Assert::AreEqual(std::string("Update check already in progress."), controller.status());

    gate->set_value();
    Assert::IsTrue(controller.waitForPendingCheck());
    Assert::AreEqual(1, calls);
    Assert::IsFalse(controller.updateAvailable());
}

TEST_CASE(UpdateController_OpenReleasePageResultUpdatesNoticeAndStatus) {
    XFApp::UpdateController controller([](bool manualRequest) {
        return successfulUpdate(manualRequest, "5.0.0", true);
    });

    Assert::IsTrue(controller.requestManualCheck());
    Assert::IsTrue(controller.waitForPendingCheck());
    Assert::IsTrue(controller.updateAvailable());
    Assert::IsFalse(controller.noticeDismissed());

    Assert::IsTrue(controller.recordReleasePageOpenResult(true));
    Assert::IsTrue(controller.noticeDismissed());
    Assert::IsFalse(controller.recordReleasePageOpenResult(true));

    Assert::IsTrue(controller.recordReleasePageOpenResult(false));
    Assert::IsTrue(controller.status().find("Could not open browser. Visit: ") == 0);
    Assert::IsTrue(controller.recordSupportPageOpenResult(false));
    Assert::IsTrue(controller.status().find("buymeacoffee.com") != std::string::npos);
}
