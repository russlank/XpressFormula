// PlatformWindowsServicesTests.cpp - Pure tests for Windows platform service planning helpers.
#include "CppUnitTest.h"
#include "../XpressFormula/Core/InputLimits.h"
#include "../XpressFormula/Platform/Windows/ClipboardService.h"
#include "../XpressFormula/Platform/Windows/FileDialogService.h"
#include "../XpressFormula/Platform/Windows/ShellService.h"
#include "../XpressFormula/Platform/Windows/WinHttpClient.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace XFW = XpressFormula::Platform::Windows;
namespace XFInputLimits = XpressFormula::Core::InputLimits;

namespace XpressFormulaTests {

TEST_CASE(FileDialogPlan_ProjectFiltersAndExtensionsAreStable) {
    const XFW::FileDialogPlan openPlan = XFW::planOpenProjectDialog();
    Assert::AreEqual(std::wstring(L"xfplot"), openPlan.defaultExtension);
    Assert::IsTrue(openPlan.fileMustExist);
    Assert::IsTrue(openPlan.pathMustExist);
    Assert::AreEqual(3, static_cast<int>(openPlan.filters.size()));
    Assert::AreEqual(std::wstring(L"*.xfplot"), openPlan.filters[0].pattern);
    Assert::AreEqual(std::wstring(L"*.json"), openPlan.filters[1].pattern);
    Assert::AreEqual(std::wstring(L"*.*"), openPlan.filters[2].pattern);

    const XFW::FileDialogPlan savePlan = XFW::planSaveProjectDialog(L"");
    Assert::AreEqual(std::wstring(L"untitled.xfplot"), savePlan.initialFileName);
    Assert::IsTrue(savePlan.overwritePrompt);
    Assert::AreEqual(std::wstring(L"C:\\temp\\plot.xfplot"),
                     XFW::appendExtensionIfMissing(L"C:\\temp\\plot", L"xfplot"));
    Assert::AreEqual(std::wstring(L"C:\\temp\\plot.json"),
                     XFW::appendExtensionIfMissing(L"C:\\temp\\plot.json", L"xfplot"));
}

TEST_CASE(FileDialogPlan_ImageFiltersAndSelectedExtensionsAreStable) {
    const XFW::FileDialogPlan pngPlan =
        XFW::planSaveImageDialog(XFW::ExportImageFormat::Png);
    Assert::AreEqual(std::wstring(L"xpressformula-plot.png"), pngPlan.initialFileName);
    Assert::AreEqual(1u, pngPlan.selectedFilterIndex);
    Assert::AreEqual(std::wstring(L"png"), pngPlan.defaultExtension);

    const XFW::FileDialogPlan bmpPlan =
        XFW::planSaveImageDialog(XFW::ExportImageFormat::Bmp);
    Assert::AreEqual(std::wstring(L"xpressformula-plot.bmp"), bmpPlan.initialFileName);
    Assert::AreEqual(2u, bmpPlan.selectedFilterIndex);
    Assert::AreEqual(std::wstring(L"bmp"), bmpPlan.defaultExtension);

    Assert::AreEqual(std::wstring(L"C:\\temp\\plot.png"),
                     XFW::applyImageDialogExtension(L"C:\\temp\\plot", 1));
    Assert::AreEqual(std::wstring(L"C:\\temp\\plot.bmp"),
                     XFW::applyImageDialogExtension(L"C:\\temp\\plot", 2));
    Assert::AreEqual(std::wstring(L"C:\\temp\\plot.custom"),
                     XFW::applyImageDialogExtension(L"C:\\temp\\plot.custom", 2));
}

TEST_CASE(ShellService_ExplorerRevealArgumentsQuoteSelectedPath) {
    Assert::AreEqual(std::wstring(L"/select,\"C:\\A B\\plot.png\""),
                     XFW::explorerRevealArguments(L"C:\\A B\\plot.png"));
}

TEST_CASE(ClipboardService_DibLayoutIsBottomUpBgra) {
    const std::vector<std::uint8_t> pixels = {
        1, 2, 3, 4,     5, 6, 7, 8,
        9, 10, 11, 12,  13, 14, 15, 16
    };

    const XFW::DibBuildResult dib = XFW::buildBottomUpDibFromBgra(pixels, 2, 2);
    Assert::IsTrue(dib.success);
    Assert::AreEqual(static_cast<int>(sizeof(BITMAPINFOHEADER) + pixels.size()),
                     static_cast<int>(dib.bytes.size()));

    const auto* header = reinterpret_cast<const BITMAPINFOHEADER*>(dib.bytes.data());
    Assert::AreEqual(static_cast<DWORD>(sizeof(BITMAPINFOHEADER)), header->biSize);
    Assert::AreEqual(2L, header->biWidth);
    Assert::AreEqual(2L, header->biHeight);
    Assert::AreEqual(32, static_cast<int>(header->biBitCount));
    Assert::AreEqual(static_cast<DWORD>(pixels.size()), header->biSizeImage);

    const auto* payload = dib.bytes.data() + sizeof(BITMAPINFOHEADER);
    for (int i = 0; i < 8; ++i) {
        Assert::AreEqual(pixels[8 + i], payload[i]);
        Assert::AreEqual(pixels[i], payload[8 + i]);
    }
}

TEST_CASE(ClipboardService_RejectsInvalidImagePayloads) {
    const std::vector<std::uint8_t> onePixel = { 0, 0, 0, 255 };

    Assert::IsFalse(XFW::buildBottomUpDibFromBgra(onePixel, 0, 1).success);
    Assert::IsFalse(XFW::buildBottomUpDibFromBgra(onePixel, 2, 2).success);
}

TEST_CASE(WinHttpClient_DefaultRequestUsesCentralResponseLimit) {
    XFW::WinHttpGetRequest request;

    Assert::AreEqual(XFInputLimits::kMaxHttpResponseBytes, request.maxResponseBytes);
}

TEST_CASE(WinHttpClient_ParsesGitHubReleaseJsonThroughSharedParser) {
    const XFW::GitHubReleaseParseResult parsed =
        XFW::parseGitHubLatestReleaseResponse(
            R"({"tag_name":"v1.2.3","html_url":"https://example.test/releases/1.2.3","ignored":true})");
    Assert::IsTrue(parsed.success);
    Assert::AreEqual(std::string("v1.2.3"), parsed.release.tagName);
    Assert::AreEqual(std::string("https://example.test/releases/1.2.3"),
                     parsed.release.htmlUrl);

    const XFW::GitHubReleaseParseResult missing =
        XFW::parseGitHubLatestReleaseResponse(R"({"html_url":"https://example.test"})");
    Assert::IsFalse(missing.success);
}

} // namespace XpressFormulaTests
