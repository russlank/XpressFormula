// SPDX-License-Identifier: MIT
// ExportController.h - Non-UI export workflow state controller.
#pragma once

#include "../Infrastructure/Export/ExportSettings.h"

#include <chrono>
#include <string>

namespace XpressFormula::Application {

class ExportController {
public:
    using Clock = std::chrono::steady_clock;

    ExportController();

    void requestOpen();
    [[nodiscard]] bool consumeOpenRequest();
    void closeDialog();

    [[nodiscard]] bool dialogOpen() const noexcept { return m_dialogOpen; }
    void setDialogOpen(bool open) noexcept { m_dialogOpen = open; }

    [[nodiscard]] bool popupOpenNextFrame() const noexcept { return m_popupOpenNextFrame; }
    void requestPopupOpenNextFrame() noexcept { m_popupOpenNextFrame = true; }
    void clearPopupOpenNextFrame() noexcept { m_popupOpenNextFrame = false; }

    [[nodiscard]] bool centerOnOpen() const noexcept { return m_centerOnOpen; }
    void clearCenterOnOpen() noexcept { m_centerOnOpen = false; }

    [[nodiscard]] bool sizeInitialized() const noexcept { return m_sizeInitialized; }
    void setSizeInitialized(bool initialized) noexcept { m_sizeInitialized = initialized; }

    [[nodiscard]] float settingsPaneWidth() const noexcept { return m_settingsPaneWidth; }
    float& settingsPaneWidthRef() noexcept { return m_settingsPaneWidth; }

    [[nodiscard]] Infrastructure::Export::ExportSettings& dialogSettings() noexcept {
        return m_dialogSettings;
    }
    [[nodiscard]] const Infrastructure::Export::ExportSettings& dialogSettings() const noexcept {
        return m_dialogSettings;
    }

    [[nodiscard]] const Infrastructure::Export::ExportSettings& pendingSettings() const noexcept {
        return m_pendingSettings;
    }

    void queueSave(const Infrastructure::Export::ExportSettings& settings);
    void queueCopy(const Infrastructure::Export::ExportSettings& settings);
    void promoteScheduledActions();
    void clearPendingActions();

    [[nodiscard]] bool scheduledSave() const noexcept { return m_scheduledSave; }
    [[nodiscard]] bool scheduledCopy() const noexcept { return m_scheduledCopy; }
    [[nodiscard]] bool pendingSave() const noexcept { return m_pendingSave; }
    [[nodiscard]] bool pendingCopy() const noexcept { return m_pendingCopy; }
    [[nodiscard]] bool hasPendingActions() const noexcept { return m_pendingSave || m_pendingCopy; }

    [[nodiscard]] bool exportBusy() const noexcept {
        return m_scheduledSave || m_scheduledCopy || m_pendingSave || m_pendingCopy ||
               m_previewRefreshRequested;
    }

    void markPreviewOutOfDate(bool autoRefreshPreview);
    void requestPreviewRefresh();
    void requestFinalQualityPreview();
    [[nodiscard]] bool consumePreviewRefreshRequest();
    [[nodiscard]] Infrastructure::Export::ExportPreviewQuality consumePreviewQuality(
        Infrastructure::Export::ExportPreviewQuality defaultQuality);
    bool tickAutoPreviewRefresh(Clock::time_point now,
                                std::chrono::milliseconds debounce =
                                    std::chrono::milliseconds(350));

    [[nodiscard]] bool previewDirty() const noexcept { return m_previewDirty; }
    [[nodiscard]] bool previewRefreshRequested() const noexcept { return m_previewRefreshRequested; }
    void setPreviewDirty(bool dirty) noexcept { m_previewDirty = dirty; }

    [[nodiscard]] float previewZoom() const noexcept { return m_previewZoom; }
    float& previewZoomRef() noexcept { return m_previewZoom; }
    float& previewPanXRef() noexcept { return m_previewPanX; }
    float& previewPanYRef() noexcept { return m_previewPanY; }
    bool& previewCheckerboardRef() noexcept { return m_previewCheckerboard; }

    [[nodiscard]] float previewPanX() const noexcept { return m_previewPanX; }
    [[nodiscard]] float previewPanY() const noexcept { return m_previewPanY; }
    [[nodiscard]] bool previewCheckerboard() const noexcept { return m_previewCheckerboard; }

    void resetPreviewNavigation() noexcept;

    [[nodiscard]] std::string& previewStatus() noexcept { return m_previewStatus; }
    [[nodiscard]] const std::string& previewStatus() const noexcept { return m_previewStatus; }
    void setPreviewStatus(std::string status);

    [[nodiscard]] std::string& status() noexcept { return m_status; }
    [[nodiscard]] const std::string& status() const noexcept { return m_status; }
    void setStatus(std::string status);

    [[nodiscard]] const std::wstring& lastSavedPath() const noexcept { return m_lastSavedPath; }
    void setLastSavedPath(std::wstring path);

private:
    bool m_dialogOpen = false;
    bool m_openRequested = false;
    bool m_popupOpenNextFrame = false;
    bool m_centerOnOpen = false;
    bool m_sizeInitialized = false;
    float m_settingsPaneWidth = 420.0f;

    Infrastructure::Export::ExportSettings m_dialogSettings;
    Infrastructure::Export::ExportSettings m_pendingSettings;
    bool m_scheduledSave = false;
    bool m_scheduledCopy = false;
    bool m_pendingSave = false;
    bool m_pendingCopy = false;

    bool m_previewDirty = false;
    bool m_previewRefreshRequested = false;
    bool m_previewUseFinalQualityOnce = false;
    Clock::time_point m_previewLastChanged;
    float m_previewZoom = 0.0f;
    float m_previewPanX = 0.0f;
    float m_previewPanY = 0.0f;
    bool m_previewCheckerboard = true;
    std::string m_previewStatus;
    std::string m_status;
    std::wstring m_lastSavedPath;
};

} // namespace XpressFormula::Application
