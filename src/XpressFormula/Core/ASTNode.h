// ASTNode.h - Abstract Syntax Tree nodes for parsed mathematical expressions.
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace XpressFormula::Core {

enum class NodeType {
    Number,
    Variable,
    BinaryOp,
    UnaryOp,
    FunctionCall
};

enum class BinaryOperator { Add, Subtract, Multiply, Divide, Power };
enum class UnaryOperator  { Negate, Plus };

/// Base class for all AST nodes.
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual NodeType type() const = 0;
};

using ASTNodePtr = std::shared_ptr<ASTNode>;

/// Numeric literal (e.g. 3.14).
class NumberNode : public ASTNode {
public:
    double value;
    explicit NumberNode(double v) : value(v) {}
    NodeType type() const override { return NodeType::Number; }
};

/// Variable reference (e.g. x, y, z).
class VariableNode : public ASTNode {
public:
    std::string name;
    explicit VariableNode(std::string n) : name(std::move(n)) {}
    NodeType type() const override { return NodeType::Variable; }
};

/// Binary operation (e.g. a + b, a ^ b).
class BinaryOpNode : public ASTNode {
public:
    BinaryOperator op;
    ASTNodePtr     left;
    ASTNodePtr     right;
    BinaryOpNode(BinaryOperator o, ASTNodePtr l, ASTNodePtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
    NodeType type() const override { return NodeType::BinaryOp; }
};

/// Unary operation (e.g. -x).
class UnaryOpNode : public ASTNode {
public:
    UnaryOperator op;
    ASTNodePtr    operand;
    UnaryOpNode(UnaryOperator o, ASTNodePtr e)
        : op(o), operand(std::move(e)) {}
    NodeType type() const override { return NodeType::UnaryOp; }
};

/// Function call (e.g. sin(x), atan2(y, x)).
class FunctionCallNode : public ASTNode {
public:
    std::string            name;
    std::vector<ASTNodePtr> arguments;
    FunctionCallNode(std::string n, std::vector<ASTNodePtr> args)
        : name(std::move(n)), arguments(std::move(args)) {}
    NodeType type() const override { return NodeType::FunctionCall; }
};

} // namespace XpressFormula::Core
