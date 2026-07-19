// FormulaEntry.h - Transitional name for the domain formula model.
#pragma once

#include "../Model/Formula.h"

#include <cstddef>

namespace XpressFormula::UI {

using FormulaEntry = Model::Formula;

inline const float kDefaultPalette[][4] = {
    { 0.10f, 0.80f, 0.25f, 1.0f },  // green
    { 0.25f, 0.60f, 1.00f, 1.0f },  // blue
    { 1.00f, 0.30f, 0.30f, 1.0f },  // red
    { 1.00f, 0.80f, 0.10f, 1.0f },  // yellow
    { 0.80f, 0.35f, 1.00f, 1.0f },  // purple
    { 0.10f, 0.80f, 0.80f, 1.0f },  // cyan
    { 1.00f, 0.50f, 0.10f, 1.0f },  // orange
    { 0.60f, 0.80f, 0.25f, 1.0f },  // lime
};
inline constexpr int kPaletteSize = sizeof(kDefaultPalette) / sizeof(kDefaultPalette[0]);

} // namespace XpressFormula::UI
