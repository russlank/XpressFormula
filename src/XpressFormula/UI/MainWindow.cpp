// MainWindow.cpp - ImGui composition for the primary workspace.
#include "MainWindow.h"

#include "PlotSettings.h"
#include "UiKit/Splitter.h"
#include "UiKit/UiMetrics.h"

#include "imgui.h"

#include <algorithm>
#include <span>
#include <string>
#include <variant>

namespace XpressFormula::UI {

namespace {

constexpr const char* kExportDialogPopupId = "Export Plot Settings";

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

bool applyFormulaPanelCommand(Model::Document& document,
                              const FormulaPanelCommand& command) {
    return std::visit(Overloaded{
        [&document](const AddFormulaCommand& add) {
            return document.addFormula(add.formula) != 0;
        },
        [&document](const UpdateFormulaCommand& update) {
            return document.updateFormula(update.formulaId, update.formula);
        },
        [&document](const RemoveFormulaCommand& remove) {
            return document.removeFormula(remove.formulaId);
        },
        [&document](const DuplicateFormulaCommand& duplicate) {
            return document.duplicateFormula(duplicate.formulaId);
        },
        [&document](const MoveFormulaCommand& move) {
            return document.moveFormula(move.formulaId, move.toIndex);
        },
        [&document](const SetFormulaVisibilityCommand& visibility) {
            return document.setFormulaVisibility(visibility.formulaId, visibility.visible);
        },
        [&document](const SetFormulaColorCommand& color) {
            return document.setFormulaColor(color.formulaId, color.color);
        },
        [&document](const SetFormulaZSliceCommand& zSlice) {
            return document.setFormulaZSlice(zSlice.formulaId, zSlice.zSlice);
        },
        [&document](const HideOtherFormulasCommand& hideOthers) {
            return document.hideOtherFormulas(hideOthers.formulaId);
        }
    }, command);
}

bool shouldResetAutoRotationRuntime(const PlotSettings& before,
                                    const PlotSettings& after) noexcept {
    return before.azimuthDeg != after.azimuthDeg ||
           before.elevationDeg != after.elevationDeg ||
           before.zScale != after.zScale ||
           (before.autoRotate && !after.autoRotate);
}

} // namespace

void MainWindow::resetFormulaColorCycle(int nextIndex) {
    m_formulaPanel.resetColorCycle(nextIndex);
}

MainWindowActions MainWindow::renderWorkspace(MainWindowContext& context,
                                              float& sidebarWidth) {
    MainWindowActions actions;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float totalW = viewport->WorkSize.x;
    const float totalH = viewport->WorkSize.y;
    const UiKit::UiMetrics& uiMetrics = UiKit::metrics();
    const float splitterWidth = uiMetrics.splitterWidth;
    const float availableForSidebar = totalW - splitterWidth - uiMetrics.minimumPlotWidth;
    const float dynamicMinSidebar =
        (std::min)(uiMetrics.minimumSidebarWidth, (std::max)(180.0f, totalW * 0.38f));
    const float maxSidebar = (std::min)(uiMetrics.maximumSidebarWidth,
        (std::max)(dynamicMinSidebar, availableForSidebar));
    sidebarWidth = std::clamp(sidebarWidth, dynamicMinSidebar, maxSidebar);
    const float sidebar = sidebarWidth;
    const float plotX = viewport->WorkPos.x + sidebar + splitterWidth;
    const float plotWidth = (std::max)(1.0f, totalW - sidebar - splitterWidth);

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(sidebar, totalH));
    ImGui::Begin("##Sidebar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse);
    actions.projectControls = Components::renderProjectControls(context.projectControls);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    {
        const Model::Document::Revision beforeRevision = context.document.revision();
        const FormulaPanelActions formulaActions = m_formulaPanel.render(
            std::span<const Model::Formula>(
                context.document.formulas().data(),
                context.document.formulas().size()));
        for (const FormulaPanelCommand& command : formulaActions.commands) {
            (void)applyFormulaPanelCommand(context.document, command);
        }
        captureDocumentChange(context.document, context.sceneSummary, beforeRevision, actions);
    }

    ImGui::Spacing();
    ImGui::Spacing();
    {
        const Model::Document::Revision beforeRevision = context.document.revision();
        const PlotSettings beforePlot = context.document.plotSettings();
        ControlPanelActions panelActions;
        {
            auto view = context.document.editViewTransform();
            auto plot = context.document.editPlotSettings();
            panelActions = m_controlPanel.render(
                view.get(), plot.get(), context.sceneSummary, std::string(context.exportStatus));
        }
        if (panelActions.requestOpenExportDialog) {
            actions.requestOpenExportDialog = true;
            actions.redrawRequested = true;
        }
        if (shouldResetAutoRotationRuntime(beforePlot, context.document.plotSettings())) {
            actions.resetAutoRotationRuntime = true;
        }
        captureDocumentChange(context.document, context.sceneSummary, beforeRevision, actions);
    }

    ImGui::Spacing();
    ImGui::Separator();
    renderUpdateNotification(context.updateNotification, context.buildMetadata, actions);
    ImGui::End();

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
                                    sidebarWidth,
                                    dynamicMinSidebar,
                                    maxSidebar,
                                    uiMetrics.defaultSidebarWidth,
                                    splitterWidth)) {
        actions.redrawRequested = true;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    actions.unsavedProjectChoice =
        Components::renderUnsavedProjectDialog(context.openUnsavedProjectDialog);

    ImGui::SetNextWindowPos(ImVec2(plotX, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(plotWidth, totalH));
    ImGui::Begin("##Plot", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoScrollbar);
    handlePlotShortcuts(context.exportDialogOpen, context.document, actions);
    renderPlotToolbar(context.document, context.sceneSummary, actions);
    {
        const Model::Document::Revision beforeRevision = context.document.revision();
        {
            auto view = context.document.editViewTransform();
            auto plot = context.document.editPlotSettings();
            m_plotPanel.render(context.document.formulas(),
                               view.get(),
                               plot.get(),
                               context.sceneSummary,
                               context.exportOverrides,
                               nullptr,
                               context.runtimeAzimuthDeg);
        }
        captureDocumentChange(context.document, context.sceneSummary, beforeRevision, actions);
    }
    ImGui::End();

    return actions;
}

ExportDialogWindowActions MainWindow::renderExportDialog(ExportDialogWindowContext& context) {
    ExportDialogWindowActions actions;
    if (!context.dialogOpen) {
        return actions;
    }

    if (context.popupOpenNextFrame) {
        ImGui::OpenPopup(kExportDialogPopupId);
        actions.clearPopupOpenNextFrame = true;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float maxWidth = (std::max)(420.0f, viewport->WorkSize.x - 32.0f);
    const float maxHeight = (std::max)(420.0f, viewport->WorkSize.y - 32.0f);
    const float desiredWidth = (std::min)(maxWidth, (std::max)(760.0f, viewport->WorkSize.x * 0.84f));
    const float desiredHeight = (std::min)(maxHeight, (std::max)(560.0f, viewport->WorkSize.y * 0.84f));
    const ImVec2 defaultDialogSize(desiredWidth, desiredHeight);
    if (context.centerOnOpen) {
        const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                            viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(defaultDialogSize, ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize(defaultDialogSize, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSizeConstraints(ImVec2((std::min)(640.0f, maxWidth),
                                               (std::min)(440.0f, maxHeight)),
                                        ImVec2(maxWidth, maxHeight));

    bool open = context.dialogOpen;
    if (ImGui::BeginPopupModal(kExportDialogPopupId, &open, ImGuiWindowFlags_NoCollapse)) {
        actions.dialog = Components::renderExportDialogContent(context.content);
        if (actions.dialog.close) {
            ImGui::CloseCurrentPopup();
            open = false;
        }
        ImGui::EndPopup();
    }

    actions.clearCenterOnOpen = true;
    if (!ImGui::IsPopupOpen(kExportDialogPopupId)) {
        actions.closeDialog = true;
    } else {
        actions.setDialogOpen = true;
        actions.dialogOpen = open;
    }
    return actions;
}

void MainWindow::refreshSceneSummary(Model::Document& document,
                                     Model::SceneSummary& sceneSummary) {
    sceneSummary = Model::analyzeScene(
        std::span<const Model::Formula>(document.formulas().data(), document.formulas().size()));
}

void MainWindow::captureDocumentChange(Model::Document& document,
                                       Model::SceneSummary& sceneSummary,
                                       Model::Document::Revision beforeRevision,
                                       MainWindowActions& actions) {
    if (document.revision() == beforeRevision) {
        return;
    }

    refreshSceneSummary(document, sceneSummary);
    actions.documentChanged = true;
    actions.redrawRequested = true;
}

void MainWindow::renderUpdateNotification(const MainWindowUpdateNotification& update,
                                          const MainWindowBuildMetadata& buildMetadata,
                                          MainWindowActions& actions) {
    const bool showUpdateAlert = update.updateAvailable && !update.noticeDismissed;
    std::string versionDetailsLabel;
    if (showUpdateAlert && !update.latestTag.empty()) {
        versionDetailsLabel = "New version available ";
        versionDetailsLabel += update.latestTag;
    } else if (showUpdateAlert) {
        versionDetailsLabel = "New version available";
    } else if (update.checkInProgress) {
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
    ImGui::SetNextItemOpen(update.versionDetailsExpanded, ImGuiCond_Always);
    const bool expanded = ImGui::CollapsingHeader(
        versionDetailsLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
    if (expanded != update.versionDetailsExpanded) {
        actions.updateDetailsExpandedChanged = true;
        actions.updateDetailsExpanded = expanded;
        actions.redrawRequested = true;
    }
    if (showUpdateAlert) {
        ImGui::PopStyleColor(3);
    }

    if (!expanded) {
        return;
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Build Metadata");
    ImGui::TextDisabled("Version: %s", std::string(buildMetadata.version).c_str());
    ImGui::TextDisabled("Repo: %s", std::string(buildMetadata.repo).c_str());
    ImGui::TextDisabled("Branch: %s", std::string(buildMetadata.branch).c_str());
    ImGui::TextDisabled("Commit: %s", std::string(buildMetadata.commit).c_str());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Updates");
    if (update.checkInProgress) {
        ImGui::TextWrapped("Checking GitHub releases...");
    } else {
        if (showUpdateAlert) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 214, 110, 255));
            ImGui::TextWrapped("New version available: %s", std::string(update.latestTag).c_str());
            ImGui::PopStyleColor();
        } else if (!update.status.empty()) {
            ImGui::TextWrapped("%s", std::string(update.status).c_str());
        } else {
            ImGui::TextWrapped("Checks for newer releases on GitHub.");
        }
    }

    if (ImGui::Button("Check For Updates", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        actions.requestManualUpdateCheck = true;
        actions.redrawRequested = true;
    }
    if (ImGui::Button("Open Releases Page", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        actions.requestOpenReleasePage = true;
        actions.redrawRequested = true;
    }
    if (ImGui::Button("Buy Me a Coffee", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        actions.requestOpenSupportPage = true;
        actions.redrawRequested = true;
    }
    if (showUpdateAlert &&
        ImGui::Button("Dismiss Update Notice", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
        actions.requestDismissUpdateNotice = true;
        actions.redrawRequested = true;
    }
}

void MainWindow::handlePlotShortcuts(bool exportDialogOpen,
                                     Model::Document& document,
                                     MainWindowActions& actions) {
    ImGuiIO& io = ImGui::GetIO();
    const bool modifiersDown = io.KeyCtrl || io.KeyAlt || io.KeySuper;
    const bool canUsePlotShortcuts =
        !exportDialogOpen && !io.WantTextInput &&
        !ImGui::IsAnyItemActive() && !modifiersDown;
    if (!canUsePlotShortcuts) {
        return;
    }

    const Model::Document::Revision beforeRevision = document.revision();
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        fitDefaultView(document, actions);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
        resetViewAndCamera(document, actions);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) {
        auto plot = document.editPlotSettings();
        plot.get().showGrid = !plot.get().showGrid;
        actions.redrawRequested = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        auto plot = document.editPlotSettings();
        plot.get().showWires = !plot.get().showWires;
        actions.redrawRequested = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        actions.requestOpenExportDialog = true;
        actions.redrawRequested = true;
    }
    if (document.revision() != beforeRevision) {
        actions.documentChanged = true;
        actions.redrawRequested = true;
    }
}

void MainWindow::renderPlotToolbar(Model::Document& document,
                                   const Model::SceneSummary& scene,
                                   MainWindowActions& actions) {
    const Model::Document::Revision beforeRevision = document.revision();
    const PlotSettings beforePlot = document.plotSettings();
    Components::PlotToolbarActions toolbarActions;
    {
        auto plot = document.editPlotSettings();
        PlotSettings& plotSettings = plot.get();
        const XYRenderMode effectiveRenderMode = plotSettings.resolveXYRenderMode(scene);
        const bool is3DMode = (effectiveRenderMode == XYRenderMode::Surface3D);

        Components::PlotToolbarContext context;
        context.is3DMode = is3DMode;
        context.effectiveRenderMode = effectiveRenderMode;
        context.availableSize = ImGui::GetContentRegionAvail();

        toolbarActions = Components::renderPlotToolbar(plotSettings, context);
    }

    if (toolbarActions.requestFit) {
        fitDefaultView(document, actions);
    }
    if (toolbarActions.requestReset) {
        resetViewAndCamera(document, actions);
    }
    if (toolbarActions.requestExport) {
        actions.requestOpenExportDialog = true;
        actions.redrawRequested = true;
    }
    if (toolbarActions.applyCameraPreset) {
        applyCameraPreset(
            document,
            toolbarActions.cameraAzimuthDeg,
            toolbarActions.cameraElevationDeg,
            actions);
    }
    if (shouldResetAutoRotationRuntime(beforePlot, document.plotSettings())) {
        actions.resetAutoRotationRuntime = true;
    }
    if (toolbarActions.requestRedraw) {
        actions.redrawRequested = true;
    }
    if (document.revision() != beforeRevision) {
        actions.documentChanged = true;
        actions.redrawRequested = true;
    }
}

void MainWindow::fitDefaultView(Model::Document& document, MainWindowActions& actions) {
    constexpr double targetWorldSpan = 20.0;
    constexpr double marginScale = 0.94;
    auto view = document.editViewTransform();
    Core::ViewTransform& viewTransform = view.get();
    const double fitScaleX = (std::max)(1.0f, viewTransform.viewport.width) / targetWorldSpan;
    const double fitScaleY = (std::max)(1.0f, viewTransform.viewport.height) / targetWorldSpan;
    const double fitScale = (std::max)(0.1, (std::min)(fitScaleX, fitScaleY) * marginScale);

    viewTransform.state.centerX = 0.0;
    viewTransform.state.centerY = 0.0;
    viewTransform.state.scaleX = fitScale;
    viewTransform.state.scaleY = fitScale;
    actions.redrawRequested = true;
}

void MainWindow::resetViewAndCamera(Model::Document& document, MainWindowActions& actions) {
    auto view = document.editViewTransform();
    auto plot = document.editPlotSettings();
    view.get().reset();
    plot.get().azimuthDeg = kDefaultAzimuthDeg;
    plot.get().elevationDeg = kDefaultElevationDeg;
    plot.get().zScale = kDefaultZScale;
    plot.get().autoRotate = false;
    actions.resetAutoRotationRuntime = true;
    actions.redrawRequested = true;
}

void MainWindow::applyCameraPreset(Model::Document& document,
                                   float azimuthDeg,
                                   float elevationDeg,
                                   MainWindowActions& actions) {
    auto plot = document.editPlotSettings();
    plot.get().azimuthDeg = azimuthDeg;
    plot.get().elevationDeg = elevationDeg;
    plot.get().autoRotate = false;
    actions.resetAutoRotationRuntime = true;
    actions.redrawRequested = true;
}

} // namespace XpressFormula::UI
