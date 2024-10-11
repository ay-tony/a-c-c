export module argument_parser;

import std;
import argparse;
import error_handler;
import metadata;

export struct argument_result {
  std::string source_file;
  std::string output_file;
  std::size_t verbosity;
};

export argument_result parse_argument(int argc, const char *argv[]) {
  // TODO: 支持 lgcc 日志功能
  argparse::argument_parser program(metadata::name, metadata::version + '-' + metadata::version_build);

  // 定义参数
  program.add_argument("-o", "--output").help("the output .ll file").default_value("stdout");

  program.add_argument("source_file").help("the source file of sysY language");

  std::size_t verbosity{};
  program.add_argument("-V", "--verbose")
      .help("set the verbosity of log, max is 2")
      .action([&](const auto &) { verbosity++; })
      .append()
      .default_value(false)
      .implicit_value(true)
      .nargs(0);

  // 用 argparse 解析参数
  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    throw std::system_error(internal_error::invalid_command_line_argument, err.what());
  }

  // 参数提取
  auto source_file{program.get<std::string>("source_file")};
  auto out_file{program.get<std::string>("--output")};

  return {source_file, out_file, verbosity};
}
