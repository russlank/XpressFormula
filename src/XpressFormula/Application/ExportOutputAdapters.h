// SPDX-License-Identifier: MIT
// ExportOutputAdapters.h - Application-layer adapters for export output workflows.
#pragma once

#include "../Infrastructure/Export/ExportMetadataSerializer.h"
#include "../Infrastructure/Export/ExportOutputWorkflow.h"
#include "../Platform/Windows/ClipboardService.h"
#include "../Platform/Windows/ShellService.h"
#include "../Platform/Windows/WicImageEncoder.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace XpressFormula::Application {

struct ExportMetadataContext {
    Infrastructure::Export::ExportAppMetadata application;
    const std::vector<Model::Formula>* formulas = nullptr;
    const Core::ViewTransform* view = nullptr;
    const Model::PlotSettings* plot = nullptr;
};

class ExportImageEncoderAdapter final
    : public Infrastructure::Export::IExportImageEncoder {
public:
    explicit ExportImageEncoderAdapter(
        const Platform::Windows::WicImageEncoder& encoder) noexcept;

    [[nodiscard]] Infrastructure::Export::ExportOperationResult saveImageBgra(
        const std::wstring& path,
        std::span<const std::uint8_t> pixels,
        int width,
        int height) override;

private:
    const Platform::Windows::WicImageEncoder& m_encoder;
};

class ExportClipboardAdapter final
    : public Infrastructure::Export::IExportClipboard {
public:
    ExportClipboardAdapter(const Platform::Windows::ClipboardService& clipboard,
                           HWND owner) noexcept;

    [[nodiscard]] Infrastructure::Export::ExportOperationResult copyImageBgra(
        std::span<const std::uint8_t> pixels,
        int width,
        int height) override;
    [[nodiscard]] Infrastructure::Export::ExportOperationResult copyText(
        const std::wstring& text) override;

private:
    const Platform::Windows::ClipboardService& m_clipboard;
    HWND m_owner = nullptr;
};

class ExportShellAdapter final
    : public Infrastructure::Export::IExportShell {
public:
    explicit ExportShellAdapter(const Platform::Windows::ShellService& shell) noexcept;

    [[nodiscard]] bool openPath(const std::wstring& path) override;
    [[nodiscard]] bool revealPath(const std::wstring& path) override;

private:
    const Platform::Windows::ShellService& m_shell;
};

class ExportMetadataSidecarWriter final
    : public Infrastructure::Export::IExportMetadataWriter {
public:
    explicit ExportMetadataSidecarWriter(ExportMetadataContext context) noexcept;

    [[nodiscard]] Infrastructure::Export::ExportOperationResult writeSidecar(
        const Infrastructure::Export::ExportSettings& settings,
        const std::wstring& imagePath,
        int width,
        int height,
        std::wstring& sidecarPath) override;

private:
    ExportMetadataContext m_context;
};

} // namespace XpressFormula::Application
