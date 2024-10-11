import std;
import antlr4;
import error_handler;
import argument_parser;
import spdlog;
import metadata;
import visitor;

int main(int argc, const char *argv[]) {
  try {
    // 解析命令行参数
    auto options = parse_argument(argc, argv);

    std::array log_levels{spdlog::level::info, spdlog::level::debug, spdlog::level::trace};
    spdlog::set_level(log_levels[std::min(options.verbosity, log_levels.size() - 1)]);
    spdlog::debug("setting log level to {} / {}...", std::min(options.verbosity, log_levels.size() - 1), log_levels.size() - 1);

    // 读取源文件并转化成输入流
    spdlog::debug("reading source file from {}...", options.source_file);
    std::ifstream ifstream;
    ifstream.open(options.source_file, std::ios::in);
    antlr4::antlr_input_stream input{ifstream};
    ifstream.close();

    // 词法分析
    spdlog::debug("lexer analysing input stream...");
    sysy_lexer lexer(&input);
    antlr4::common_token_stream tokens(&lexer);

    // 语法分析
    spdlog::debug("parser generating parse tree...");
    sysy_parser parser(&tokens);
    parser.removeErrorListeners();
    lgcc_error_listener error_listener(options.source_file);
    parser.addErrorListener(&error_listener);
    antlr4::tree::parse_tree *tree = parser.program();

    // 日志打印词法语法分析结果
    spdlog::trace("token stream:");
    for (antlr4::token *token : tokens.getTokens())
      spdlog::trace("{}", token->toString());
    spdlog::trace("parse tree:");
    for (auto line : tree->toStringTree(true) | std::views::split('\n'))
      spdlog::trace("{}", std::string_view(line));

    // 使用 visitor 访问语法树
    spdlog::debug("visitor generating ir...");
    visitor vis;
    vis.visit(tree);

    // 日志打印 ir
    spdlog::trace("llvm ir:");
    for (auto line : vis.ir() | std::views::split('\n'))
      spdlog::trace("{}", std::string_view(line));

    // 输出 ir
    if (options.output_file == "stdout")
      std::print("{}", vis.ir());
    else {
      std::ofstream ofstream;
      ofstream.open(options.output_file, std::ios::out);
      std::print(ofstream, "{}", vis.ir());
      ofstream.close();
    }
  } catch (std::system_error &e) {
    spdlog::critical("{}[{}]: {}", e.code().category().name(), e.code().value(), e.what());
    spdlog::critical("exiting {}.", metadata::name);
    return -1;
  }

  return 0;
}
