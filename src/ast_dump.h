#pragma once

#include "ast.h"

#include <ostream>

namespace ASTDump {
void dumpModule(const AST::Module& module, std::ostream& out);
}
