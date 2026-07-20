// SPDX-License-Identifier: MIT
// Application.cpp - Win32 + D3D11 + ImGui application implementation.
#include "Application.h"
#include "../Core/UpdateVersionUtils.h"
#include "../Infrastructure/FileSystem/AtomicFileWriter.h"
#include "../Infrastructure/Export/ExportMetadataSerializer.h"
#include "../Infrastructure/Export/ExportOutputWorkflow.h"
#include "../Infrastructure/Export/ExportRenderRequest.h"
#include "../Infrastructure/Export/ImageProcessor.h"
#include "../Platform/Windows/ComPtr.h"
#include "../Platform/Windows/WinHttpClient.h"
#include "../Platform/Windows/Utf.h"
#include "../Version.h"
#include "../resource.h"
#include "FormulaEntry.h"
#include "Components/ExportDialog.h"
#include "Components/PlotToolbar.h"
#include "UiKit/Splitter.h"
#include "UiKit/UiMetrics.h"

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
namespace XFPersistence = XpressFormula::Infrastructure::Persistence;
namespace XFWin = XpressFormula::Platform::Windows;
namespace XFWUtf = XpressFormula::Platform::Windows;

namespace {

constexpr const wchar_t* kGitHubLatestReleaseApiHost = L"api.github.com";
constexpr const wchar_t* kGitHubLatestReleaseApiPath = L"/repos/russlank/XpressFormula/releases/latest";
constexpr const wchar_t* kGitHubReleasesUrlW = L"https://github.com/russlank/XpressFormula/releases";
constexpr const char* kGitHubReleasesUrlUtf8 = "https://github.com/russlank/XpressFormula/releases";
constexpr const wchar_t* kBuyMeACoffeeUrlW = L"https://buymeacoffee.com/russlank";
constexpr const char* kBuyMeACoffeeUrlUtf8 = "https://buymeacoffee.com/russlank";

// Background worker: query GitHub Releases API, parse the latest tag/url, and compare against the
// app semantic version. This runs off the UI thread so startup and manual checks do not stall ImGui.
XpressFormula::UI::Application::UpdateCheckResult fetchLatestReleaseFromGitHub(bool manualRequest) {
    using namespace XpressFormula::Core::UpdateVersionUtils;

    XpressFormula::UI::Application::UpdateCheckResult result;
    result.manualRequest = manualRequest;
    result.releaseUrl = kGitHubReleasesUrlUtf8;

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

Application::Application()  = default;
Application::~Application() = default;

void Application::resetToDefaultProject() {
    m_formulas.clear();

    Model::Formula defaultEntry;
    defaultEntry.setExpression("sin(sqrt(x^2+y^2))");
    for (std::size_t channel = 0; channel < defaultEntry.color.size(); ++channel) {
        defaultEntry.color[channel] = kDefaultPalette[0][channel];
    }
    defaultEntry.compile();
    m_formulas.push_back(std::move(defaultEntry));
    refreshSceneSummary();

    m_viewTransform.reset();
    m_plotSettings = PlotSettings{};
    m_plotSettings.applyCoordinateOverlayPolicy();
    m_formulaPanel.resetColorCycle(1);
    m_exportController.dialogSettings() = defaultExportSettings();
    m_exportController.setSizeInitialized(false);
    markExportPreviewOutOfDate();
}

void Application::refreshSceneSummary() {
    m_sceneSummary = Model::analyzeScene(
        std::span<const Model::Formula>(m_formulas.data(), m_formulas.size()));
}

XFPersistence::ProjectSession Application::currentProjectSession() const {
    return XFPersistence::makeProjectSession(m_formulas, m_viewTransform, m_plotSettings);
}

void Application::markProjectClean() {
    m_savedProjectSnapshot =
        XFPersistence::serializeCurrentProjectSession(m_formulas, m_viewTransform, m_plotSettings);
    m_projectDirty = false;
}

void Application::refreshProjectDirtyState() {
    if (m_savedProjectSnapshot.empty()) {
        markProjectClean();
        return;
    }
    m_projectDirty =
        (XFPersistence::serializeCurrentProjectSession(m_formulas, m_viewTransform, m_plotSettings) !=
         m_savedProjectSnapshot);
}

std::string Application::projectDisplayName() const {
    std::string name = m_projectPath.empty()
        ? std::string("Untitled.xfplot")
        : XFWUtf::utf16ToUtf8OrEmpty(std::filesystem::path(m_projectPath).filename().wstring());
    if (name.empty()) {
        name = "Untitled.xfplot";
    }
    if (m_projectDirty) {
        name += " *";
    }
    return name;
}

void Application::loadRecentProjectPaths() {
    m_recentProjectPaths = m_recentProjectsStore.load().paths;
}

void Application::saveRecentProjectPaths() const {
    m_recentProjectsStore.save(m_recentProjectPaths);
}

void Application::addRecentProjectPath(const std::wstring& path) {
    m_recentProjectsStore.add(m_recentProjectPaths, path);
}

bool Application::saveProjectToPath(const std::wstring& path, std::string& error) {
    error.clear();
    const std::filesystem::path projectPath(path);
    const XFPersistence::ProjectSaveResult saveResult =
        m_projectRepository.save(projectPath, currentProjectSession());
    if (!saveResult) {
        error = "Could not save project file: " + saveResult.error;
        return false;
    }

    m_projectPath = projectPath.wstring();
    addRecentProjectPath(m_projectPath);
    markProjectClean();
    m_projectStatus = "Saved project: " + XFWUtf::utf16ToUtf8OrEmpty(m_projectPath);
    m_redrawRequested = true;
    return true;
}

bool Application::saveProjectAs() {
    const XFWin::DialogResult dialog = m_fileDialogService.saveProject(m_hWnd, m_projectPath);
    if (dialog.cancelled()) {
        m_projectStatus = "Save project canceled.";
        return false;
    }
    if (!dialog.selected()) {
        m_projectStatus = "Save project failed: " + dialog.error;
        return false;
    }

    std::string error;
    if (!saveProjectToPath(dialog.path, error)) {
        m_projectStatus = "Save project failed: " + error;
        return false;
    }
    return true;
}

bool Application::saveProject() {
    if (m_projectPath.empty()) {
        return saveProjectAs();
    }

    std::string error;
    if (!saveProjectToPath(m_projectPath, error)) {
        m_projectStatus = "Save project failed: " + error;
        return false;
    }
    return true;
}

bool Application::openProjectFromPath(const std::wstring& path, std::string& error) {
    error.clear();
    const XFPersistence::ProjectLoadResult loaded =
        m_projectRepository.load(std::filesystem::path(path));
    if (!loaded) {
        error = loaded.error;
        return false;
    }

    XFPersistence::ProjectMapResult mapped =
        XFPersistence::mapProjectSessionToDocument(loaded.session);
    std::vector<std::string> warnings = loaded.warnings;
    warnings.insert(warnings.end(), mapped.warnings.begin(), mapped.warnings.end());

    m_formulas = std::move(mapped.document.formulas);
    m_viewTransform.state = mapped.document.view.state;
    m_plotSettings = mapped.document.plot;
    m_formulaPanel.resetColorCycle(static_cast<int>(m_formulas.size()));
    refreshSceneSummary();
    m_projectPath = std::filesystem::absolute(std::filesystem::path(path)).wstring();
    addRecentProjectPath(m_projectPath);
    markProjectClean();
    markExportPreviewOutOfDate();

    std::ostringstream status;
    status << "Opened project: " << XFWUtf::utf16ToUtf8OrEmpty(m_projectPath);
    if (!warnings.empty()) {
        status << " (" << warnings.size() << " warning";
        if (warnings.size() != 1) {
            status << "s";
        }
        status << ": " << warnings.front() << ")";
    }
    m_projectStatus = status.str();
    m_redrawRequested = true;
    return true;
}

bool Application::openProjectFromDialog() {
    const XFWin::DialogResult dialog = m_fileDialogService.openProject(m_hWnd);
    if (dialog.cancelled()) {
        m_projectStatus = "Open project canceled.";
        return false;
    }
    if (!dialog.selected()) {
        m_projectStatus = "Open project failed: " + dialog.error;
        return false;
    }

    std::string error;
    if (!openProjectFromPath(dialog.path, error)) {
        m_projectStatus = "Open project failed: " + error;
        return false;
    }
    return true;
}

void Application::executeProjectAction(PendingProjectAction action, const std::wstring& path) {
    switch (action) {
        case PendingProjectAction::NewProject:
            resetToDefaultProject();
            m_projectPath.clear();
            markProjectClean();
            m_projectStatus = "Started a new project.";
            break;
        case PendingProjectAction::OpenDialog:
            openProjectFromDialog();
            break;
        case PendingProjectAction::OpenRecent: {
            std::string error;
            if (!openProjectFromPath(path, error)) {
                m_projectStatus = "Open recent project failed: " + error;
                std::error_code pathError;
                if (!std::filesystem::exists(std::filesystem::path(path), pathError)) {
                    m_recentProjectsStore.remove(m_recentProjectPaths, path);
                }
            }
            break;
        }
        case PendingProjectAction::CloseApp:
            m_closeRequestedAfterFrame = true;
            break;
        case PendingProjectAction::None:
        default:
            break;
    }
    m_redrawRequested = true;
}

void Application::requestProjectAction(PendingProjectAction action, std::wstring path) {
    refreshProjectDirtyState();
    if (m_projectDirty) {
        m_pendingProjectAction = action;
        m_pendingProjectPath = std::move(path);
        m_openProjectDiscardPopupNextFrame = true;
        m_redrawRequested = true;
        return;
    }

    executeProjectAction(action, path);
}

bool Application::requestClose() {
    refreshProjectDirtyState();
    if (!m_projectDirty) {
        m_closeRequestedAfterFrame = true;
        m_redrawRequested = true;
        return false;
    }

    m_pendingProjectAction = PendingProjectAction::CloseApp;
    m_pendingProjectPath.clear();
    m_openProjectDiscardPopupNextFrame = true;
    m_redrawRequested = true;
    return false;
}

void Application::renderProjectControls() {
    ImGui::TextUnformatted("Project");
    ImGui::Separator();
    ImGui::TextWrapped("%s", projectDisplayName().c_str());
    if (!m_projectPath.empty()) {
        ImGui::SetItemTooltip("%s", XFWUtf::utf16ToUtf8OrEmpty(m_projectPath).c_str());
    }

    const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("New", ImVec2(buttonWidth, 0.0f))) {
        requestProjectAction(PendingProjectAction::NewProject);
    }
    ImGui::SameLine();
    if (ImGui::Button("Open...", ImVec2(buttonWidth, 0.0f))) {
        requestProjectAction(PendingProjectAction::OpenDialog);
    }
    if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f))) {
        saveProject();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As...", ImVec2(buttonWidth, 0.0f))) {
        saveProjectAs();
    }

    if (!m_projectStatus.empty()) {
        ImGui::TextWrapped("%s", m_projectStatus.c_str());
    }

    if (!m_recentProjectPaths.empty() &&
        ImGui::CollapsingHeader("Recent Projects", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < static_cast<int>(m_recentProjectPaths.size()); ++i) {
            ImGui::PushID(i);
            const std::wstring& path = m_recentProjectPaths[static_cast<std::size_t>(i)];
            std::string label = XFWUtf::utf16ToUtf8OrEmpty(std::filesystem::path(path).filename().wstring());
            if (label.empty()) {
                label = XFWUtf::utf16ToUtf8OrEmpty(path);
            }
            std::error_code pathError;
            const bool pathExists = std::filesystem::exists(std::filesystem::path(path), pathError);
            if (!pathExists) {
                ImGui::BeginDisabled();
            }
            if (ImGui::SmallButton(label.c_str()) && pathExists) {
                requestProjectAction(PendingProjectAction::OpenRecent, path);
            }
            if (!pathExists) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (pathExists) {
                    ImGui::SetTooltip("%s", XFWUtf::utf16ToUtf8OrEmpty(path).c_str());
                } else {
                    ImGui::SetTooltip("Missing: %s", XFWUtf::utf16ToUtf8OrEmpty(path).c_str());
                }
            }
            ImGui::PopID();
        }
    }
}

void Application::renderProjectDiscardDialog() {
    static constexpr const char* kDiscardPopupId = "Unsaved Project Changes";

    if (m_openProjectDiscardPopupNextFrame) {
        ImGui::OpenPopup(kDiscardPopupId);
        m_openProjectDiscardPopupNextFrame = false;
    }

    if (ImGui::BeginPopupModal(kDiscardPopupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("The current project has unsaved changes.");
        ImGui::TextWrapped("Save before continuing, discard the changes, or cancel.");
        ImGui::Spacing();

        if (ImGui::Button("Save", ImVec2(96.0f, 0.0f))) {
            if (saveProject()) {
                const PendingProjectAction action = m_pendingProjectAction;
                const std::wstring path = m_pendingProjectPath;
                m_pendingProjectAction = PendingProjectAction::None;
                m_pendingProjectPath.clear();
                ImGui::CloseCurrentPopup();
                executeProjectAction(action, path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(96.0f, 0.0f))) {
            const PendingProjectAction action = m_pendingProjectAction;
            const std::wstring path = m_pendingProjectPath;
            m_pendingProjectAction = PendingProjectAction::None;
            m_pendingProjectPath.clear();
            ImGui::CloseCurrentPopup();
            executeProjectAction(action, path);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(96.0f, 0.0f))) {
            m_pendingProjectAction = PendingProjectAction::None;
            m_pendingProjectPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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
        requestProjectAction(PendingProjectAction::NewProject);
    } else if (ImGui::IsKeyPressed(ImGuiKey_O)) {
        requestProjectAction(PendingProjectAction::OpenDialog);
    } else if (ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (shift) {
            saveProjectAs();
        } else {
            saveProject();
        }
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

    resetToDefaultProject();
    markProjectClean();
    loadRecentProjectPaths();

    // Record startup time so we can defer the automatic update check.
    // Delaying the network call avoids triggering antivirus heuristics that flag
    // executables making outbound connections immediately after launch.
    m_startupTime = std::chrono::steady_clock::now();

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

        if (m_closeRequestedAfterFrame && m_hWnd) {
            m_closeRequestedAfterFrame = false;
            ::DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
            continue;
        }

        const Model::SceneSummary& scene = m_sceneSummary;
        const XYRenderMode effectiveRenderMode = m_plotSettings.resolveXYRenderMode(scene);
        const bool continuousRender =
            m_plotSettings.autoRotate &&
            scene.hasVisible3D() &&
            effectiveRenderMode == XYRenderMode::Surface3D;
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
        if (m_closeRequestedAfterFrame && m_hWnd) {
            m_closeRequestedAfterFrame = false;
            ::DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
            continue;
        }
        // Auto-rotate requires continuous redraws; otherwise render on demand when optimization is enabled.
        m_redrawRequested = optimizeRendering ? continuousRender : true;
    }
    return static_cast<int>(msg.wParam);
}

// ---- per-frame render -------------------------------------------------------

void Application::renderFrame() {
    // Deferred startup update check: wait ~60 seconds after launch before contacting the
    // network. This avoids antivirus heuristics that flag immediate outbound connections.
    if (!m_startupCheckDone && !m_updateCheckInProgress) {
        const auto elapsed = std::chrono::steady_clock::now() - m_startupTime;
        if (elapsed >= std::chrono::seconds(60)) {
            m_startupCheckDone = true;
            startUpdateCheck(false);
        }
    }

    // Poll async update-check completion before building the UI so the sidebar can show any
    // newly available result in the same visible frame.
    pollUpdateCheckResult();

    // Export is intentionally deferred to a fresh frame after the export dialog closes.
    // This prevents the dialog window from being captured inside the exported image.
    m_exportController.promoteScheduledActions();

    m_exportController.tickAutoPreviewRefresh(std::chrono::steady_clock::now());

    if (m_exportController.dialogOpen() && m_exportController.consumePreviewRefreshRequest()) {
        refreshExportPreviewTexture();
        // Offscreen preview rendering uses a separate hidden ImGui frame in the same context,
        // which can clear popup state. Re-open the export popup in the visible UI frame.
        m_exportController.requestPopupOpenNextFrame();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    refreshProjectDirtyState();
    handleProjectShortcuts();

    // We fill the entire OS window with the sidebar, a splitter, and the plot.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float totalW = viewport->WorkSize.x;
    float totalH = viewport->WorkSize.y;
    const UiKit::UiMetrics& uiMetrics = UiKit::metrics();
    const float splitterWidth = uiMetrics.splitterWidth;
    const float availableForSidebar = totalW - splitterWidth - uiMetrics.minimumPlotWidth;
    const float dynamicMinSidebar =
        (std::min)(uiMetrics.minimumSidebarWidth, (std::max)(180.0f, totalW * 0.38f));
    const float maxSidebar = (std::min)(uiMetrics.maximumSidebarWidth,
        (std::max)(dynamicMinSidebar, availableForSidebar));
    m_sidebarWidth = std::clamp(m_sidebarWidth, dynamicMinSidebar, maxSidebar);
    const float sidebar = m_sidebarWidth;
    const float plotX = viewport->WorkPos.x + sidebar + splitterWidth;
    const float plotWidth = (std::max)(1.0f, totalW - sidebar - splitterWidth);

    // ---- Left sidebar ----
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(sidebar, totalH));
    ImGui::Begin("##Sidebar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse);
    renderProjectControls();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    m_formulaPanel.render(m_formulas);
    refreshSceneSummary();
    const Model::SceneSummary& scene = m_sceneSummary;
    ImGui::Spacing();
    ImGui::Spacing();
    ControlPanelActions actions = m_controlPanel.render(
        m_viewTransform, m_plotSettings, scene, m_exportController.status());
    if (actions.requestOpenExportDialog) {
        m_exportController.requestOpen();
    }

    ImGui::Spacing();
    ImGui::Separator();
    const bool showUpdateAlert = m_updateAvailable && !m_updateNoticeDismissed;
    std::string versionDetailsLabel;
    if (showUpdateAlert && !m_updateLatestTag.empty()) {
        versionDetailsLabel = "New version available " + m_updateLatestTag;
    } else if (showUpdateAlert) {
        versionDetailsLabel = "New version available";
    } else if (m_updateCheckInProgress) {
        versionDetailsLabel = "Version details (checking...)";
    } else {
        versionDetailsLabel = "Version details";
    }
    versionDetailsLabel += "##VersionDetailsToggle";

    if (showUpdateAlert) {
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.76f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.86f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.62f, 0.12f, 0.12f, 1.0f));
    }
    ImGui::SetNextItemOpen(m_versionDetailsExpanded, ImGuiCond_Always);
    m_versionDetailsExpanded = ImGui::CollapsingHeader(
        versionDetailsLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
    if (showUpdateAlert) {
        ImGui::PopStyleColor(3);
    }

    if (m_versionDetailsExpanded) {
        ImGui::Spacing();
        ImGui::TextDisabled("Build Metadata");
        ImGui::TextDisabled("Version: %s", XF_BUILD_VERSION);
        ImGui::TextDisabled("Repo: %s", XF_BUILD_REPO_URL);
        ImGui::TextDisabled("Branch: %s", XF_BUILD_BRANCH);
        ImGui::TextDisabled("Commit: %s", XF_BUILD_COMMIT);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Updates");
        if (m_updateCheckInProgress) {
            ImGui::TextWrapped("Checking GitHub releases...");
        } else {
            if (showUpdateAlert) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 214, 110, 255));
                ImGui::TextWrapped("New version available: %s", m_updateLatestTag.c_str());
                ImGui::PopStyleColor();
            } else if (!m_updateStatus.empty()) {
                ImGui::TextWrapped("%s", m_updateStatus.c_str());
            } else {
                ImGui::TextWrapped("Checks for newer releases on GitHub.");
            }
        }

        if (ImGui::Button("Check For Updates", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            startUpdateCheck(true);
        }
        if (ImGui::Button("Open Releases Page", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            std::wstring releaseUrlWide = m_updateReleaseUrl.empty()
                ? std::wstring(kGitHubReleasesUrlW)
                : XFWUtf::utf8ToUtf16OrEmpty(m_updateReleaseUrl);
            if (releaseUrlWide.empty()) {
                releaseUrlWide = kGitHubReleasesUrlW;
            }
            if (!m_shellService.openUrl(releaseUrlWide)) {
                m_updateStatus = "Could not open browser. Visit: " + std::string(kGitHubReleasesUrlUtf8);
            } else if (m_updateAvailable) {
                m_updateNoticeDismissed = true;
            }
            m_redrawRequested = true;
        }
        if (ImGui::Button("Buy Me a Coffee", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            if (!m_shellService.openUrl(kBuyMeACoffeeUrlW)) {
                m_updateStatus = "Could not open browser. Visit: " + std::string(kBuyMeACoffeeUrlUtf8);
            }
            m_redrawRequested = true;
        }
        if (m_updateAvailable && !m_updateNoticeDismissed &&
            ImGui::Button("Dismiss Update Notice", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            m_updateNoticeDismissed = true;
            m_redrawRequested = true;
        }
    }
    ImGui::End();

    // ---- Sidebar splitter ----
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + sidebar, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(splitterWidth, totalH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##SidebarSplitter", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBackground);
    if (UiKit::drawVerticalSplitter("##SidebarSplitterHandle",
                                    totalH,
                                    m_sidebarWidth,
                                    dynamicMinSidebar,
                                    maxSidebar,
                                    uiMetrics.defaultSidebarWidth,
                                    splitterWidth)) {
        m_redrawRequested = true;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    renderExportDialog(sidebar, totalH);
    renderProjectDiscardDialog();

    // ---- Plot area ----
    ImGui::SetNextWindowPos(ImVec2(plotX, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(plotWidth, totalH));
    ImGui::Begin("##Plot", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoScrollbar);
    handlePlotShortcuts();
    renderPlotToolbar(scene);
    PlotRenderOverrides exportOverrides;
    if (m_exportController.pendingSave() || m_exportController.pendingCopy()) {
        exportOverrides = plotRenderOverridesForExport(m_exportController.pendingSettings());
    }
    m_plotPanel.render(m_formulas, m_viewTransform, m_plotSettings, scene,
                       exportOverrides.active ? &exportOverrides : nullptr);
    ImGui::End();

    // ---- Render ----
    ImGui::Render();
    const float clearColor[4] = { 0.06f, 0.06f, 0.08f, 1.0f };
    m_deviceContext->OMSetRenderTargets(1, &m_renderTargetView, nullptr);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    processPendingExportActions();
    refreshProjectDirtyState();

    HRESULT hr = m_swapChain->Present(1, 0); // VSync
    m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
}

void Application::renderPlotToolbar(const Model::SceneSummary& scene) {
    const XYRenderMode effectiveRenderMode = m_plotSettings.resolveXYRenderMode(scene);
    const bool is3DMode = (effectiveRenderMode == XYRenderMode::Surface3D);

    Components::PlotToolbarContext context;
    context.is3DMode = is3DMode;
    context.effectiveRenderMode = effectiveRenderMode;
    context.availableSize = ImGui::GetContentRegionAvail();

    const Components::PlotToolbarActions actions =
        Components::renderPlotToolbar(m_plotSettings, context);
    if (actions.requestFit) {
        fitDefaultView();
    }
    if (actions.requestReset) {
        resetViewAndCamera();
    }
    if (actions.requestExport) {
        m_exportController.requestOpen();
        m_redrawRequested = true;
    }
    if (actions.applyCameraPreset) {
        applyCameraPreset(actions.cameraAzimuthDeg, actions.cameraElevationDeg);
    }
    if (actions.requestRedraw) {
        m_redrawRequested = true;
    }
}

void Application::handlePlotShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    const bool modifiersDown = io.KeyCtrl || io.KeyAlt || io.KeySuper;
    const bool canUsePlotShortcuts =
        !m_exportController.dialogOpen() && !io.WantTextInput &&
        !ImGui::IsAnyItemActive() && !modifiersDown;
    if (!canUsePlotShortcuts) {
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        fitDefaultView();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
        resetViewAndCamera();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) {
        m_plotSettings.showGrid = !m_plotSettings.showGrid;
        m_redrawRequested = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        m_plotSettings.showWires = !m_plotSettings.showWires;
        m_redrawRequested = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        m_exportController.requestOpen();
        m_redrawRequested = true;
    }
}

void Application::fitDefaultView() {
    constexpr double targetWorldSpan = 20.0;
    constexpr double marginScale = 0.94;
    const double fitScaleX = (std::max)(1.0f, m_viewTransform.viewport.width) / targetWorldSpan;
    const double fitScaleY = (std::max)(1.0f, m_viewTransform.viewport.height) / targetWorldSpan;
    const double fitScale = (std::max)(0.1, (std::min)(fitScaleX, fitScaleY) * marginScale);

    m_viewTransform.state.centerX = 0.0;
    m_viewTransform.state.centerY = 0.0;
    m_viewTransform.state.scaleX = fitScale;
    m_viewTransform.state.scaleY = fitScale;
    m_redrawRequested = true;
}

void Application::resetViewAndCamera() {
    m_viewTransform.reset();
    m_plotSettings.azimuthDeg = kDefaultAzimuthDeg;
    m_plotSettings.elevationDeg = kDefaultElevationDeg;
    m_plotSettings.zScale = kDefaultZScale;
    m_plotSettings.autoRotate = false;
    m_redrawRequested = true;
}

void Application::applyCameraPreset(float azimuthDeg, float elevationDeg) {
    m_plotSettings.azimuthDeg = azimuthDeg;
    m_plotSettings.elevationDeg = elevationDeg;
    m_plotSettings.autoRotate = false;
    m_redrawRequested = true;
}

void Application::startUpdateCheck(bool manualRequest) {
    if (m_updateCheckInProgress) {
        if (manualRequest) {
            m_updateStatus = "Update check already in progress.";
            m_redrawRequested = true;
        }
        return;
    }

    // If a previous future was never consumed (for example after a refactor/early-return path),
    // clear it here before launching a new request to keep ownership of the async state simple.
    if (m_updateCheckFuture.valid()) {
        if (m_updateCheckFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            (void)m_updateCheckFuture.get();
        } else if (manualRequest) {
            m_updateStatus = "Previous update check is still running.";
            m_redrawRequested = true;
            return;
        } else {
            return;
        }
    }

    m_updateCheckInProgress = true;
    m_startupCheckDone = true; // Prevent deferred auto-check from firing again
    m_updateStatus = manualRequest ? "Checking GitHub releases..." : "Checking for updates in background...";
    m_redrawRequested = true;

    try {
        m_updateCheckFuture = std::async(std::launch::async, [manualRequest]() {
            return fetchLatestReleaseFromGitHub(manualRequest);
        });
    } catch (const std::exception& ex) {
        m_updateCheckInProgress = false;
        m_updateStatus = std::string("Could not start update check worker: ") + ex.what();
        m_redrawRequested = true;
    } catch (...) {
        m_updateCheckInProgress = false;
        m_updateStatus = "Could not start update check worker.";
        m_redrawRequested = true;
    }
}

void Application::pollUpdateCheckResult() {
    if (!m_updateCheckInProgress || !m_updateCheckFuture.valid()) {
        return;
    }

    if (m_updateCheckFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        return;
    }

    // `get()` transfers the completed worker result back to the UI thread exactly once.
    UpdateCheckResult result = m_updateCheckFuture.get();
    m_updateCheckInProgress = false;

    if (!result.releaseUrl.empty()) {
        m_updateReleaseUrl = result.releaseUrl;
    } else {
        m_updateReleaseUrl = kGitHubReleasesUrlUtf8;
    }
    m_updateLatestTag = result.latestTag;
    m_updateAvailable = result.requestSucceeded && result.updateAvailable;
    if (!m_updateAvailable && result.manualRequest) {
        m_updateNoticeDismissed = false;
    }
    if (result.updateAvailable) {
        m_updateNoticeDismissed = false;
    }
    if (!result.statusMessage.empty()) {
        m_updateStatus = result.statusMessage;
    } else if (result.requestSucceeded) {
        m_updateStatus = result.updateAvailable ? "Update available." : "You are running the latest version.";
    } else {
        m_updateStatus = "Update check failed.";
    }

    m_redrawRequested = true;
}

void Application::initialiseExportDialogSize() {
    if (m_exportController.sizeInitialized()) {
        return;
    }

    int width = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.width)));
    int height = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.height)));
    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;

    auto& settings = m_exportController.dialogSettings();
    settings.size.width = width;
    settings.size.height = height;
    settings.size.scale = 1;
    settings.size.selectedPreset = kExportSizePresetCurrent;
    m_exportController.setSizeInitialized(true);
}

void Application::renderExportDialog(float, float) {
    static const char* kExportDialogPopupId = "Export Plot Settings";

    if (m_exportController.consumeOpenRequest()) {
        auto& settings = m_exportController.dialogSettings();
        if (settings.profile == ExportProfile::CurrentView) {
            applyCurrentViewScene(settings, exportSceneSettingsFromPlot(m_plotSettings));
        }
        normalizeExportSettings(settings);
    }

    if (!m_exportController.dialogOpen()) {
        return;
    }

    initialiseExportDialogSize();

    if (m_exportController.popupOpenNextFrame()) {
        ImGui::OpenPopup(kExportDialogPopupId);
        m_exportController.clearPopupOpenNextFrame();
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float maxWidth = (std::max)(420.0f, viewport->WorkSize.x - 32.0f);
    const float maxHeight = (std::max)(420.0f, viewport->WorkSize.y - 32.0f);
    const float desiredWidth = (std::min)(maxWidth, (std::max)(760.0f, viewport->WorkSize.x * 0.84f));
    const float desiredHeight = (std::min)(maxHeight, (std::max)(560.0f, viewport->WorkSize.y * 0.84f));
    const ImVec2 defaultDialogSize(desiredWidth, desiredHeight);
    if (m_exportController.centerOnOpen()) {
        const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                            viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(defaultDialogSize, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize(defaultDialogSize, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2((std::min)(640.0f, maxWidth), (std::min)(440.0f, maxHeight)),
                                        ImVec2(maxWidth, maxHeight));

    bool open = m_exportController.dialogOpen();
    if (ImGui::BeginPopupModal(kExportDialogPopupId, &open, ImGuiWindowFlags_NoCollapse)) {
        const int sourceWidth = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.width)));
        const int sourceHeight = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.height)));
        const ExportWorldBounds sourceBounds{
            m_viewTransform.worldXMin(), m_viewTransform.worldXMax(),
            m_viewTransform.worldYMin(), m_viewTransform.worldYMax()
        };

        Components::ExportDialogContext context{
            m_exportController.dialogSettings(),
            m_exportController.settingsPaneWidthRef(),
            m_exportController.previewZoomRef(),
            m_exportController.previewPanXRef(),
            m_exportController.previewPanYRef(),
            m_exportController.previewCheckerboardRef(),
            m_exportController.previewStatus(),
            m_exportController.status()
        };
        context.sourceWidth = sourceWidth;
        context.sourceHeight = sourceHeight;
        context.sourceBounds = sourceBounds;
        context.currentScene = exportSceneSettingsFromPlot(m_plotSettings);
        context.previewDirty = m_exportController.previewDirty();
        context.previewRefreshRequested = m_exportController.previewRefreshRequested();
        context.hasPreviewTexture = m_exportPreview.hasTexture();
        context.previewTexture = reinterpret_cast<ImTextureID>(m_exportPreview.srv());
        context.previewWidth = m_exportPreview.width();
        context.previewHeight = m_exportPreview.height();
        context.exportBusy = m_exportController.exportBusy();
        context.lastSavedPathUtf8 = XFWUtf::utf16ToUtf8OrEmpty(m_exportController.lastSavedPath());

        const Components::ExportDialogAction action = Components::renderExportDialogContent(context);
        if (action.settingsChanged) {
            markExportPreviewOutOfDate();
        }
        if (action.requestFinalPreview) {
            m_exportController.requestFinalQualityPreview();
            m_redrawRequested = true;
        } else if (action.requestPreview) {
            requestExportPreviewRefresh();
        }
        if (action.requestCopy) {
            m_exportController.queueCopy(m_exportController.dialogSettings());
            m_redrawRequested = true;
        }
        if (action.requestSave) {
            m_exportController.queueSave(m_exportController.dialogSettings());
            m_redrawRequested = true;
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
            m_redrawRequested = true;
        }
        if (action.close) {
            ImGui::CloseCurrentPopup();
            open = false;
        }

        ImGui::EndPopup();
    }

    m_exportController.clearCenterOnOpen();
    if (!ImGui::IsPopupOpen(kExportDialogPopupId)) {
        m_exportController.closeDialog();
    } else {
        m_exportController.setDialogOpen(open);
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
    if (m_updateCheckFuture.valid()) {
        // `std::future` from std::async may block on destruction. Wait/get here while the UI is
        // still alive so we shut down deterministically and avoid implicit blocking elsewhere.
        m_updateCheckFuture.wait();
        (void)m_updateCheckFuture.get();
    }
    m_updateCheckInProgress = false;

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
    m_redrawRequested = true;
}

void Application::requestExportPreviewRefresh() {
    m_exportController.requestPreviewRefresh();
    m_redrawRequested = true;
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

    int left = static_cast<int>(std::floor(m_viewTransform.viewport.originX));
    int top = static_cast<int>(std::floor(m_viewTransform.viewport.originY));
    int right = left + static_cast<int>(std::floor(m_viewTransform.viewport.width));
    int bottom = top + static_cast<int>(std::floor(m_viewTransform.viewport.height));

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
        XFExport::buildExportRenderRequest(settings, m_viewTransform, m_plotSettings, m_sceneSummary);

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
            m_plotPanel.render(m_formulas, request.view, request.plotSettings, request.scene,
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

bool Application::writeExportMetadataSidecar(const ExportSettings& settings,
                                             const std::wstring& imagePath,
                                             int width,
                                             int height,
                                             std::string& error) const {
    error.clear();
    const std::filesystem::path sidecarPath = XFExport::exportMetadataSidecarPath(
        std::filesystem::path(imagePath));
    const XFExport::ExportAppMetadata appMetadata{
        "XpressFormula",
        XF_BUILD_VERSION,
        XF_BUILD_REPO_URL,
        XF_BUILD_BRANCH,
        XF_BUILD_COMMIT
    };
    const XFExport::ExportMetadataModel metadata = XFExport::makeExportMetadataModel(
        settings,
        XFWUtf::utf16ToUtf8OrEmpty(imagePath),
        std::filesystem::path(imagePath),
        width,
        height,
        appMetadata,
        m_formulas,
        m_viewTransform,
        m_plotSettings);
    const XFExport::ExportOperationResult writeResult = [&]() {
        const std::string json = XFExport::serializeExportMetadata(metadata);
        const auto result = XpressFormula::Infrastructure::FileSystem::writeTextAtomically(sidecarPath, json);
        if (!result) {
            return XFExport::ExportOperationResult{ false, "Could not write metadata sidecar: " + result.error };
        }
        return XFExport::ExportOperationResult{ true, {} };
    }();
    if (!writeResult) {
        error = writeResult.error;
        return false;
    }
    return true;
}
void Application::openLastSavedExport() {
    const std::wstring& lastSavedPath = m_exportController.lastSavedPath();
    if (lastSavedPath.empty()) {
        m_exportController.setStatus("No saved export path is available.");
    } else if (m_shellService.openPath(lastSavedPath)) {
        m_exportController.setStatus("Opened saved image.");
    } else {
        m_exportController.setStatus("Could not open saved image.");
    }
    m_redrawRequested = true;
}

void Application::showLastSavedExportInFolder() {
    const std::wstring& lastSavedPath = m_exportController.lastSavedPath();
    if (lastSavedPath.empty()) {
        m_exportController.setStatus("No saved export path is available.");
    } else if (m_shellService.revealPath(lastSavedPath)) {
        m_exportController.setStatus("Opened saved image location.");
    } else {
        m_exportController.setStatus("Could not show saved image in folder.");
    }
    m_redrawRequested = true;
}

void Application::copyLastSavedExportPath() {
    const std::wstring& lastSavedPath = m_exportController.lastSavedPath();
    if (lastSavedPath.empty()) {
        m_exportController.setStatus("No saved export path is available.");
    } else if (m_clipboardService.copyUtf16Text(m_hWnd, lastSavedPath)) {
        m_exportController.setStatus("Copied saved image path.");
    } else {
        m_exportController.setStatus("Could not copy saved image path.");
    }
    m_redrawRequested = true;
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
            m_redrawRequested = true;
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
        m_redrawRequested = true;
        m_exportController.setStatus(messages.front());
        return;
    }

    struct ImageEncoderAdapter final : XFExport::IExportImageEncoder {
        explicit ImageEncoderAdapter(const XFWin::WicImageEncoder& encoderIn)
            : encoder(encoderIn) {}

        XFExport::ExportOperationResult saveImageBgra(const std::wstring& path,
                                                      std::span<const std::uint8_t> pixels,
                                                      int width,
                                                      int height) override {
            const XFWin::ImageEncodeResult result =
                encoder.saveByExtensionBgra(std::filesystem::path(path), pixels, width, height);
            return { static_cast<bool>(result), result.error };
        }

        const XFWin::WicImageEncoder& encoder;
    } imageEncoder(m_imageEncoder);

    struct ClipboardAdapter final : XFExport::IExportClipboard {
        ClipboardAdapter(XFWin::ClipboardService& clipboardIn, HWND ownerIn)
            : clipboard(clipboardIn), owner(ownerIn) {}

        XFExport::ExportOperationResult copyImageBgra(std::span<const std::uint8_t> pixels,
                                                      int width,
                                                      int height) override {
            const XFWin::ClipboardResult result = clipboard.copyDibImageBgra(owner, pixels, width, height);
            return { static_cast<bool>(result), result.error };
        }

        XFExport::ExportOperationResult copyText(const std::wstring& text) override {
            const XFWin::ClipboardResult result = clipboard.copyUtf16Text(owner, text);
            return { static_cast<bool>(result), result.error };
        }

        XFWin::ClipboardService& clipboard;
        HWND owner;
    } clipboard(m_clipboardService, m_hWnd);

    struct ShellAdapter final : XFExport::IExportShell {
        explicit ShellAdapter(XFWin::ShellService& shellIn) : shell(shellIn) {}

        bool openPath(const std::wstring& path) override {
            return shell.openPath(path);
        }

        bool revealPath(const std::wstring& path) override {
            return shell.revealPath(path);
        }

        XFWin::ShellService& shell;
    } shell(m_shellService);

    struct MetadataWriterAdapter final : XFExport::IExportMetadataWriter {
        explicit MetadataWriterAdapter(const Application& appIn) : app(appIn) {}

        XFExport::ExportOperationResult writeSidecar(const ExportSettings& settings,
                                                     const std::wstring& imagePath,
                                                     int width,
                                                     int height,
                                                     std::wstring& sidecarPath) override {
            sidecarPath = XFExport::exportMetadataSidecarPath(std::filesystem::path(imagePath)).wstring();
            std::string error;
            if (!app.writeExportMetadataSidecar(settings, imagePath, width, height, error)) {
                return { false, error };
            }
            return { true, {} };
        }

        const Application& app;
    } metadataWriter(*this);

    if (m_exportController.pendingSave()) {
        const XFWin::ExportImageFormat preferredFormat =
            (settings.output.format == ExportFormat::Bmp)
                ? XFWin::ExportImageFormat::Bmp
                : XFWin::ExportImageFormat::Png;
        const XFWin::DialogResult dialog = m_fileDialogService.saveImage(m_hWnd, preferredFormat);
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
    m_redrawRequested = true;
    if (m_exportController.dialogOpen()) {
        m_exportController.requestPopupOpenNextFrame();
    }

    if (!messages.empty()) {
        m_exportController.setStatus(XFExport::joinExportMessages(messages));
    }
}
} // namespace XpressFormula::UI
