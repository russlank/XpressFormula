// SPDX-License-Identifier: MIT
// Application.cpp - Win32 + D3D11 + ImGui application implementation.
#include "Application.h"
#include "../Core/UpdateVersionUtils.h"
#include "../Infrastructure/FileSystem/AtomicFileWriter.h"
#include "../Infrastructure/Serialization/JsonWriter.h"
#include "../Platform/Windows/ComPtr.h"
#include "../Platform/Windows/WinHttpClient.h"
#include "../Platform/Windows/Utf.h"
#include "../Version.h"
#include "../resource.h"
#include "ExportMetadata.h"
#include "FormulaEntry.h"
#include "FormulaPresentation.h"
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
#include <cwctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

// Forward-declare the ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// We store a pointer to the Application so the WndProc can access it.
static XpressFormula::UI::Application* g_app = nullptr;

namespace XFAtomic = XpressFormula::Infrastructure::FileSystem;
namespace XFJson = XpressFormula::Infrastructure::Serialization;
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

const char* formulaRenderKindLabel(XpressFormula::UI::FormulaRenderKind kind) {
    using XpressFormula::UI::FormulaRenderKind;
    switch (kind) {
        case FormulaRenderKind::Curve2D: return "Curve2D";
        case FormulaRenderKind::Surface3D: return "Surface3D";
        case FormulaRenderKind::Implicit2D: return "Implicit2D";
        case FormulaRenderKind::ScalarField3D: return "ScalarField3D";
        case FormulaRenderKind::Invalid: return "Invalid";
        default: return "Invalid";
    }
}

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
    m_exportDialogSettings = defaultExportSettings();
    m_exportDialogSizeInitialized = false;
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
    if (m_scheduledSavePlotImage || m_scheduledCopyPlotImage) {
        m_pendingSavePlotImage = m_pendingSavePlotImage || m_scheduledSavePlotImage;
        m_pendingCopyPlotImage = m_pendingCopyPlotImage || m_scheduledCopyPlotImage;
        m_scheduledSavePlotImage = false;
        m_scheduledCopyPlotImage = false;
    }

    if (m_exportDialogOpen && m_exportDialogSettings.quality.autoRefreshPreview &&
        m_exportPreviewDirty && !m_exportPreviewRefreshRequested) {
        const auto elapsed = std::chrono::steady_clock::now() - m_exportPreviewLastChanged;
        if (elapsed >= std::chrono::milliseconds(350)) {
            m_exportPreviewRefreshRequested = true;
            m_exportPreviewStatus = "Rendering preview...";
        }
    }

    if (m_exportDialogOpen && m_exportPreviewRefreshRequested) {
        m_exportPreviewRefreshRequested = false;
        refreshExportPreviewTexture();
        // Offscreen preview rendering uses a separate hidden ImGui frame in the same context,
        // which can clear popup state. Re-open the export popup in the visible UI frame.
        m_exportDialogPopupOpenNextFrame = true;
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
        m_viewTransform, m_plotSettings, scene, m_exportStatus);
    m_exportDialogOpenRequested = m_exportDialogOpenRequested || actions.requestOpenExportDialog;

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
    if (m_pendingSavePlotImage || m_pendingCopyPlotImage) {
        exportOverrides = plotRenderOverridesForExport(m_pendingExportSettings);
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
        m_exportDialogOpenRequested = true;
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
        !m_exportDialogOpen && !io.WantTextInput && !ImGui::IsAnyItemActive() && !modifiersDown;
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
        m_exportDialogOpenRequested = true;
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
    if (m_exportDialogSizeInitialized) {
        return;
    }

    int width = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.width)));
    int height = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.height)));
    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;

    m_exportDialogSettings.size.width = width;
    m_exportDialogSettings.size.height = height;
    m_exportDialogSettings.size.scale = 1;
    m_exportDialogSettings.size.selectedPreset = kExportSizePresetCurrent;
    m_exportDialogSizeInitialized = true;
}

void Application::renderExportDialog(float, float) {
    static const char* kExportDialogPopupId = "Export Plot Settings";

    if (m_exportDialogOpenRequested) {
        m_exportDialogOpenRequested = false;
        m_exportDialogOpen = true;
        m_exportDialogPopupOpenNextFrame = true;
        m_exportDialogCenterOnOpen = true;
        m_exportDialogSizeInitialized = false;
        if (m_exportDialogSettings.profile == ExportProfile::CurrentView) {
            applyCurrentViewScene(m_exportDialogSettings, exportSceneSettingsFromPlot(m_plotSettings));
        }
        normalizeExportSettings(m_exportDialogSettings);
        m_exportPreviewZoom = 0.0f;
        m_exportPreviewPanX = 0.0f;
        m_exportPreviewPanY = 0.0f;
        m_exportPreviewDirty = true;
        m_exportPreviewRefreshRequested = false;
        m_exportPreviewLastChanged = std::chrono::steady_clock::now();
        m_exportPreviewStatus = "Preview out of date.";
    }

    if (!m_exportDialogOpen) {
        return;
    }

    initialiseExportDialogSize();

    if (m_exportDialogPopupOpenNextFrame) {
        ImGui::OpenPopup(kExportDialogPopupId);
        m_exportDialogPopupOpenNextFrame = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float maxWidth = (std::max)(420.0f, viewport->WorkSize.x - 32.0f);
    const float maxHeight = (std::max)(420.0f, viewport->WorkSize.y - 32.0f);
    const float desiredWidth = (std::min)(maxWidth, (std::max)(760.0f, viewport->WorkSize.x * 0.84f));
    const float desiredHeight = (std::min)(maxHeight, (std::max)(560.0f, viewport->WorkSize.y * 0.84f));
    const ImVec2 defaultDialogSize(desiredWidth, desiredHeight);
    if (m_exportDialogCenterOnOpen) {
        const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                            viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(defaultDialogSize, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize(defaultDialogSize, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2((std::min)(640.0f, maxWidth), (std::min)(440.0f, maxHeight)),
                                        ImVec2(maxWidth, maxHeight));

    bool open = m_exportDialogOpen;
    if (ImGui::BeginPopupModal(kExportDialogPopupId, &open, ImGuiWindowFlags_NoCollapse)) {
        bool previewChanged = false;
        const int sourceWidth = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.width)));
        const int sourceHeight = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.viewport.height)));
        const ExportWorldBounds sourceBounds{
            m_viewTransform.worldXMin(), m_viewTransform.worldXMax(),
            m_viewTransform.worldYMin(), m_viewTransform.worldYMax()
        };
        const ExportSceneSettings currentScene = exportSceneSettingsFromPlot(m_plotSettings);
        auto& settings = m_exportDialogSettings;
        normalizeExportSettings(settings);
        previewChanged = resolveCoordinateOverlayPolicy(settings.scene.showCoordinates,
                                                        settings.scene.showAxisTriad) ||
                         previewChanged;

        auto applySelectedSizePreset = [&]() {
            applyExportSizePreset(settings, sourceWidth, sourceHeight);
        };

        auto markCustomProfile = [&]() {
            if (syncExportProfileAfterManualChange(settings, &currentScene)) {
                m_redrawRequested = true;
            }
        };

        auto markCustomSize = [&]() {
            markExportSizeCustom(settings);
            markCustomProfile();
        };

        auto applyExportProfile = [&](ExportProfile profile) {
            if (profile == ExportProfile::Custom) {
                settings.profile = ExportProfile::Custom;
                m_exportStatus = "Custom export profile selected.";
                return;
            }

            settings = exportSettingsForProfile(profile, &currentScene);
            applySelectedSizePreset();
            m_exportStatus = std::string("Applied export profile: ") + exportProfileLabel(profile) + ".";
        };

        auto resetExportSettings = [&]() {
            settings = defaultExportSettings();
            m_exportDialogSizeInitialized = false;
            initialiseExportDialogSize();
            applyExportProfile(ExportProfile::CurrentView);
            m_exportPreviewZoom = 0.0f;
            m_exportPreviewPanX = 0.0f;
            m_exportPreviewPanY = 0.0f;
            m_exportStatus = "Export settings reset.";
            markExportPreviewOutOfDate();
        };

        auto queueCopyExport = [&]() {
            m_pendingExportSettings = settings;
            m_scheduledCopyPlotImage = true;
            m_scheduledSavePlotImage = false;
            m_exportStatus = "Clipboard export queued.";
            m_redrawRequested = true;
        };

        auto queueSaveExport = [&]() {
            m_pendingExportSettings = settings;
            m_scheduledSavePlotImage = true;
            m_scheduledCopyPlotImage = false;
            m_exportStatus = "Save export queued.";
            m_redrawRequested = true;
        };

        auto drawCheckerboard = [](ImVec2 min, ImVec2 max) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float checkerSize = 10.0f;
            const ImU32 checkerA = IM_COL32(82, 82, 90, 255);
            const ImU32 checkerB = IM_COL32(126, 126, 136, 255);
            drawList->AddRectFilled(min, max, checkerA);
            drawList->PushClipRect(min, max, true);
            for (float y = min.y; y < max.y; y += checkerSize) {
                for (float x = min.x; x < max.x; x += checkerSize) {
                    const int ix = static_cast<int>((x - min.x) / checkerSize);
                    const int iy = static_cast<int>((y - min.y) / checkerSize);
                    if (((ix + iy) & 1) == 0) {
                        continue;
                    }
                    drawList->AddRectFilled(
                        ImVec2(x, y),
                        ImVec2((std::min)(x + checkerSize, max.x),
                               (std::min)(y + checkerSize, max.y)),
                        checkerB);
                }
            }
            drawList->PopClipRect();
        };

        const ImGuiStyle& style = ImGui::GetStyle();
        const float footerHeight = ImGui::GetFrameHeightWithSpacing() * 2.45f;
        ImGui::BeginChild("##ExportDialogBody", ImVec2(0.0f, -footerHeight), false);
        const float bodyWidth = ImGui::GetContentRegionAvail().x;
        const float bodyHeight = ImGui::GetContentRegionAvail().y;
        const float splitterWidth = UiKit::metrics().splitterWidth;
        const float minSettingsWidth = 300.0f;
        const float minPreviewWidth = 240.0f;
        const float maxSettingsWidth = (std::max)(minSettingsWidth, bodyWidth - splitterWidth - minPreviewWidth);
        m_exportSettingsPaneWidth = std::clamp(m_exportSettingsPaneWidth, minSettingsWidth, maxSettingsWidth);
        const float settingsPaneWidth = m_exportSettingsPaneWidth;

        ImGui::BeginChild("##ExportSettingsPane", ImVec2(settingsPaneWidth, 0.0f), true);
        if (ImGui::BeginCombo("Profile", exportProfileLabel(settings.profile))) {
            for (ExportProfile profile : exportProfiles()) {
                const bool selected = (settings.profile == profile);
                if (ImGui::Selectable(exportProfileLabel(profile), selected)) {
                    applyExportProfile(profile);
                    previewChanged = (profile != ExportProfile::Custom) || previewChanged;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SetItemTooltip("Profiles update size, appearance, scene, quality, output, and metadata settings.");
        ImGui::Separator();

        if (ImGui::BeginTabBar("##ExportSettingsTabs")) {
            if (ImGui::BeginTabItem("Size")) {
                if (sourceWidth > 0 && sourceHeight > 0) {
                    ImGui::Text("Current plot: %d x %d", sourceWidth, sourceHeight);
                } else {
                    ImGui::TextDisabled("Current plot size unavailable.");
                }

                if (ImGui::Button("Use Current", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    settings.size.selectedPreset = kExportSizePresetCurrent;
                    applySelectedSizePreset();
                    markCustomProfile();
                    previewChanged = true;
                }

                ImGui::Spacing();
                const auto& presets = exportSizePresets();
                const char* presetLabel = presets[static_cast<size_t>(settings.size.selectedPreset)].label;
                if (ImGui::BeginCombo("Preset", presetLabel)) {
                    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
                        const bool selected = (settings.size.selectedPreset == i);
                        if (ImGui::Selectable(presets[static_cast<size_t>(i)].label, selected)) {
                            settings.size.selectedPreset = i;
                            applySelectedSizePreset();
                            markCustomProfile();
                            previewChanged = !presets[static_cast<size_t>(i)].custom || previewChanged;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                std::string scaleLabel = std::to_string(settings.size.scale) + "x";
                if (ImGui::BeginCombo("Scale", scaleLabel.c_str())) {
                    for (const int scaleOption : exportScaleOptions()) {
                        std::string optionLabel = std::to_string(scaleOption) + "x";
                        const bool selected = (settings.size.scale == scaleOption);
                        if (ImGui::Selectable(optionLabel.c_str(), selected)) {
                            const int previousScale = (std::max)(1, settings.size.scale);
                            settings.size.scale = scaleOption;
                            if (exportSizePresets()[static_cast<size_t>(settings.size.selectedPreset)].custom) {
                                settings.size.width = clampExportDimension(static_cast<int>(
                                    std::lround(static_cast<double>(settings.size.width) * scaleOption / previousScale)));
                                settings.size.height = clampExportDimension(static_cast<int>(
                                    std::lround(static_cast<double>(settings.size.height) * scaleOption / previousScale)));
                            } else {
                                applySelectedSizePreset();
                            }
                            markCustomProfile();
                            previewChanged = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                int width = settings.size.width;
                int height = settings.size.height;
                const int previousWidth = (std::max)(1, width);
                const int previousHeight = (std::max)(1, height);
                const bool widthChanged = ImGui::InputInt("Width", &width, 16, 128);
                const bool heightChanged = ImGui::InputInt("Height", &height, 16, 128);
                validateExportSize(width, height);

                if (settings.size.lockAspectRatio && widthChanged && !heightChanged) {
                    height = aspectLockedHeight(width, previousWidth, previousHeight);
                } else if (settings.size.lockAspectRatio && heightChanged && !widthChanged) {
                    width = aspectLockedWidth(height, previousWidth, previousHeight);
                }

                if (widthChanged || heightChanged) {
                    settings.size.width = width;
                    settings.size.height = height;
                    markCustomSize();
                    previewChanged = true;
                }
                if (ImGui::Checkbox("Lock Aspect Ratio", &settings.size.lockAspectRatio)) {
                    markCustomProfile();
                    previewChanged = true;
                }

                ImGui::Spacing();
                ImGui::TextUnformatted("Aspect Handling");
                const ExportAspectMode aspectModes[] = {
                    ExportAspectMode::PreserveMathematicalScale,
                    ExportAspectMode::PreserveVisibleBounds,
                    ExportAspectMode::CropToFill,
                    ExportAspectMode::StretchToOutput
                };
                for (ExportAspectMode mode : aspectModes) {
                    if (ImGui::RadioButton(exportAspectModeLabel(mode), settings.output.aspectMode == mode)) {
                        settings.output.aspectMode = mode;
                        markCustomProfile();
                        previewChanged = true;
                    }
                    ImGui::SetItemTooltip("%s", exportAspectModeTooltip(mode));
                }

                const auto resolvedView =
                    resolveExportView(settings.size.width,
                                      settings.size.height,
                                      sourceBounds,
                                      settings.output.aspectMode);
                ImGui::Spacing();
                ImGui::Text("X: [%.4g, %.4g]",
                            resolvedView.visibleBounds.xMin, resolvedView.visibleBounds.xMax);
                ImGui::Text("Y: [%.4g, %.4g]",
                            resolvedView.visibleBounds.yMin, resolvedView.visibleBounds.yMax);
                if (resolvedView.uniformScale) {
                    ImGui::Text("Scale: %.4g px/unit", resolvedView.scaleX);
                } else {
                    ImGui::Text("Scale: %.4g x %.4g px/unit",
                                resolvedView.scaleX, resolvedView.scaleY);
                }
                if (resolvedView.marginLeftPx > 0.5 || resolvedView.marginTopPx > 0.5) {
                    ImGui::Text("Margins: %.0f x %.0f px",
                                resolvedView.marginLeftPx + resolvedView.marginRightPx,
                                resolvedView.marginTopPx + resolvedView.marginBottomPx);
                }

                const ExportSupersampling sampling = effectiveExportSupersampling(settings);
                const auto estimatedBytes = estimateRgbaBufferBytes(
                    settings.size.width, settings.size.height, sampling);
                const double estimatedMiB = bytesToMiB(estimatedBytes);
                ImGui::Spacing();
                ImGui::Text("Output: %d x %d", settings.size.width, settings.size.height);
                ImGui::Text("Render buffer: %.1f MiB", estimatedMiB);
                if (estimatedMiB >= kLargeExportWarningMiB) {
                    ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                       "Large export: expect slower rendering and higher memory use.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Appearance")) {
                int backgroundMode = static_cast<int>(settings.appearance.backgroundMode);
                const ExportBackgroundMode backgroundModes[] = {
                    ExportBackgroundMode::Current,
                    ExportBackgroundMode::Transparent,
                    ExportBackgroundMode::White,
                    ExportBackgroundMode::Black,
                    ExportBackgroundMode::Custom
                };
                for (ExportBackgroundMode mode : backgroundModes) {
                    const int modeValue = static_cast<int>(mode);
                    if (ImGui::RadioButton(exportBackgroundModeLabel(mode), backgroundMode == modeValue)) {
                        backgroundMode = modeValue;
                        settings.appearance.backgroundMode = mode;
                        markCustomProfile();
                        previewChanged = true;
                    }
                }

                if (settings.appearance.backgroundMode == ExportBackgroundMode::Custom) {
                    if (ImGui::ColorEdit3("Custom Color", settings.appearance.backgroundColor.data(),
                                          ImGuiColorEditFlags_DisplayRGB)) {
                        markCustomProfile();
                        previewChanged = true;
                    }
                    if (ImGui::SliderFloat("Opacity", &settings.appearance.backgroundColor[3],
                                           0.0f, 1.0f, "%.2f")) {
                        markCustomProfile();
                        previewChanged = true;
                    }
                } else if (settings.appearance.backgroundMode == ExportBackgroundMode::Transparent) {
                    ImGui::TextDisabled("Transparency is best preserved by PNG output.");
                    if (settings.output.format == ExportFormat::Bmp) {
                        ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                           "BMP export does not preserve alpha transparency.");
                    }
                }

                ImGui::Spacing();
                if (ImGui::RadioButton("Color", !settings.appearance.grayscaleOutput)) {
                    settings.appearance.grayscaleOutput = false;
                    markCustomProfile();
                    previewChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Grayscale", settings.appearance.grayscaleOutput)) {
                    settings.appearance.grayscaleOutput = true;
                    markCustomProfile();
                    previewChanged = true;
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Scene")) {
                if (ImGui::Checkbox("Grid", &settings.scene.showGrid)) {
                    markCustomProfile();
                    previewChanged = true;
                }
                if (ImGui::Checkbox("Coordinates", &settings.scene.showCoordinates)) {
                    markCustomProfile();
                    resolveCoordinateOverlayPolicy(settings.scene.showCoordinates,
                                                   settings.scene.showAxisTriad);
                    previewChanged = true;
                }
                if (ImGui::Checkbox("Wires", &settings.scene.showWires)) {
                    markCustomProfile();
                    previewChanged = true;
                }
                if (ImGui::Checkbox("Envelope", &settings.scene.showEnvelope)) {
                    markCustomProfile();
                    previewChanged = true;
                }
                ImGui::BeginDisabled(settings.scene.showCoordinates);
                if (ImGui::Checkbox("Axis Triad", &settings.scene.showAxisTriad)) {
                    markCustomProfile();
                    previewChanged = true;
                }
                ImGui::EndDisabled();
                if (settings.scene.showCoordinates) {
                    ImGui::SetItemTooltip("Axis triad is disabled while coordinate axes and labels are enabled.");
                }

                ImGui::Spacing();
                int framingSelection = 0;
                const char* framingOptions[] = { "Current viewport", "Fit visible formulas (planned)" };
                ImGui::BeginDisabled();
                ImGui::Combo("Framing", &framingSelection, framingOptions,
                             IM_ARRAYSIZE(framingOptions));
                ImGui::EndDisabled();
                ImGui::TextDisabled("Fit visible formulas is planned.");
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Quality")) {
                bool overrideQuality = (settings.quality.mode == ExportQualityMode::Override);
                if (ImGui::Checkbox("Override Interactive Quality", &overrideQuality)) {
                    settings.quality.mode = overrideQuality ? ExportQualityMode::Override
                                                            : ExportQualityMode::Interactive;
                    markCustomProfile();
                    previewChanged = true;
                }
                if (!overrideQuality) {
                    ImGui::TextDisabled("Uses the current plot quality without changing it.");
                }

                ImGui::BeginDisabled(!overrideQuality);
                const ExportQualityPreset qualityPresets[] = {
                    ExportQualityPreset::Draft,
                    ExportQualityPreset::Normal,
                    ExportQualityPreset::High,
                    ExportQualityPreset::Ultra,
                    ExportQualityPreset::Custom
                };
                if (ImGui::BeginCombo("Preset", exportQualityPresetLabel(settings.quality.preset))) {
                    for (ExportQualityPreset preset : qualityPresets) {
                        const bool selected = (settings.quality.preset == preset);
                        if (ImGui::Selectable(exportQualityPresetLabel(preset), selected)) {
                            const ExportQualityMode currentMode = settings.quality.mode;
                            settings.quality = qualitySettingsForPreset(preset);
                            settings.quality.mode = currentMode;
                            markCustomProfile();
                            previewChanged = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                const PlotLimits& limits = plotLimits();
                int surfaceResolution = settings.quality.surfaceResolution;
                if (ImGui::InputInt("Surface Density", &surfaceResolution, 4, 16)) {
                    settings.quality.surfaceResolution = clampSurfaceResolution(surfaceResolution);
                    settings.quality.preset = ExportQualityPreset::Custom;
                    markCustomProfile();
                    previewChanged = true;
                }

                int implicitResolution = settings.quality.implicitSurfaceResolution;
                if (ImGui::InputInt("Implicit Resolution", &implicitResolution, 4, 16)) {
                    settings.quality.implicitSurfaceResolution =
                        clampImplicitSurfaceResolution(implicitResolution);
                    settings.quality.preset = ExportQualityPreset::Custom;
                    markCustomProfile();
                    previewChanged = true;
                }

                ImGui::BeginDisabled(!settings.scene.showWires);
                if (ImGui::SliderFloat("Wire Thickness Scale", &settings.quality.wireThicknessScale,
                                       limits.wireThicknessScale.min,
                                       limits.wireThicknessScale.max,
                                       "%.2fx")) {
                    settings.quality.preset = ExportQualityPreset::Custom;
                    markCustomProfile();
                    previewChanged = true;
                }
                ImGui::EndDisabled();
                if (!settings.scene.showWires) {
                    ImGui::SetItemTooltip("Wire thickness scale has no effect while wires are excluded.");
                }

                const ExportSupersampling supersamplingOptions[] = {
                    ExportSupersampling::Off,
                    ExportSupersampling::X2,
                    ExportSupersampling::X4
                };
                if (ImGui::BeginCombo("Supersampling",
                                      exportSupersamplingLabel(settings.quality.supersampling))) {
                    for (ExportSupersampling option : supersamplingOptions) {
                        const bool selected = (settings.quality.supersampling == option);
                        if (ImGui::Selectable(exportSupersamplingLabel(option), selected)) {
                            settings.quality.supersampling = option;
                            settings.quality.preset = ExportQualityPreset::Custom;
                            markCustomProfile();
                            previewChanged = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Output")) {
                if (ImGui::RadioButton("PNG", settings.output.format == ExportFormat::Png)) {
                    settings.output.format = ExportFormat::Png;
                    markCustomProfile();
                    previewChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("BMP", settings.output.format == ExportFormat::Bmp)) {
                    settings.output.format = ExportFormat::Bmp;
                    markCustomProfile();
                    previewChanged = true;
                }
                if (settings.output.format == ExportFormat::Bmp &&
                    settings.appearance.backgroundMode == ExportBackgroundMode::Transparent) {
                    ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                       "BMP flattens transparency in many viewers. Use PNG for alpha.");
                }

                ImGui::Spacing();
                if (ImGui::Checkbox("Open Image After Save", &settings.output.openAfterSave)) {
                    markCustomProfile();
                }
                if (ImGui::Checkbox("Show In Folder After Save", &settings.output.showInFolderAfterSave)) {
                    markCustomProfile();
                }
                if (ImGui::Checkbox("Copy Path After Save", &settings.output.copyPathAfterSave)) {
                    markCustomProfile();
                }
                if (ImGui::Checkbox("Write Metadata Sidecar JSON", &settings.output.saveMetadataSidecar)) {
                    markCustomProfile();
                }
                ImGui::SetItemTooltip("Writes a versioned .json sidecar next to the exported image.");

                ImGui::Spacing();
                if (!m_lastExportSavedPath.empty()) {
                    ImGui::TextWrapped("Last saved: %s", XFWUtf::utf16ToUtf8OrEmpty(m_lastExportSavedPath).c_str());
                    if (ImGui::Button("Open Image", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                        openLastSavedExport();
                    }
                    if (ImGui::Button("Show In Folder", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                        showLastSavedExportInFolder();
                    }
                    if (ImGui::Button("Copy Saved Path", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                        copyLastSavedExportPath();
                    }
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);
        if (UiKit::drawVerticalSplitter("##ExportDialogSplitter",
                                        bodyHeight,
                                        m_exportSettingsPaneWidth,
                                        minSettingsWidth,
                                        maxSettingsWidth,
                                        bodyWidth * 0.42f,
                                        splitterWidth)) {
            m_redrawRequested = true;
        }

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("##ExportPreviewPane", ImVec2(0.0f, 0.0f), true);
        ImGui::TextUnformatted("Preview");
        ImGui::SameLine();
        const char* previewState = m_exportPreviewRefreshRequested ? "Rendering"
            : (m_exportPreviewDirty ? "Out of date"
                                    : (m_exportPreviewSrv ? "Ready" : "No preview"));
        ImGui::TextDisabled("%s", previewState);
        ImGui::Separator();

        if (ImGui::Button("Refresh Preview")) {
            requestExportPreviewRefresh();
        }
        ImGui::SameLine();
        bool autoRefresh = settings.quality.autoRefreshPreview;
        if (ImGui::Checkbox("Auto Refresh", &autoRefresh)) {
            settings.quality.autoRefreshPreview = autoRefresh;
            markCustomProfile();
            if (settings.quality.autoRefreshPreview && m_exportPreviewDirty) {
                m_exportPreviewLastChanged = std::chrono::steady_clock::now();
            }
            m_redrawRequested = true;
        }
        const ExportPreviewQuality previewQualityOptions[] = {
            ExportPreviewQuality::Draft,
            ExportPreviewQuality::Normal
        };
        if (ImGui::BeginCombo("Preview Quality",
                              exportPreviewQualityLabel(settings.quality.previewQuality))) {
            for (ExportPreviewQuality quality : previewQualityOptions) {
                const bool selected = (settings.quality.previewQuality == quality);
                if (ImGui::Selectable(exportPreviewQualityLabel(quality), selected)) {
                    settings.quality.previewQuality = quality;
                    markCustomProfile();
                    markExportPreviewOutOfDate();
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Render Final-Quality Preview")) {
            m_exportPreviewUseFinalQualityOnce = true;
            requestExportPreviewRefresh();
        }

        const ExportSupersampling effectiveSampling = effectiveExportSupersampling(settings);
        std::string samplingText = exportSupersamplingLabel(effectiveSampling);
        const int requestedSamplingFactor = supersamplingFactor(effectiveSampling);
        const int actualSamplingFactor =
            effectiveSupersamplingFactor(settings.size.width, settings.size.height, effectiveSampling);
        if (actualSamplingFactor != requestedSamplingFactor) {
            samplingText += " (effective ";
            samplingText += std::to_string(actualSamplingFactor);
            samplingText += "x)";
        }
        ImGui::Text("Output %d x %d  |  %s  |  %s  |  %s",
                    settings.size.width, settings.size.height,
                    exportFormatLabel(settings.output.format),
                    samplingText.c_str(),
                    exportProfileLabel(settings.profile));
        ImGui::TextDisabled("%s", exportAspectModeLabel(settings.output.aspectMode));

        if (!m_exportPreviewStatus.empty()) {
            ImGui::TextWrapped("%s", m_exportPreviewStatus.c_str());
        }

        ImGui::Spacing();
        if (ImGui::Button("Fit")) {
            m_exportPreviewZoom = 0.0f;
            m_exportPreviewPanX = 0.0f;
            m_exportPreviewPanY = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("100%")) {
            m_exportPreviewZoom = 1.0f;
            m_exportPreviewPanX = 0.0f;
            m_exportPreviewPanY = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("-")) {
            const float baseZoom = (m_exportPreviewZoom > 0.0f) ? m_exportPreviewZoom : 1.0f;
            m_exportPreviewZoom = std::clamp(baseZoom / 1.25f, 0.05f, 8.0f);
        }
        ImGui::SameLine();
        const char* zoomLabel = (m_exportPreviewZoom > 0.0f) ? "Zoom %.0f%%" : "Zoom Fit";
        if (m_exportPreviewZoom > 0.0f) {
            ImGui::Text(zoomLabel, m_exportPreviewZoom * 100.0f);
        } else {
            ImGui::TextUnformatted("Zoom Fit");
        }
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            const float baseZoom = (m_exportPreviewZoom > 0.0f) ? m_exportPreviewZoom : 1.0f;
            m_exportPreviewZoom = std::clamp(baseZoom * 1.25f, 0.05f, 8.0f);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Checkerboard", &m_exportPreviewCheckerboard);

        const ImVec2 previewStart = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        canvasSize.x = (std::max)(180.0f, canvasSize.x);
        canvasSize.y = (std::max)(160.0f, canvasSize.y);
        ImGui::InvisibleButton("##ExportPreviewCanvas", canvasSize,
                               ImGuiButtonFlags_MouseButtonLeft);
        const bool previewHovered = ImGui::IsItemHovered();
        const bool previewActive = ImGui::IsItemActive();
        const bool hasPreviewTexture =
            m_exportPreviewSrv && m_exportPreviewWidth > 0 && m_exportPreviewHeight > 0;
        const ImVec2 previewEnd(previewStart.x + canvasSize.x, previewStart.y + canvasSize.y);
        ImDrawList* previewDrawList = ImGui::GetWindowDrawList();
        if (m_exportPreviewCheckerboard) {
            drawCheckerboard(previewStart, previewEnd);
        } else {
            previewDrawList->AddRectFilled(previewStart, previewEnd, IM_COL32(44, 44, 50, 255));
        }

        if (hasPreviewTexture) {
            const float textureW = static_cast<float>(m_exportPreviewWidth);
            const float textureH = static_cast<float>(m_exportPreviewHeight);
            const float fitZoom = (std::min)(canvasSize.x / textureW, canvasSize.y / textureH);
            if (previewHovered && ImGui::GetIO().MouseWheel != 0.0f) {
                const float baseZoom = (m_exportPreviewZoom > 0.0f) ? m_exportPreviewZoom : fitZoom;
                const float factor = ImGui::GetIO().MouseWheel > 0.0f ? 1.15f : (1.0f / 1.15f);
                m_exportPreviewZoom = std::clamp(baseZoom * factor, 0.05f, 8.0f);
            }
            if (previewHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_exportPreviewZoom = 0.0f;
                m_exportPreviewPanX = 0.0f;
                m_exportPreviewPanY = 0.0f;
            }

            const float drawZoom = (m_exportPreviewZoom > 0.0f) ? m_exportPreviewZoom : fitZoom;
            const float drawW = textureW * drawZoom;
            const float drawH = textureH * drawZoom;
            const float maxPanX = (std::max)(0.0f, (drawW - canvasSize.x) * 0.5f);
            const float maxPanY = (std::max)(0.0f, (drawH - canvasSize.y) * 0.5f);
            if (previewActive && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
                (maxPanX > 0.0f || maxPanY > 0.0f)) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                m_exportPreviewPanX += delta.x;
                m_exportPreviewPanY += delta.y;
            }
            m_exportPreviewPanX = std::clamp(m_exportPreviewPanX, -maxPanX, maxPanX);
            m_exportPreviewPanY = std::clamp(m_exportPreviewPanY, -maxPanY, maxPanY);

            const ImVec2 imageMin(
                previewStart.x + (canvasSize.x - drawW) * 0.5f + m_exportPreviewPanX,
                previewStart.y + (canvasSize.y - drawH) * 0.5f + m_exportPreviewPanY);
            const ImVec2 imageMax(imageMin.x + drawW, imageMin.y + drawH);
            previewDrawList->PushClipRect(previewStart, previewEnd, true);
            previewDrawList->AddImage(reinterpret_cast<ImTextureID>(m_exportPreviewSrv),
                                      imageMin, imageMax);
            previewDrawList->PopClipRect();
        } else {
            const ImVec2 textSize = ImGui::CalcTextSize("No preview");
            previewDrawList->AddText(
                ImVec2(previewStart.x + (canvasSize.x - textSize.x) * 0.5f,
                       previewStart.y + (canvasSize.y - textSize.y) * 0.5f),
                IM_COL32(220, 220, 225, 220),
                "No preview");
        }
        previewDrawList->AddRect(previewStart, previewEnd, IM_COL32(145, 145, 155, 190));
        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::Separator();
        const bool exportBusy =
            m_scheduledSavePlotImage || m_scheduledCopyPlotImage ||
            m_pendingSavePlotImage || m_pendingCopyPlotImage ||
            m_exportPreviewRefreshRequested;
        const ExportSupersampling footerSampling = effectiveExportSupersampling(settings);
        const double footerMiB = bytesToMiB(
            estimateRgbaBufferBytes(settings.size.width, settings.size.height, footerSampling));
        const char* footerPreviewState = m_exportPreviewRefreshRequested ? "Preview rendering"
            : (m_exportPreviewDirty ? "Preview out of date"
                                    : (m_exportPreviewSrv ? "Preview ready" : "Preview not rendered"));
        ImGui::Text("%s | %s | %d x %d | %s | %s | %.1f MiB | %s",
                    footerPreviewState,
                    exportProfileLabel(settings.profile),
                    settings.size.width,
                    settings.size.height,
                    exportFormatLabel(settings.output.format),
                    exportSupersamplingLabel(footerSampling),
                    footerMiB,
                    exportAspectModeLabel(settings.output.aspectMode));
        if (!m_exportStatus.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("| %s", m_exportStatus.c_str());
        }

        if (!exportBusy && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            !ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            queueSaveExport();
        }

        if (ImGui::Button("Reset Settings", ImVec2(124.0f, 0.0f))) {
            resetExportSettings();
        }
        const float copyWidth = 74.0f;
        const float saveWidth = 104.0f;
        const float closeWidth = 74.0f;
        const float actionWidth = copyWidth + saveWidth + closeWidth + style.ItemSpacing.x * 2.0f;
        const float rightStart = ImGui::GetWindowWidth() - style.WindowPadding.x - actionWidth;
        if (rightStart > ImGui::GetCursorPosX() + style.ItemSpacing.x) {
            ImGui::SameLine(rightStart);
        } else {
            ImGui::SameLine();
        }
        ImGui::BeginDisabled(exportBusy);
        if (ImGui::Button("Copy", ImVec2(copyWidth, 0.0f))) {
            queueCopyExport();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As...", ImVec2(saveWidth, 0.0f))) {
            queueSaveExport();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(closeWidth, 0.0f))) {
            ImGui::CloseCurrentPopup();
            open = false;
        }

        if (previewChanged) {
            markExportPreviewOutOfDate();
        }

        ImGui::EndPopup();
    }
    m_exportDialogCenterOnOpen = false;
    if (!ImGui::IsPopupOpen(kExportDialogPopupId)) {
        m_exportDialogOpen = false;
        m_exportDialogPopupOpenNextFrame = false;
    } else {
        m_exportDialogOpen = open;
    }
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

void Application::convertPixelsRgbaToBgra(std::vector<std::uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        std::swap(pixels[i + 0], pixels[i + 2]);
    }
}

void Application::unpremultiplyPixels(std::vector<std::uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        const int a = pixels[i + 3];
        if (a <= 0) {
            pixels[i + 0] = 0;
            pixels[i + 1] = 0;
            pixels[i + 2] = 0;
            continue;
        }
        if (a >= 255) {
            continue;
        }

        const float scale = 255.0f / static_cast<float>(a);
        pixels[i + 0] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(pixels[i + 0] * scale)), 0, 255));
        pixels[i + 1] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(pixels[i + 1] * scale)), 0, 255));
        pixels[i + 2] = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(pixels[i + 2] * scale)), 0, 255));
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

void Application::convertPixelsToGrayscaleRgba(std::vector<std::uint8_t>& pixels) {
    for (size_t i = 0; i + 3 < pixels.size(); i += 4) {
        const float r = static_cast<float>(pixels[i + 0]);
        const float g = static_cast<float>(pixels[i + 1]);
        const float b = static_cast<float>(pixels[i + 2]);
        const std::uint8_t gray = static_cast<std::uint8_t>(std::clamp(
            static_cast<int>(std::lround(0.299f * r + 0.587f * g + 0.114f * b)), 0, 255));
        pixels[i + 0] = gray;
        pixels[i + 1] = gray;
        pixels[i + 2] = gray;
    }
}

void Application::cleanupExportPreviewResources() {
    if (m_exportPreviewSrv) {
        m_exportPreviewSrv->Release();
        m_exportPreviewSrv = nullptr;
    }
    if (m_exportPreviewTexture) {
        m_exportPreviewTexture->Release();
        m_exportPreviewTexture = nullptr;
    }
    m_exportPreviewWidth = 0;
    m_exportPreviewHeight = 0;
}

bool Application::refreshExportPreviewTexture() {
    m_exportPreviewDirty = false;
    m_exportPreviewStatus.clear();

    if (!m_device || !m_deviceContext || !m_hWnd || !::IsWindow(m_hWnd)) {
        m_exportPreviewStatus = "Preview unavailable: renderer not initialized.";
        return false;
    }

    const ExportPreviewQuality requestedPreviewQuality = m_exportPreviewUseFinalQualityOnce
        ? ExportPreviewQuality::Final
        : m_exportDialogSettings.quality.previewQuality;
    m_exportPreviewUseFinalQualityOnce = false;
    const ExportPreviewSize previewSize =
        resolveExportPreviewSize(m_exportDialogSettings.size.width,
                                 m_exportDialogSettings.size.height,
                                 requestedPreviewQuality);
    const int previewW = previewSize.width;
    const int previewH = previewSize.height;

    ExportSettings previewSettings = m_exportDialogSettings;
    previewSettings.size.width = previewW;
    previewSettings.size.height = previewH;
    if (requestedPreviewQuality != ExportPreviewQuality::Final) {
        previewSettings.quality.mode = ExportQualityMode::Override;
        previewSettings.quality = qualitySettingsForPreset(
            requestedPreviewQuality == ExportPreviewQuality::Draft
                ? ExportQualityPreset::Draft
                : ExportQualityPreset::Normal);
        previewSettings.quality.mode = ExportQualityMode::Override;
        previewSettings.quality.supersampling = ExportSupersampling::Off;
    }

    std::vector<std::uint8_t> pixels;
    int renderedW = 0;
    int renderedH = 0;
    if (!renderPlotPixelsOffscreen(previewSettings, pixels, renderedW, renderedH)) {
        m_exportPreviewStatus = "Preview render failed.";
        cleanupExportPreviewResources();
        return false;
    }

    unpremultiplyPixels(pixels);
    if (previewSettings.appearance.grayscaleOutput) {
        convertPixelsToGrayscaleRgba(pixels);
    }

    if (renderedW <= 0 || renderedH <= 0) {
        m_exportPreviewStatus = "Preview render produced invalid size.";
        cleanupExportPreviewResources();
        return false;
    }

    if (renderedW != previewW || renderedH != previewH) {
        std::vector<std::uint8_t> resizedPixels;
        resizePixelsBilinear(pixels, renderedW, renderedH, previewW, previewH, resizedPixels);
        pixels = std::move(resizedPixels);
        renderedW = previewW;
        renderedH = previewH;
    }

    const bool recreateTexture =
        (m_exportPreviewTexture == nullptr || m_exportPreviewSrv == nullptr ||
         m_exportPreviewWidth != renderedW || m_exportPreviewHeight != renderedH);
    if (recreateTexture) {
        cleanupExportPreviewResources();

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = static_cast<UINT>(renderedW);
        texDesc.Height = static_cast<UINT>(renderedH);
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = pixels.data();
        initData.SysMemPitch = static_cast<UINT>(renderedW * 4);

        if (FAILED(m_device->CreateTexture2D(&texDesc, &initData, &m_exportPreviewTexture)) ||
            !m_exportPreviewTexture) {
            m_exportPreviewStatus = "Preview texture creation failed.";
            cleanupExportPreviewResources();
            return false;
        }

        if (FAILED(m_device->CreateShaderResourceView(m_exportPreviewTexture, nullptr, &m_exportPreviewSrv)) ||
            !m_exportPreviewSrv) {
            m_exportPreviewStatus = "Preview texture view creation failed.";
            cleanupExportPreviewResources();
            return false;
        }
        m_exportPreviewWidth = renderedW;
        m_exportPreviewHeight = renderedH;
    } else {
        m_deviceContext->UpdateSubresource(m_exportPreviewTexture, 0, nullptr, pixels.data(),
                                           static_cast<UINT>(renderedW * 4), 0);
    }

    m_exportPreviewStatus = "Preview ready (" + std::to_string(renderedW) + "x" +
        std::to_string(renderedH) + ", " +
        exportPreviewQualityLabel(requestedPreviewQuality) + ", " +
        exportAspectModeLabel(m_exportDialogSettings.output.aspectMode);
    if (previewSize.reducedFromOutput) {
        m_exportPreviewStatus += ", reduced";
    }
    m_exportPreviewStatus += ").";
    return true;
}

void Application::applyExportPostProcessing(std::vector<std::uint8_t>& pixels,
                                            int sourceWidth, int sourceHeight,
                                            std::vector<std::uint8_t>& outputPixels,
                                            int& outputWidth, int& outputHeight) {
    outputWidth = sourceWidth;
    outputHeight = sourceHeight;
    outputPixels = pixels;

    const int targetWidth = clampExportDimension(m_pendingExportSettings.size.width);
    const int targetHeight = clampExportDimension(m_pendingExportSettings.size.height);
    if (targetWidth != sourceWidth || targetHeight != sourceHeight) {
        resizePixelsBilinear(pixels, sourceWidth, sourceHeight,
                             targetWidth, targetHeight, outputPixels);
        outputWidth = targetWidth;
        outputHeight = targetHeight;
    }

    // D3D11 render-target readback for DXGI_FORMAT_R8G8B8A8_UNORM returns RGBA bytes, while
    // file/clipboard export paths below expect BGRA. Convert once here, then normalize alpha.
    convertPixelsRgbaToBgra(outputPixels);
    // ImGui rendering over a transparent target stores premultiplied color in the render target.
    // PNG/clipboard consumers generally expect straight alpha color channels.
    unpremultiplyPixels(outputPixels);

    if (m_pendingExportSettings.appearance.grayscaleOutput) {
        convertPixelsToGrayscale(outputPixels);
    }
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

    cleanupExportPreviewResources();
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
    cleanupExportPreviewResources();
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
    m_exportPreviewDirty = true;
    m_exportPreviewLastChanged = std::chrono::steady_clock::now();
    m_exportPreviewStatus = m_exportDialogSettings.quality.autoRefreshPreview
        ? "Preview out of date. Auto refresh pending."
        : "Preview out of date.";
    m_redrawRequested = true;
}

void Application::requestExportPreviewRefresh() {
    m_exportPreviewDirty = true;
    m_exportPreviewRefreshRequested = true;
    m_exportPreviewStatus = "Rendering preview...";
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

    const int outputWidth = clampExportDimension(settings.size.width);
    const int outputHeight = clampExportDimension(settings.size.height);
    const int samplingFactor =
        effectiveSupersamplingFactor(outputWidth, outputHeight, effectiveExportSupersampling(settings));
    const int targetWidth = clampExportDimension(outputWidth * samplingFactor);
    const int targetHeight = clampExportDimension(outputHeight * samplingFactor);
    const ExportWorldBounds sourceBounds{
        m_viewTransform.worldXMin(), m_viewTransform.worldXMax(),
        m_viewTransform.worldYMin(), m_viewTransform.worldYMax()
    };
    const ExportResolvedView resolvedView =
        resolveExportView(targetWidth, targetHeight, sourceBounds, settings.output.aspectMode);

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = static_cast<UINT>(targetWidth);
    texDesc.Height = static_cast<UINT>(targetHeight);
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
    exportViewport.Width = static_cast<float>(targetWidth);
    exportViewport.Height = static_cast<float>(targetHeight);
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
    io.DisplaySize = ImVec2(static_cast<float>(targetWidth), static_cast<float>(targetHeight));
    io.MousePos = ImVec2(-100000.0f, -100000.0f);
    io.MouseWheel = 0.0f;
    io.MouseWheelH = 0.0f;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(targetWidth), static_cast<float>(targetHeight)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::Begin("##ExportPlotOffscreen", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoInputs)) {
        const std::array<float, 4> backgroundColor = resolveExportBackgroundColor(settings);
        const ImVec2 windowPos = ImGui::GetWindowPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            windowPos,
            ImVec2(windowPos.x + static_cast<float>(targetWidth),
                   windowPos.y + static_cast<float>(targetHeight)),
            ImGui::ColorConvertFloat4ToU32(
                ImVec4(backgroundColor[0], backgroundColor[1],
                       backgroundColor[2], backgroundColor[3])));

        Core::ViewTransform exportView = m_viewTransform;
        PlotSettings exportSettings = m_plotSettings;
        exportSettings.autoRotate = false;

        exportView.state.centerX = (resolvedView.visibleBounds.xMin + resolvedView.visibleBounds.xMax) * 0.5;
        exportView.state.centerY = (resolvedView.visibleBounds.yMin + resolvedView.visibleBounds.yMax) * 0.5;
        exportView.state.scaleX = resolvedView.scaleX;
        exportView.state.scaleY = resolvedView.scaleY;
        const Model::SceneSummary& scene = m_sceneSummary;

        PlotRenderOverrides exportOverrides = plotRenderOverridesForExport(settings);
        PlotQualityDecision exportQuality = plotQualityDecisionForExport(settings);

        ImGui::SetCursorScreenPos(
            ImVec2(windowPos.x + static_cast<float>(resolvedView.marginLeftPx),
                   windowPos.y + static_cast<float>(resolvedView.marginTopPx)));
        const ImVec2 contentSize(
            static_cast<float>(resolvedView.contentWidthPx),
            static_cast<float>(resolvedView.contentHeightPx));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::BeginChild("##ExportPlotOffscreenContent", contentSize, ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse |
                              ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoInputs |
                              ImGuiWindowFlags_NoBackground)) {
            m_plotPanel.render(m_formulas, exportView, exportSettings, scene,
                               &exportOverrides, &exportQuality);
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

bool Application::saveImageToPath(const std::wstring& path,
                                  const std::vector<std::uint8_t>& pixels,
                                  int width, int height, std::string& error) {
    const XFWin::ImageEncodeResult result =
        m_imageEncoder.saveByExtensionBgra(std::filesystem::path(path), pixels, width, height);
    if (!result) {
        error = result.error;
        return false;
    }
    error.clear();
    return true;
}

std::string Application::buildExportMetadataJson(const ExportSettings& settings,
                                                 const std::wstring& imagePath,
                                                 int width,
                                                 int height) const {
    auto writeColor = [](XFJson::JsonWriter& writer, const auto& color) {
        writer.beginArray();
        writer.value(color[0]);
        writer.value(color[1]);
        writer.value(color[2]);
        writer.value(color[3]);
        writer.endArray();
    };

    const int presetIndex = std::clamp(
        settings.size.selectedPreset, 0, static_cast<int>(exportSizePresets().size()) - 1);
    const auto& sizePreset = exportSizePresets()[static_cast<size_t>(presetIndex)];
    const std::array<float, 4> resolvedBackground = resolveExportBackgroundColor(settings);
    const std::string imagePathUtf8 = XFWUtf::utf16ToUtf8OrEmpty(imagePath);
    std::filesystem::path imageFsPath(imagePath);
    std::string actualFormat = exportFormatLabel(settings.output.format);
    std::wstring extension = imageFsPath.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    if (extension == L".bmp") {
        actualFormat = "BMP";
    } else if (extension == L".png") {
        actualFormat = "PNG";
    }

    XFJson::JsonWriter writer;
    writer.beginObject();
    writer.key("schemaVersion");
    writer.value(kExportMetadataSchemaVersion);

    writer.key("application");
    writer.beginObject();
    writer.key("name");
    writer.value("XpressFormula");
    writer.key("version");
    writer.value(XF_BUILD_VERSION);
    writer.key("repoUrl");
    writer.value(XF_BUILD_REPO_URL);
    writer.key("branch");
    writer.value(XF_BUILD_BRANCH);
    writer.key("commit");
    writer.value(XF_BUILD_COMMIT);
    writer.endObject();

    writer.key("image");
    writer.beginObject();
    writer.key("path");
    writer.value(imagePathUtf8);
    writer.key("width");
    writer.value(width);
    writer.key("height");
    writer.value(height);
    writer.key("format");
    writer.value(actualFormat);
    writer.endObject();

    writer.key("formulas");
    writer.beginArray();
    for (size_t i = 0; i < m_formulas.size(); ++i) {
        const Model::Formula& formula = m_formulas[i];
        const FormulaRenderKind renderKind = formulaRenderKindFor(formula.compiled.kind);
        const char* diagnostic = formulaDiagnosticText(formula);
        writer.beginObject();
        writer.key("index");
        writer.value(static_cast<unsigned long long>(i + 1));
        writer.key("expression");
        writer.value(formula.expression);
        writer.key("visible");
        writer.value(formula.visible);
        writer.key("color");
        writeColor(writer, formula.color);
        writer.key("valid");
        writer.value(formula.isValid());
        writer.key("type");
        writer.value(formulaTypeLabel(formula));
        writer.key("renderKind");
        writer.value(formulaRenderKindLabel(renderKind));
        writer.key("equation");
        writer.value(formula.compiled.equation);
        writer.key("variableCount");
        writer.value(displayedVariableCount(formula));
        writer.key("variables");
        writer.beginArray();
        for (const std::string& variable : formula.compiled.variables) {
            writer.value(variable);
        }
        writer.endArray();
        writer.key("zSlice");
        writer.value(formula.zSlice);
        writer.key("error");
        writer.value(diagnostic);
        writer.endObject();
    }
    writer.endArray();

    writer.key("view");
    writer.beginObject();
    writer.key("center");
    writer.beginObject();
    writer.key("x");
    writer.value(m_viewTransform.state.centerX);
    writer.key("y");
    writer.value(m_viewTransform.state.centerY);
    writer.endObject();
    writer.key("scale");
    writer.beginObject();
    writer.key("x");
    writer.value(m_viewTransform.state.scaleX);
    writer.key("y");
    writer.value(m_viewTransform.state.scaleY);
    writer.endObject();
    writer.key("screen");
    writer.beginObject();
    writer.key("width");
    writer.value(m_viewTransform.viewport.width);
    writer.key("height");
    writer.value(m_viewTransform.viewport.height);
    writer.key("originX");
    writer.value(m_viewTransform.viewport.originX);
    writer.key("originY");
    writer.value(m_viewTransform.viewport.originY);
    writer.endObject();
    writer.key("worldBounds");
    writer.beginObject();
    writer.key("xMin");
    writer.value(m_viewTransform.worldXMin());
    writer.key("xMax");
    writer.value(m_viewTransform.worldXMax());
    writer.key("yMin");
    writer.value(m_viewTransform.worldYMin());
    writer.key("yMax");
    writer.value(m_viewTransform.worldYMax());
    writer.endObject();
    writer.endObject();

    writer.key("camera");
    writer.beginObject();
    writer.key("azimuthDeg");
    writer.value(m_plotSettings.azimuthDeg);
    writer.key("elevationDeg");
    writer.value(m_plotSettings.elevationDeg);
    writer.key("zScale");
    writer.value(m_plotSettings.zScale);
    writer.key("autoRotate");
    writer.value(m_plotSettings.autoRotate);
    writer.key("autoRotateSpeedDegPerSec");
    writer.value(m_plotSettings.autoRotateSpeedDegPerSec);
    writer.endObject();

    writer.key("display");
    writer.beginObject();
    writer.key("xyRenderModePreference");
    writer.value(toDisplayLabel(m_plotSettings.xyRenderModePreference));
    writer.key("xyRenderModePreferenceId");
    writer.value(toStorageName(m_plotSettings.xyRenderModePreference));
    writer.key("hudMode");
    writer.value(plotHudModeLabel(m_plotSettings.hudMode));
    writer.key("hudModeId");
    writer.value(toStorageName(m_plotSettings.hudMode));
    writer.key("optimizeRendering");
    writer.value(m_plotSettings.optimizeRendering);
    writer.key("showGrid");
    writer.value(m_plotSettings.showGrid);
    writer.key("showCoordinates");
    writer.value(m_plotSettings.showCoordinates);
    writer.key("showWires");
    writer.value(m_plotSettings.showWires);
    writer.key("showSurfaceEnvelope");
    writer.value(m_plotSettings.showSurfaceEnvelope);
    writer.key("showAxisTriad");
    writer.value(m_plotSettings.showAxisTriad);
    writer.key("effectiveShowAxisTriad");
    writer.value(m_plotSettings.effectiveShowAxisTriad());
    writer.key("surfaceResolution");
    writer.value(m_plotSettings.surfaceResolution);
    writer.key("implicitSurfaceResolution");
    writer.value(m_plotSettings.implicitSurfaceResolution);
    writer.key("surfaceOpacity");
    writer.value(m_plotSettings.surfaceOpacity);
    writer.key("wireOpacity");
    writer.value(m_plotSettings.wireOpacity);
    writer.key("wireThickness");
    writer.value(m_plotSettings.wireThickness);
    writer.key("wireStride");
    writer.value(m_plotSettings.wireStride);
    writer.key("envelopeThickness");
    writer.value(m_plotSettings.envelopeThickness);
    writer.key("heatmapOpacity");
    writer.value(m_plotSettings.heatmapOpacity);
    writer.endObject();

    writer.key("export");
    writer.beginObject();
    writer.key("profile");
    writer.value(exportProfileLabel(settings.profile));
    writer.key("profileId");
    writer.value(toStorageName(settings.profile));
    writer.key("requestedWidth");
    writer.value(settings.size.width);
    writer.key("requestedHeight");
    writer.value(settings.size.height);
    writer.key("outputWidth");
    writer.value(width);
    writer.key("outputHeight");
    writer.value(height);
    writer.key("scale");
    writer.value(settings.size.scale);
    writer.key("sizePreset");
    writer.value(sizePreset.label);
    writer.key("sizePresetId");
    writer.value(sizePreset.storageName);
    writer.key("lockAspectRatio");
    writer.value(settings.size.lockAspectRatio);
    writer.key("format");
    writer.value(exportFormatLabel(settings.output.format));
    writer.key("formatId");
    writer.value(toStorageName(settings.output.format));
    writer.key("backgroundMode");
    writer.value(exportBackgroundModeLabel(settings.appearance.backgroundMode));
    writer.key("backgroundModeId");
    writer.value(toStorageName(settings.appearance.backgroundMode));
    writer.key("customBackgroundColor");
    writeColor(writer, settings.appearance.backgroundColor);
    writer.key("resolvedBackgroundColor");
    writeColor(writer, resolvedBackground);
    writer.key("grayscaleOutput");
    writer.value(settings.appearance.grayscaleOutput);
    writer.key("aspectMode");
    writer.value(exportAspectModeLabel(settings.output.aspectMode));
    writer.key("aspectModeId");
    writer.value(toStorageName(settings.output.aspectMode));
    writer.key("showGrid");
    writer.value(settings.scene.showGrid);
    writer.key("showCoordinates");
    writer.value(settings.scene.showCoordinates);
    writer.key("showWires");
    writer.value(settings.scene.showWires);
    writer.key("showEnvelope");
    writer.value(settings.scene.showEnvelope);
    writer.key("showAxisTriad");
    writer.value(settings.scene.showAxisTriad);
    writer.key("effectiveShowAxisTriad");
    writer.value(isAxisTriadVisible(settings.scene.showCoordinates,
                                    settings.scene.showAxisTriad));
    writer.key("qualityMode");
    writer.value(exportQualityModeLabel(settings.quality.mode));
    writer.key("qualityModeId");
    writer.value(toStorageName(settings.quality.mode));
    writer.key("qualityPreset");
    writer.value(exportQualityPresetLabel(settings.quality.preset));
    writer.key("qualityPresetId");
    writer.value(toStorageName(settings.quality.preset));
    writer.key("surfaceResolution");
    writer.value(settings.quality.surfaceResolution);
    writer.key("implicitSurfaceResolution");
    writer.value(settings.quality.implicitSurfaceResolution);
    writer.key("wireThicknessScale");
    writer.value(settings.quality.wireThicknessScale);
    writer.key("supersampling");
    writer.value(exportSupersamplingLabel(settings.quality.supersampling));
    writer.key("supersamplingId");
    writer.value(toStorageName(settings.quality.supersampling));
    writer.key("previewQuality");
    writer.value(exportPreviewQualityLabel(settings.quality.previewQuality));
    writer.key("previewQualityId");
    writer.value(toStorageName(settings.quality.previewQuality));
    writer.key("autoRefreshPreview");
    writer.value(settings.quality.autoRefreshPreview);
    writer.key("openAfterSave");
    writer.value(settings.output.openAfterSave);
    writer.key("showInFolderAfterSave");
    writer.value(settings.output.showInFolderAfterSave);
    writer.key("copyPathAfterSave");
    writer.value(settings.output.copyPathAfterSave);
    writer.key("saveMetadataSidecar");
    writer.value(settings.output.saveMetadataSidecar);
    writer.endObject();

    writer.endObject();
    std::string json = writer.str();
    json += '\n';
    return json;
}

bool Application::writeExportMetadataSidecar(const ExportSettings& settings,
                                             const std::wstring& imagePath,
                                             int width,
                                             int height,
                                             std::string& error) const {
    error.clear();
    const std::filesystem::path sidecarPath = exportMetadataSidecarPath(std::filesystem::path(imagePath));
    const std::string json = buildExportMetadataJson(settings, imagePath, width, height);
    const XFAtomic::AtomicWriteResult writeResult = XFAtomic::writeTextAtomically(sidecarPath, json);
    if (!writeResult) {
        error = "Could not write metadata sidecar: " + writeResult.error;
        return false;
    }

    return true;
}

bool Application::copyPixelsToClipboard(const std::vector<std::uint8_t>& pixels,
                                        int width, int height, std::string& error) {
    const XFWin::ClipboardResult result =
        m_clipboardService.copyDibImageBgra(m_hWnd, pixels, width, height);
    if (!result) {
        error = result.error;
        return false;
    }
    error.clear();
    return true;
}

void Application::openLastSavedExport() {
    if (m_lastExportSavedPath.empty()) {
        m_exportStatus = "No saved export path is available.";
    } else if (m_shellService.openPath(m_lastExportSavedPath)) {
        m_exportStatus = "Opened saved image.";
    } else {
        m_exportStatus = "Could not open saved image.";
    }
    m_redrawRequested = true;
}

void Application::showLastSavedExportInFolder() {
    if (m_lastExportSavedPath.empty()) {
        m_exportStatus = "No saved export path is available.";
    } else if (m_shellService.revealPath(m_lastExportSavedPath)) {
        m_exportStatus = "Opened saved image location.";
    } else {
        m_exportStatus = "Could not show saved image in folder.";
    }
    m_redrawRequested = true;
}

void Application::copyLastSavedExportPath() {
    if (m_lastExportSavedPath.empty()) {
        m_exportStatus = "No saved export path is available.";
    } else if (m_clipboardService.copyUtf16Text(m_hWnd, m_lastExportSavedPath)) {
        m_exportStatus = "Copied saved image path.";
    } else {
        m_exportStatus = "Could not copy saved image path.";
    }
    m_redrawRequested = true;
}

void Application::processPendingExportActions() {
    if (!m_pendingSavePlotImage && !m_pendingCopyPlotImage) {
        return;
    }

    std::vector<std::string> messages;
    std::vector<std::uint8_t> capturedPixels;
    int capturedWidth = 0;
    int capturedHeight = 0;
    if (!renderPlotPixelsOffscreen(m_pendingExportSettings,
                                   capturedPixels, capturedWidth, capturedHeight)) {
        // Fallback to visible backbuffer capture if offscreen rendering fails unexpectedly.
        if (!capturePlotPixels(capturedPixels, capturedWidth, capturedHeight)) {
            messages.emplace_back("Export failed: unable to render/capture plot area.");
            m_pendingSavePlotImage = false;
            m_pendingCopyPlotImage = false;
            m_redrawRequested = true;
            m_exportStatus = messages.front();
            return;
        }
        messages.emplace_back("Warning: export used fallback screen capture path.");
    }

    std::vector<std::uint8_t> outputPixels;
    int outputWidth = 0;
    int outputHeight = 0;
    applyExportPostProcessing(capturedPixels, capturedWidth, capturedHeight,
                              outputPixels, outputWidth, outputHeight);

    if (m_pendingSavePlotImage) {
        const XFWin::ExportImageFormat preferredFormat =
            (m_pendingExportSettings.output.format == ExportFormat::Bmp)
                ? XFWin::ExportImageFormat::Bmp
                : XFWin::ExportImageFormat::Png;
        const XFWin::DialogResult dialog =
            m_fileDialogService.saveImage(m_hWnd, preferredFormat);
        if (dialog.selected()) {
            const std::wstring& path = dialog.path;
            std::string error;
            if (saveImageToPath(path, outputPixels, outputWidth, outputHeight, error)) {
                m_lastExportSavedPath = path;
                messages.emplace_back("Saved plot image to: " + XFWUtf::utf16ToUtf8OrEmpty(path));
                if (m_pendingExportSettings.output.saveMetadataSidecar) {
                    std::string metadataError;
                    if (writeExportMetadataSidecar(m_pendingExportSettings,
                                                   path,
                                                   outputWidth,
                                                   outputHeight,
                                                   metadataError)) {
                        const std::filesystem::path sidecarPath =
                            exportMetadataSidecarPath(std::filesystem::path(path));
                        messages.emplace_back("Saved metadata sidecar: " +
                            XFWUtf::utf16ToUtf8OrEmpty(sidecarPath.wstring()));
                    } else {
                        messages.emplace_back("Metadata sidecar failed: " + metadataError);
                    }
                }
                if (m_pendingExportSettings.output.openAfterSave) {
                    messages.emplace_back(m_shellService.openPath(path)
                        ? "Opened saved image."
                        : "Could not open saved image.");
                }
                if (m_pendingExportSettings.output.showInFolderAfterSave) {
                    messages.emplace_back(m_shellService.revealPath(path)
                        ? "Opened saved image location."
                        : "Could not show saved image in folder.");
                }
                if (m_pendingExportSettings.output.copyPathAfterSave) {
                    messages.emplace_back(m_clipboardService.copyUtf16Text(m_hWnd, path)
                        ? "Copied saved image path."
                        : "Could not copy saved image path.");
                }
            } else {
                messages.emplace_back("Save failed: " + error);
            }
        } else if (!dialog.cancelled()) {
            messages.emplace_back("Save failed: " + dialog.error);
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
    if (m_exportDialogOpen) {
        m_exportDialogPopupOpenNextFrame = true;
    }

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

} // namespace XpressFormula::UI
