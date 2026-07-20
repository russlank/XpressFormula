// SPDX-License-Identifier: MIT
// ExportDialog.h - ImGui export settings dialog component.
#pragma once

#include "../ExportSettings.h"

#include "imgui.h"

#include <string>

namespace XpressFormula::UI::Components {

struct ExportDialogAction {
    bool requestPreview = false;
    bool requestFinalPreview = false;
    bool requestSave = false;
    bool requestCopy = false;
    bool close = false;
    bool openSavedImage = false;
    bool showSavedImageInFolder = false;
    bool copySavedImagePath = false;
    bool settingsChanged = false;
    bool redrawRequested = false;
};

struct ExportDialogContext {
    ExportSettings& settings;
    float& settingsPaneWidth;
    float& previewZoom;
    float& previewPanX;
    float& previewPanY;
    bool& previewCheckerboard;
    std::string& previewStatus;
    std::string& status;

    int sourceWidth = 0;
    int sourceHeight = 0;
    ExportWorldBounds sourceBounds;
    ExportSceneSettings currentScene;

    bool previewDirty = false;
    bool previewRefreshRequested = false;
    bool hasPreviewTexture = false;
    ImTextureID previewTexture = 0;
    int previewWidth = 0;
    int previewHeight = 0;
    bool exportBusy = false;
    std::string lastSavedPathUtf8;
};

[[nodiscard]] ExportDialogAction renderExportDialogContent(ExportDialogContext& context);

} // namespace XpressFormula::UI::Components
