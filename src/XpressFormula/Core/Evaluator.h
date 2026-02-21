// Evaluator.h - Evaluates an AST given a set of variable bindings.
#pragma once

#include "ASTNode.h"
#include <unordered_map>
#include <string>

namespace XpressFormula::Core {

class Evaluator {
public:
    using Variables = std::unordered_map<std::string, double>;

    /// Evaluate the AST with the given variable values. Returns NaN on error.
    static double evaluate(const ASTNodePtr& node, const Variables& vars);

private:
    static double evaluateFunction(const std::string& name,
                                   const std::vector<double>& args);
};

} // namespace XpressFormula::Core
