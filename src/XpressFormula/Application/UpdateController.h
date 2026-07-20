// UpdateController.h - Non-UI update-check workflow controller.
#pragma once

#include <chrono>
#include <future>
#include <functional>
#include <string>
#include <string_view>

namespace XpressFormula::Application {

struct UpdateCheckResult {
    bool requestSucceeded = false;
    bool updateAvailable = false;
    bool manualRequest = false;
    std::string latestTag;
    std::string releaseUrl;
    std::string statusMessage;
};

struct UpdateNotificationState {
    bool checkInProgress = false;
    bool updateAvailable = false;
    bool noticeDismissed = false;
    bool versionDetailsExpanded = false;
    std::string latestTag;
    std::string releaseUrl;
    std::string status;
};

class UpdateController {
public:
    using Clock = std::chrono::steady_clock;
    using ReleaseFetcher = std::function<UpdateCheckResult(bool manualRequest)>;

    explicit UpdateController(ReleaseFetcher releaseFetcher = {});
    ~UpdateController();

    UpdateController(const UpdateController&) = delete;
    UpdateController& operator=(const UpdateController&) = delete;
    UpdateController(UpdateController&&) = delete;
    UpdateController& operator=(UpdateController&&) = delete;

    void resetStartupDelay(Clock::time_point now = Clock::now()) noexcept;
    [[nodiscard]] bool tick(Clock::time_point now = Clock::now());
    [[nodiscard]] bool requestManualCheck();
    [[nodiscard]] bool poll();
    [[nodiscard]] bool waitForPendingCheck();

    [[nodiscard]] UpdateNotificationState notificationState() const;
    [[nodiscard]] bool checkInProgress() const noexcept { return m_checkInProgress; }
    [[nodiscard]] bool startupCheckDone() const noexcept { return m_startupCheckDone; }
    [[nodiscard]] bool updateAvailable() const noexcept { return m_updateAvailable; }
    [[nodiscard]] bool noticeDismissed() const noexcept { return m_noticeDismissed; }
    [[nodiscard]] bool versionDetailsExpanded() const noexcept { return m_versionDetailsExpanded; }
    [[nodiscard]] const std::string& latestTag() const noexcept { return m_latestTag; }
    [[nodiscard]] const std::string& releaseUrl() const noexcept { return m_releaseUrl; }
    [[nodiscard]] const std::string& status() const noexcept { return m_status; }

    void setVersionDetailsExpanded(bool expanded) noexcept;
    [[nodiscard]] bool dismissNotice() noexcept;
    [[nodiscard]] bool recordReleasePageOpenResult(bool opened);
    [[nodiscard]] bool recordSupportPageOpenResult(bool opened);

    [[nodiscard]] static std::string_view defaultReleaseUrlUtf8() noexcept;
    [[nodiscard]] static std::string_view supportUrlUtf8() noexcept;

private:
    [[nodiscard]] bool startCheck(bool manualRequest);
    void applyResult(UpdateCheckResult result);

    ReleaseFetcher m_releaseFetcher;
    std::future<UpdateCheckResult> m_future;
    bool m_checkInProgress = false;
    bool m_startupCheckDone = false;
    Clock::time_point m_startupTime = Clock::now();
    std::chrono::seconds m_startupDelay{60};

    bool m_updateAvailable = false;
    bool m_noticeDismissed = false;
    bool m_versionDetailsExpanded = false;
    std::string m_latestTag;
    std::string m_releaseUrl;
    std::string m_status;
};

[[nodiscard]] UpdateCheckResult fetchLatestReleaseFromGitHub(bool manualRequest);

} // namespace XpressFormula::Application
