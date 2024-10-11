export module visitor;

import std;
import antlr4;
import symbol;
import error_handler;

export class visitor : public sysy_base_visitor {
private:
  std::size_t m_indent{};
  std::string m_ir{};
  variable::ir_cnt_t m_ir_cnt{1};
  std::optional<std::string> m_current_function_name;
  std::vector<scope> m_scopes{1};

private:
  void pd();
  template <class... Args> void p(std::format_string<Args...> fmt, Args &&...args);
  template <class... Args> void pl(std::format_string<Args...> fmt, Args &&...args);

  scope &current_scope() { return m_scopes.back(); }
  variable resolve_variable(const std::string &name);
  function resolve_function(const std::string &name);

  void convert_expression(variable::TYPE from_type, variable::ir_cnt_t from_ir_cnt, variable::TYPE to_type,
                          variable::ir_cnt_t to_ir_cnt);
  variable::TYPE get_common_type(variable::TYPE type1, variable::TYPE type2);

private:
  struct expression {
    variable::ir_cnt_t ir_cnt;
    variable::TYPE type;
  };

public:
  visitor() = default;

  std::string_view ir() { return m_ir; };

  virtual std::any visitFunctionDefinition(sysy_parser::FunctionDefinitionContext *ctx) override;
  virtual std::any visitTerminal(antlr4::tree::terminal_node *ctx) override;
  virtual std::any visitBraceExpression(sysy_parser::BraceExpressionContext *ctx) override;
  virtual std::any visitUnaryExpression(sysy_parser::UnaryExpressionContext *ctx) override;
  virtual std::any visitIntegerConstantExpression(sysy_parser::IntegerConstantExpressionContext *ctx) override;
  virtual std::any visitFloatingConstantExpression(sysy_parser::FloatingConstantExpressionContext *ctx) override;
  virtual std::any visitBinaryExpression(sysy_parser::BinaryExpressionContext *ctx) override;
  virtual std::any visitReturnStatement(sysy_parser::ReturnStatementContext *ctx) override;
};
