// ConstantRegistry.cpp - Built-in expression constant metadata.
#include "ConstantRegistry.h"

#include "MathConstants.h"

#include <array>

namespace XpressFormula::Core {

namespace {

constexpr auto kConstants = std::to_array<ConstantInfo>({
    { ConstantId::Pi, "pi", PI, "circle constant pi" },
    { ConstantId::E, "e", E, "Euler's number" },
    { ConstantId::Tau, "tau", TAU, "circle constant tau, equal to 2*pi" },
});

} // namespace

std::span<const ConstantInfo> constantRegistry() {
    return kConstants;
}

const ConstantInfo* findConstantInfo(std::string_view name) {
    for (const ConstantInfo& info : kConstants) {
        if (std::string_view(info.name) == name) {
            return &info;
        }
    }
    return nullptr;
}

bool isBuiltinConstant(std::string_view name) {
    return findConstantInfo(name) != nullptr;
}

} // namespace XpressFormula::Core
