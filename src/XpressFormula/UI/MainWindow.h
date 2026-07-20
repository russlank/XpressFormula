// MainWindow.h - ImGui composition for the primary workspace.
#pragma once

#include "ControlPanel.h"
#include "FormulaPanel.h"
#include "PlotPanel.h"
#include "Components/ExportDialog.h"
#include "Components/PlotToolbar.h"
#include "Components/ProjectControls.h"
#include "../Model/Document.h"
#include "../Model/SceneSummary.h"

#include <string_view>

namespace XpressFormula::UI {

struct MainWindowBuildMetadata {
    std::string_view version;
    std::string_view repo;
    std::string_view branch;
    std::string_view commit;
};

struct MainWindowUpdateNotification {
    bool checkInProgress = false;
    bool updateAvailable = false;
    bool noticeDismissed = false;
    bool versionDetailsExpanded = false;
    std::string_view latestTag;
    std::string_view releaseUrl;
    std::string_view status;
};

struct MainWindowContext {
    Model::Document& document;
    Model::SceneSummary& sceneSummary;
    Components::ProjectControlsContext projectControls;
    MainWindowUpdateNotification updateNotification;
    MainWindowBuildMetadata buildMetadata;
    std::string_view exportStatus;
    bool exportDialogOpen = false;
    bool openUnsavedProjectDialog = false;
    const PlotRenderOverrides* exportOverrides = nullptr;
};

struct MainWindowActions {
    Components::ProjectControlsAction projectControls;
    Components::UnsavedProjectDialogChoice unsavedProjectChoice =
        Components::UnsavedProjectDialogChoice::None;
    bool documentChanged = false;
    bool requestOpenExportDialog = false;
    bool requestManualUpdateCheck = false;
    bool requestOpenReleasePage = false;
    bool requestOpenSupportPage = false;
    bool requestDismissUpdateNotice = false;
    bool updateDetailsExpandedChanged = false;
    bool updateDetailsExpanded = false;
    bool redrawRequested = false;
};

struct ExportDialogWindowContext {
    bool dialogOpen = false;
    bool popupOpenNextFrame = false;
    bool centerOnOpen = false;
    Components::ExportDialogContext& content;
};

struct ExportDialogWindowActions {
    Components::ExportDialogAction dialog;
    bool clearPopupOpenNextFrame = false;
    bool clearCenterOnOpen = false;
    bool closeDialog = false;
    bool setDialogOpen = false;
    bool dialogOpen = false;
};

class MainWindow {
public:
    void resetFormulaColorCycle(int nextIndex = 0);

    [[nodiscard]] MainWindowActions renderWorkspace(MainWindowContext& context,
                                                    float& sidebarWidth);
    [[nodiscard]] ExportDialogWindowActions renderExportDialog(
        ExportDialogWindowContext& context);

private:
    void refreshSceneSummary(Model::Document& document, Model::SceneSummary& sceneSummary);
    void captureDocumentChange(Model::Document& document,
                               Model::SceneSummary& sceneSummary,
                               Model::Document::Revision beforeRevision,
                               MainWindowActions& actions);
    void renderUpdateNotification(const MainWindowUpdateNotification& update,
                                  const MainWindowBuildMetadata& buildMetadata,
                                  MainWindowActions& actions);
    void handlePlotShortcuts(bool exportDialogOpen,
                             Model::Document& document,
                             MainWindowActions& actions);
    void renderPlotToolbar(Model::Document& document,
                           const Model::SceneSummary& scene,
                           MainWindowActions& actions);
    void fitDefaultView(Model::Document& document, MainWindowActions& actions);
    void resetViewAndCamera(Model::Document& document, MainWindowActions& actions);
    void applyCameraPreset(Model::Document& document,
                           float azimuthDeg,
                           float elevationDeg,
                           MainWindowActions& actions);

    FormulaPanel m_formulaPanel;
    ControlPanel m_controlPanel;
    PlotPanel m_plotPanel;
};

} // namespace XpressFormula::UI
