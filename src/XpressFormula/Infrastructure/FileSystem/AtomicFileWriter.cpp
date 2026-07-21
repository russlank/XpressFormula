// SPDX-License-Identifier: MIT
// AtomicFileWriter.cpp - Win32-backed atomic replacement helpers.
#include "AtomicFileWriter.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

namespace XpressFormula::Infrastructure::FileSystem {
namespace {

AtomicWriteResult failure(std::string message, unsigned long win32Error = 0) {
    AtomicWriteResult result;
    result.success = false;
    result.win32Error = win32Error;
    if (win32Error != 0) {
        message += ": Win32 error " + std::to_string(win32Error) + ".";
    }
    result.error = std::move(message);
    return result;
}

void cleanupTempFile(const std::filesystem::path& tempPath) {
    std::error_code ignored;
    std::filesystem::remove(tempPath, ignored);
}

std::filesystem::path uniqueAtomicTempPathFor(const std::filesystem::path& targetPath,
                                              unsigned int attempt) {
    std::filesystem::path tempPath = targetPath;
    tempPath += L".tmp.";
    tempPath += std::to_wstring(::GetCurrentProcessId());
    tempPath += L".";
    tempPath += std::to_wstring(::GetTickCount64());
    tempPath += L".";
    tempPath += std::to_wstring(attempt);
    return tempPath;
}

bool writeAll(HANDLE fileHandle, std::span<const std::uint8_t> bytes, AtomicWriteResult& result) {
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t remaining = bytes.size() - offset;
        const DWORD chunkSize = static_cast<DWORD>((std::min)(
            remaining,
            static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));

        DWORD bytesWritten = 0;
        if (!::WriteFile(fileHandle, bytes.data() + offset, chunkSize, &bytesWritten, nullptr)) {
            result = failure("Could not write temp file", ::GetLastError());
            return false;
        }
        if (bytesWritten == 0 || bytesWritten > chunkSize) {
            result = failure("Could not write temp file completely");
            return false;
        }
        offset += bytesWritten;
    }
    return true;
}

} // namespace

std::filesystem::path atomicTempPathFor(const std::filesystem::path& targetPath) {
    std::filesystem::path tempPath = targetPath;
    tempPath += L".tmp";
    return tempPath;
}

AtomicWriteResult writeTextAtomically(const std::filesystem::path& targetPath,
                                      std::string_view utf8Text) {
    const auto* data = reinterpret_cast<const std::uint8_t*>(utf8Text.data());
    return writeBytesAtomically(targetPath, std::span<const std::uint8_t>(data, utf8Text.size()));
}

AtomicWriteResult writeBytesAtomically(const std::filesystem::path& targetPath,
                                       std::span<const std::uint8_t> bytes) {
    if (targetPath.empty()) {
        return failure("Target path is empty");
    }

    std::filesystem::path tempPath;
    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    for (unsigned int attempt = 0; attempt < 64; ++attempt) {
        tempPath = uniqueAtomicTempPathFor(targetPath, attempt);
        fileHandle = ::CreateFileW(tempPath.wstring().c_str(),
                                   GENERIC_WRITE,
                                   0,
                                   nullptr,
                                   CREATE_NEW,
                                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                   nullptr);
        if (fileHandle != INVALID_HANDLE_VALUE) {
            break;
        }
        const DWORD error = ::GetLastError();
        if (error != ERROR_FILE_EXISTS) {
            return failure("Could not create temp file", error);
        }
    }
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return failure("Could not create unique temp file", ::GetLastError());
    }

    AtomicWriteResult result;
    if (!writeAll(fileHandle, bytes, result)) {
        const DWORD closeError = ::CloseHandle(fileHandle) ? 0 : ::GetLastError();
        (void)closeError;
        cleanupTempFile(tempPath);
        return result;
    }

    if (!::FlushFileBuffers(fileHandle)) {
        const DWORD win32Error = ::GetLastError();
        ::CloseHandle(fileHandle);
        cleanupTempFile(tempPath);
        return failure("Could not flush temp file", win32Error);
    }

    if (!::CloseHandle(fileHandle)) {
        const DWORD win32Error = ::GetLastError();
        cleanupTempFile(tempPath);
        return failure("Could not close temp file", win32Error);
    }

    if (!::MoveFileExW(tempPath.wstring().c_str(),
                       targetPath.wstring().c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD win32Error = ::GetLastError();
        cleanupTempFile(tempPath);
        return failure("Could not replace target file", win32Error);
    }

    result.success = true;
    return result;
}

} // namespace XpressFormula::Infrastructure::FileSystem
