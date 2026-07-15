// SPDX-License-Identifier: MIT
// Application.cpp - Win32 + D3D11 + ImGui application implementation.
#include "Application.h"
#include "../Core/UpdateVersionUtils.h"
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
#include <shellapi.h>
#include <tchar.h>
#include <winhttp.h>
#include <wincodec.h>
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
#include <string>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
#endif

// Forward-declare the ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// We store a pointer to the Application so the WndProc can access it.
static XpressFormula::UI::Application* g_app = nullptr;

namespace {

constexpr const wchar_t* kGitHubLatestReleaseApiHost = L"api.github.com";
constexpr const wchar_t* kGitHubLatestReleaseApiPath = L"/repos/russlank/XpressFormula/releases/latest";
constexpr const wchar_t* kGitHubReleasesUrlW = L"https://github.com/russlank/XpressFormula/releases";
constexpr const char* kGitHubReleasesUrlUtf8 = "https://github.com/russlank/XpressFormula/releases";
constexpr const wchar_t* kBuyMeACoffeeUrlW = L"https://buymeacoffee.com/russlank";
constexpr const char* kBuyMeACoffeeUrlUtf8 = "https://buymeacoffee.com/russlank";

std::string readWinHttpResponseBody(HINTERNET requestHandle) {
    std::string response;
    if (!requestHandle) {
        return response;
    }

    for (;;) {
        DWORD bytesAvailable = 0;
        if (!::WinHttpQueryDataAvailable(requestHandle, &bytesAvailable)) {
            return {};
        }
        if (bytesAvailable == 0) {
            break;
        }

        const size_t oldSize = response.size();
        response.resize(oldSize + static_cast<size_t>(bytesAvailable));
        DWORD bytesRead = 0;
        if (!::WinHttpReadData(requestHandle, response.data() + oldSize, bytesAvailable, &bytesRead)) {
            return {};
        }
        response.resize(oldSize + static_cast<size_t>(bytesRead));
        if (bytesRead == 0) {
            break;
        }
    }

    return response;
}

bool openUrlInBrowser(const wchar_t* url) {
    if (!url || url[0] == L'\0') {
        return false;
    }
    const HINSTANCE result = ::ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool openPathWithShell(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    const HINSTANCE result = ::ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool revealPathInExplorer(const std::wstring& path) {
    if (path.empty()) {
        return false;
    }
    std::wstring args = L"/select,\"";
    args += path;
    args += L"\"";
    const HINSTANCE result = ::ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(),
                                            nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool copyUnicodeTextToClipboard(HWND owner, const std::wstring& text) {
    const size_t byteCount = (text.size() + 1u) * sizeof(wchar_t);
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, byteCount);
    if (!memory) {
        return false;
    }

    void* data = ::GlobalLock(memory);
    if (!data) {
        ::GlobalFree(memory);
        return false;
    }
    std::memcpy(data, text.c_str(), byteCount);
    ::GlobalUnlock(memory);

    if (!::OpenClipboard(owner)) {
        ::GlobalFree(memory);
        return false;
    }

    ::EmptyClipboard();
    if (!::SetClipboardData(CF_UNICODETEXT, memory)) {
        ::CloseClipboard();
        ::GlobalFree(memory);
        return false;
    }
    ::CloseClipboard();
    return true;
}

// Background worker: query GitHub Releases API, parse the latest tag/url, and compare against the
// app semantic version. This runs off the UI thread so startup and manual checks do not stall ImGui.
XpressFormula::UI::Application::UpdateCheckResult fetchLatestReleaseFromGitHub(bool manualRequest) {
    using namespace XpressFormula::Core::UpdateVersionUtils;

    XpressFormula::UI::Application::UpdateCheckResult result;
    result.manualRequest = manualRequest;
    result.releaseUrl = kGitHubReleasesUrlUtf8;

    HINTERNET session = ::WinHttpOpen(L"XpressFormula/" XF_VERSION_WSTRING,
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS,
                                      0);
    if (!session) {
        result.statusMessage = "Update check failed: could not initialize HTTP session.";
        return result;
    }

    ::WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);

    HINTERNET connection = ::WinHttpConnect(session, kGitHubLatestReleaseApiHost,
                                            INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connection) {
        ::WinHttpCloseHandle(session);
        result.statusMessage = "Update check failed: could not connect to GitHub.";
        return result;
    }

    HINTERNET request = ::WinHttpOpenRequest(connection, L"GET", kGitHubLatestReleaseApiPath,
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!request) {
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        result.statusMessage = "Update check failed: could not create HTTP request.";
        return result;
    }

    const wchar_t* headers =
        L"Accept: application/vnd.github+json\r\n"
        L"X-GitHub-Api-Version: 2022-11-28\r\n";
    BOOL sendOk = ::WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L),
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (sendOk) {
        sendOk = ::WinHttpReceiveResponse(request, nullptr);
    }
    if (!sendOk) {
        ::WinHttpCloseHandle(request);
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        result.statusMessage = "Update check failed: request to GitHub was not successful.";
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!::WinHttpQueryHeaders(request,
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX,
                               &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX) ||
        statusCode != 200) {
        ::WinHttpCloseHandle(request);
        ::WinHttpCloseHandle(connection);
        ::WinHttpCloseHandle(session);
        std::ostringstream oss;
        oss << "Update check failed: GitHub returned HTTP " << statusCode << ".";
        result.statusMessage = oss.str();
        return result;
    }

    const std::string body = readWinHttpResponseBody(request);
    ::WinHttpCloseHandle(request);
    ::WinHttpCloseHandle(connection);
    ::WinHttpCloseHandle(session);
    if (body.empty()) {
        result.statusMessage = "Update check failed: empty response from GitHub.";
        return result;
    }

    // We only need two fields from the GitHub JSON payload for the in-app notification.
    const std::string latestTag = extractJsonStringField(body, "tag_name");
    const std::string htmlUrl = extractJsonStringField(body, "html_url");
    if (!htmlUrl.empty()) {
        result.releaseUrl = htmlUrl;
    }
    if (latestTag.empty()) {
        result.statusMessage = "Update check failed: release tag not found in GitHub response.";
        return result;
    }

    result.requestSucceeded = true;
    result.latestTag = latestTag;
    result.updateAvailable = isRemoteVersionNewer(XF_VERSION_STRING, latestTag);

    if (result.updateAvailable) {
        result.statusMessage = "Update available: " + latestTag + " (current " XF_VERSION_STRING ").";
    } else {
        result.statusMessage = "You are running the latest version (" XF_VERSION_STRING ").";
    }
    return result;
}

} // namespace

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

        bool hasSurfaceFormula = false;
        bool has2DFormula = false;
        for (const FormulaEntry& formula : m_formulas) {
            if (!formula.visible || !formula.isValid()) {
                continue;
            }

            if (formula.uses3DSurface()) {
                hasSurfaceFormula = true;
            }

            switch (formula.renderKind) {
                case FormulaRenderKind::Curve2D:
                case FormulaRenderKind::Implicit2D:
                    has2DFormula = true;
                    break;
                case FormulaRenderKind::ScalarField3D:
                    if (!formula.isEquation) {
                        has2DFormula = true;
                    }
                    break;
                default:
                    break;
            }

            if (hasSurfaceFormula && has2DFormula) {
                break;
            }
        }

        const XYRenderMode effectiveRenderMode =
            m_plotSettings.resolveXYRenderMode(has2DFormula, hasSurfaceFormula);
        const bool continuousRender =
            m_plotSettings.autoRotate &&
            hasSurfaceFormula &&
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

    if (m_exportDialogOpen && m_exportDialogSettings.autoRefreshPreview &&
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

    // We fill the entire OS window with the sidebar, a splitter, and the plot.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    float totalW = viewport->WorkSize.x;
    float totalH = viewport->WorkSize.y;
    const float splitterWidth = kSidebarSplitterWidth;
    const float availableForSidebar = totalW - splitterWidth - kMinPlotWidth;
    const float dynamicMinSidebar = (std::min)(kMinSidebarWidth, (std::max)(180.0f, totalW * 0.38f));
    const float maxSidebar = (std::min)(kMaxSidebarWidth,
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
    m_formulaPanel.render(m_formulas);
    bool hasSurfaceFormula = false;
    bool has2DFormula = false;
    for (const FormulaEntry& formula : m_formulas) {
        if (!formula.visible || !formula.isValid()) {
            continue;
        }

        if (formula.uses3DSurface()) {
            hasSurfaceFormula = true;
        }

        switch (formula.renderKind) {
            case FormulaRenderKind::Curve2D:
            case FormulaRenderKind::Implicit2D:
                has2DFormula = true;
                break;
            case FormulaRenderKind::ScalarField3D:
                if (!formula.isEquation) {
                    has2DFormula = true;
                }
                break;
            default:
                break;
        }

        if (hasSurfaceFormula && has2DFormula) {
            break;
        }
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ControlPanelActions actions = m_controlPanel.render(
        m_viewTransform, m_plotSettings, has2DFormula, hasSurfaceFormula, m_exportStatus);
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
                : utf8ToWide(m_updateReleaseUrl.c_str());
            if (releaseUrlWide.empty()) {
                releaseUrlWide = kGitHubReleasesUrlW;
            }
            if (!openUrlInBrowser(releaseUrlWide.c_str())) {
                m_updateStatus = "Could not open browser. Visit: " + std::string(kGitHubReleasesUrlUtf8);
            } else if (m_updateAvailable) {
                m_updateNoticeDismissed = true;
            }
            m_redrawRequested = true;
        }
        if (ImGui::Button("Buy Me a Coffee", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            if (!openUrlInBrowser(kBuyMeACoffeeUrlW)) {
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
    ImGui::InvisibleButton("##SidebarSplitterHandle", ImVec2(splitterWidth, totalH),
                           ImGuiButtonFlags_MouseButtonLeft);
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemActive()) {
        m_sidebarWidth = std::clamp(m_sidebarWidth + ImGui::GetIO().MouseDelta.x,
                                    dynamicMinSidebar, maxSidebar);
        m_redrawRequested = true;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        m_sidebarWidth = std::clamp(kDefaultSidebarWidth, dynamicMinSidebar, maxSidebar);
        m_redrawRequested = true;
    }
    {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const bool active = ImGui::IsItemActive();
        const bool hovered = ImGui::IsItemHovered();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2((min.x + max.x) * 0.5f, min.y + 6.0f),
            ImVec2((min.x + max.x) * 0.5f, max.y - 6.0f),
            IM_COL32(120, 120, 128, hovered ? 230 : 150),
            active ? 2.0f : 1.0f);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    renderExportDialog(sidebar, totalH);

    // ---- Plot area ----
    ImGui::SetNextWindowPos(ImVec2(plotX, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(plotWidth, totalH));
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
        exportOverrides.showAxisTriad = m_pendingExportSettings.showAxisTriad;
        exportOverrides.showCanvasBorder = false;
        exportOverrides.backgroundColor = resolveExportBackgroundColor(m_pendingExportSettings);
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

    int width = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenWidth)));
    int height = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenHeight)));
    if (width <= 0) width = 1024;
    if (height <= 0) height = 768;

    m_exportDialogSettings.width = width;
    m_exportDialogSettings.height = height;
    m_exportDialogSettings.scale = 1;
    m_exportDialogSettings.selectedSizePreset = 0;
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
        // Default export visibility follows the current interactive view state.
        m_exportDialogSettings.showGrid = m_plotSettings.showGrid;
        m_exportDialogSettings.showCoordinates = m_plotSettings.showCoordinates;
        m_exportDialogSettings.showWires = m_plotSettings.showWires;
        m_exportDialogSettings.showEnvelope = m_plotSettings.showSurfaceEnvelope;
        m_exportDialogSettings.showAxisTriad = m_plotSettings.showAxisTriad;
        if (m_exportDialogSettings.showCoordinates) {
            m_exportDialogSettings.showAxisTriad = false;
        }
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
        const int sourceWidth = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenWidth)));
        const int sourceHeight = static_cast<int>(std::lround((std::max)(1.0f, m_viewTransform.screenHeight)));
        const ExportWorldBounds sourceBounds{
            m_viewTransform.worldXMin(), m_viewTransform.worldXMax(),
            m_viewTransform.worldYMin(), m_viewTransform.worldYMax()
        };
        auto& settings = m_exportDialogSettings;
        settings.selectedSizePreset = std::clamp(settings.selectedSizePreset, 0,
            static_cast<int>(exportSizePresets().size()) - 1);
        settings.scale = std::clamp(settings.scale, 1, 4);

        auto applySelectedSizePreset = [&]() {
            const auto& preset = exportSizePresets()[static_cast<size_t>(settings.selectedSizePreset)];
            if (preset.custom) {
                return;
            }
            const int baseWidth = preset.useCurrentSize ? sourceWidth : preset.width;
            const int baseHeight = preset.useCurrentSize ? sourceHeight : preset.height;
            if (baseWidth <= 0 || baseHeight <= 0) {
                return;
            }
            settings.width = clampExportDimension(baseWidth * settings.scale);
            settings.height = clampExportDimension(baseHeight * settings.scale);
        };

        auto markCustomSize = [&]() {
            settings.selectedSizePreset = static_cast<int>(exportSizePresets().size()) - 1;
        };

        auto resetExportSettings = [&]() {
            settings = ExportDialogSettings{};
            m_exportDialogSizeInitialized = false;
            initialiseExportDialogSize();
            settings.showGrid = m_plotSettings.showGrid;
            settings.showCoordinates = m_plotSettings.showCoordinates;
            settings.showWires = m_plotSettings.showWires;
            settings.showEnvelope = m_plotSettings.showSurfaceEnvelope;
            settings.showAxisTriad = m_plotSettings.showAxisTriad;
            if (settings.showCoordinates) {
                settings.showAxisTriad = false;
            }
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
        const float splitterWidth = 8.0f;
        const float minSettingsWidth = 300.0f;
        const float minPreviewWidth = 240.0f;
        const float maxSettingsWidth = (std::max)(minSettingsWidth, bodyWidth - splitterWidth - minPreviewWidth);
        m_exportSettingsPaneWidth = std::clamp(m_exportSettingsPaneWidth, minSettingsWidth, maxSettingsWidth);
        const float settingsPaneWidth = m_exportSettingsPaneWidth;

        ImGui::BeginChild("##ExportSettingsPane", ImVec2(settingsPaneWidth, 0.0f), true);
        if (ImGui::BeginTabBar("##ExportSettingsTabs")) {
            if (ImGui::BeginTabItem("Size")) {
                if (sourceWidth > 0 && sourceHeight > 0) {
                    ImGui::Text("Current plot: %d x %d", sourceWidth, sourceHeight);
                } else {
                    ImGui::TextDisabled("Current plot size unavailable.");
                }

                if (ImGui::Button("Use Current", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                    settings.selectedSizePreset = 0;
                    applySelectedSizePreset();
                    previewChanged = true;
                }

                ImGui::Spacing();
                const auto& presets = exportSizePresets();
                const char* presetLabel = presets[static_cast<size_t>(settings.selectedSizePreset)].label;
                if (ImGui::BeginCombo("Preset", presetLabel)) {
                    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
                        const bool selected = (settings.selectedSizePreset == i);
                        if (ImGui::Selectable(presets[static_cast<size_t>(i)].label, selected)) {
                            settings.selectedSizePreset = i;
                            applySelectedSizePreset();
                            previewChanged = !presets[static_cast<size_t>(i)].custom || previewChanged;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                std::string scaleLabel = std::to_string(settings.scale) + "x";
                if (ImGui::BeginCombo("Scale", scaleLabel.c_str())) {
                    for (const int scaleOption : exportScaleOptions()) {
                        std::string optionLabel = std::to_string(scaleOption) + "x";
                        const bool selected = (settings.scale == scaleOption);
                        if (ImGui::Selectable(optionLabel.c_str(), selected)) {
                            const int previousScale = (std::max)(1, settings.scale);
                            settings.scale = scaleOption;
                            if (exportSizePresets()[static_cast<size_t>(settings.selectedSizePreset)].custom) {
                                settings.width = clampExportDimension(static_cast<int>(
                                    std::lround(static_cast<double>(settings.width) * scaleOption / previousScale)));
                                settings.height = clampExportDimension(static_cast<int>(
                                    std::lround(static_cast<double>(settings.height) * scaleOption / previousScale)));
                            } else {
                                applySelectedSizePreset();
                            }
                            previewChanged = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                int width = settings.width;
                int height = settings.height;
                const int previousWidth = (std::max)(1, width);
                const int previousHeight = (std::max)(1, height);
                const bool widthChanged = ImGui::InputInt("Width", &width, 16, 128);
                const bool heightChanged = ImGui::InputInt("Height", &height, 16, 128);
                validateExportSize(width, height);

                if (settings.lockAspectRatio && widthChanged && !heightChanged) {
                    height = aspectLockedHeight(width, previousWidth, previousHeight);
                } else if (settings.lockAspectRatio && heightChanged && !widthChanged) {
                    width = aspectLockedWidth(height, previousWidth, previousHeight);
                }

                if (widthChanged || heightChanged) {
                    settings.width = width;
                    settings.height = height;
                    markCustomSize();
                    previewChanged = true;
                }
                if (ImGui::Checkbox("Lock Aspect Ratio", &settings.lockAspectRatio)) {
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
                    if (ImGui::RadioButton(exportAspectModeLabel(mode), settings.aspectMode == mode)) {
                        settings.aspectMode = mode;
                        previewChanged = true;
                    }
                    ImGui::SetItemTooltip("%s", exportAspectModeTooltip(mode));
                }

                const auto resolvedView =
                    resolveExportView(settings.width, settings.height, sourceBounds, settings.aspectMode);
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

                const ExportSupersampling sampling =
                    settings.qualityMode == ExportQualityMode::Override
                        ? settings.quality.supersampling
                        : ExportSupersampling::Off;
                const auto estimatedBytes = estimateRgbaBufferBytes(settings.width, settings.height, sampling);
                const double estimatedMiB = bytesToMiB(estimatedBytes);
                ImGui::Spacing();
                ImGui::Text("Output: %d x %d", settings.width, settings.height);
                ImGui::Text("Render buffer: %.1f MiB", estimatedMiB);
                if (estimatedMiB >= kLargeExportWarningMiB) {
                    ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                       "Large export: expect slower rendering and higher memory use.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Appearance")) {
                int backgroundMode = static_cast<int>(settings.backgroundMode);
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
                        settings.backgroundMode = mode;
                        previewChanged = true;
                    }
                }

                if (settings.backgroundMode == ExportBackgroundMode::Custom) {
                    previewChanged = ImGui::ColorEdit3("Custom Color", settings.backgroundColor.data(),
                                                       ImGuiColorEditFlags_DisplayRGB) || previewChanged;
                    previewChanged = ImGui::SliderFloat("Opacity", &settings.backgroundColor[3],
                                                        0.0f, 1.0f, "%.2f") || previewChanged;
                } else if (settings.backgroundMode == ExportBackgroundMode::Transparent) {
                    ImGui::TextDisabled("Transparency is best preserved by PNG output.");
                    if (settings.format == ExportFormat::Bmp) {
                        ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                           "BMP export does not preserve alpha transparency.");
                    }
                }

                ImGui::Spacing();
                if (ImGui::RadioButton("Color", !settings.grayscaleOutput)) {
                    settings.grayscaleOutput = false;
                    previewChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Grayscale", settings.grayscaleOutput)) {
                    settings.grayscaleOutput = true;
                    previewChanged = true;
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Scene")) {
                previewChanged = ImGui::Checkbox("Grid", &settings.showGrid) || previewChanged;
                if (ImGui::Checkbox("Coordinates", &settings.showCoordinates)) {
                    if (settings.showCoordinates) {
                        settings.showAxisTriad = false;
                    }
                    previewChanged = true;
                }
                previewChanged = ImGui::Checkbox("Wires", &settings.showWires) || previewChanged;
                previewChanged = ImGui::Checkbox("Envelope", &settings.showEnvelope) || previewChanged;
                ImGui::BeginDisabled(settings.showCoordinates);
                if (ImGui::Checkbox("Axis Triad", &settings.showAxisTriad)) {
                    previewChanged = true;
                }
                ImGui::EndDisabled();
                if (settings.showCoordinates) {
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
                bool overrideQuality = (settings.qualityMode == ExportQualityMode::Override);
                if (ImGui::Checkbox("Override Interactive Quality", &overrideQuality)) {
                    settings.qualityMode = overrideQuality ? ExportQualityMode::Override
                                                           : ExportQualityMode::Interactive;
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
                            settings.quality = qualitySettingsForPreset(preset);
                            previewChanged = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                int surfaceResolution = settings.quality.surfaceResolution;
                if (ImGui::InputInt("Surface Density", &surfaceResolution, 4, 16)) {
                    settings.quality.surfaceResolution = std::clamp(surfaceResolution, 16, 256);
                    settings.quality.preset = ExportQualityPreset::Custom;
                    previewChanged = true;
                }

                int implicitResolution = settings.quality.implicitSurfaceResolution;
                if (ImGui::InputInt("Implicit Resolution", &implicitResolution, 4, 16)) {
                    settings.quality.implicitSurfaceResolution = std::clamp(implicitResolution, 16, 192);
                    settings.quality.preset = ExportQualityPreset::Custom;
                    previewChanged = true;
                }

                ImGui::BeginDisabled(!settings.showWires);
                if (ImGui::SliderFloat("Wire Thickness Scale", &settings.quality.wireThicknessScale,
                                       0.25f, 3.0f, "%.2fx")) {
                    settings.quality.preset = ExportQualityPreset::Custom;
                    previewChanged = true;
                }
                ImGui::EndDisabled();
                if (!settings.showWires) {
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
                if (ImGui::RadioButton("PNG", settings.format == ExportFormat::Png)) {
                    settings.format = ExportFormat::Png;
                    previewChanged = true;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("BMP", settings.format == ExportFormat::Bmp)) {
                    settings.format = ExportFormat::Bmp;
                    previewChanged = true;
                }
                if (settings.format == ExportFormat::Bmp &&
                    settings.backgroundMode == ExportBackgroundMode::Transparent) {
                    ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.22f, 1.0f),
                                       "BMP flattens transparency in many viewers. Use PNG for alpha.");
                }

                ImGui::Spacing();
                ImGui::Checkbox("Open Image After Save", &settings.openAfterSave);
                ImGui::Checkbox("Show In Folder After Save", &settings.showInFolderAfterSave);
                ImGui::Checkbox("Copy Path After Save", &settings.copyPathAfterSave);
                ImGui::BeginDisabled();
                ImGui::Checkbox("Write Metadata Sidecar JSON", &settings.saveMetadataSidecar);
                ImGui::EndDisabled();
                ImGui::TextDisabled("Metadata sidecar is planned.");

                ImGui::Spacing();
                if (!m_lastExportSavedPath.empty()) {
                    ImGui::TextWrapped("Last saved: %s", narrowUtf8(m_lastExportSavedPath).c_str());
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
        ImGui::InvisibleButton("##ExportDialogSplitter", ImVec2(splitterWidth, bodyHeight));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (ImGui::IsItemActive()) {
            m_exportSettingsPaneWidth = std::clamp(
                m_exportSettingsPaneWidth + ImGui::GetIO().MouseDelta.x,
                minSettingsWidth, maxSettingsWidth);
            m_redrawRequested = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            m_exportSettingsPaneWidth = std::clamp(bodyWidth * 0.42f, minSettingsWidth, maxSettingsWidth);
            m_redrawRequested = true;
        }
        {
            const ImVec2 splitterMin = ImGui::GetItemRectMin();
            const ImVec2 splitterMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2((splitterMin.x + splitterMax.x) * 0.5f, splitterMin.y + 4.0f),
                ImVec2((splitterMin.x + splitterMax.x) * 0.5f, splitterMax.y - 4.0f),
                IM_COL32(120, 120, 128, ImGui::IsItemHovered() ? 220 : 140),
                ImGui::IsItemActive() ? 2.0f : 1.0f);
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
        bool autoRefresh = settings.autoRefreshPreview;
        if (ImGui::Checkbox("Auto Refresh", &autoRefresh)) {
            settings.autoRefreshPreview = autoRefresh;
            if (settings.autoRefreshPreview && m_exportPreviewDirty) {
                m_exportPreviewLastChanged = std::chrono::steady_clock::now();
            }
            m_redrawRequested = true;
        }
        const ExportPreviewQuality previewQualityOptions[] = {
            ExportPreviewQuality::Draft,
            ExportPreviewQuality::Normal
        };
        if (ImGui::BeginCombo("Preview Quality",
                              exportPreviewQualityLabel(settings.previewQuality))) {
            for (ExportPreviewQuality quality : previewQualityOptions) {
                const bool selected = (settings.previewQuality == quality);
                if (ImGui::Selectable(exportPreviewQualityLabel(quality), selected)) {
                    settings.previewQuality = quality;
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

        const ExportSupersampling effectiveSampling =
            settings.qualityMode == ExportQualityMode::Override
                ? settings.quality.supersampling
                : ExportSupersampling::Off;
        std::string samplingText = exportSupersamplingLabel(effectiveSampling);
        const int requestedSamplingFactor = supersamplingFactor(effectiveSampling);
        const int actualSamplingFactor =
            effectiveSupersamplingFactor(settings.width, settings.height, effectiveSampling);
        if (actualSamplingFactor != requestedSamplingFactor) {
            samplingText += " (effective ";
            samplingText += std::to_string(actualSamplingFactor);
            samplingText += "x)";
        }
        ImGui::Text("Output %d x %d  |  %s  |  %s",
                    settings.width, settings.height,
                    exportFormatLabel(settings.format),
                    samplingText.c_str());
        ImGui::TextDisabled("%s", exportAspectModeLabel(settings.aspectMode));

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
        const ExportSupersampling footerSampling =
            settings.qualityMode == ExportQualityMode::Override
                ? settings.quality.supersampling
                : ExportSupersampling::Off;
        const double footerMiB = bytesToMiB(
            estimateRgbaBufferBytes(settings.width, settings.height, footerSampling));
        const char* footerPreviewState = m_exportPreviewRefreshRequested ? "Preview rendering"
            : (m_exportPreviewDirty ? "Preview out of date"
                                    : (m_exportPreviewSrv ? "Preview ready" : "Preview not rendered"));
        ImGui::Text("%s | %d x %d | %s | %s | %.1f MiB | %s",
                    footerPreviewState,
                    settings.width,
                    settings.height,
                    exportFormatLabel(settings.format),
                    exportSupersamplingLabel(footerSampling),
                    footerMiB,
                    exportAspectModeLabel(settings.aspectMode));
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
        : m_exportDialogSettings.previewQuality;
    m_exportPreviewUseFinalQualityOnce = false;
    const ExportPreviewSize previewSize =
        resolveExportPreviewSize(m_exportDialogSettings.width,
                                 m_exportDialogSettings.height,
                                 requestedPreviewQuality);
    const int previewW = previewSize.width;
    const int previewH = previewSize.height;

    ExportDialogSettings previewSettings = m_exportDialogSettings;
    previewSettings.width = previewW;
    previewSettings.height = previewH;
    if (requestedPreviewQuality != ExportPreviewQuality::Final) {
        previewSettings.qualityMode = ExportQualityMode::Override;
        previewSettings.quality = qualitySettingsForPreset(
            requestedPreviewQuality == ExportPreviewQuality::Draft
                ? ExportQualityPreset::Draft
                : ExportQualityPreset::Normal);
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
    if (previewSettings.grayscaleOutput) {
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
        exportAspectModeLabel(m_exportDialogSettings.aspectMode);
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

    const int targetWidth = std::clamp(m_pendingExportSettings.width, 16, 8192);
    const int targetHeight = std::clamp(m_pendingExportSettings.height, 16, 8192);
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

    if (m_pendingExportSettings.grayscaleOutput) {
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
    const bool preferBmp = (m_pendingExportSettings.format == ExportFormat::Bmp);
    wcsncpy_s(fileName.data(), fileName.size(),
              preferBmp ? L"xpressformula-plot.bmp" : L"xpressformula-plot.png",
              _TRUNCATE);

    wchar_t filter[] =
        L"PNG Image (*.png)\0*.png\0"
        L"Bitmap Image (*.bmp)\0*.bmp\0\0";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = preferBmp ? 2 : 1;
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = preferBmp ? L"bmp" : L"png";

    if (!::GetSaveFileNameW(&ofn)) {
        return false;
    }

    path = fileName.data();
    if (std::filesystem::path(path).extension().empty()) {
        path += (ofn.nFilterIndex == 2) ? L".bmp" : L".png";
    }
    return true;
}

std::array<float, 4> Application::resolveExportBackgroundColor(
    const ExportDialogSettings& settings) const {
    switch (settings.backgroundMode) {
        case ExportBackgroundMode::Transparent:
            return { 0.0f, 0.0f, 0.0f, 0.0f };
        case ExportBackgroundMode::White:
            return { 1.0f, 1.0f, 1.0f, 1.0f };
        case ExportBackgroundMode::Black:
            return { 0.0f, 0.0f, 0.0f, 1.0f };
        case ExportBackgroundMode::Custom:
            return settings.backgroundColor;
        case ExportBackgroundMode::Current:
        default:
            return { 0.098f, 0.098f, 0.118f, 1.0f };
    }
}

void Application::markExportPreviewOutOfDate() {
    m_exportPreviewDirty = true;
    m_exportPreviewLastChanged = std::chrono::steady_clock::now();
    m_exportPreviewStatus = m_exportDialogSettings.autoRefreshPreview
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

    ID3D11Texture2D* stagingTexture = nullptr;
    if (FAILED(m_device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture)) || !stagingTexture) {
        return false;
    }

    m_deviceContext->CopyResource(stagingTexture, sourceTexture);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT mapResult = m_deviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(mapResult)) {
        stagingTexture->Release();
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

    m_deviceContext->Unmap(stagingTexture, 0);
    stagingTexture->Release();
    return true;
}

bool Application::renderPlotPixelsOffscreen(const Application::ExportDialogSettings& settings,
                                            std::vector<std::uint8_t>& pixels,
                                            int& width, int& height) {
    width = 0;
    height = 0;
    pixels.clear();

    if (!m_device || !m_deviceContext) {
        return false;
    }

    const int outputWidth = clampExportDimension(settings.width);
    const int outputHeight = clampExportDimension(settings.height);
    const int samplingFactor = settings.qualityMode == ExportQualityMode::Override
        ? effectiveSupersamplingFactor(outputWidth, outputHeight, settings.quality.supersampling)
        : 1;
    const int targetWidth = clampExportDimension(outputWidth * samplingFactor);
    const int targetHeight = clampExportDimension(outputHeight * samplingFactor);
    const ExportWorldBounds sourceBounds{
        m_viewTransform.worldXMin(), m_viewTransform.worldXMax(),
        m_viewTransform.worldYMin(), m_viewTransform.worldYMax()
    };
    const ExportResolvedView resolvedView =
        resolveExportView(targetWidth, targetHeight, sourceBounds, settings.aspectMode);

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = static_cast<UINT>(targetWidth);
    texDesc.Height = static_cast<UINT>(targetHeight);
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    ID3D11Texture2D* renderTexture = nullptr;
    ID3D11RenderTargetView* exportRTV = nullptr;
    if (FAILED(m_device->CreateTexture2D(&texDesc, nullptr, &renderTexture)) || !renderTexture) {
        return false;
    }
    if (FAILED(m_device->CreateRenderTargetView(renderTexture, nullptr, &exportRTV)) || !exportRTV) {
        renderTexture->Release();
        return false;
    }

    ID3D11RenderTargetView* previousRTV = nullptr;
    ID3D11DepthStencilView* previousDSV = nullptr;
    m_deviceContext->OMGetRenderTargets(1, &previousRTV, &previousDSV);

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

    m_deviceContext->OMSetRenderTargets(1, &exportRTV, nullptr);
    m_deviceContext->RSSetViewports(1, &exportViewport);
    const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_deviceContext->ClearRenderTargetView(exportRTV, clear);

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
        if (settings.qualityMode == ExportQualityMode::Override) {
            exportSettings.optimizeRendering = false;
            exportSettings.surfaceResolution = std::clamp(settings.quality.surfaceResolution, 16, 256);
            exportSettings.implicitSurfaceResolution =
                std::clamp(settings.quality.implicitSurfaceResolution, 16, 192);
            exportSettings.wireThickness = (std::max)(
                0.05f, exportSettings.wireThickness * settings.quality.wireThicknessScale);
        }

        exportView.centerX = (resolvedView.visibleBounds.xMin + resolvedView.visibleBounds.xMax) * 0.5;
        exportView.centerY = (resolvedView.visibleBounds.yMin + resolvedView.visibleBounds.yMax) * 0.5;
        exportView.scaleX = resolvedView.scaleX;
        exportView.scaleY = resolvedView.scaleY;

        PlotRenderOverrides exportOverrides;
        exportOverrides.active = true;
        exportOverrides.showGrid = settings.showGrid;
        exportOverrides.showCoordinates = settings.showCoordinates;
        exportOverrides.showWires = settings.showWires;
        exportOverrides.showEnvelope = settings.showEnvelope;
        exportOverrides.showAxisTriad = settings.showAxisTriad;
        exportOverrides.showCanvasBorder = false;
        exportOverrides.backgroundColor = backgroundColor;

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
            m_plotPanel.render(m_formulas, exportView, exportSettings, &exportOverrides);
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

    const bool readOk = readTexturePixelsRgba(renderTexture, pixels, width, height);

    if (prevViewportCount > 0) {
        m_deviceContext->RSSetViewports(1, &prevViewport);
    }
    m_deviceContext->OMSetRenderTargets(1, &previousRTV, previousDSV);

    if (previousDSV) previousDSV->Release();
    if (previousRTV) previousRTV->Release();
    if (exportRTV) exportRTV->Release();
    if (renderTexture) renderTexture->Release();
    return readOk;
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

void Application::openLastSavedExport() {
    if (m_lastExportSavedPath.empty()) {
        m_exportStatus = "No saved export path is available.";
    } else if (openPathWithShell(m_lastExportSavedPath)) {
        m_exportStatus = "Opened saved image.";
    } else {
        m_exportStatus = "Could not open saved image.";
    }
    m_redrawRequested = true;
}

void Application::showLastSavedExportInFolder() {
    if (m_lastExportSavedPath.empty()) {
        m_exportStatus = "No saved export path is available.";
    } else if (revealPathInExplorer(m_lastExportSavedPath)) {
        m_exportStatus = "Opened saved image location.";
    } else {
        m_exportStatus = "Could not show saved image in folder.";
    }
    m_redrawRequested = true;
}

void Application::copyLastSavedExportPath() {
    if (m_lastExportSavedPath.empty()) {
        m_exportStatus = "No saved export path is available.";
    } else if (copyUnicodeTextToClipboard(m_hWnd, m_lastExportSavedPath)) {
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
        std::wstring path;
        if (promptSaveImagePath(path)) {
            std::string error;
            if (saveImageToPath(path, outputPixels, outputWidth, outputHeight, error)) {
                m_lastExportSavedPath = path;
                messages.emplace_back("Saved plot image to: " + narrowUtf8(path));
                if (m_pendingExportSettings.openAfterSave) {
                    messages.emplace_back(openPathWithShell(path)
                        ? "Opened saved image."
                        : "Could not open saved image.");
                }
                if (m_pendingExportSettings.showInFolderAfterSave) {
                    messages.emplace_back(revealPathInExplorer(path)
                        ? "Opened saved image location."
                        : "Could not show saved image in folder.");
                }
                if (m_pendingExportSettings.copyPathAfterSave) {
                    messages.emplace_back(copyUnicodeTextToClipboard(m_hWnd, path)
                        ? "Copied saved image path."
                        : "Could not copy saved image path.");
                }
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
