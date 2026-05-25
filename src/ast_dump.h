#pragma once

#include "ast.h"

#include <ostream>

namespace ASTDump {

// Печатает AST в удобном для чтения виде.
void dumpModule(const AST::Module& module, std::ostream& out);

} // namespace ASTDump
