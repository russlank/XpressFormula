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
#include <string>
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
    bool                      m_pendingSavePlotImage = false;
    bool                      m_pendingCopyPlotImage = false;
    std::string               m_exportStatus;

    // UI panels
    FormulaPanel  m_formulaPanel;
    ControlPanel  m_controlPanel;
    PlotPanel     m_plotPanel;

    static constexpr float kSidebarWidth = 380.0f;
};

} // namespace XpressFormula::UI
