// Evaluator.cpp - AST evaluation through fixed variable slots and registry callbacks.
#include "Evaluator.h"

#include "FunctionRegistry.h"

#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace XpressFormula::Core {

namespace {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr std::size_t kMaxEvaluatedFunctionArguments = 8;

EvaluationContext contextFromVariables(const Evaluator::Variables& vars) {
    EvaluationContext context{ NaN, NaN, NaN };

    if (const auto it = vars.find("x"); it != vars.end()) {
        context.x = it->second;
    }
    if (const auto it = vars.find("y"); it != vars.end()) {
        context.y = it->second;
    }
    if (const auto it = vars.find("z"); it != vars.end()) {
        context.z = it->second;
    }

    return context;
}

} // namespace

double Evaluator::evaluate(const ASTNodePtr& node, const EvaluationContext& context) {
    if (!node) {
        return NaN;
    }

    switch (node->type()) {
        case NodeType::Number:
            return static_cast<NumberNode*>(node.get())->value;

        case NodeType::Variable:
            return variableValue(*static_cast<VariableNode*>(node.get()), context);

        case NodeType::BinaryOp: {
            auto* bin = static_cast<BinaryOpNode*>(node.get());
            const double l = evaluate(bin->left, context);
            const double r = evaluate(bin->right, context);
            switch (bin->op) {
                case BinaryOperator::Add:      return l + r;
                case BinaryOperator::Subtract: return l - r;
                case BinaryOperator::Multiply: return l * r;
                case BinaryOperator::Divide:
                    return (r == 0.0) ? NaN : l / r;
                case BinaryOperator::Power:
                    return std::pow(l, r);
            }
            return NaN;
        }

        case NodeType::UnaryOp: {
            auto* un = static_cast<UnaryOpNode*>(node.get());
            const double value = evaluate(un->operand, context);
            switch (un->op) {
                case UnaryOperator::Negate: return -value;
                case UnaryOperator::Plus:   return value;
            }
            return NaN;
        }

        case NodeType::FunctionCall: {
            auto* fn = static_cast<FunctionCallNode*>(node.get());
            if (fn->arguments.size() > kMaxEvaluatedFunctionArguments) {
                return NaN;
            }

            std::array<double, kMaxEvaluatedFunctionArguments> args{};
            std::size_t argCount = 0;
            for (const ASTNodePtr& argument : fn->arguments) {
                args[argCount++] = evaluate(argument, context);
            }

            const std::span<const double> argSpan(args.data(), argCount);

            if (fn->definition) {
                return evaluateFunction(*fn->definition, argSpan);
            }
            return evaluateFunction(fn->name, argSpan);
        }
    }

    return NaN;
}

double Evaluator::evaluate(const ASTNodePtr& node, const Variables& vars) {
    return evaluate(node, contextFromVariables(vars));
}

double Evaluator::variableValue(const VariableNode& variable,
                                const EvaluationContext& context) {
    switch (variable.slot) {
        case VariableSlot::X: return context.x;
        case VariableSlot::Y: return context.y;
        case VariableSlot::Z: return context.z;
        case VariableSlot::Unknown:
            break;
    }
    return NaN;
}

} // namespace XpressFormula::Core
