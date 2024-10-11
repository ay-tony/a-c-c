module;

#include <antlr4-runtime.h>

export module antlr4_runtime;

export namespace antlr4 {

using common_token_stream = CommonTokenStream;
using antlr_input_stream = ANTLRInputStream;
using token = Token;
using base_error_listener = BaseErrorListener;
using recognizer = Recognizer;

namespace tree {
using terminal_node = TerminalNode;
using parse_tree = ParseTree;
} // namespace tree

} // namespace antlr4
