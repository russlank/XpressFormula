// ConstantRegistry.h - Shared metadata for built-in expression constants.
#pragma once

#include <span>
#include <string_view>

namespace XpressFormula::Core {

enum class ConstantId {
    Pi,
    E,
    Tau
};

struct ConstantInfo {
    ConstantId id;
    const char* name;
    double value;
    const char* description;
};

std::span<const ConstantInfo> constantRegistry();
const ConstantInfo* findConstantInfo(std::string_view name);
bool isBuiltinConstant(std::string_view name);

} // namespace XpressFormula::Core
