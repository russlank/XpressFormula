// Formula.h - Domain formula state.
#pragma once

#include "Color.h"
#include "FormulaId.h"
#include "../Expression/FormulaCompiler.h"

#include <string>

namespace XpressFormula::Model {

struct Formula {
    FormulaId id = nextFormulaId();
    std::string expression;
    ColorRgba color;
    bool visible = true;
    double zSlice = 0.0;
    Expression::CompiledFormula compiled;
};

} // namespace XpressFormula::Model
