// UpdateController.cpp - Non-UI update-check workflow controller.
#include "UpdateController.h"

#include "../Core/UpdateVersionUtils.h"
#include "../Platform/Windows/WinHttpClient.h"
#include "../Version.h"

#include <exception>
#include <sstream>
#include <utility>

namespace XpressFormula::Application {

namespace XFWin = XpressFormula::Platform::Windows;

namespace {

constexpr const wchar_t* kGitHubLatestReleaseApiHost = L"api.github.com";
constexpr const wchar_t* kGitHubLatestReleaseApiPath =
    L"/repos/russlank/XpressFormula/releases/latest";
constexpr const char* kGitHubReleasesUrlUtf8 =
    "https://github.com/russlank/XpressFormula/releases";
constexpr const char* kBuyMeACoffeeUrlUtf8 = "https://buymeacoffee.com/russlank";

UpdateCheckResult makeWorkerFailure(bool manualRequest, std::string message) {
    UpdateCheckResult result;
    result.manualRequest = manualRequest;
    result.releaseUrl = std::string(kGitHubReleasesUrlUtf8);
    result.statusMessage = std::move(message);
    return result;
}

} // namespace

UpdateController::UpdateController(ReleaseFetcher releaseFetcher)
    : m_releaseFetcher(std::move(releaseFetcher)),
      m_releaseUrl(kGitHubReleasesUrlUtf8) {
    if (!m_releaseFetcher) {
        m_releaseFetcher = fetchLatestReleaseFromGitHub;
    }
}

UpdateController::~UpdateController() {
    (void)waitForPendingCheck();
}

void UpdateController::resetStartupDelay(Clock::time_point now) noexcept {
    m_startupTime = now;
    m_startupCheckDone = false;
}

bool UpdateController::tick(Clock::time_point now) {
    bool changed = false;

    if (!m_startupCheckDone && !m_checkInProgress &&
        now - m_startupTime >= m_startupDelay) {
        changed = startCheck(false) || changed;
    }

    changed = poll() || changed;
    return changed;
}

bool UpdateController::requestManualCheck() {
    return startCheck(true);
}

bool UpdateController::poll() {
    if (!m_checkInProgress || !m_future.valid()) {
        return false;
    }

    if (m_future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return false;
    }

    applyResult(m_future.get());
    return true;
}

bool UpdateController::waitForPendingCheck() {
    if (!m_future.valid()) {
        m_checkInProgress = false;
        return false;
    }

    m_future.wait();
    applyResult(m_future.get());
    return true;
}

UpdateNotificationState UpdateController::notificationState() const {
    UpdateNotificationState state;
    state.checkInProgress = m_checkInProgress;
    state.updateAvailable = m_updateAvailable;
    state.noticeDismissed = m_noticeDismissed;
    state.versionDetailsExpanded = m_versionDetailsExpanded;
    state.latestTag = m_latestTag;
    state.releaseUrl = m_releaseUrl;
    state.status = m_status;
    return state;
}

void UpdateController::setVersionDetailsExpanded(bool expanded) noexcept {
    m_versionDetailsExpanded = expanded;
}

bool UpdateController::dismissNotice() noexcept {
    if (!m_updateAvailable || m_noticeDismissed) {
        return false;
    }

    m_noticeDismissed = true;
    return true;
}

bool UpdateController::recordReleasePageOpenResult(bool opened) {
    if (opened) {
        if (!m_updateAvailable || m_noticeDismissed) {
            return false;
        }

        m_noticeDismissed = true;
        return true;
    }

    const std::string target = m_releaseUrl.empty()
        ? std::string(kGitHubReleasesUrlUtf8)
        : m_releaseUrl;
    const std::string newStatus = "Could not open browser. Visit: " + target;
    if (m_status == newStatus) {
        return false;
    }

    m_status = newStatus;
    return true;
}

bool UpdateController::recordSupportPageOpenResult(bool opened) {
    if (opened) {
        return false;
    }

    const std::string newStatus =
        "Could not open browser. Visit: " + std::string(kBuyMeACoffeeUrlUtf8);
    if (m_status == newStatus) {
        return false;
    }

    m_status = newStatus;
    return true;
}

std::string_view UpdateController::defaultReleaseUrlUtf8() noexcept {
    return kGitHubReleasesUrlUtf8;
}

std::string_view UpdateController::supportUrlUtf8() noexcept {
    return kBuyMeACoffeeUrlUtf8;
}

bool UpdateController::startCheck(bool manualRequest) {
    if (m_checkInProgress) {
        if (manualRequest && m_status != "Update check already in progress.") {
            m_status = "Update check already in progress.";
            return true;
        }
        return false;
    }

    if (m_future.valid()) {
        if (m_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            applyResult(m_future.get());
        } else if (manualRequest) {
            if (m_status != "Previous update check is still running.") {
                m_status = "Previous update check is still running.";
                return true;
            }
            return false;
        } else {
            return false;
        }
    }

    m_checkInProgress = true;
    m_startupCheckDone = true;
    m_status = manualRequest
        ? "Checking GitHub releases..."
        : "Checking for updates in background...";

    try {
        ReleaseFetcher fetcher = m_releaseFetcher;
        m_future = std::async(std::launch::async, [fetcher = std::move(fetcher), manualRequest]() {
            try {
                return fetcher(manualRequest);
            } catch (const std::exception& ex) {
                return makeWorkerFailure(
                    manualRequest,
                    std::string("Could not complete update check worker: ") + ex.what());
            } catch (...) {
                return makeWorkerFailure(manualRequest, "Could not complete update check worker.");
            }
        });
    } catch (const std::exception& ex) {
        m_checkInProgress = false;
        m_status = std::string("Could not start update check worker: ") + ex.what();
    } catch (...) {
        m_checkInProgress = false;
        m_status = "Could not start update check worker.";
    }

    return true;
}

void UpdateController::applyResult(UpdateCheckResult result) {
    m_checkInProgress = false;

    if (!result.releaseUrl.empty()) {
        m_releaseUrl = std::move(result.releaseUrl);
    } else {
        m_releaseUrl = kGitHubReleasesUrlUtf8;
    }

    m_latestTag = std::move(result.latestTag);
    m_updateAvailable = result.requestSucceeded && result.updateAvailable;
    if (!m_updateAvailable && result.manualRequest) {
        m_noticeDismissed = false;
    }
    if (result.updateAvailable) {
        m_noticeDismissed = false;
    }

    if (!result.statusMessage.empty()) {
        m_status = std::move(result.statusMessage);
    } else if (result.requestSucceeded) {
        m_status = result.updateAvailable
            ? "Update available."
            : "You are running the latest version.";
    } else {
        m_status = "Update check failed.";
    }
}

UpdateCheckResult fetchLatestReleaseFromGitHub(bool manualRequest) {
    using namespace XpressFormula::Core::UpdateVersionUtils;

    UpdateCheckResult result;
    result.manualRequest = manualRequest;
    result.releaseUrl = std::string(kGitHubReleasesUrlUtf8);

    XFWin::WinHttpGetRequest request;
    request.userAgent = L"XpressFormula/" XF_VERSION_WSTRING;
    request.host = kGitHubLatestReleaseApiHost;
    request.path = kGitHubLatestReleaseApiPath;
    request.headers =
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";

    const XFWin::WinHttpResponse response = XFWin::WinHttpClient{}.get(request);
    if (!response) {
        result.statusMessage = "Update check failed: " + response.error;
        return result;
    }

    if (response.statusCode != 200) {
        std::ostringstream oss;
        oss << "Update check failed: GitHub returned HTTP " << response.statusCode << ".";
        result.statusMessage = oss.str();
        return result;
    }

    if (response.body.empty()) {
        result.statusMessage = "Update check failed: empty response from GitHub.";
        return result;
    }

    const XFWin::GitHubReleaseParseResult parsed =
        XFWin::parseGitHubLatestReleaseResponse(response.body);
    if (!parsed) {
        result.statusMessage = "Update check failed: " + parsed.error;
        return result;
    }
    if (!parsed.release.htmlUrl.empty()) {
        result.releaseUrl = parsed.release.htmlUrl;
    }

    result.requestSucceeded = true;
    result.latestTag = parsed.release.tagName;
    result.updateAvailable = isRemoteVersionNewer(XF_VERSION_STRING, parsed.release.tagName);

    if (result.updateAvailable) {
        result.statusMessage = "Update available: " + parsed.release.tagName +
            " (current " XF_VERSION_STRING ").";
    } else {
        result.statusMessage = "You are running the latest version (" XF_VERSION_STRING ").";
    }
    return result;
}

} // namespace XpressFormula::Application
