// SPDX-License-Identifier: MIT
// Application.cpp - Win32 + D3D11 + ImGui application implementation.
#include "Application.h"
#include "../Version.h"
#include "../resource.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>
#include <d3d11.h>
#include <dxgi.h>
#include <objbase.h>
#include <tchar.h>
#include <wincodec.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#pragma comment(lib, "windowscodecs.lib")

// Forward-declare the ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// We store a pointer to the Application so the WndProc can access it.
static XpressFormula::UI::Application* g_app = nullptr;

static std::wstring utf8ToWide(const char* text) {
    if (!text || text[0] == '\0') {
        return {};
    }

    int size = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 1) {
        return {};
    }

    std::wstring output(static_cast<size_t>(size - 1), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text, -1, output.data(), size);
    return output;
}

static std::wstring shortCommitWide(const char* commit) {
    std::wstring commitWide = utf8ToWide(commit);
    if (commitWide.size() > 12) {
        commitWide.resize(12);
    }
    return commitWide;
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg,
                               WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) return 0;
            if (g_app) {
                g_app->resizeWidth  = LOWORD(lParam);
                g_app->resizeHeight = HIWORD(lParam);
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

namespace XpressFormula::UI {

Application::Application()  = default;
Application::~Application() = default;

// ---- initialisation ---------------------------------------------------------

bool Application::initialize(HINSTANCE hInstance, int width, int height) {
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"XpressFormulaClass";
    wc.hIcon = static_cast<HICON>(::LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    wc.hIconSm = static_cast<HICON>(::LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    ::RegisterClassExW(&wc);

    std::wstring windowTitle = L"XpressFormula v";
    windowTitle += XF_VERSION_WSTRING;

    std::wstring branchWide = utf8ToWide(XF_BUILD_BRANCH);
    std::wstring commitWide = shortCommitWide(XF_BUILD_COMMIT);
    if (!branchWide.empty() && branchWide != L"unknown") {
        windowTitle += L" [";
        windowTitle += branchWide;
        if (!commitWide.empty() && commitWide != L"unknown") {
            windowTitle += L" @ ";
            windowTitle += commitWide;
        }
        windowTitle += L"]";
    }

    windowTitle += L" - Math Expression Plotter";

    m_hWnd = ::CreateWindowExW(
        0, wc.lpszClassName, windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (m_hWnd == nullptr) {
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    if (!createDeviceD3D(m_hWnd)) {
        cleanupDeviceD3D();
        ::DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    ::ShowWindow(m_hWnd, SW_SHOW);
    ::UpdateWindow(m_hWnd);
    ::SetForegroundWindow(m_hWnd);

    g_app = this;

    HRESULT comResult = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    m_comInitialized = (comResult == S_OK || comResult == S_FALSE);

    // ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_device, m_deviceContext);

    // Add a default formula so the plot is not empty
    FormulaEntry defaultEntry;
    strncpy_s(defaultEntry.inputBuffer, sizeof(defaultEntry.inputBuffer),
              // "sin(x)", _TRUNCATE);
              "sin(sqrt(x^2+y^2))", _TRUNCATE);
    std::memcpy(defaultEntry.color, kDefaultPalette[0],
                sizeof(defaultEntry.color));
    defaultEntry.parse();
    m_formulas.push_back(std::move(defaultEntry));

    return true;
}

// ---- main loop --------------------------------------------------------------

int Application::run() {
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message != WM_QUIT) {
                // Any user/system message may affect layout, hover state, or plot interaction.
                m_redrawRequested = true;
            }
            continue;
        }

        const bool continuousRender = m_plotSettings.autoRotate;
        const bool optimizeRendering = m_plotSettings.optimizeRendering;
        if (optimizeRendering && !m_redrawRequested && !continuousRender) {
            // Event-driven idle mode: avoid presenting frames when nothing changes.
            ::WaitMessage();
            continue;
        }

        // Handle swap-chain being occluded (minimised, etc.)
        if (m_swapChainOccluded &&
            m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(continuousRender ? 10 : 30);
            continue;
        }
        m_swapChainOccluded = false;

        // Handle resize
        if (resizeWidth != 0 && resizeHeight != 0) {
            cleanupRenderTarget();
            m_swapChain->ResizeBuffers(0, resizeWidth, resizeHeight,
                                       DXGI_FORMAT_UNKNOWN, 0);
            resizeWidth = resizeHeight = 0;
            createRenderTarget();
        }

        renderFrame();
        // Auto-rotate requires continuous redraws; otherwise render on demand when optimization is enabled.
        m_redrawRequested = optimizeRendering ? m_plotSettings.autoRotate : true;
    }
    return static_cast<int>(msg.wParam);
}

// ---- per-frame render -------------------------------------------------------

void Application::renderFrame() {
    // Export is intentionally deferred to a fresh frame after the export dialog closes.
    // This prevents the dialog window from being captured inside the exported image.
    if (m_scheduledSavePlotImage || m_scheduledCopyPlotImage) {
        m_pendingSavePlotImage = m_pendingSavePlotImage || m_scheduledSavePlotImage;
        m_pendingCopyPlotImage = m_pendingCopyPlotImage || m_scheduledCopyPlotImage;
        m_scheduledSavePlotImage = false;
        m_scheduledCopyPlotImage = false;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // We fill the entire OS window with two fixed ImGui windows (sidebar + plot)
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float totalW = viewport->WorkSize.x;
    float totalH = viewport->WorkSize.y;
    float sidebar = kSidebarWidth;
    if (sidebar > totalW * 0.45f) sidebar = totalW * 0.45f;

    // ---- Left sidebar ----
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(sidebar, totalH));
    ImGui::Begin("##Sidebar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse);
    m_formulaPanel.render(m_formulas);
    bool hasSurfaceFormula = false;
    for (const FormulaEntry& formula : m_formulas) {
        if (formula.visible && formula.isValid() && formula.uses3DSurface()) {
            hasSurfaceFormula = true;
            break;
        }
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ControlPanelActions actions = m_controlPanel.render(
        m_viewTransform, m_plotSettings, hasSurfaceFormula, m_exportStatus);
    m_exportDialogOpenRequested = m_exportDialogOpenRequested || actions.requestOpenExportDialog;

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Build Metadata");
    ImGui::TextDisabled("Version: %s", XF_BUILD_VERSION);
    ImGui::TextDisabled("Repo: %s", XF_BUILD_REPO_URL);
    ImGui::TextDisabled("Branch: %s", XF_BUILD_BRANCH);
    ImGui::TextDisabled("Commit: %s", XF_BUILD_COMMIT);
    ImGui::End();

    renderExportDialog(sidebar, totalH);

    // ---- Plot area ----
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + sidebar,
                                   viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(totalW - sidebar, totalH));
    ImGui::Begin("##Plot", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoScrollbar);
    PlotRenderOverrides exportOverrides;
    if (m_pendingSavePlotImage || m_pendingCopyPlotImage) {
        exportOverrides.active = true;
        exportOverrides.showGrid = m_pendingExportSettings.showGrid;
        exportOverrides.showCoordinates = m_pendingExportSettings.showCoordinates;
        exportOverrides.showWires = m_pendingExportSettings.showWires;
        exportOverrides.showEnvelope = m_pendingExportSettings.showEnvelope;
        exportOverrides.backgroundColor = m_pendingExportSettings.backgroundColor;
    }
    m_plotPanel.render(m_formulas, m_viewTransform, m_plotSettings,
                       exportOverrides.active ? &exportOverrides : nullptr);
    ImGui::End();

    // ---- Render ----
    ImGui::Render();
    const float clearColor[4] = { 0.06f, 0.06f, 0.08f, 1.0f };
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, nullptr);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    processPendingExportActions();

    HRESULT hr = m_swapChain->Present(1, 0); // VSync
    m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
}

void Application::initialiseExportDialogSize() {
    if (m_exportDialogSizeInitialized) {
        return;
    }

    int width = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenWidth)));
    int height = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenHeight)));
    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;

    m_exportDialogSettings.width = width;
    m_exportDialogSettings.height = height;
    m_exportDialogSizeInitialized = true;
}

void Application::renderExportDialog(float sidebarWidth, float viewportHeight) {
    if (m_exportDialogOpenRequested) {
        m_exportDialogOpenRequested = false;
        m_exportDialogOpen = true;
        m_exportDialogSizeInitialized = false;
        // Default export visibility follows the current interactive view state.
        m_exportDialogSettings.showGrid = m_plotSettings.showGrid;
        m_exportDialogSettings.showCoordinates = m_plotSettings.showCoordinates;
        m_exportDialogSettings.showWires = m_plotSettings.showWires;
        m_exportDialogSettings.showEnvelope = m_plotSettings.showSurfaceEnvelope;
    }

    if (!m_exportDialogOpen) {
        return;
    }

    initialiseExportDialogSize();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float maxWidth = (std::max)(300.0f, sidebarWidth - 20.0f);
    const float maxHeight = (std::max)(360.0f, viewportHeight - 24.0f);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 10.0f, viewport->WorkPos.y + 10.0f),
                            ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(maxWidth, (std::min)(620.0f, maxHeight)), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 320.0f), ImVec2(maxWidth, maxHeight));

    bool open = m_exportDialogOpen;
    if (ImGui::Begin("Export Plot Settings", &open, ImGuiWindowFlags_NoCollapse)) {
        int sourceWidth = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenWidth)));
        int sourceHeight = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenHeight)));
        if (sourceWidth > 0 && sourceHeight > 0) {
            ImGui::Text("Current plot capture size: %d x %d", sourceWidth, sourceHeight);
        } else {
            ImGui::TextUnformatted("Current plot capture size: unavailable (render the plot once).");
        }
        if (ImGui::Button("Use Current Plot Size")) {
            if (sourceWidth > 0 && sourceHeight > 0) {
                m_exportDialogSettings.width = sourceWidth;
                m_exportDialogSettings.height = sourceHeight;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Output Size");

        int width = m_exportDialogSettings.width;
        int height = m_exportDialogSettings.height;
        const int prevWidth = (std::max)(1, width);
        const int prevHeight = (std::max)(1, height);
        bool widthChanged = ImGui::InputInt("Width", &width, 16, 128);
        bool heightChanged = ImGui::InputInt("Height", &height, 16, 128);
        width = std::clamp(width, 16, 8192);
        height = std::clamp(height, 16, 8192);

        if (m_exportDialogSettings.lockAspectRatio && widthChanged && !heightChanged) {
            height = std::clamp(static_cast<int>(std::lround(
                                    static_cast<double>(width) * prevHeight / prevWidth)),
                                16, 8192);
        } else if (m_exportDialogSettings.lockAspectRatio && heightChanged && !widthChanged) {
            width = std::clamp(static_cast<int>(std::lround(
                                   static_cast<double>(height) * prevWidth / prevHeight)),
                               16, 8192);
        }

        m_exportDialogSettings.width = width;
        m_exportDialogSettings.height = height;
        ImGui::Checkbox("Lock Aspect Ratio", &m_exportDialogSettings.lockAspectRatio);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Appearance");
        if (ImGui::RadioButton("Color", !m_exportDialogSettings.grayscaleOutput)) {
            m_exportDialogSettings.grayscaleOutput = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Grayscale", m_exportDialogSettings.grayscaleOutput)) {
            m_exportDialogSettings.grayscaleOutput = true;
        }
        ImGui::ColorEdit4("Background Color", m_exportDialogSettings.backgroundColor.data(),
                          ImGuiColorEditFlags_DisplayRGB);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Include In Export");
        ImGui::Checkbox("Grid", &m_exportDialogSettings.showGrid);
        ImGui::Checkbox("Coordinates (axes + labels)", &m_exportDialogSettings.showCoordinates);
        ImGui::Checkbox("Wires / Wireframe", &m_exportDialogSettings.showWires);
        ImGui::Checkbox("Envelope Box (3D)", &m_exportDialogSettings.showEnvelope);
        ImGui::TextWrapped("Wires affect 3D surfaces/implicit meshes. 2D curves are always drawn.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextWrapped("Export uses the current formulas and view. Size changes are applied to the captured plot image.");

        const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Copy To Clipboard", ImVec2(buttonWidth, 0.0f))) {
            m_pendingExportSettings = m_exportDialogSettings;
            m_scheduledCopyPlotImage = true;
            m_scheduledSavePlotImage = false;
            open = false;
            m_redrawRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save To File...", ImVec2(buttonWidth, 0.0f))) {
            m_pendingExportSettings = m_exportDialogSettings;
            m_scheduledSavePlotImage = true;
            m_scheduledCopyPlotImage = false;
            open = false;
            m_redrawRequested = true;
        }
    }
    ImGui::End();
    m_exportDialogOpen = open;
}

void Application::resizePixelsBilinear(const std::vector<std::uint8_t>& srcPixels,
                                       int srcWidth, int srcHeight,
                                       int dstWidth, int dstHeight,
                                       std::vector<std::uint8_t>& dstPixels) {
    dstPixels.clear();
    if (srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) {
        return;
    }
    if (srcWidth == dstWidth && srcHeight == dstHeight) {
        dstPixels = srcPixels;
        return;
    }

    dstPixels.resize(static_cast<size_t>(dstWidth) * static_cast<size_t>(dstHeight) * 4u);

    const auto sampleIndex = [srcWidth](int x, int y) {
        return (static_cast<size_t>(y) * static_cast<size_t>(srcWidth) +
                static_cast<size_t>(x)) * 4u;
    };

    for (int y = 0; y < dstHeight; ++y) {
        double srcY = ((static_cast<double>(y) + 0.5) * srcHeight / dstHeight) - 0.5;
        int y0 = static_cast<int>(std::floor(srcY));
        int y1 = y0 + 1;
        double fy = srcY - y0;
        y0 = std::clamp(y0, 0, srcHeight - 1);
        y1 = std::clamp(y1, 0, srcHeight - 1);

        for (int x = 0; x < dstWidth; ++x) {
            double srcX = ((static_cast<double>(x) + 0.5) * srcWidth / dstWidth) - 0.5;
            int x0 = static_cast<int>(std::floor(srcX));
            int x1 = x0 + 1;
            double fx = srcX - x0;
            x0 = std::clamp(x0, 0, srcWidth - 1);
            x1 = std::clamp(x1, 0, srcWidth - 1);

            size_t outIndex = (static_cast<size_t>(y) * static_cast<size_t>(dstWidth) +
                               static_cast<size_t>(x)) * 4u;
            const size_t i00 = sampleIndex(x0, y0);
            const size_t i10 = sampleIndex(x1, y0);
            const size_t i01 = sampleIndex(x0, y1);
            const size_t i11 = sampleIndex(x1, y1);

            for (int c = 0; c < 4; ++c) {
                const double v00 = srcPixels[i00 + c];
                const double v10 = srcPixels[i10 + c];
                const double v01 = srcPixels[i01 + c];
                const double v11 = srcPixels[i11 + c];
                const double top = v00 + (v10 - v00) * fx;
                const double bottom = v01 + (v11 - v01) * fx;
                const double value = top + (bottom - top) * fy;
                dstPixels[outIndex + c] = static_cast<std::uint8_t>(
                    std::clamp(static_cast<int>(std::lround(value)), 0, 255));
            }
        }
    }
}

void Application::convertPixelsToGrayscale(std::vector<std::uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        const float b = static_cast<float>(pixels[i + 0]);
        const float g = static_cast<float>(pixels[i + 1]);
        const float r = static_cast<float>(pixels[i + 2]);
        const std::uint8_t gray = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(0.114f * b + 0.587f * g + 0.299f * r)), 0, 255));
        pixels[i + 0] = gray;
        pixels[i + 1] = gray;
        pixels[i + 2] = gray;
    }
}

void Application::applyExportPostProcessing(std::vector<std::uint8_t>& pixels,
                                            int sourceWidth, int sourceHeight,
                                            std::vector<std::uint8_t>& outputPixels,
                                            int& outputWidth, int& outputHeight) {
    outputWidth = sourceWidth;
    outputHeight = sourceHeight;
    outputPixels = pixels;

    const int targetWidth = std::clamp(m_pendingExportSettings.width, 16, 8192);
    const int targetHeight = std::clamp(m_pendingExportSettings.height, 16, 8192);
    if (targetWidth != sourceWidth || targetHeight != sourceHeight) {
        resizePixelsBilinear(pixels, sourceWidth, sourceHeight,
                             targetWidth, targetHeight, outputPixels);
        outputWidth = targetWidth;
        outputHeight = targetHeight;
    }

    if (m_pendingExportSettings.grayscaleOutput) {
        convertPixelsToGrayscale(outputPixels);
    }
}

// ---- shutdown ---------------------------------------------------------------

void Application::shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (m_comInitialized) {
        ::CoUninitialize();
        m_comInitialized = false;
    }

    cleanupDeviceD3D();
    ::DestroyWindow(m_hWnd);
    g_app = nullptr;
}

// ---- D3D11 helpers ----------------------------------------------------------

bool Application::createDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    // In Debug builds, the D3D debug layer may not be installed. Try with debug
    // layer first, then gracefully retry without it.
    const UINT flagCandidates[] = {
#ifdef _DEBUG
        D3D11_CREATE_DEVICE_DEBUG,
#endif
        0u
    };
    const D3D_DRIVER_TYPE driverCandidates[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP
    };

    HRESULT hr = E_FAIL;
    for (UINT flags : flagCandidates) {
        for (D3D_DRIVER_TYPE driver : driverCandidates) {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, driver, nullptr, flags,
                levels, 2, D3D11_SDK_VERSION,
                &sd, &m_swapChain, &m_device, &featureLevel, &m_deviceContext);
            if (SUCCEEDED(hr)) {
                createRenderTarget();
                return true;
            }
        }
    }
    return false;
}

void Application::cleanupDeviceD3D() {
    cleanupRenderTarget();
    if (m_swapChain)     { m_swapChain->Release();     m_swapChain     = nullptr; }
    if (m_deviceContext) { m_deviceContext->Release(); m_deviceContext = nullptr; }
    if (m_device)        { m_device->Release();        m_device        = nullptr; }
}

void Application::createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTargetView);
        backBuffer->Release();
    }
}

void Application::cleanupRenderTarget() {
    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
}

bool Application::promptSaveImagePath(std::wstring& path) {
    std::array<wchar_t, MAX_PATH> fileName = {};
    wcsncpy_s(fileName.data(), fileName.size(), L"xpressformula-plot.png", _TRUNCATE);

    wchar_t filter[] =
        L"PNG Image (*.png)\0*.png\0"
        L"Bitmap Image (*.bmp)\0*.bmp\0\0";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"png";

    if (!::GetSaveFileNameW(&ofn)) {
        return false;
    }

    path = fileName.data();
    if (std::filesystem::path(path).extension().empty()) {
        path += (ofn.nFilterIndex == 2) ? L".bmp" : L".png";
    }
    return true;
}

bool Application::capturePlotPixels(std::vector<std::uint8_t>& pixels, int& width, int& height) {
    width = 0;
    height = 0;
    pixels.clear();

    if (!m_swapChain || !m_device || !m_deviceContext) {
        return false;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) || !backBuffer) {
        return false;
    }

    D3D11_TEXTURE2D_DESC backDesc = {};
    backBuffer->GetDesc(&backDesc);

    int left = static_cast<int>(std::floor(m_viewTransform.screenOriginX));
    int top = static_cast<int>(std::floor(m_viewTransform.screenOriginY));
    int right = left + static_cast<int>(std::floor(m_viewTransform.screenWidth));
    int bottom = top + static_cast<int>(std::floor(m_viewTransform.screenHeight));

    left = std::clamp(left, 0, static_cast<int>(backDesc.Width));
    top = std::clamp(top, 0, static_cast<int>(backDesc.Height));
    right = std::clamp(right, 0, static_cast<int>(backDesc.Width));
    bottom = std::clamp(bottom, 0, static_cast<int>(backDesc.Height));

    width = right - left;
    height = bottom - top;
    if (width <= 0 || height <= 0) {
        backBuffer->Release();
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = backDesc;
    stagingDesc.Width = static_cast<UINT>(width);
    stagingDesc.Height = static_cast<UINT>(height);
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture2D* stagingTexture = nullptr;
    if (FAILED(m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture)) || !stagingTexture) {
        backBuffer->Release();
        return false;
    }

    D3D11_BOX sourceBox = {};
    sourceBox.left = static_cast<UINT>(left);
    sourceBox.top = static_cast<UINT>(top);
    sourceBox.front = 0;
    sourceBox.right = static_cast<UINT>(right);
    sourceBox.bottom = static_cast<UINT>(bottom);
    sourceBox.back = 1;

    m_deviceContext->CopySubresourceRegion(
        stagingTexture, 0, 0, 0, 0, backBuffer, 0, &sourceBox);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT mapResult = m_deviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(mapResult)) {
        stagingTexture->Release();
        backBuffer->Release();
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(width) * 4;
    pixels.resize(static_cast<size_t>(height) * rowBytes);
    for (int y = 0; y < height; ++y) {
        const auto* src = static_cast<const std::uint8_t*>(mapped.pData) +
            static_cast<size_t>(y) * mapped.RowPitch;
        auto* dst = pixels.data() + static_cast<size_t>(y) * rowBytes;
        std::memcpy(dst, src, rowBytes);
    }

    m_deviceContext->Unmap(stagingTexture, 0);
    stagingTexture->Release();
    backBuffer->Release();
    return true;
}

bool Application::saveImageToPath(const std::wstring& path,
                                  const std::vector<std::uint8_t>& pixels,
                                  int width, int height, std::string& error) {
    std::wstring extension = std::filesystem::path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });

    if (extension == L".bmp") {
        return saveBmpToPath(path, pixels, width, height, error);
    }
    return savePngToPath(path, pixels, width, height, error);
}

bool Application::savePngToPath(const std::wstring& path,
                                const std::vector<std::uint8_t>& pixels,
                                int width, int height, std::string& error) {
    auto formatError = [](HRESULT hr) {
        std::ostringstream oss;
        oss << "WIC error 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
        return oss.str();
    };

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* properties = nullptr;

    HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        error = formatError(hr);
        return false;
    }

    hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) {
        hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    }
    if (SUCCEEDED(hr)) {
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->CreateNewFrame(&frame, &properties);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Initialize(properties);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
    }
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) {
        hr = frame->SetPixelFormat(&pixelFormat);
    }
    if (SUCCEEDED(hr)) {
        hr = frame->WritePixels(static_cast<UINT>(height), static_cast<UINT>(width * 4),
                                static_cast<UINT>(pixels.size()),
                                const_cast<BYTE*>(pixels.data()));
    }
    if (SUCCEEDED(hr)) {
        hr = frame->Commit();
    }
    if (SUCCEEDED(hr)) {
        hr = encoder->Commit();
    }

    if (properties) properties->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();

    if (FAILED(hr)) {
        error = formatError(hr);
        return false;
    }
    return true;
}

bool Application::saveBmpToPath(const std::wstring& path,
                                const std::vector<std::uint8_t>& pixels,
                                int width, int height, std::string& error) {
    const std::uint32_t rowBytes = static_cast<std::uint32_t>(width) * 4u;
    const std::uint32_t pixelBytes = rowBytes * static_cast<std::uint32_t>(height);

#pragma pack(push, 1)
    struct BmpFileHeader {
        std::uint16_t type;
        std::uint32_t size;
        std::uint16_t reserved1;
        std::uint16_t reserved2;
        std::uint32_t offBits;
    };
#pragma pack(pop)

    BmpFileHeader fileHeader = {};
    fileHeader.type = 0x4D42; // BM
    fileHeader.offBits = sizeof(BmpFileHeader) + sizeof(BITMAPINFOHEADER);
    fileHeader.size = fileHeader.offBits + pixelBytes;

    BITMAPINFOHEADER infoHeader = {};
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height; // bottom-up DIB
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = pixelBytes;

    std::ofstream out(std::filesystem::path(path), std::ios::binary);
    if (!out) {
        error = "Failed to open output file.";
        return false;
    }

    out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    out.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));
    for (int y = height - 1; y >= 0; --y) {
        const auto* row = pixels.data() + static_cast<size_t>(y) * rowBytes;
        out.write(reinterpret_cast<const char*>(row), rowBytes);
    }

    if (!out.good()) {
        error = "Failed while writing BMP data.";
        return false;
    }
    return true;
}

bool Application::copyPixelsToClipboard(const std::vector<std::uint8_t>& pixels,
                                        int width, int height, std::string& error) {
    if (width <= 0 || height <= 0) {
        error = "Invalid image dimensions.";
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(width) * 4;
    const size_t imageBytes = rowBytes * static_cast<size_t>(height);
    const size_t totalBytes = sizeof(BITMAPINFOHEADER) + imageBytes;

    HGLOBAL hGlobal = ::GlobalAlloc(GMEM_MOVEABLE, totalBytes);
    if (!hGlobal) {
        error = "GlobalAlloc failed.";
        return false;
    }

    void* raw = ::GlobalLock(hGlobal);
    if (!raw) {
        ::GlobalFree(hGlobal);
        error = "GlobalLock failed.";
        return false;
    }

    auto* header = static_cast<BITMAPINFOHEADER*>(raw);
    *header = {};
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = width;
    header->biHeight = height; // bottom-up DIB
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(imageBytes);

    auto* dst = reinterpret_cast<std::uint8_t*>(header + 1);
    for (int y = 0; y < height; ++y) {
        const auto* srcRow = pixels.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
        auto* dstRow = dst + static_cast<size_t>(y) * rowBytes;
        std::memcpy(dstRow, srcRow, rowBytes);
    }

    ::GlobalUnlock(hGlobal);

    if (!::OpenClipboard(m_hWnd)) {
        ::GlobalFree(hGlobal);
        error = "Could not open clipboard.";
        return false;
    }

    ::EmptyClipboard();
    if (!::SetClipboardData(CF_DIB, hGlobal)) {
        ::CloseClipboard();
        ::GlobalFree(hGlobal);
        error = "SetClipboardData failed.";
        return false;
    }
    ::CloseClipboard();
    return true;
}

void Application::processPendingExportActions() {
    if (!m_pendingSavePlotImage && !m_pendingCopyPlotImage) {
        return;
    }

    std::vector<std::string> messages;
    std::vector<std::uint8_t> capturedPixels;
    int capturedWidth = 0;
    int capturedHeight = 0;
    if (!capturePlotPixels(capturedPixels, capturedWidth, capturedHeight)) {
        messages.emplace_back("Export failed: unable to capture plot area.");
        m_pendingSavePlotImage = false;
        m_pendingCopyPlotImage = false;
        m_redrawRequested = true;
        m_exportStatus = messages.front();
        return;
    }

    std::vector<std::uint8_t> outputPixels;
    int outputWidth = 0;
    int outputHeight = 0;
    applyExportPostProcessing(capturedPixels, capturedWidth, capturedHeight,
                              outputPixels, outputWidth, outputHeight);

    if (m_pendingSavePlotImage) {
        std::wstring path;
        if (promptSaveImagePath(path)) {
            std::string error;
            if (saveImageToPath(path, outputPixels, outputWidth, outputHeight, error)) {
                messages.emplace_back("Saved plot image to: " + narrowUtf8(path));
            } else {
                messages.emplace_back("Save failed: " + error);
            }
        } else {
            messages.emplace_back("Save canceled.");
        }
    }

    if (m_pendingCopyPlotImage) {
        std::string error;
        if (copyPixelsToClipboard(outputPixels, outputWidth, outputHeight, error)) {
            messages.emplace_back("Copied plot image to clipboard.");
        } else {
            messages.emplace_back("Clipboard copy failed: " + error);
        }
    }

    m_pendingSavePlotImage = false;
    m_pendingCopyPlotImage = false;
    // The export frame may be rendered with export-only overrides. Request one more frame
    // so the interactive view returns to the user's normal display settings.
    m_redrawRequested = true;

    if (!messages.empty()) {
        std::ostringstream oss;
        for (size_t i = 0; i < messages.size(); ++i) {
            if (i > 0) {
                oss << " | ";
            }
            oss << messages[i];
        }
        m_exportStatus = oss.str();
    }
}

std::string Application::narrowUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    int size = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                     static_cast<int>(text.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string output(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                          output.data(), size, nullptr, nullptr);
    return output;
}

} // namespace XpressFormula::UI
