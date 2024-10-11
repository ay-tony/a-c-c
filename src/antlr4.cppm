module;

#include "sysyBaseVisitor.h"
#include "sysyLexer.h"
#include "sysyParser.h"

export module antlr4;

export import antlr4_runtime;

export using sysy_base_visitor = sysyBaseVisitor;
export using sysy_lexer = sysyLexer;
export using sysy_parser = sysyParser;
