// Evaluator.h - Evaluates an AST given a set of variable bindings.
#pragma once

#include "ASTNode.h"

#include <string>
#include <unordered_map>

namespace XpressFormula::Core {

struct EvaluationContext {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

class Evaluator {
public:
    using Variables = std::unordered_map<std::string, double>;

    /// Evaluate the AST with fixed x/y/z slots. Returns NaN on error.
    static double evaluate(const ASTNodePtr& node, const EvaluationContext& context);

    /// Compatibility adapter for callers that still pass variable maps.
    static double evaluate(const ASTNodePtr& node, const Variables& vars);

private:
    static double variableValue(const VariableNode& variable,
                                const EvaluationContext& context);
};

} // namespace XpressFormula::Core
