// SPDX-License-Identifier: MIT
// Application.h - Main application class: owns the window, D3D11 device,
//                  ImGui context, and orchestrates the UI panels.
#pragma once

#include "ExportSettings.h"
#include "MainWindow.h"
#include "PlotSettings.h"
#include "../Application/ApplicationComposition.h"
#include "../Application/ExportController.h"
#include "../Application/ExportPreviewTexture.h"
#include "../Application/ApplicationState.h"
#include "../Application/ProjectController.h"
#include "../Application/UpdateController.h"
#include "../Core/ViewTransform.h"
#include "../Model/Document.h"
#include "../Model/Formula.h"

#include <chrono>
#include <cstdint>
#include <d3d11.h>
#include <array>
#include <string>
#include <string_view>
#include <vector>

struct HWND__;
typedef HWND__* HWND;
struct HINSTANCE__;
typedef HINSTANCE__* HINSTANCE;

namespace XpressFormula::UI {

class Application {
public:
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
    class ProjectFileDialogAdapter final : public XpressFormula::Application::IProjectFileDialog {
    public:
        explicit ProjectFileDialogAdapter(Application& application) noexcept
            : m_application(application) {}

        [[nodiscard]] Platform::Windows::DialogResult openProject() override;
        [[nodiscard]] Platform::Windows::DialogResult saveProject(
            std::wstring_view currentPath) override;

    private:
        Application& m_application;
    };

    bool createDeviceD3D(HWND hWnd);
    void cleanupDeviceD3D();
    void renderFrame();
    void updatePlotCamera(float deltaSeconds);
    void handleProjectShortcuts();
    void handleMainWindowActions(const MainWindowActions& actions);
    void syncDocumentDependentState();
    void consumeProjectControllerEffects();
    void refreshSceneSummary();
    void markExportPreviewOutOfDate();
    void requestExportPreviewRefresh();
    bool capturePlotPixels(std::vector<std::uint8_t>& pixels, int& width, int& height);
    bool renderPlotPixelsOffscreen(const ExportSettings& settings,
                                   std::vector<std::uint8_t>& pixels, int& width, int& height);
    bool readTexturePixelsRgba(ID3D11Texture2D* sourceTexture,
                               std::vector<std::uint8_t>& pixels,
                               int& width, int& height);
    void renderExportDialog();
    void initialiseExportDialogSize();
    bool refreshExportPreviewTexture();
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

    // Application state
    XpressFormula::Application::ApplicationState m_state;
    Model::Document            m_document;
    XpressFormula::Application::ApplicationComposition m_composition;
    ProjectFileDialogAdapter  m_projectFileDialogAdapter;
    XpressFormula::Application::ProjectController m_projectController;
    XpressFormula::Application::ExportController m_exportController;
    XpressFormula::Application::ExportPreviewTexture m_exportPreview;
    XpressFormula::Application::UpdateController m_updateController;
    MainWindow m_mainWindow;
};

} // namespace XpressFormula::UI
