// FormulaExamples.h - UI-facing aliases for shared built-in formula examples.
#pragma once

#include "../Core/ExampleFormulaCatalog.h"

namespace XpressFormula::UI {

using ExamplePattern = XpressFormula::Core::ExampleFormula;

inline std::span<const ExamplePattern> examplePatterns() {
    return XpressFormula::Core::exampleFormulas();
}

} // namespace XpressFormula::UI
