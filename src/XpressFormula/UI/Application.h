// SPDX-License-Identifier: MIT
// Application.h - Main application class: owns the window, D3D11 device,
//                  ImGui context, and orchestrates the UI panels.
#pragma once

#include "FormulaEntry.h"
#include "FormulaPanel.h"
#include "ControlPanel.h"
#include "ExportSettings.h"
#include "PlotPanel.h"
#include "PlotSettings.h"
#include "ProjectSession.h"
#include "../Core/ViewTransform.h"

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
    struct ExportDialogSettings {
        int width = 0;
        int height = 0;
        int scale = 1;
        int selectedSizePreset = 0;
        ExportProfile selectedProfile = ExportProfile::CurrentView;
        bool lockAspectRatio = true;
        bool grayscaleOutput = false;
        bool showGrid = true;
        bool showCoordinates = true;
        bool showWires = true;
        bool showEnvelope = true;
        bool showAxisTriad = false;
        ExportBackgroundMode backgroundMode = ExportBackgroundMode::Current;
        ExportFormat format = ExportFormat::Png;
        ExportAspectMode aspectMode = ExportAspectMode::PreserveMathematicalScale;
        ExportQualityMode qualityMode = ExportQualityMode::Interactive;
        ExportQualitySettings quality = qualitySettingsForPreset(ExportQualityPreset::Normal);
        ExportPreviewQuality previewQuality = ExportPreviewQuality::Normal;
        bool autoRefreshPreview = false;
        bool openAfterSave = false;
        bool showInFolderAfterSave = false;
        bool copyPathAfterSave = false;
        bool saveMetadataSidecar = false;
        std::array<float, 4> backgroundColor = { 0.098f, 0.098f, 0.118f, 1.0f };
    };

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

    /// Return false when the app should keep running while a discard prompt is shown.
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
    ProjectSession currentProjectSession() const;
    void refreshProjectDirtyState();
    void markProjectClean();
    bool promptOpenProjectPath(std::wstring& path) const;
    bool promptSaveProjectPath(std::wstring& path) const;
    bool saveProject();
    bool saveProjectAs();
    bool saveProjectToPath(const std::wstring& path, std::string& error);
    bool openProjectFromDialog();
    bool openProjectFromPath(const std::wstring& path, std::string& error);
    void addRecentProjectPath(const std::wstring& path);
    void loadRecentProjectPaths();
    void saveRecentProjectPaths() const;
    std::filesystem::path recentProjectStorePath() const;
    std::string projectDisplayName() const;
    void renderPlotToolbar(bool has2DFormula, bool hasSurfaceFormula);
    void handlePlotShortcuts();
    void fitDefaultView();
    void resetViewAndCamera();
    void applyCameraPreset(float azimuthDeg, float elevationDeg);
    void startUpdateCheck(bool manualRequest);
    void pollUpdateCheckResult();
    bool promptSaveImagePath(std::wstring& path);
    std::array<float, 4> resolveExportBackgroundColor(const ExportDialogSettings& settings) const;
    void markExportPreviewOutOfDate();
    void requestExportPreviewRefresh();
    bool capturePlotPixels(std::vector<std::uint8_t>& pixels, int& width, int& height);
    bool renderPlotPixelsOffscreen(const ExportDialogSettings& settings,
                                   std::vector<std::uint8_t>& pixels, int& width, int& height);
    bool readTexturePixelsRgba(ID3D11Texture2D* sourceTexture,
                               std::vector<std::uint8_t>& pixels,
                               int& width, int& height);
    void renderExportDialog(float sidebarWidth, float viewportHeight);
    void initialiseExportDialogSize();
    void cleanupExportPreviewResources();
    bool refreshExportPreviewTexture();
    void applyExportPostProcessing(std::vector<std::uint8_t>& pixels,
                                   int sourceWidth, int sourceHeight,
                                   std::vector<std::uint8_t>& outputPixels,
                                   int& outputWidth, int& outputHeight);
    static void resizePixelsBilinear(const std::vector<std::uint8_t>& srcPixels,
                                     int srcWidth, int srcHeight,
                                     int dstWidth, int dstHeight,
                                     std::vector<std::uint8_t>& dstPixels);
    static void convertPixelsRgbaToBgra(std::vector<std::uint8_t>& pixels);
    static void unpremultiplyPixels(std::vector<std::uint8_t>& pixels);
    static void convertPixelsToGrayscale(std::vector<std::uint8_t>& pixels);
    static void convertPixelsToGrayscaleRgba(std::vector<std::uint8_t>& pixels);
    bool saveImageToPath(const std::wstring& path,
                         const std::vector<std::uint8_t>& pixels,
                         int width, int height, std::string& error);
    std::string buildExportMetadataJson(const ExportDialogSettings& settings,
                                        const std::wstring& imagePath,
                                        int width, int height) const;
    bool writeExportMetadataSidecar(const ExportDialogSettings& settings,
                                    const std::wstring& imagePath,
                                    int width, int height,
                                    std::string& error) const;
    bool savePngToPath(const std::wstring& path,
                       const std::vector<std::uint8_t>& pixels,
                       int width, int height, std::string& error);
    bool saveBmpToPath(const std::wstring& path,
                       const std::vector<std::uint8_t>& pixels,
                       int width, int height, std::string& error);
    bool copyPixelsToClipboard(const std::vector<std::uint8_t>& pixels,
                               int width, int height, std::string& error);
    void openLastSavedExport();
    void showLastSavedExportInFolder();
    void copyLastSavedExportPath();
    void processPendingExportActions();
    static std::string narrowUtf8(const std::wstring& text);

    HWND                      m_hWnd               = nullptr;
    ID3D11Device*             m_device              = nullptr;
    ID3D11DeviceContext*      m_deviceContext        = nullptr;
    IDXGISwapChain*           m_swapChain           = nullptr;
    ID3D11RenderTargetView*   m_renderTargetView    = nullptr;
    bool                      m_swapChainOccluded   = false;
    bool                      m_comInitialized      = false;
    bool                      m_redrawRequested     = true;

    // Application state
    std::vector<FormulaEntry> m_formulas;
    Core::ViewTransform       m_viewTransform;
    PlotSettings              m_plotSettings;
    std::wstring              m_projectPath;
    bool                      m_projectDirty = false;
    std::string               m_savedProjectSnapshot;
    std::string               m_projectStatus;
    std::vector<std::wstring> m_recentProjectPaths;
    PendingProjectAction      m_pendingProjectAction = PendingProjectAction::None;
    std::wstring              m_pendingProjectPath;
    bool                      m_openProjectDiscardPopupNextFrame = false;
    float                     m_sidebarWidth = 360.0f;
    bool                      m_exportDialogOpen = false;
    bool                      m_exportDialogOpenRequested = false;
    bool                      m_exportDialogPopupOpenNextFrame = false;
    bool                      m_exportDialogCenterOnOpen = false;
    bool                      m_exportDialogSizeInitialized = false;
    float                     m_exportSettingsPaneWidth = 420.0f;
    ExportDialogSettings      m_exportDialogSettings;
    ExportDialogSettings      m_pendingExportSettings;
    bool                      m_scheduledSavePlotImage = false;
    bool                      m_scheduledCopyPlotImage = false;
    bool                      m_pendingSavePlotImage = false;
    bool                      m_pendingCopyPlotImage = false;
    ID3D11Texture2D*          m_exportPreviewTexture = nullptr;
    ID3D11ShaderResourceView* m_exportPreviewSrv = nullptr;
    int                       m_exportPreviewWidth = 0;
    int                       m_exportPreviewHeight = 0;
    bool                      m_exportPreviewDirty = false;
    bool                      m_exportPreviewRefreshRequested = false;
    bool                      m_exportPreviewUseFinalQualityOnce = false;
    std::chrono::steady_clock::time_point m_exportPreviewLastChanged;
    float                     m_exportPreviewZoom = 0.0f; // 0 means fit to preview pane.
    float                     m_exportPreviewPanX = 0.0f;
    float                     m_exportPreviewPanY = 0.0f;
    bool                      m_exportPreviewCheckerboard = true;
    std::string               m_exportPreviewStatus;
    std::string               m_exportStatus;
    std::wstring              m_lastExportSavedPath;
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
