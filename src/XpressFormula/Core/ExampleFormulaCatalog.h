// ExampleFormulaCatalog.h - Shared built-in formula examples.
#pragma once

#include <span>

namespace XpressFormula::Core {

struct ExampleFormula {
    const char* label;
    const char* expression;
    bool includeInPresets = true;
};

std::span<const ExampleFormula> exampleFormulas();

} // namespace XpressFormula::Core
