// SPDX-License-Identifier: MIT
// WinHttpClient.h - Narrow WinHTTP GET client and release JSON parsing.
#pragma once

#include "../../Core/InputLimits.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace XpressFormula::Platform::Windows {

struct WinHttpGetRequest {
    std::wstring userAgent;
    std::wstring host;
    std::wstring path;
    std::wstring headers;
    unsigned short port = 443;
    bool secure = true;
    int resolveTimeoutMs = 3000;
    int connectTimeoutMs = 3000;
    int sendTimeoutMs = 5000;
    int receiveTimeoutMs = 5000;
    std::size_t maxResponseBytes = Core::InputLimits::kMaxHttpResponseBytes;
};

struct WinHttpResponse {
    bool success = false;
    unsigned int statusCode = 0;
    std::string body;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

struct GitHubReleaseInfo {
    std::string tagName;
    std::string htmlUrl;
};

struct GitHubReleaseParseResult {
    bool success = false;
    GitHubReleaseInfo release;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

class WinHttpClient {
public:
    [[nodiscard]] WinHttpResponse get(const WinHttpGetRequest& request) const;
};

[[nodiscard]] GitHubReleaseParseResult parseGitHubLatestReleaseResponse(std::string_view body);

} // namespace XpressFormula::Platform::Windows
