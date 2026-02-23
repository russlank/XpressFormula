// SPDX-License-Identifier: MIT
// Application.h - Main application class: owns the window, D3D11 device,
//                  ImGui context, and orchestrates the UI panels.
#pragma once

#include "FormulaEntry.h"
#include "FormulaPanel.h"
#include "ControlPanel.h"
#include "PlotPanel.h"
#include "PlotSettings.h"
#include "../Core/ViewTransform.h"

#include <cstdint>
#include <d3d11.h>
#include <array>
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
        bool lockAspectRatio = true;
        bool grayscaleOutput = false;
        bool showGrid = true;
        bool showCoordinates = true;
        bool showWires = true;
        bool showEnvelope = true;
        std::array<float, 4> backgroundColor = { 0.098f, 0.098f, 0.118f, 1.0f };
    };

    Application();
    ~Application();

    /// Create the Win32 window and Direct3D 11 device, then initialise ImGui.
    bool initialize(HINSTANCE hInstance, int width = 1400, int height = 900);

    /// Enter the main loop. Returns the process exit code.
    int run();

    /// Tear everything down.
    void shutdown();

    // --- D3D11 helpers (also used by the global WndProc) ---
    void createRenderTarget();
    void cleanupRenderTarget();

    UINT resizeWidth  = 0;
    UINT resizeHeight = 0;

private:
    bool createDeviceD3D(HWND hWnd);
    void cleanupDeviceD3D();
    void renderFrame();
    bool promptSaveImagePath(std::wstring& path);
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
    bool savePngToPath(const std::wstring& path,
                       const std::vector<std::uint8_t>& pixels,
                       int width, int height, std::string& error);
    bool saveBmpToPath(const std::wstring& path,
                       const std::vector<std::uint8_t>& pixels,
                       int width, int height, std::string& error);
    bool copyPixelsToClipboard(const std::vector<std::uint8_t>& pixels,
                               int width, int height, std::string& error);
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
    bool                      m_exportDialogOpen = false;
    bool                      m_exportDialogOpenRequested = false;
    bool                      m_exportDialogPopupOpenNextFrame = false;
    bool                      m_exportDialogCenterOnOpen = false;
    bool                      m_exportDialogSizeInitialized = false;
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
    std::string               m_exportPreviewStatus;
    std::string               m_exportStatus;

    // UI panels
    FormulaPanel  m_formulaPanel;
    ControlPanel  m_controlPanel;
    PlotPanel     m_plotPanel;

    static constexpr float kSidebarWidth = 380.0f;
};

} // namespace XpressFormula::UI
