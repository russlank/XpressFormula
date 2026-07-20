// SPDX-License-Identifier: MIT
// FileDialogService.h - Win32 common-dialog service and pure dialog planning.
#pragma once

#include <string>
#include <string_view>
#include <vector>

struct HWND__;
using HWND = HWND__*;

namespace XpressFormula::Platform::Windows {

enum class DialogStatus {
    Selected,
    Cancelled,
    Failed
};

struct DialogResult {
    DialogStatus status = DialogStatus::Cancelled;
    std::wstring path;
    std::string error;

    [[nodiscard]] bool selected() const noexcept {
        return status == DialogStatus::Selected;
    }

    [[nodiscard]] bool cancelled() const noexcept {
        return status == DialogStatus::Cancelled;
    }
};

struct DialogFilter {
    std::wstring label;
    std::wstring pattern;
};

struct FileDialogPlan {
    std::wstring initialFileName;
    std::wstring defaultExtension;
    std::vector<DialogFilter> filters;
    unsigned int selectedFilterIndex = 1;
    bool fileMustExist = false;
    bool pathMustExist = true;
    bool overwritePrompt = false;
};

enum class ExportImageFormat {
    Png,
    Bmp
};

[[nodiscard]] std::wstring makeWin32FilterString(const std::vector<DialogFilter>& filters);
[[nodiscard]] FileDialogPlan planOpenProjectDialog();
[[nodiscard]] FileDialogPlan planSaveProjectDialog(std::wstring_view currentPath);
[[nodiscard]] FileDialogPlan planSaveImageDialog(ExportImageFormat preferredFormat);
[[nodiscard]] std::wstring appendExtensionIfMissing(std::wstring path,
                                                    std::wstring_view extensionWithoutDot);
[[nodiscard]] std::wstring applyImageDialogExtension(std::wstring path,
                                                     unsigned int selectedFilterIndex);

class FileDialogService {
public:
    [[nodiscard]] DialogResult openProject(HWND owner) const;
    [[nodiscard]] DialogResult saveProject(HWND owner, std::wstring_view currentPath) const;
    [[nodiscard]] DialogResult saveImage(HWND owner, ExportImageFormat preferredFormat) const;
};

} // namespace XpressFormula::Platform::Windows

