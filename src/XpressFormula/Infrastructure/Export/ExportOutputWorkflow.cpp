// SPDX-License-Identifier: MIT
// ExportOutputWorkflow.cpp - Testable save/copy/post-save export output actions.
#include "ExportOutputWorkflow.h"

#include <sstream>
#include <utility>

namespace XpressFormula::Infrastructure::Export {

ExportOutputResult saveRenderedImage(
    const ExportSettings& settings,
    const std::wstring& path,
    std::span<const std::uint8_t> pixels,
    int width,
    int height,
    IExportImageEncoder& encoder,
    IExportMetadataWriter& metadataWriter,
    IExportShell& shell,
    IExportClipboard& clipboard,
    std::string pathUtf8,
    std::string sidecarPathUtf8Fallback) {
    ExportOutputResult result;

    const ExportOperationResult imageResult = encoder.saveImageBgra(path, pixels, width, height);
    if (!imageResult) {
        result.messages.emplace_back("Save failed: " + imageResult.error);
        return result;
    }

    result.savedImage = true;
    result.savedPath = path;
    result.messages.emplace_back("Saved plot image to: " + std::move(pathUtf8));

    if (settings.output.saveMetadataSidecar) {
        std::wstring sidecarPath;
        const ExportOperationResult metadataResult =
            metadataWriter.writeSidecar(settings, path, width, height, sidecarPath);
        if (metadataResult) {
            std::string sidecarText = std::move(sidecarPathUtf8Fallback);
            if (sidecarText.empty()) {
                sidecarText = "metadata sidecar";
            }
            result.messages.emplace_back("Saved metadata sidecar: " + sidecarText);
        } else {
            result.messages.emplace_back("Metadata sidecar failed: " + metadataResult.error);
        }
    }

    if (settings.output.openAfterSave) {
        result.messages.emplace_back(shell.openPath(path)
            ? "Opened saved image."
            : "Could not open saved image.");
    }
    if (settings.output.showInFolderAfterSave) {
        result.messages.emplace_back(shell.revealPath(path)
            ? "Opened saved image location."
            : "Could not show saved image in folder.");
    }
    if (settings.output.copyPathAfterSave) {
        result.messages.emplace_back(clipboard.copyText(path)
            ? "Copied saved image path."
            : "Could not copy saved image path.");
    }

    return result;
}

ExportOutputResult copyRenderedImage(std::span<const std::uint8_t> pixels,
                                     int width,
                                     int height,
                                     IExportClipboard& clipboard) {
    ExportOutputResult result;
    const ExportOperationResult copyResult = clipboard.copyImageBgra(pixels, width, height);
    if (copyResult) {
        result.copiedImage = true;
        result.messages.emplace_back("Copied plot image to clipboard.");
    } else {
        result.messages.emplace_back("Clipboard copy failed: " + copyResult.error);
    }
    return result;
}

void appendMessages(std::vector<std::string>& destination,
                    const std::vector<std::string>& source) {
    destination.insert(destination.end(), source.begin(), source.end());
}

std::string joinExportMessages(const std::vector<std::string>& messages) {
    std::ostringstream oss;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) {
            oss << " | ";
        }
        oss << messages[i];
    }
    return oss.str();
}

} // namespace XpressFormula::Infrastructure::Export
