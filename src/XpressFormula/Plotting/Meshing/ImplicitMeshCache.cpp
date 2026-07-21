// ImplicitMeshCache.cpp - Owned bounded cache for implicit surface meshes.
#include "ImplicitMeshCache.h"

#include <algorithm>
#include <utility>

namespace XpressFormula::Plotting::Meshing {

bool ImplicitMeshKey::operator==(const ImplicitMeshKey& other) const noexcept {
    return formulaId == other.formulaId &&
           compilationRevision == other.compilationRevision &&
           astIdentity == other.astIdentity &&
           gridResolution == other.gridResolution &&
           xMin == other.xMin &&
           xMax == other.xMax &&
           yMin == other.yMin &&
           yMax == other.yMax &&
           zCenter == other.zCenter &&
           zMin == other.zMin &&
           zMax == other.zMax;
}

ImplicitMeshCache::ImplicitMeshCache(std::size_t maxEntries)
    : m_maxEntries(std::max<std::size_t>(1, maxEntries)) {
}

const ImplicitMeshEntry* ImplicitMeshCache::find(const ImplicitMeshKey& key) {
    ++m_tick;
    for (Slot& slot : m_slots) {
        if (slot.key == key) {
            slot.lastUsed = m_tick;
            return &slot.entry;
        }
    }
    return nullptr;
}

const ImplicitMeshEntry& ImplicitMeshCache::store(ImplicitMeshKey key, ImplicitMeshEntry entry) {
    ++m_tick;
    for (Slot& slot : m_slots) {
        if (slot.key == key) {
            slot.entry = std::move(entry);
            slot.lastUsed = m_tick;
            return slot.entry;
        }
    }

    m_slots.push_back(Slot{ key, std::move(entry), m_tick });
    enforceLimit();
    for (Slot& slot : m_slots) {
        if (slot.key == key) {
            return slot.entry;
        }
    }

    return m_slots.back().entry;
}

void ImplicitMeshCache::clear() noexcept {
    m_slots.clear();
}

void ImplicitMeshCache::invalidateFormula(Model::FormulaId formulaId) {
    m_slots.erase(
        std::remove_if(
            m_slots.begin(),
            m_slots.end(),
            [formulaId](const Slot& slot) { return slot.key.formulaId == formulaId; }),
        m_slots.end());
}

void ImplicitMeshCache::retainFormulaIds(std::span<const Model::FormulaId> formulaIds) {
    m_slots.erase(
        std::remove_if(
            m_slots.begin(),
            m_slots.end(),
            [formulaIds](const Slot& slot) {
                return std::find(formulaIds.begin(), formulaIds.end(), slot.key.formulaId) ==
                       formulaIds.end();
            }),
        m_slots.end());
}

std::size_t ImplicitMeshCache::size() const noexcept {
    return m_slots.size();
}

std::size_t ImplicitMeshCache::maxEntries() const noexcept {
    return m_maxEntries;
}

void ImplicitMeshCache::enforceLimit() {
    while (m_slots.size() > m_maxEntries) {
        const auto oldest = std::min_element(
            m_slots.begin(),
            m_slots.end(),
            [](const Slot& lhs, const Slot& rhs) {
                return lhs.lastUsed < rhs.lastUsed;
            });
        if (oldest == m_slots.end()) {
            return;
        }
        m_slots.erase(oldest);
    }
}

} // namespace XpressFormula::Plotting::Meshing
