// SPDX-License-Identifier: MIT
// Application.cpp - Win32 + D3D11 + ImGui application implementation.
#include "Application.h"
#include "../Application/ExportOutputAdapters.h"
#include "../Infrastructure/Export/ExportMetadataSerializer.h"
#include "../Infrastructure/Export/ExportOutputWorkflow.h"
#include "../Infrastructure/Export/ExportRenderRequest.h"
#include "../Infrastructure/Export/ImageProcessor.h"
#include "../Platform/Windows/ComPtr.h"
#include "../Platform/Windows/Utf.h"
#include "../Version.h"
#include "../resource.h"
#include "FormulaEntry.h"
#include "Components/ExportDialog.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <objbase.h>
#include <tchar.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// Forward-declare the ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// We store a pointer to the Application so the WndProc can access it.
static XpressFormula::UI::Application* g_app = nullptr;

namespace XFExport = XpressFormula::Infrastructure::Export;
namespace XFApp = XpressFormula::Application;
namespace XFWin = XpressFormula::Platform::Windows;
namespace XFWUtf = XpressFormula::Platform::Windows;

namespace {

XpressFormula::Model::Document makeDefaultProjectDocument() {
    XpressFormula::Model::Formula defaultEntry;
    defaultEntry.setExpression("sin(sqrt(x^2+y^2))");
    for (std::size_t channel = 0; channel < defaultEntry.color.size(); ++channel) {
        defaultEntry.color[channel] = XpressFormula::UI::kDefaultPalette[0][channel];
    }
    defaultEntry.compile();

    XpressFormula::Core::ViewTransform view;
    view.reset();
    XpressFormula::Model::PlotSettings plot;
    plot.applyCoordinateOverlayPolicy();

    std::vector<XpressFormula::Model::Formula> formulas;
    formulas.push_back(std::move(defaultEntry));

    XpressFormula::Model::Document document;
    document.replaceState(std::move(formulas), view, plot, true);
    return document;
}

std::vector<XpressFormula::UI::Components::RecentProjectItem> makeRecentProjectItems(
    const std::vector<std::wstring>& paths) {
    std::vector<XpressFormula::UI::Components::RecentProjectItem> items;
    items.reserve(paths.size());
    for (const std::wstring& path : paths) {
        XpressFormula::UI::Components::RecentProjectItem item;
        item.path = path;
        item.fullPath = XFWUtf::utf16ToUtf8OrEmpty(path);
        item.label = XFWUtf::utf16ToUtf8OrEmpty(
            std::filesystem::path(path).filename().wstring());
        if (item.label.empty()) {
            item.label = item.fullPath;
        }
        std::error_code pathError;
        item.exists = std::filesystem::exists(std::filesystem::path(path), pathError);
        items.push_back(std::move(item));
    }
    return items;
}

} // namespace

static std::wstring shortCommitWide(const char* commit) {
    std::wstring commitWide = XFWUtf::utf8ToUtf16OrEmpty(commit ? std::string_view(commit) : std::string_view());
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
        case WM_CLOSE:
            if (g_app) {
                g_app->requestClose();
                return 0;
            }
            ::DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

namespace XpressFormula::UI {

Application::Application()
    : m_projectFileDialogAdapter(*this),
      m_projectController(
          m_composition.projectRepository,
          m_composition.recentProjectsStore,
          m_projectFileDialogAdapter,
          [] { return makeDefaultProjectDocument(); }) {
}

Application::~Application() = default;

XFWin::DialogResult Application::ProjectFileDialogAdapter::openProject() {
    return m_application.m_composition.fileDialogService.openProject(m_application.m_hWnd);
}

XFWin::DialogResult Application::ProjectFileDialogAdapter::saveProject(
    std::wstring_view currentPath) {
    return m_application.m_composition.fileDialogService.saveProject(
        m_application.m_hWnd, currentPath);
}

void Application::refreshSceneSummary() {
    m_state.sceneSummary = Model::analyzeScene(
        std::span<const Model::Formula>(m_document.formulas().data(), m_document.formulas().size()));
}

void Application::syncDocumentDependentState() {
    if (m_projectController.consumeDocumentReplaced()) {
        m_mainWindow.resetFormulaColorCycle(static_cast<int>(m_document.formulas().size()));
        m_state.plotRuntime.resetAutoRotation();
        m_exportController.dialogSettings() = defaultExportSettings();
        m_exportController.setSizeInitialized(false);
    }

    const Model::Document::Revision revision = m_document.revision();
    if (revision != m_state.observedDocumentRevision) {
        refreshSceneSummary();
        markExportPreviewOutOfDate();
        m_state.observedDocumentRevision = revision;
        m_state.redrawRequested = true;
    }
}

void Application::consumeProjectControllerEffects() {
    if (m_projectController.consumeCloseRequest()) {
        m_state.closeRequestedAfterFrame = true;
    }
    syncDocumentDependentState();
    m_state.redrawRequested = true;
}

bool Application::requestClose() {
    m_projectController.requestClose(m_document);
    consumeProjectControllerEffects();
    return false;
}

void Application::handleMainWindowActions(const MainWindowActions& actions) {
    switch (actions.projectControls.command) {
        case Components::ProjectControlCommand::NewProject:
            m_projectController.requestNew(m_document);
            break;
        case Components::ProjectControlCommand::OpenProject:
            m_projectController.requestOpenDialog(m_document);
            break;
        case Components::ProjectControlCommand::Save:
            (void)m_projectController.save(m_document);
            break;
        case Components::ProjectControlCommand::SaveAs:
            (void)m_projectController.saveAs(m_document);
            break;
        case Components::ProjectControlCommand::OpenRecent:
            m_projectController.requestOpenRecent(m_document, actions.projectControls.recentPath);
            break;
        case Components::ProjectControlCommand::None:
        default:
            break;
    }

    if (actions.projectControls.command != Components::ProjectControlCommand::None) {
        consumeProjectControllerEffects();
    }

    XpressFormula::Application::UnsavedProjectChoice controllerChoice =
        XpressFormula::Application::UnsavedProjectChoice::None;
    switch (actions.unsavedProjectChoice) {
        case Components::UnsavedProjectDialogChoice::Save:
            controllerChoice = XpressFormula::Application::UnsavedProjectChoice::Save;
            break;
        case Components::UnsavedProjectDialogChoice::Discard:
            controllerChoice = XpressFormula::Application::UnsavedProjectChoice::Discard;
            break;
        case Components::UnsavedProjectDialogChoice::Cancel:
            controllerChoice = XpressFormula::Application::UnsavedProjectChoice::Cancel;
            break;
        case Components::UnsavedProjectDialogChoice::None:
        default:
            break;
    }

    if (controllerChoice != XpressFormula::Application::UnsavedProjectChoice::None) {
        m_projectController.handleUnsavedChoice(m_document, controllerChoice);
        consumeProjectControllerEffects();
    }

    if (actions.requestOpenExportDialog) {
        m_exportController.requestOpen();
        m_state.redrawRequested = true;
    }

    if (actions.requestManualUpdateCheck && m_updateController.requestManualCheck()) {
        m_state.redrawRequested = true;
    }
    if (actions.updateDetailsExpandedChanged) {
        m_updateController.setVersionDetailsExpanded(actions.updateDetailsExpanded);
        m_state.redrawRequested = true;
    }
    if (actions.requestDismissUpdateNotice && m_updateController.dismissNotice()) {
        m_state.redrawRequested = true;
    }
    if (actions.requestOpenReleasePage) {
        std::string releaseUrl = m_updateController.releaseUrl();
        if (releaseUrl.empty()) {
            releaseUrl = std::string(
                XpressFormula::Application::UpdateController::defaultReleaseUrlUtf8());
        }
        std::wstring releaseUrlWide = XFWUtf::utf8ToUtf16OrEmpty(releaseUrl);
        if (releaseUrlWide.empty()) {
            releaseUrlWide = XFWUtf::utf8ToUtf16OrEmpty(
                XpressFormula::Application::UpdateController::defaultReleaseUrlUtf8());
        }
        const bool opened = !releaseUrlWide.empty() &&
            m_composition.shellService.openUrl(releaseUrlWide);
        if (m_updateController.recordReleasePageOpenResult(opened)) {
            m_state.redrawRequested = true;
        }
    }
    if (actions.requestOpenSupportPage) {
        const std::string supportUrl(
            XpressFormula::Application::UpdateController::supportUrlUtf8());
        const std::wstring supportUrlWide = XFWUtf::utf8ToUtf16OrEmpty(supportUrl);
        const bool opened = !supportUrlWide.empty() &&
            m_composition.shellService.openUrl(supportUrlWide);
        if (m_updateController.recordSupportPageOpenResult(opened)) {
            m_state.redrawRequested = true;
        }
    }

    if (actions.documentChanged) {
        syncDocumentDependentState();
    }
    if (actions.resetAutoRotationRuntime) {
        m_state.plotRuntime.resetAutoRotation();
    }
    if (actions.redrawRequested) {
        m_state.redrawRequested = true;
    }
}

void Application::handleProjectShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsAnyItemActive()) {
        return;
    }

    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;
    const bool alt = io.KeyAlt;
    if (!ctrl || alt) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_N)) {
        m_projectController.requestNew(m_document);
        consumeProjectControllerEffects();
    } else if (ImGui::IsKeyPressed(ImGuiKey_O)) {
        m_projectController.requestOpenDialog(m_document);
        consumeProjectControllerEffects();
    } else if (ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (shift) {
            (void)m_projectController.saveAs(m_document);
        } else {
            (void)m_projectController.save(m_document);
        }
        consumeProjectControllerEffects();
    }
}

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

    std::wstring branchWide = XFWUtf::utf8ToUtf16OrEmpty(XF_BUILD_BRANCH);
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

    m_projectController.startNewCleanDocument(m_document);
    m_mainWindow.resetFormulaColorCycle(static_cast<int>(m_document.formulas().size()));
    syncDocumentDependentState();
    m_projectController.loadRecentProjectPaths();

    m_updateController.resetStartupDelay(std::chrono::steady_clock::now());

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
                m_state.redrawRequested = true;
            }
            continue;
        }

        if (m_state.closeRequestedAfterFrame && m_hWnd) {
            m_state.closeRequestedAfterFrame = false;
            ::DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
            continue;
        }

        const Model::SceneSummary& scene = m_state.sceneSummary;
        const Model::PlotSettings& plotSettings = m_document.plotSettings();
        const XYRenderMode effectiveRenderMode = plotSettings.resolveXYRenderMode(scene);
        const XpressFormula::Application::RenderFramePolicyInput renderPolicy{
            plotSettings.optimizeRendering,
            m_state.redrawRequested,
            plotSettings.autoRotate,
            scene.hasVisible3D(),
            effectiveRenderMode
        };
        if (XpressFormula::Application::shouldWaitForNextMessage(renderPolicy)) {
            // Event-driven idle mode: avoid presenting frames when nothing changes.
            ::WaitMessage();
            continue;
        }

        // Handle swap-chain being occluded (minimised, etc.)
        if (m_swapChainOccluded &&
            m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            ::Sleep(XpressFormula::Application::occludedSwapChainSleepMilliseconds(renderPolicy));
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
        if (m_state.closeRequestedAfterFrame && m_hWnd) {
            m_state.closeRequestedAfterFrame = false;
            ::DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
            continue;
        }
        // Auto-rotate requires continuous redraws; otherwise render on demand when optimization is enabled.
        m_state.redrawRequested =
            XpressFormula::Application::redrawRequestedAfterPresentedFrame(renderPolicy);
    }
    return static_cast<int>(msg.wParam);
}

// ---- per-frame render -------------------------------------------------------

void Application::renderFrame() {
    const auto now = std::chrono::steady_clock::now();
    if (m_updateController.tick(now)) {
        m_state.redrawRequested = true;
    }

    // Export is intentionally deferred to a fresh frame after the export dialog closes.
    // This prevents the dialog window from being captured inside the exported image.
    m_exportController.promoteScheduledActions();

    m_exportController.tickAutoPreviewRefresh(now);

    if (m_exportController.dialogOpen() && m_exportController.consumePreviewRefreshRequest()) {
        refreshExportPreviewTexture();
        // Offscreen preview rendering uses a separate hidden ImGui frame in the same context,
        // which can clear popup state. Re-open the export popup in the visible UI frame.
        m_exportController.requestPopupOpenNextFrame();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    updatePlotCamera(ImGui::GetIO().DeltaTime);
    handleProjectShortcuts();

    PlotRenderOverrides exportOverrides;
    if (m_exportController.pendingSave() || m_exportController.pendingCopy()) {
        exportOverrides = plotRenderOverridesForExport(m_exportController.pendingSettings());
    }

    PlotSettings runtimePlotSettings = m_document.plotSettings();
    normalizePlotSettings(runtimePlotSettings);
    const XYRenderMode runtimeRenderMode =
        runtimePlotSettings.resolveXYRenderMode(m_state.sceneSummary);
    std::optional<float> runtimeAzimuthDeg;
    if (runtimePlotSettings.autoRotate &&
        m_state.sceneSummary.hasVisible3D() &&
        runtimeRenderMode == XYRenderMode::Surface3D) {
        runtimeAzimuthDeg = XpressFormula::Application::effectiveRuntimeAzimuthDeg(
            runtimePlotSettings,
            m_state.plotRuntime);
    }

    const XpressFormula::Application::UpdateNotificationState updateState =
        m_updateController.notificationState();
    const std::vector<Components::RecentProjectItem> recentProjectItems =
        makeRecentProjectItems(m_projectController.recentProjectPaths());
    MainWindowContext workspaceContext{
        m_document,
        m_state.sceneSummary,
        Components::ProjectControlsContext{
            m_projectController.displayName(m_document),
            m_projectController.currentPathUtf8(),
            m_projectController.status(),
            &recentProjectItems
        },
        MainWindowUpdateNotification{
            updateState.checkInProgress,
            updateState.updateAvailable,
            updateState.noticeDismissed,
            updateState.versionDetailsExpanded,
            updateState.latestTag,
            updateState.releaseUrl,
            updateState.status
        },
        MainWindowBuildMetadata{
            XF_BUILD_VERSION,
            XF_BUILD_REPO_URL,
            XF_BUILD_BRANCH,
            XF_BUILD_COMMIT
        },
        m_exportController.status(),
        m_exportController.dialogOpen(),
        m_projectController.consumeUnsavedPromptRequest(),
        exportOverrides.active ? &exportOverrides : nullptr,
        runtimeAzimuthDeg
    };

    const MainWindowActions mainWindowActions =
        m_mainWindow.renderWorkspace(workspaceContext, m_state.sidebarWidth);
    handleMainWindowActions(mainWindowActions);
    renderExportDialog();
    syncDocumentDependentState();

    // ---- Render ----
    ImGui::Render();
    const float clearColor[4] = { 0.06f, 0.06f, 0.08f, 1.0f };
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, nullptr);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    processPendingExportActions();
    syncDocumentDependentState();

    HRESULT hr = m_swapChain->Present(1, 0); // VSync
    m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
}

void Application::updatePlotCamera(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) {
        return;
    }

    PlotSettings settingsSnapshot = m_document.plotSettings();
    normalizePlotSettings(settingsSnapshot);
    if (!settingsSnapshot.autoRotate) {
        if (m_state.plotRuntime.autoRotationOffsetDeg != 0.0f) {
            m_state.plotRuntime.resetAutoRotation();
            m_state.redrawRequested = true;
        }
        return;
    }

    const XYRenderMode renderMode = settingsSnapshot.resolveXYRenderMode(m_state.sceneSummary);
    if (!m_state.sceneSummary.hasVisible3D() ||
        renderMode != XYRenderMode::Surface3D) {
        return;
    }

    if (XpressFormula::Application::advanceAutoRotation(
            m_state.plotRuntime,
            settingsSnapshot,
            deltaSeconds)) {
        m_state.redrawRequested = true;
    }
}

void Application::initialiseExportDialogSize() {
    if (m_exportController.sizeInitialized()) {
        return;
    }

    const Core::ViewTransform& viewTransform = m_document.viewTransform();
    int width = static_cast<int>(std::lround((std::max)(1.0f, viewTransform.viewport.width)));
    int height = static_cast<int>(std::lround((std::max)(1.0f, viewTransform.viewport.height)));
    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;

    auto& settings = m_exportController.dialogSettings();
    settings.size.width = width;
    settings.size.height = height;
    settings.size.scale = 1;
    settings.size.selectedPreset = kExportSizePresetCurrent;
    m_exportController.setSizeInitialized(true);
}

void Application::renderExportDialog() {
    if (m_exportController.consumeOpenRequest()) {
        auto& settings = m_exportController.dialogSettings();
        if (settings.profile == ExportProfile::CurrentView) {
            applyCurrentViewScene(settings, exportSceneSettingsFromPlot(m_document.plotSettings()));
        }
        normalizeExportSettings(settings);
    }

    if (!m_exportController.dialogOpen()) {
        return;
    }

    initialiseExportDialogSize();

    const Core::ViewTransform& viewTransform = m_document.viewTransform();
    const int sourceWidth = static_cast<int>(std::lround((std::max)(1.0f, viewTransform.viewport.width)));
    const int sourceHeight = static_cast<int>(std::lround((std::max)(1.0f, viewTransform.viewport.height)));
    const ExportWorldBounds sourceBounds{
        viewTransform.worldXMin(), viewTransform.worldXMax(),
        viewTransform.worldYMin(), viewTransform.worldYMax()
    };

    Components::ExportDialogContext contentContext{
        m_exportController.dialogSettings(),
        m_exportController.settingsPaneWidthRef(),
        m_exportController.previewZoomRef(),
        m_exportController.previewPanXRef(),
        m_exportController.previewPanYRef(),
        m_exportController.previewCheckerboardRef(),
        m_exportController.previewStatus(),
        m_exportController.status()
    };
    contentContext.sourceWidth = sourceWidth;
    contentContext.sourceHeight = sourceHeight;
    contentContext.sourceBounds = sourceBounds;
    contentContext.currentScene = exportSceneSettingsFromPlot(m_document.plotSettings());
    contentContext.previewDirty = m_exportController.previewDirty();
    contentContext.previewRefreshRequested = m_exportController.previewRefreshRequested();
    contentContext.hasPreviewTexture = m_exportPreview.hasTexture();
    contentContext.previewTexture = reinterpret_cast<ImTextureID>(m_exportPreview.srv());
    contentContext.previewWidth = m_exportPreview.width();
    contentContext.previewHeight = m_exportPreview.height();
    contentContext.exportBusy = m_exportController.exportBusy();
    contentContext.lastSavedPathUtf8 = XFWUtf::utf16ToUtf8OrEmpty(m_exportController.lastSavedPath());

    ExportDialogWindowContext windowContext{
        m_exportController.dialogOpen(),
        m_exportController.popupOpenNextFrame(),
        m_exportController.centerOnOpen(),
        contentContext
    };
    const ExportDialogWindowActions windowActions =
        m_mainWindow.renderExportDialog(windowContext);

    if (windowActions.clearPopupOpenNextFrame) {
        m_exportController.clearPopupOpenNextFrame();
    }
    if (windowActions.clearCenterOnOpen) {
        m_exportController.clearCenterOnOpen();
    }

    const Components::ExportDialogAction& action = windowActions.dialog;
    if (action.settingsChanged) {
        markExportPreviewOutOfDate();
    }
    if (action.requestFinalPreview) {
        m_exportController.requestFinalQualityPreview();
        m_state.redrawRequested = true;
    } else if (action.requestPreview) {
        requestExportPreviewRefresh();
    }
    if (action.requestCopy) {
        m_exportController.queueCopy(m_exportController.dialogSettings());
        m_state.redrawRequested = true;
    }
    if (action.requestSave) {
        m_exportController.queueSave(m_exportController.dialogSettings());
        m_state.redrawRequested = true;
    }
    if (action.openSavedImage) {
        openLastSavedExport();
    }
    if (action.showSavedImageInFolder) {
        showLastSavedExportInFolder();
    }
    if (action.copySavedImagePath) {
        copyLastSavedExportPath();
    }
    if (action.redrawRequested) {
        m_state.redrawRequested = true;
    }

    if (windowActions.closeDialog) {
        m_exportController.closeDialog();
    } else if (windowActions.setDialogOpen) {
        m_exportController.setDialogOpen(windowActions.dialogOpen);
    }
}

bool Application::refreshExportPreviewTexture() {
    m_exportController.setPreviewDirty(false);
    m_exportController.previewStatus().clear();

    if (!m_device || !m_deviceContext || !m_hWnd || !::IsWindow(m_hWnd)) {
        m_exportController.setPreviewStatus("Preview unavailable: renderer not initialized.");
        return false;
    }

    const ExportPreviewQuality requestedPreviewQuality =
        m_exportController.consumePreviewQuality(
            m_exportController.dialogSettings().quality.previewQuality);

    ExportPreviewSize previewSize;
    const ExportSettings previewSettings = XFExport::settingsForPreviewRender(
        m_exportController.dialogSettings(), requestedPreviewQuality, previewSize);

    std::vector<std::uint8_t> pixels;
    int renderedW = 0;
    int renderedH = 0;
    if (!renderPlotPixelsOffscreen(previewSettings, pixels, renderedW, renderedH)) {
        m_exportController.setPreviewStatus("Preview render failed.");
        m_exportPreview.reset();
        return false;
    }

    XFExport::ProcessedImage previewImage = XFExport::preparePreviewRgbaImage(
        pixels, renderedW, renderedH, previewSize.width, previewSize.height,
        previewSettings.appearance.grayscaleOutput);
    if (previewImage.width <= 0 || previewImage.height <= 0) {
        m_exportController.setPreviewStatus("Preview render produced invalid size.");
        m_exportPreview.reset();
        return false;
    }

    std::string textureError;
    if (!m_exportPreview.update(m_device,
                                m_deviceContext,
                                previewImage.pixels,
                                previewImage.width,
                                previewImage.height,
                                textureError)) {
        m_exportController.setPreviewStatus(textureError);
        return false;
    }

    std::string status = "Preview ready (" + std::to_string(previewImage.width) + "x" +
        std::to_string(previewImage.height) + ", " +
        exportPreviewQualityLabel(requestedPreviewQuality) + ", " +
        exportAspectModeLabel(m_exportController.dialogSettings().output.aspectMode);
    if (previewSize.reducedFromOutput) {
        status += ", reduced";
    }
    status += ").";
    m_exportController.setPreviewStatus(std::move(status));
    return true;
}
// ---- shutdown ---------------------------------------------------------------

void Application::shutdown() {
    (void)m_updateController.waitForPendingCheck();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (m_comInitialized) {
        ::CoUninitialize();
        m_comInitialized = false;
    }

    m_exportPreview.reset();
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
    m_exportPreview.reset();
    cleanupRenderTarget();
    if (m_swapChain)     { m_swapChain->Release();     m_swapChain     = nullptr; }
    if (m_deviceContext) { m_deviceContext->Release(); m_deviceContext = nullptr; }
    if (m_device)        { m_device->Release();        m_device        = nullptr; }
}

void Application::createRenderTarget() {
    XFWin::ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0,
                           __uuidof(ID3D11Texture2D),
                           reinterpret_cast<void**>(backBuffer.put()));
    if (backBuffer) {
        m_device->CreateRenderTargetView(backBuffer.get(), nullptr, &m_renderTargetView);
    }
}

void Application::cleanupRenderTarget() {
    if (m_renderTargetView) {
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
}

void Application::markExportPreviewOutOfDate() {
    m_exportController.markPreviewOutOfDate(
        m_exportController.dialogSettings().quality.autoRefreshPreview);
    m_state.redrawRequested = true;
}

void Application::requestExportPreviewRefresh() {
    m_exportController.requestPreviewRefresh();
    m_state.redrawRequested = true;
}

bool Application::capturePlotPixels(std::vector<std::uint8_t>& pixels, int& width, int& height) {
    width = 0;
    height = 0;
    pixels.clear();

    if (!m_swapChain || !m_device || !m_deviceContext) {
        return false;
    }

    XFWin::ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(m_swapChain->GetBuffer(0,
                                      __uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void**>(backBuffer.put()))) ||
        !backBuffer) {
        return false;
    }

    D3D11_TEXTURE2D_DESC backDesc = {};
    backBuffer->GetDesc(&backDesc);

    const Core::ViewTransform& viewTransform = m_document.viewTransform();
    int left = static_cast<int>(std::floor(viewTransform.viewport.originX));
    int top = static_cast<int>(std::floor(viewTransform.viewport.originY));
    int right = left + static_cast<int>(std::floor(viewTransform.viewport.width));
    int bottom = top + static_cast<int>(std::floor(viewTransform.viewport.height));

    left = std::clamp(left, 0, static_cast<int>(backDesc.Width));
    top = std::clamp(top, 0, static_cast<int>(backDesc.Height));
    right = std::clamp(right, 0, static_cast<int>(backDesc.Width));
    bottom = std::clamp(bottom, 0, static_cast<int>(backDesc.Height));

    width = right - left;
    height = bottom - top;
    if (width <= 0 || height <= 0) {
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc = backDesc;
    stagingDesc.Width = static_cast<UINT>(width);
    stagingDesc.Height = static_cast<UINT>(height);
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;

    XFWin::ComPtr<ID3D11Texture2D> stagingTexture;
    if (FAILED(m_device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.put())) ||
        !stagingTexture) {
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
        stagingTexture.get(), 0, 0, 0, 0, backBuffer.get(), 0, &sourceBox);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT mapResult = m_deviceContext->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(mapResult)) {
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

    m_deviceContext->Unmap(stagingTexture.get(), 0);
    return true;
}

bool Application::readTexturePixelsRgba(ID3D11Texture2D* sourceTexture,
                                        std::vector<std::uint8_t>& pixels,
                                        int& width, int& height) {
    width = 0;
    height = 0;
    pixels.clear();

    if (!sourceTexture || !m_device || !m_deviceContext) {
        return false;
    }

    D3D11_TEXTURE2D_DESC srcDesc = {};
    sourceTexture->GetDesc(&srcDesc);
    if (srcDesc.Width == 0 || srcDesc.Height == 0) {
        return false;
    }

    width = static_cast<int>(srcDesc.Width);
    height = static_cast<int>(srcDesc.Height);

    D3D11_TEXTURE2D_DESC stagingDesc = srcDesc;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;

    XFWin::ComPtr<ID3D11Texture2D> stagingTexture;
    if (FAILED(m_device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.put())) ||
        !stagingTexture) {
        return false;
    }

    m_deviceContext->CopyResource(stagingTexture.get(), sourceTexture);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT mapResult = m_deviceContext->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(mapResult)) {
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(width) * 4u;
    pixels.resize(static_cast<size_t>(height) * rowBytes);
    for (int y = 0; y < height; ++y) {
        const auto* src = static_cast<const std::uint8_t*>(mapped.pData) +
            static_cast<size_t>(y) * mapped.RowPitch;
        auto* dst = pixels.data() + static_cast<size_t>(y) * rowBytes;
        std::memcpy(dst, src, rowBytes);
    }

    m_deviceContext->Unmap(stagingTexture.get(), 0);
    return true;
}

bool Application::renderPlotPixelsOffscreen(const ExportSettings& settings,
                                            std::vector<std::uint8_t>& pixels,
                                            int& width, int& height) {
    width = 0;
    height = 0;
    pixels.clear();

    if (!m_device || !m_deviceContext) {
        return false;
    }

    XFExport::ExportRenderRequest request =
        XFExport::buildExportRenderRequest(
            settings,
            m_document.viewTransform(),
            m_document.plotSettings(),
            m_state.sceneSummary);

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = static_cast<UINT>(request.targetWidth);
    texDesc.Height = static_cast<UINT>(request.targetHeight);
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    XFWin::ComPtr<ID3D11Texture2D> renderTexture;
    XFWin::ComPtr<ID3D11RenderTargetView> exportRTV;
    if (FAILED(m_device->CreateTexture2D(&texDesc, nullptr, renderTexture.put())) ||
        !renderTexture) {
        return false;
    }
    if (FAILED(m_device->CreateRenderTargetView(renderTexture.get(), nullptr, exportRTV.put())) ||
        !exportRTV) {
        return false;
    }

    XFWin::ComPtr<ID3D11RenderTargetView> previousRTV;
    XFWin::ComPtr<ID3D11DepthStencilView> previousDSV;
    m_deviceContext->OMGetRenderTargets(1, previousRTV.put(), previousDSV.put());

    UINT prevViewportCount = 1;
    D3D11_VIEWPORT prevViewport = {};
    m_deviceContext->RSGetViewports(&prevViewportCount, &prevViewport);

    D3D11_VIEWPORT exportViewport = {};
    exportViewport.TopLeftX = 0.0f;
    exportViewport.TopLeftY = 0.0f;
    exportViewport.Width = static_cast<float>(request.targetWidth);
    exportViewport.Height = static_cast<float>(request.targetHeight);
    exportViewport.MinDepth = 0.0f;
    exportViewport.MaxDepth = 1.0f;

    ID3D11RenderTargetView* exportTarget = exportRTV.get();
    m_deviceContext->OMSetRenderTargets(1, &exportTarget, nullptr);
    m_deviceContext->RSSetViewports(1, &exportViewport);
    const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_deviceContext->ClearRenderTargetView(exportRTV.get(), clear);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 prevDisplaySize = io.DisplaySize;
    const ImVec2 prevMousePos = io.MousePos;
    const float prevMouseWheel = io.MouseWheel;
    const float prevMouseWheelH = io.MouseWheelH;
    io.DisplaySize = ImVec2(static_cast<float>(request.targetWidth), static_cast<float>(request.targetHeight));
    io.MousePos = ImVec2(-100000.0f, -100000.0f);
    io.MouseWheel = 0.0f;
    io.MouseWheelH = 0.0f;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(request.targetWidth), static_cast<float>(request.targetHeight)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::Begin("##ExportPlotOffscreen", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            windowPos,
            ImVec2(windowPos.x + static_cast<float>(request.targetWidth),
                   windowPos.y + static_cast<float>(request.targetHeight)),
            ImGui::ColorConvertFloat4ToU32(
                ImVec4(request.backgroundColor[0], request.backgroundColor[1],
                       request.backgroundColor[2], request.backgroundColor[3])));

        ImGui::SetCursorScreenPos(
            ImVec2(windowPos.x + static_cast<float>(request.resolvedView.marginLeftPx),
                   windowPos.y + static_cast<float>(request.resolvedView.marginTopPx)));
        const ImVec2 contentSize(
            static_cast<float>(request.resolvedView.contentWidthPx),
            static_cast<float>(request.resolvedView.contentHeightPx));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::BeginChild("##ExportPlotOffscreenContent", contentSize, ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse |
                              ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoInputs |
                              ImGuiWindowFlags_NoBackground)) {
            PlotPanel exportPlotPanel;
            exportPlotPanel.render(m_document.formulas(), request.view, request.plotSettings, request.scene,
                                   &request.overrides, &request.quality);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    io.DisplaySize = prevDisplaySize;
    io.MousePos = prevMousePos;
    io.MouseWheel = prevMouseWheel;
    io.MouseWheelH = prevMouseWheelH;

    const bool readOk = readTexturePixelsRgba(renderTexture.get(), pixels, width, height);

    if (prevViewportCount > 0) {
        m_deviceContext->RSSetViewports(1, &prevViewport);
    }
    ID3D11RenderTargetView* previousTarget = previousRTV.get();
    m_deviceContext->OMSetRenderTargets(1, &previousTarget, previousDSV.get());
    return readOk;
}

void Application::openLastSavedExport() {
    const std::wstring& lastSavedPath = m_exportController.lastSavedPath();
    XFApp::ExportShellAdapter shell(m_composition.shellService);
    if (lastSavedPath.empty()) {
        m_exportController.setStatus("No saved export path is available.");
    } else if (shell.openPath(lastSavedPath)) {
        m_exportController.setStatus("Opened saved image.");
    } else {
        m_exportController.setStatus("Could not open saved image.");
    }
    m_state.redrawRequested = true;
}

void Application::showLastSavedExportInFolder() {
    const std::wstring& lastSavedPath = m_exportController.lastSavedPath();
    XFApp::ExportShellAdapter shell(m_composition.shellService);
    if (lastSavedPath.empty()) {
        m_exportController.setStatus("No saved export path is available.");
    } else if (shell.revealPath(lastSavedPath)) {
        m_exportController.setStatus("Opened saved image location.");
    } else {
        m_exportController.setStatus("Could not show saved image in folder.");
    }
    m_state.redrawRequested = true;
}

void Application::copyLastSavedExportPath() {
    const std::wstring& lastSavedPath = m_exportController.lastSavedPath();
    XFApp::ExportClipboardAdapter clipboard(m_composition.clipboardService, m_hWnd);
    if (lastSavedPath.empty()) {
        m_exportController.setStatus("No saved export path is available.");
    } else if (clipboard.copyText(lastSavedPath)) {
        m_exportController.setStatus("Copied saved image path.");
    } else {
        m_exportController.setStatus("Could not copy saved image path.");
    }
    m_state.redrawRequested = true;
}

void Application::processPendingExportActions() {
    if (!m_exportController.hasPendingActions()) {
        return;
    }

    const ExportSettings settings = m_exportController.pendingSettings();
    std::vector<std::string> messages;
    std::vector<std::uint8_t> capturedPixels;
    int capturedWidth = 0;
    int capturedHeight = 0;
    if (!renderPlotPixelsOffscreen(settings, capturedPixels, capturedWidth, capturedHeight)) {
        // Fallback to visible backbuffer capture if offscreen rendering fails unexpectedly.
        if (!capturePlotPixels(capturedPixels, capturedWidth, capturedHeight)) {
            messages.emplace_back("Export failed: unable to render/capture plot area.");
            m_exportController.clearPendingActions();
            m_state.redrawRequested = true;
            m_exportController.setStatus(messages.front());
            return;
        }
        messages.emplace_back("Warning: export used fallback screen capture path.");
    }

    XFExport::ProcessedImage outputImage = XFExport::prepareFinalBgraImage(
        capturedPixels, capturedWidth, capturedHeight, settings);
    if (outputImage.width <= 0 || outputImage.height <= 0 || outputImage.pixels.empty()) {
        messages.emplace_back("Export failed: rendered image was invalid.");
        m_exportController.clearPendingActions();
        m_state.redrawRequested = true;
        m_exportController.setStatus(messages.front());
        return;
    }

    XFApp::ExportImageEncoderAdapter imageEncoder(m_composition.imageEncoder);
    XFApp::ExportClipboardAdapter clipboard(m_composition.clipboardService, m_hWnd);
    XFApp::ExportShellAdapter shell(m_composition.shellService);
    const XFExport::ExportAppMetadata appMetadata{
        "XpressFormula",
        XF_BUILD_VERSION,
        XF_BUILD_REPO_URL,
        XF_BUILD_BRANCH,
        XF_BUILD_COMMIT
    };
    XFApp::ExportMetadataSidecarWriter metadataWriter(
        XFApp::ExportMetadataContext{
            appMetadata,
            &m_document.formulas(),
            &m_document.viewTransform(),
            &m_document.plotSettings()
        });

    if (m_exportController.pendingSave()) {
        const XFWin::ExportImageFormat preferredFormat =
            (settings.output.format == ExportFormat::Bmp)
                ? XFWin::ExportImageFormat::Bmp
                : XFWin::ExportImageFormat::Png;
        const XFWin::DialogResult dialog =
            m_composition.fileDialogService.saveImage(m_hWnd, preferredFormat);
        if (dialog.selected()) {
            const std::wstring& path = dialog.path;
            const std::filesystem::path sidecarPath = XFExport::exportMetadataSidecarPath(
                std::filesystem::path(path));
            XFExport::ExportOutputResult saveResult = XFExport::saveRenderedImage(
                settings,
                path,
                outputImage.pixels,
                outputImage.width,
                outputImage.height,
                imageEncoder,
                metadataWriter,
                shell,
                clipboard,
                XFWUtf::utf16ToUtf8OrEmpty(path),
                XFWUtf::utf16ToUtf8OrEmpty(sidecarPath.wstring()));
            if (saveResult.savedImage) {
                m_exportController.setLastSavedPath(saveResult.savedPath);
            }
            XFExport::appendMessages(messages, saveResult.messages);
        } else if (!dialog.cancelled()) {
            messages.emplace_back("Save failed: " + dialog.error);
        } else {
            messages.emplace_back("Save canceled.");
        }
    }

    if (m_exportController.pendingCopy()) {
        XFExport::ExportOutputResult copyResult = XFExport::copyRenderedImage(
            outputImage.pixels, outputImage.width, outputImage.height, clipboard);
        XFExport::appendMessages(messages, copyResult.messages);
    }

    m_exportController.clearPendingActions();
    // The export frame may be rendered with export-only overrides. Request one more frame
    // so the interactive view returns to the user's normal display settings.
    m_state.redrawRequested = true;
    if (m_exportController.dialogOpen()) {
        m_exportController.requestPopupOpenNextFrame();
    }

    if (!messages.empty()) {
        m_exportController.setStatus(XFExport::joinExportMessages(messages));
    }
}
} // namespace XpressFormula::UI
