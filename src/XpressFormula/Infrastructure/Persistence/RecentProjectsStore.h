// SPDX-License-Identifier: MIT
// RecentProjectsStore.h - UTF-safe recent project path persistence.
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace XpressFormula::Infrastructure::Persistence {

struct RecentProjectsLoadResult {
    std::vector<std::wstring> paths;
    bool changed = false;
};

class RecentProjectsStore {
public:
    explicit RecentProjectsStore(std::size_t maxCount = 8);
    RecentProjectsStore(std::filesystem::path storePathOverride, std::size_t maxCount = 8);

    [[nodiscard]] std::filesystem::path storePath() const;
    [[nodiscard]] RecentProjectsLoadResult load() const;
    void save(const std::vector<std::wstring>& paths) const;
    void add(std::vector<std::wstring>& paths, const std::wstring& path) const;
    void remove(std::vector<std::wstring>& paths, const std::wstring& path) const;

private:
    [[nodiscard]] std::wstring normalizeForComparison(std::wstring value) const;
    [[nodiscard]] std::wstring normalizeAbsolutePath(const std::wstring& path) const;
    [[nodiscard]] std::vector<std::wstring> normalizeList(const std::vector<std::wstring>& paths,
                                                          bool requireExisting,
                                                          bool& changed) const;

    std::size_t m_maxCount = 8;
    std::filesystem::path m_storePathOverride;
};

} // namespace XpressFormula::Infrastructure::Persistence
