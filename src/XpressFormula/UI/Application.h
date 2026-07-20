// SPDX-License-Identifier: MIT
// Application.h - Main application class: owns the window, D3D11 device,
//                  ImGui context, and orchestrates the UI panels.
#pragma once

#include "FormulaPanel.h"
#include "ControlPanel.h"
#include "ExportSettings.h"
#include "PlotPanel.h"
#include "PlotSettings.h"
#include "../Application/ExportController.h"
#include "../Application/ExportPreviewTexture.h"
#include "../Core/ViewTransform.h"
#include "../Infrastructure/Persistence/ProjectMapper.h"
#include "../Infrastructure/Persistence/ProjectRepository.h"
#include "../Infrastructure/Persistence/RecentProjectsStore.h"
#include "../Model/Formula.h"
#include "../Model/SceneSummary.h"
#include "../Platform/Windows/ClipboardService.h"
#include "../Platform/Windows/FileDialogService.h"
#include "../Platform/Windows/ShellService.h"
#include "../Platform/Windows/WicImageEncoder.h"

#include <chrono>
#include <cstdint>
#include <d3d11.h>
#include <array>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

struct HWND__;
typedef HWND__* HWND;
struct HINSTANCE__;
typedef HINSTANCE__* HINSTANCE;

namespace XpressFormula::UI {

class Application {
public:
    struct UpdateCheckResult {
        bool requestSucceeded = false;
        bool updateAvailable = false;
        bool manualRequest = false;
        std::string latestTag;
        std::string releaseUrl;
        std::string statusMessage;
    };

    Application();
    ~Application();

    /// Create the Win32 window and Direct3D 11 device, then initialise ImGui.
    bool initialize(HINSTANCE hInstance, int width = 1400, int height = 900);

    /// Enter the main loop. Returns the process exit code.
    int run();

    /// Tear everything down.
    void shutdown();

    /// Queue or prompt a close request; callers should not destroy the HWND directly.
    bool requestClose();

    // --- D3D11 helpers (also used by the global WndProc) ---
    void createRenderTarget();
    void cleanupRenderTarget();

    UINT resizeWidth  = 0;
    UINT resizeHeight = 0;

private:
    enum class PendingProjectAction {
        None,
        NewProject,
        OpenDialog,
        OpenRecent,
        CloseApp
    };

    bool createDeviceD3D(HWND hWnd);
    void cleanupDeviceD3D();
    void renderFrame();
    void renderProjectControls();
    void renderProjectDiscardDialog();
    void handleProjectShortcuts();
    void requestProjectAction(PendingProjectAction action, std::wstring path = {});
    void executeProjectAction(PendingProjectAction action, const std::wstring& path);
    void resetToDefaultProject();
    void refreshSceneSummary();
    Infrastructure::Persistence::ProjectSession currentProjectSession() const;
    void refreshProjectDirtyState();
    void markProjectClean();
    bool saveProject();
    bool saveProjectAs();
    bool saveProjectToPath(const std::wstring& path, std::string& error);
    bool openProjectFromDialog();
    bool openProjectFromPath(const std::wstring& path, std::string& error);
    void addRecentProjectPath(const std::wstring& path);
    void loadRecentProjectPaths();
    void saveRecentProjectPaths() const;
    std::string projectDisplayName() const;
    void renderPlotToolbar(const Model::SceneSummary& scene);
    void handlePlotShortcuts();
    void fitDefaultView();
    void resetViewAndCamera();
    void applyCameraPreset(float azimuthDeg, float elevationDeg);
    void startUpdateCheck(bool manualRequest);
    void pollUpdateCheckResult();
    void markExportPreviewOutOfDate();
    void requestExportPreviewRefresh();
    bool capturePlotPixels(std::vector<std::uint8_t>& pixels, int& width, int& height);
    bool renderPlotPixelsOffscreen(const ExportSettings& settings,
                                   std::vector<std::uint8_t>& pixels, int& width, int& height);
    bool readTexturePixelsRgba(ID3D11Texture2D* sourceTexture,
                               std::vector<std::uint8_t>& pixels,
                               int& width, int& height);
    void renderExportDialog(float sidebarWidth, float viewportHeight);
    void initialiseExportDialogSize();
    bool refreshExportPreviewTexture();
    bool writeExportMetadataSidecar(const ExportSettings& settings,
                                    const std::wstring& imagePath,
                                    int width, int height,
                                    std::string& error) const;
    void openLastSavedExport();
    void showLastSavedExportInFolder();
    void copyLastSavedExportPath();
    void processPendingExportActions();

    HWND                      m_hWnd               = nullptr;
    ID3D11Device*             m_device              = nullptr;
    ID3D11DeviceContext*      m_deviceContext        = nullptr;
    IDXGISwapChain*           m_swapChain           = nullptr;
    ID3D11RenderTargetView*   m_renderTargetView    = nullptr;
    bool                      m_swapChainOccluded   = false;
    bool                      m_comInitialized      = false;
    bool                      m_redrawRequested     = true;
    bool                      m_closeRequestedAfterFrame = false;

    // Application state
    std::vector<Model::Formula> m_formulas;
    Model::SceneSummary       m_sceneSummary;
    Core::ViewTransform       m_viewTransform;
    PlotSettings              m_plotSettings;
    std::wstring              m_projectPath;
    bool                      m_projectDirty = false;
    std::string               m_savedProjectSnapshot;
    std::string               m_projectStatus;
    std::vector<std::wstring> m_recentProjectPaths;
    Infrastructure::Persistence::ProjectRepository m_projectRepository;
    Infrastructure::Persistence::RecentProjectsStore m_recentProjectsStore;
    Platform::Windows::FileDialogService m_fileDialogService;
    Platform::Windows::ShellService m_shellService;
    Platform::Windows::ClipboardService m_clipboardService;
    Platform::Windows::WicImageEncoder m_imageEncoder;
    PendingProjectAction      m_pendingProjectAction = PendingProjectAction::None;
    std::wstring              m_pendingProjectPath;
    bool                      m_openProjectDiscardPopupNextFrame = false;
    float                     m_sidebarWidth = 360.0f;
    XpressFormula::Application::ExportController m_exportController;
    XpressFormula::Application::ExportPreviewTexture m_exportPreview;
    std::future<UpdateCheckResult> m_updateCheckFuture;
    bool                      m_updateCheckInProgress = false;
    bool                      m_startupCheckDone = false;
    std::chrono::steady_clock::time_point m_startupTime;
    bool                      m_updateAvailable = false;
    bool                      m_updateNoticeDismissed = false;
    bool                      m_versionDetailsExpanded = false;
    std::string               m_updateLatestTag;
    std::string               m_updateReleaseUrl = "https://github.com/russlank/XpressFormula/releases";
    std::string               m_updateStatus;

    // UI panels
    FormulaPanel  m_formulaPanel;
    ControlPanel  m_controlPanel;
    PlotPanel     m_plotPanel;
};

} // namespace XpressFormula::UI
