// SPDX-License-Identifier: MIT
// FileDialogService.cpp - Win32 common-dialog service and pure dialog planning.
#include "FileDialogService.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>

#include <array>
#include <cwchar>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>

#pragma comment(lib, "comdlg32.lib")

namespace XpressFormula::Platform::Windows {
namespace {

std::vector<DialogFilter> projectOpenFilters() {
    return {
        { L"XpressFormula Project (*.xfplot)", L"*.xfplot" },
        { L"JSON Files (*.json)", L"*.json" },
        { L"All Files (*.*)", L"*.*" }
    };
}

std::vector<DialogFilter> projectSaveFilters() {
    return {
        { L"XpressFormula Project (*.xfplot)", L"*.xfplot" },
        { L"JSON Files (*.json)", L"*.json" }
    };
}

std::vector<DialogFilter> imageSaveFilters() {
    return {
        { L"PNG Image (*.png)", L"*.png" },
        { L"Bitmap Image (*.bmp)", L"*.bmp" }
    };
}

std::string commonDialogErrorMessage(DWORD code) {
    std::ostringstream oss;
    oss << "Common dialog error " << code << ".";
    return oss.str();
}

DialogResult cancelledOrFailed() {
    const DWORD code = ::CommDlgExtendedError();
    if (code == 0) {
        return {};
    }

    DialogResult result;
    result.status = DialogStatus::Failed;
    result.error = commonDialogErrorMessage(code);
    return result;
}

DWORD flagsFor(const FileDialogPlan& plan) {
    DWORD flags = 0;
    if (plan.fileMustExist) {
        flags |= OFN_FILEMUSTEXIST;
    }
    if (plan.pathMustExist) {
        flags |= OFN_PATHMUSTEXIST;
    }
    if (plan.overwritePrompt) {
        flags |= OFN_OVERWRITEPROMPT;
    }
    return flags;
}

std::array<wchar_t, MAX_PATH> initialFileBuffer(std::wstring_view initialFileName) {
    std::array<wchar_t, MAX_PATH> fileName = {};
    if (!initialFileName.empty()) {
        const std::wstring value(initialFileName);
        wcsncpy_s(fileName.data(), fileName.size(), value.c_str(), _TRUNCATE);
    }
    return fileName;
}

bool isImageSavePlan(const FileDialogPlan& plan) {
    return plan.filters.size() == 2 &&
           plan.filters[0].pattern == L"*.png" &&
           plan.filters[1].pattern == L"*.bmp";
}

DialogResult runOpenDialog(HWND owner, const FileDialogPlan& plan) {
    std::array<wchar_t, MAX_PATH> fileName = initialFileBuffer(plan.initialFileName);
    std::wstring filter = makeWin32FilterString(plan.filters);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.Flags = flagsFor(plan);
    ofn.lpstrDefExt = plan.defaultExtension.c_str();

    if (!::GetOpenFileNameW(&ofn)) {
        return cancelledOrFailed();
    }

    DialogResult result;
    result.status = DialogStatus::Selected;
    result.path = fileName.data();
    return result;
}

DialogResult runSaveDialog(HWND owner, const FileDialogPlan& plan) {
    std::array<wchar_t, MAX_PATH> fileName = initialFileBuffer(plan.initialFileName);
    std::wstring filter = makeWin32FilterString(plan.filters);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = plan.selectedFilterIndex;
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.Flags = flagsFor(plan);
    ofn.lpstrDefExt = plan.defaultExtension.c_str();

    if (!::GetSaveFileNameW(&ofn)) {
        return cancelledOrFailed();
    }

    DialogResult result;
    result.status = DialogStatus::Selected;
    result.path = fileName.data();
    if (isImageSavePlan(plan)) {
        result.path = applyImageDialogExtension(std::move(result.path), ofn.nFilterIndex);
    } else {
        result.path = appendExtensionIfMissing(std::move(result.path), plan.defaultExtension);
    }
    return result;
}

} // namespace

std::wstring makeWin32FilterString(const std::vector<DialogFilter>& filters) {
    std::wstring value;
    for (const DialogFilter& filter : filters) {
        value += filter.label;
        value.push_back(L'\0');
        value += filter.pattern;
        value.push_back(L'\0');
    }
    value.push_back(L'\0');
    return value;
}

FileDialogPlan planOpenProjectDialog() {
    FileDialogPlan plan;
    plan.defaultExtension = L"xfplot";
    plan.filters = projectOpenFilters();
    plan.fileMustExist = true;
    plan.pathMustExist = true;
    return plan;
}

FileDialogPlan planSaveProjectDialog(std::wstring_view currentPath) {
    FileDialogPlan plan;
    plan.initialFileName = currentPath.empty() ? L"untitled.xfplot" : std::wstring(currentPath);
    plan.defaultExtension = L"xfplot";
    plan.filters = projectSaveFilters();
    plan.pathMustExist = true;
    plan.overwritePrompt = true;
    return plan;
}

FileDialogPlan planSaveImageDialog(ExportImageFormat preferredFormat) {
    const bool preferBmp = (preferredFormat == ExportImageFormat::Bmp);
    FileDialogPlan plan;
    plan.initialFileName = preferBmp ? L"xpressformula-plot.bmp" : L"xpressformula-plot.png";
    plan.defaultExtension = preferBmp ? L"bmp" : L"png";
    plan.filters = imageSaveFilters();
    plan.selectedFilterIndex = preferBmp ? 2u : 1u;
    plan.pathMustExist = true;
    plan.overwritePrompt = true;
    return plan;
}

std::wstring appendExtensionIfMissing(std::wstring path, std::wstring_view extensionWithoutDot) {
    if (!path.empty() && std::filesystem::path(path).extension().empty()) {
        path += L".";
        path += extensionWithoutDot;
    }
    return path;
}

std::wstring applyImageDialogExtension(std::wstring path, unsigned int selectedFilterIndex) {
    if (!path.empty() && std::filesystem::path(path).extension().empty()) {
        path += (selectedFilterIndex == 2) ? L".bmp" : L".png";
    }
    return path;
}

DialogResult FileDialogService::openProject(HWND owner) const {
    return runOpenDialog(owner, planOpenProjectDialog());
}

DialogResult FileDialogService::saveProject(HWND owner, std::wstring_view currentPath) const {
    return runSaveDialog(owner, planSaveProjectDialog(currentPath));
}

DialogResult FileDialogService::saveImage(HWND owner, ExportImageFormat preferredFormat) const {
    return runSaveDialog(owner, planSaveImageDialog(preferredFormat));
}

} // namespace XpressFormula::Platform::Windows
