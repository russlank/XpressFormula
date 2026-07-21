// ImplicitMeshCache.h - Owned bounded cache for implicit surface meshes.
#pragma once

#include "../Geometry/Bounds.h"
#include "../Geometry/Vec3.h"
#include "../../Model/FormulaId.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace XpressFormula::Plotting::Meshing {

struct ImplicitMeshTriangle {
    Geometry::Vec3 p0;
    Geometry::Vec3 p1;
    Geometry::Vec3 p2;
};

struct ImplicitMeshKey {
    Model::FormulaId formulaId = 0;
    std::uint64_t compilationRevision = 0;
    const void* astIdentity = nullptr;
    int gridResolution = 0;
    double xMin = 0.0;
    double xMax = 0.0;
    double yMin = 0.0;
    double yMax = 0.0;
    double zCenter = 0.0;
    double zMin = 0.0;
    double zMax = 0.0;

    [[nodiscard]] bool operator==(const ImplicitMeshKey& other) const noexcept;
};

struct ImplicitMeshEntry {
    std::vector<ImplicitMeshTriangle> faces;
    Geometry::Bounds3D surfaceBounds;
};

class ImplicitMeshCache {
public:
    explicit ImplicitMeshCache(std::size_t maxEntries = 8);

    [[nodiscard]] const ImplicitMeshEntry* find(const ImplicitMeshKey& key);
    [[nodiscard]] const ImplicitMeshEntry& store(ImplicitMeshKey key, ImplicitMeshEntry entry);

    void clear() noexcept;
    void invalidateFormula(Model::FormulaId formulaId);
    void retainFormulaIds(std::span<const Model::FormulaId> formulaIds);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t maxEntries() const noexcept;

private:
    struct Slot {
        ImplicitMeshKey key;
        ImplicitMeshEntry entry;
        std::uint64_t lastUsed = 0;
    };

    void enforceLimit();

    std::vector<Slot> m_slots;
    std::size_t m_maxEntries = 8;
    std::uint64_t m_tick = 0;
};

} // namespace XpressFormula::Plotting::Meshing
