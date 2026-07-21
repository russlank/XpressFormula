// SPDX-License-Identifier: MIT
// ExportController.cpp - Non-UI export workflow state controller.
#include "ExportController.h"

#include <utility>

namespace XpressFormula::Application {

namespace XFExport = XpressFormula::Infrastructure::Export;

ExportController::ExportController()
    : m_dialogSettings(XFExport::defaultExportSettings()),
      m_pendingSettings(m_dialogSettings),
      m_previewLastChanged(Clock::now()) {
}

void ExportController::requestOpen() {
    m_openRequested = true;
}

bool ExportController::consumeOpenRequest() {
    if (!m_openRequested) {
        return false;
    }

    m_openRequested = false;
    m_dialogOpen = true;
    m_popupOpenNextFrame = true;
    m_centerOnOpen = true;
    m_sizeInitialized = false;
    resetPreviewNavigation();
    m_previewDirty = true;
    m_previewRefreshRequested = false;
    m_previewUseFinalQualityOnce = false;
    m_previewLastChanged = Clock::now();
    m_previewStatus = "Preview out of date.";
    return true;
}

void ExportController::closeDialog() {
    m_dialogOpen = false;
    m_popupOpenNextFrame = false;
}

void ExportController::queueSave(const XFExport::ExportSettings& settings) {
    m_pendingSettings = settings;
    m_scheduledSave = true;
    m_scheduledCopy = false;
    m_status = "Save export queued.";
}

void ExportController::queueCopy(const XFExport::ExportSettings& settings) {
    m_pendingSettings = settings;
    m_scheduledCopy = true;
    m_scheduledSave = false;
    m_status = "Clipboard export queued.";
}

void ExportController::promoteScheduledActions() {
    if (m_scheduledSave || m_scheduledCopy) {
        m_pendingSave = m_pendingSave || m_scheduledSave;
        m_pendingCopy = m_pendingCopy || m_scheduledCopy;
        m_scheduledSave = false;
        m_scheduledCopy = false;
    }
}

void ExportController::clearPendingActions() {
    m_pendingSave = false;
    m_pendingCopy = false;
}

void ExportController::markPreviewOutOfDate(bool autoRefreshPreview) {
    m_previewDirty = true;
    m_previewLastChanged = Clock::now();
    m_previewStatus = autoRefreshPreview
        ? "Preview out of date. Auto refresh pending."
        : "Preview out of date.";
}

void ExportController::requestPreviewRefresh() {
    m_previewDirty = true;
    m_previewRefreshRequested = true;
    m_previewStatus = "Rendering preview...";
}

void ExportController::requestFinalQualityPreview() {
    m_previewUseFinalQualityOnce = true;
    requestPreviewRefresh();
}

bool ExportController::consumePreviewRefreshRequest() {
    if (!m_previewRefreshRequested) {
        return false;
    }
    m_previewRefreshRequested = false;
    return true;
}

XFExport::ExportPreviewQuality ExportController::consumePreviewQuality(
    XFExport::ExportPreviewQuality defaultQuality) {
    if (!m_previewUseFinalQualityOnce) {
        return defaultQuality;
    }
    m_previewUseFinalQualityOnce = false;
    return XFExport::ExportPreviewQuality::Final;
}

bool ExportController::tickAutoPreviewRefresh(Clock::time_point now,
                                              std::chrono::milliseconds debounce) {
    if (!m_dialogOpen || !m_dialogSettings.quality.autoRefreshPreview ||
        !m_previewDirty || m_previewRefreshRequested) {
        return false;
    }

    if (now - m_previewLastChanged < debounce) {
        return false;
    }

    m_previewRefreshRequested = true;
    m_previewStatus = "Rendering preview...";
    return true;
}

void ExportController::resetPreviewNavigation() noexcept {
    m_previewZoom = 0.0f;
    m_previewPanX = 0.0f;
    m_previewPanY = 0.0f;
}

void ExportController::setPreviewStatus(std::string status) {
    m_previewStatus = std::move(status);
}

void ExportController::setStatus(std::string status) {
    m_status = std::move(status);
}

void ExportController::setLastSavedPath(std::wstring path) {
    m_lastSavedPath = std::move(path);
}

} // namespace XpressFormula::Application
