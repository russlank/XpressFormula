// SPDX-License-Identifier: MIT
// ExportOutputAdapters.cpp - Application-layer export output adapter implementation.
#include "ExportOutputAdapters.h"

#include "../Infrastructure/FileSystem/AtomicFileWriter.h"
#include "../Platform/Windows/Utf.h"

#include <filesystem>
#include <utility>

namespace XpressFormula::Application {

namespace XFExport = Infrastructure::Export;
namespace XFFileSystem = Infrastructure::FileSystem;
namespace XFWin = Platform::Windows;

ExportImageEncoderAdapter::ExportImageEncoderAdapter(
    const XFWin::WicImageEncoder& encoder) noexcept
    : m_encoder(encoder) {
}

XFExport::ExportOperationResult ExportImageEncoderAdapter::saveImageBgra(
    const std::wstring& path,
    std::span<const std::uint8_t> pixels,
    int width,
    int height) {
    const XFWin::ImageEncodeResult result =
        m_encoder.saveByExtensionBgra(std::filesystem::path(path), pixels, width, height);
    return { static_cast<bool>(result), result.error };
}

ExportClipboardAdapter::ExportClipboardAdapter(const XFWin::ClipboardService& clipboard,
                                               HWND owner) noexcept
    : m_clipboard(clipboard),
      m_owner(owner) {
}

XFExport::ExportOperationResult ExportClipboardAdapter::copyImageBgra(
    std::span<const std::uint8_t> pixels,
    int width,
    int height) {
    const XFWin::ClipboardResult result =
        m_clipboard.copyDibImageBgra(m_owner, pixels, width, height);
    return { static_cast<bool>(result), result.error };
}

XFExport::ExportOperationResult ExportClipboardAdapter::copyText(
    const std::wstring& text) {
    const XFWin::ClipboardResult result = m_clipboard.copyUtf16Text(m_owner, text);
    return { static_cast<bool>(result), result.error };
}

ExportShellAdapter::ExportShellAdapter(const XFWin::ShellService& shell) noexcept
    : m_shell(shell) {
}

bool ExportShellAdapter::openPath(const std::wstring& path) {
    return m_shell.openPath(path);
}

bool ExportShellAdapter::revealPath(const std::wstring& path) {
    return m_shell.revealPath(path);
}

ExportMetadataSidecarWriter::ExportMetadataSidecarWriter(
    ExportMetadataContext context) noexcept
    : m_context(std::move(context)) {
}

XFExport::ExportOperationResult ExportMetadataSidecarWriter::writeSidecar(
    const XFExport::ExportSettings& settings,
    const std::wstring& imagePath,
    int width,
    int height,
    std::wstring& sidecarPath) {
    if (!m_context.formulas || !m_context.view || !m_context.plot) {
        return { false, "Metadata context is incomplete." };
    }

    const std::filesystem::path imageFilePath(imagePath);
    const std::filesystem::path sidecarFilePath =
        XFExport::exportMetadataSidecarPath(imageFilePath);
    sidecarPath = sidecarFilePath.wstring();

    const XFExport::ExportMetadataModel metadata = XFExport::makeExportMetadataModel(
        settings,
        XFWin::utf16ToUtf8OrEmpty(imagePath),
        imageFilePath,
        width,
        height,
        m_context.application,
        *m_context.formulas,
        *m_context.view,
        *m_context.plot);
    const std::string json = XFExport::serializeExportMetadata(metadata);
    const auto writeResult = XFFileSystem::writeTextAtomically(sidecarFilePath, json);
    if (!writeResult) {
        return { false, "Could not write metadata sidecar: " + writeResult.error };
    }
    return { true, {} };
}

} // namespace XpressFormula::Application
