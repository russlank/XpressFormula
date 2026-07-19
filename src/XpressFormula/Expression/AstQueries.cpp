// AstQueries.cpp - Shared pure queries over expression AST nodes.
#include "AstQueries.h"

namespace XpressFormula::Expression {

void collectVariables(const Core::ASTNodePtr& node, VariableSet& variables) {
    if (!node) {
        return;
    }

    switch (node->type()) {
        case Core::NodeType::Variable:
            variables.insert(static_cast<Core::VariableNode*>(node.get())->name);
            break;
        case Core::NodeType::BinaryOp: {
            const auto* binary = static_cast<Core::BinaryOpNode*>(node.get());
            collectVariables(binary->left, variables);
            collectVariables(binary->right, variables);
            break;
        }
        case Core::NodeType::UnaryOp:
            collectVariables(static_cast<Core::UnaryOpNode*>(node.get())->operand, variables);
            break;
        case Core::NodeType::FunctionCall: {
            const auto* function = static_cast<Core::FunctionCallNode*>(node.get());
            for (const Core::ASTNodePtr& argument : function->arguments) {
                collectVariables(argument, variables);
            }
            break;
        }
        default:
            break;
    }
}

VariableSet collectVariables(const Core::ASTNodePtr& node) {
    VariableSet variables;
    collectVariables(node, variables);
    return variables;
}

bool containsVariable(const Core::ASTNodePtr& node, const std::string& name) {
    if (!node) {
        return false;
    }

    switch (node->type()) {
        case Core::NodeType::Variable:
            return static_cast<Core::VariableNode*>(node.get())->name == name;
        case Core::NodeType::BinaryOp: {
            const auto* binary = static_cast<Core::BinaryOpNode*>(node.get());
            return containsVariable(binary->left, name) ||
                   containsVariable(binary->right, name);
        }
        case Core::NodeType::UnaryOp:
            return containsVariable(
                static_cast<Core::UnaryOpNode*>(node.get())->operand, name);
        case Core::NodeType::FunctionCall: {
            const auto* function = static_cast<Core::FunctionCallNode*>(node.get());
            for (const Core::ASTNodePtr& argument : function->arguments) {
                if (containsVariable(argument, name)) {
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

bool isVariableNode(const Core::ASTNodePtr& node, const std::string& name) {
    return node &&
           node->type() == Core::NodeType::Variable &&
           static_cast<Core::VariableNode*>(node.get())->name == name;
}

} // namespace XpressFormula::Expression
