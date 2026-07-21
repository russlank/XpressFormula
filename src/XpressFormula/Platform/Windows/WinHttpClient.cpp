// SPDX-License-Identifier: MIT
// WinHttpClient.cpp - Narrow WinHTTP GET client and release JSON parsing.
#include "WinHttpClient.h"
#include "../../Infrastructure/Serialization/JsonParser.h"
#include "../../Infrastructure/Serialization/JsonValue.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>

#include <sstream>
#include <utility>

#pragma comment(lib, "winhttp.lib")

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
#endif

namespace XpressFormula::Platform::Windows {
namespace {

class WinHttpHandle {
public:
    WinHttpHandle() noexcept = default;
    explicit WinHttpHandle(HINTERNET handle) noexcept
        : m_handle(handle) {
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept
        : m_handle(std::exchange(other.m_handle, nullptr)) {
    }

    WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.m_handle, nullptr));
        }
        return *this;
    }

    ~WinHttpHandle() {
        reset();
    }

    [[nodiscard]] HINTERNET get() const noexcept {
        return m_handle;
    }

    void reset(HINTERNET handle = nullptr) noexcept {
        if (m_handle) {
            ::WinHttpCloseHandle(m_handle);
        }
        m_handle = handle;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_handle != nullptr;
    }

private:
    HINTERNET m_handle = nullptr;
};

WinHttpResponse failure(std::string message, unsigned int statusCode = 0) {
    WinHttpResponse response;
    response.error = std::move(message);
    response.statusCode = statusCode;
    return response;
}

bool readResponseBody(HINTERNET requestHandle,
                      std::size_t maxResponseBytes,
                      std::string& response,
                      std::string& error) {
    response.clear();
    if (!requestHandle) {
        error = "HTTP request handle is not valid.";
        return false;
    }

    for (;;) {
        DWORD bytesAvailable = 0;
        if (!::WinHttpQueryDataAvailable(requestHandle, &bytesAvailable)) {
            error = "could not query HTTP response data.";
            return false;
        }
        if (bytesAvailable == 0) {
            break;
        }

        const std::size_t available = static_cast<std::size_t>(bytesAvailable);
        if (response.size() > maxResponseBytes ||
            available > maxResponseBytes - response.size()) {
            error = "HTTP response body exceeds the supported limit.";
            return false;
        }
        if (available > response.max_size() - response.size()) {
            error = "HTTP response body is too large to store.";
            return false;
        }

        const std::size_t oldSize = response.size();
        response.resize(oldSize + available);
        DWORD bytesRead = 0;
        if (!::WinHttpReadData(requestHandle,
                               response.data() + oldSize,
                               bytesAvailable,
                               &bytesRead)) {
            error = "could not read HTTP response data.";
            return false;
        }
        response.resize(oldSize + static_cast<std::size_t>(bytesRead));
        if (bytesRead == 0) {
            break;
        }
    }

    return true;
}

} // namespace

WinHttpResponse WinHttpClient::get(const WinHttpGetRequest& request) const {
    WinHttpHandle session(::WinHttpOpen(request.userAgent.c_str(),
                                        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS,
                                        0));
    if (!session) {
        return failure("could not initialize HTTP session.");
    }

    ::WinHttpSetTimeouts(session.get(),
                         request.resolveTimeoutMs,
                         request.connectTimeoutMs,
                         request.sendTimeoutMs,
                         request.receiveTimeoutMs);

    WinHttpHandle connection(::WinHttpConnect(session.get(), request.host.c_str(), request.port, 0));
    if (!connection) {
        return failure("could not connect to host.");
    }

    WinHttpHandle httpRequest(::WinHttpOpenRequest(connection.get(),
                                                  L"GET",
                                                  request.path.c_str(),
                                                  nullptr,
                                                  WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  request.secure ? WINHTTP_FLAG_SECURE : 0));
    if (!httpRequest) {
        return failure("could not create HTTP request.");
    }

    const wchar_t* headers = request.headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS
                                                     : request.headers.c_str();
    const DWORD headersLength = request.headers.empty() ? 0u : static_cast<DWORD>(-1L);
    BOOL sendOk = ::WinHttpSendRequest(httpRequest.get(),
                                       headers,
                                       headersLength,
                                       WINHTTP_NO_REQUEST_DATA,
                                       0,
                                       0,
                                       0);
    if (sendOk) {
        sendOk = ::WinHttpReceiveResponse(httpRequest.get(), nullptr);
    }
    if (!sendOk) {
        return failure("request was not successful.");
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!::WinHttpQueryHeaders(httpRequest.get(),
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX,
                               &statusCode,
                               &statusCodeSize,
                               WINHTTP_NO_HEADER_INDEX)) {
        return failure("could not read HTTP status code.");
    }

    WinHttpResponse response;
    response.statusCode = static_cast<unsigned int>(statusCode);
    std::string readError;
    if (!readResponseBody(httpRequest.get(), request.maxResponseBytes, response.body, readError)) {
        return failure(std::move(readError), response.statusCode);
    }
    response.success = true;
    return response;
}

GitHubReleaseParseResult parseGitHubLatestReleaseResponse(std::string_view body) {
    GitHubReleaseParseResult result;
    Infrastructure::Serialization::JsonValue root;
    std::string error;
    Infrastructure::Serialization::JsonParser parser(body);
    if (!parser.parse(root, error)) {
        result.error = "release JSON parse failed: " + error;
        return result;
    }

    const auto* tagName = root.find("tag_name");
    if (!tagName || tagName->type != Infrastructure::Serialization::JsonValue::Type::String ||
        tagName->text.empty()) {
        result.error = "release tag not found in GitHub response.";
        return result;
    }

    result.release.tagName = tagName->text;
    if (const auto* htmlUrl = root.find("html_url");
        htmlUrl && htmlUrl->type == Infrastructure::Serialization::JsonValue::Type::String) {
        result.release.htmlUrl = htmlUrl->text;
    }

    result.success = true;
    return result;
}

} // namespace XpressFormula::Platform::Windows
