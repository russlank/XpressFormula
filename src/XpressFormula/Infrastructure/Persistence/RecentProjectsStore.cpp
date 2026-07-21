// SPDX-License-Identifier: MIT
// RecentProjectsStore.cpp - UTF-safe recent project path persistence.
#include "RecentProjectsStore.h"
#include "../FileSystem/AtomicFileWriter.h"
#include "../../Platform/Windows/Utf.h"

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <fstream>
#include <utility>

namespace XpressFormula::Infrastructure::Persistence {

RecentProjectsStore::RecentProjectsStore(std::size_t maxCount)
    : m_maxCount(maxCount == 0 ? 1 : maxCount) {
}

RecentProjectsStore::RecentProjectsStore(std::filesystem::path storePathOverride,
                                         std::size_t maxCount)
    : m_maxCount(maxCount == 0 ? 1 : maxCount),
      m_storePathOverride(std::move(storePathOverride)) {
}

std::filesystem::path RecentProjectsStore::storePath() const {
    if (!m_storePathOverride.empty()) {
        return m_storePathOverride;
    }

    wchar_t* appDataBuffer = nullptr;
    size_t appDataLength = 0;
    std::filesystem::path base = std::filesystem::temp_directory_path();
    if (_wdupenv_s(&appDataBuffer, &appDataLength, L"APPDATA") == 0 &&
        appDataBuffer && appDataBuffer[0] != L'\0') {
        base = std::filesystem::path(appDataBuffer);
    }
    if (appDataBuffer) {
        std::free(appDataBuffer);
    }
    return base / L"XpressFormula" / L"recent-projects.txt";
}

RecentProjectsLoadResult RecentProjectsStore::load() const {
    RecentProjectsLoadResult result;
    std::ifstream in(storePath(), std::ios::binary);
    if (!in) {
        return result;
    }

    std::vector<std::wstring> rawPaths;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            result.changed = true;
            continue;
        }
        Platform::Windows::Utf8ToUtf16Result converted =
            Platform::Windows::utf8ToUtf16(line);
        if (!converted) {
            result.changed = true;
            continue;
        }
        rawPaths.push_back(std::move(converted.text));
    }
    in.close();

    bool normalizedChanged = false;
    result.paths = normalizeList(rawPaths, true, normalizedChanged);
    result.changed = result.changed || normalizedChanged;
    if (result.changed) {
        save(result.paths);
    }
    return result;
}

void RecentProjectsStore::save(const std::vector<std::wstring>& paths) const {
    bool changed = false;
    const std::vector<std::wstring> normalized = normalizeList(paths, false, changed);
    std::string text;
    for (const std::wstring& path : normalized) {
        text += Platform::Windows::utf16ToUtf8OrEmpty(path);
        text += '\n';
    }

    const std::filesystem::path path = storePath();
    std::error_code ignored;
    std::filesystem::create_directories(path.parent_path(), ignored);
    (void)FileSystem::writeTextAtomically(path, text);
}

void RecentProjectsStore::add(std::vector<std::wstring>& paths, const std::wstring& path) const {
    if (path.empty()) {
        return;
    }

    const std::wstring normalized = normalizeAbsolutePath(path);
    const std::wstring comparable = normalizeForComparison(normalized);
    paths.erase(std::remove_if(paths.begin(),
                               paths.end(),
                               [&](const std::wstring& existing) {
                                   return normalizeForComparison(existing) == comparable;
                               }),
                paths.end());
    paths.insert(paths.begin(), normalized);
    if (paths.size() > m_maxCount) {
        paths.resize(m_maxCount);
    }
    save(paths);
}

void RecentProjectsStore::remove(std::vector<std::wstring>& paths, const std::wstring& path) const {
    const std::wstring comparable = normalizeForComparison(normalizeAbsolutePath(path));
    paths.erase(std::remove_if(paths.begin(),
                               paths.end(),
                               [&](const std::wstring& existing) {
                                   return normalizeForComparison(existing) == comparable;
                               }),
                paths.end());
    save(paths);
}

std::wstring RecentProjectsStore::normalizeForComparison(std::wstring value) const {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring RecentProjectsStore::normalizeAbsolutePath(const std::wstring& path) const {
    std::error_code pathError;
    std::filesystem::path absolutePath =
        std::filesystem::absolute(std::filesystem::path(path), pathError);
    return (pathError ? std::filesystem::path(path) : absolutePath).wstring();
}

std::vector<std::wstring> RecentProjectsStore::normalizeList(const std::vector<std::wstring>& paths,
                                                             bool requireExisting,
                                                             bool& changed) const {
    changed = false;
    std::vector<std::wstring> normalizedPaths;
    normalizedPaths.reserve((std::min)(paths.size(), m_maxCount));

    for (const std::wstring& rawPath : paths) {
        if (rawPath.empty()) {
            changed = true;
            continue;
        }

        std::wstring path = normalizeAbsolutePath(rawPath);
        std::error_code pathError;
        if (requireExisting && !std::filesystem::exists(std::filesystem::path(path), pathError)) {
            changed = true;
            continue;
        }

        const std::wstring comparable = normalizeForComparison(path);
        const bool alreadyPresent = std::any_of(
            normalizedPaths.begin(),
            normalizedPaths.end(),
            [&](const std::wstring& existing) {
                return normalizeForComparison(existing) == comparable;
            });
        if (alreadyPresent) {
            changed = true;
            continue;
        }

        if (path != rawPath) {
            changed = true;
        }
        normalizedPaths.push_back(std::move(path));
        if (normalizedPaths.size() >= m_maxCount) {
            if (paths.size() > normalizedPaths.size()) {
                changed = true;
            }
            break;
        }
    }

    return normalizedPaths;
}

} // namespace XpressFormula::Infrastructure::Persistence
