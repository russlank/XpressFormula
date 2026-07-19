// AstQueries.h - Shared pure queries over expression AST nodes.
#pragma once

#include "../Core/ASTNode.h"

#include <set>
#include <string>

namespace XpressFormula::Expression {

using VariableSet = std::set<std::string>;

void collectVariables(const Core::ASTNodePtr& node, VariableSet& variables);
VariableSet collectVariables(const Core::ASTNodePtr& node);
bool containsVariable(const Core::ASTNodePtr& node, const std::string& name);
bool isVariableNode(const Core::ASTNodePtr& node, const std::string& name);

} // namespace XpressFormula::Expression
