// FormulaId.h - Stable formula identity.
#pragma once

#include <atomic>
#include <cstdint>

namespace XpressFormula::Model {

using FormulaId = std::uint64_t;

inline FormulaId nextFormulaId() {
    static std::atomic<FormulaId> nextId{ 1 };
    return nextId.fetch_add(1, std::memory_order_relaxed);
}

} // namespace XpressFormula::Model
