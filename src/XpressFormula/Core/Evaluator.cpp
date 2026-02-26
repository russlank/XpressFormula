// Evaluator.cpp - AST evaluation with built-in math function dispatch.
#include "Evaluator.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace XpressFormula::Core {

// Single canonical NaN used for all invalid evaluation paths.
static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

double Evaluator::evaluate(const ASTNodePtr& node, const Variables& vars) {
    if (!node) return NaN;

    switch (node->type()) {
        case NodeType::Number:
            return static_cast<NumberNode*>(node.get())->value;

        case NodeType::Variable: {
            const auto& name = static_cast<VariableNode*>(node.get())->name;
            auto it = vars.find(name);
            // Variables not present in the evaluation context are treated as invalid.
            return (it != vars.end()) ? it->second : NaN;
        }

        case NodeType::BinaryOp: {
            auto* bin = static_cast<BinaryOpNode*>(node.get());
            double l = evaluate(bin->left,  vars);
            double r = evaluate(bin->right, vars);
            switch (bin->op) {
                case BinaryOperator::Add:      return l + r;
                case BinaryOperator::Subtract: return l - r;
                case BinaryOperator::Multiply: return l * r;
                case BinaryOperator::Divide:
                    // Keep undefined operations explicit for renderer-side filtering.
                    return (r == 0.0) ? NaN : l / r;
                case BinaryOperator::Power:
                    return std::pow(l, r);
            }
            return NaN;
        }

        case NodeType::UnaryOp: {
            auto* un = static_cast<UnaryOpNode*>(node.get());
            double val = evaluate(un->operand, vars);
            switch (un->op) {
                case UnaryOperator::Negate: return -val;
                case UnaryOperator::Plus:   return val;
            }
            return NaN;
        }

        case NodeType::FunctionCall: {
            auto* fn = static_cast<FunctionCallNode*>(node.get());
            std::vector<double> args;
            args.reserve(fn->arguments.size());
            for (const auto& arg : fn->arguments)
                args.push_back(evaluate(arg, vars));
            return evaluateFunction(fn->name, args);
        }
    }
    return NaN;
}

double Evaluator::evaluateFunction(const std::string& name,
                                   const std::vector<double>& args) {
    if (args.empty()) return NaN;

    // ---------- single-argument functions ----------
    if (args.size() == 1) {
        double a = args[0];
        if (name == "sin")   return std::sin(a);
        if (name == "cos")   return std::cos(a);
        if (name == "tan")   return std::tan(a);
        if (name == "asin")  return std::asin(a);
        if (name == "acos")  return std::acos(a);
        if (name == "atan")  return std::atan(a);
        if (name == "sinh")  return std::sinh(a);
        if (name == "cosh")  return std::cosh(a);
        if (name == "tanh")  return std::tanh(a);
        if (name == "sqrt")  return (a >= 0.0) ? std::sqrt(a) : NaN;
        if (name == "cbrt")  return std::cbrt(a);
        if (name == "abs")   return std::abs(a);
        if (name == "ceil")  return std::ceil(a);
        if (name == "floor") return std::floor(a);
        if (name == "round") return std::round(a);
        if (name == "log")   return (a > 0.0) ? std::log(a)   : NaN; // natural log
        if (name == "log2")  return (a > 0.0) ? std::log2(a)  : NaN;
        if (name == "log10") return (a > 0.0) ? std::log10(a) : NaN;
        if (name == "exp")   return std::exp(a);
        if (name == "sign")  return (a > 0.0) ? 1.0 : (a < 0.0) ? -1.0 : 0.0;
    }

    // ---------- two-argument functions ----------
    if (args.size() == 2) {
        double a = args[0], b = args[1];
        if (name == "atan2") return std::atan2(a, b);
        if (name == "pow")   return std::pow(a, b);
        if (name == "min")   return std::min(a, b);
        if (name == "max")   return std::max(a, b);
        if (name == "mod")   return (b != 0.0) ? std::fmod(a, b) : NaN;
        // Optional 2-arg log form: log(base, value).
        if (name == "log")
            return (a > 0.0 && b > 0.0 && a != 1.0) ? std::log(b) / std::log(a) : NaN;

        // Known single-argument function called with 2 args — evaluate with the first arg and
        // silently ignore the extras. This mirrors common calculator UX where sin(x, extra) = sin(x).
        // Fall through to re-dispatch as single-arg below.
        return evaluateFunction(name, { args[0] });
    }

    // Three or more arguments — no built-in function supports them.
    // Attempt single-arg dispatch so e.g. sin(x, y, z) degrades gracefully to sin(x).
    if (args.size() > 2) {
        return evaluateFunction(name, { args[0] });
    }

    return NaN;
}

} // namespace XpressFormula::Core
