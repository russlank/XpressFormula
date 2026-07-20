// SPDX-License-Identifier: MIT
// ExportOutputWorkflow.h - Testable save/copy/post-save export output actions.
#pragma once

#include "ExportSettings.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace XpressFormula::Infrastructure::Export {

struct ExportOperationResult {
    bool success = false;
    std::string error;

    explicit operator bool() const noexcept {
        return success;
    }
};

class IExportImageEncoder {
public:
    virtual ~IExportImageEncoder() = default;

    [[nodiscard]] virtual ExportOperationResult saveImageBgra(
        const std::wstring& path,
        std::span<const std::uint8_t> pixels,
        int width,
        int height) = 0;
};

class IExportClipboard {
public:
    virtual ~IExportClipboard() = default;

    [[nodiscard]] virtual ExportOperationResult copyImageBgra(
        std::span<const std::uint8_t> pixels,
        int width,
        int height) = 0;
    [[nodiscard]] virtual ExportOperationResult copyText(const std::wstring& text) = 0;
};

class IExportShell {
public:
    virtual ~IExportShell() = default;

    [[nodiscard]] virtual bool openPath(const std::wstring& path) = 0;
    [[nodiscard]] virtual bool revealPath(const std::wstring& path) = 0;
};

class IExportMetadataWriter {
public:
    virtual ~IExportMetadataWriter() = default;

    [[nodiscard]] virtual ExportOperationResult writeSidecar(
        const ExportSettings& settings,
        const std::wstring& imagePath,
        int width,
        int height,
        std::wstring& sidecarPath) = 0;
};

struct ExportOutputResult {
    bool savedImage = false;
    bool copiedImage = false;
    std::wstring savedPath;
    std::vector<std::string> messages;
};

[[nodiscard]] ExportOutputResult saveRenderedImage(
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
    std::string sidecarPathUtf8Fallback = {});

[[nodiscard]] ExportOutputResult copyRenderedImage(
    std::span<const std::uint8_t> pixels,
    int width,
    int height,
    IExportClipboard& clipboard);

void appendMessages(std::vector<std::string>& destination,
                    const std::vector<std::string>& source);

[[nodiscard]] std::string joinExportMessages(const std::vector<std::string>& messages);

} // namespace XpressFormula::Infrastructure::Export
