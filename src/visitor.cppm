export module visitor;

import std;
import antlr4;
import symbol;
import error_handler;

export class visitor : public sysy_base_visitor {
private:
  std::size_t m_indent{};
  std::string m_ir{};
  variable::ir_cnt_t m_ir_cnt{};
  std::optional<std::string> m_current_function_name;
  std::vector<scope> m_scopes{1};

private:
  struct expression {
    bool is_const;
    variable::TYPE type;
    variable::ir_cnt_t ir_cnt;               // valid only non-const
    std::variant<std::int32_t, float> value; // valid only const

    std::string to_string() {
      if (is_const)
        switch (type) {
        case variable::TYPE::INT32:
          return std::format("{}", std::get<std::int32_t>(value));
        case variable::TYPE::FLOAT:
          return std::format("{:e}", std::get<float>(value));
        }
      else
        return std::format("%{}", ir_cnt);
    }
  };

private:
  void pd();
  template <class... Args> void p(std::format_string<Args...> fmt, Args &&...args);
  template <class... Args> void pl(std::format_string<Args...> fmt, Args &&...args);

  variable::ir_cnt_t new_ir_cnt() { return ++m_ir_cnt; };

  scope &current_scope() { return m_scopes.back(); }
  variable resolve_variable(const std::string &name);
  function resolve_function(const std::string &name);

  expression expression_cast(expression raw_expression, variable::TYPE target_type, bool to_non_const = false);
  variable::TYPE get_common_type(variable::TYPE type1, variable::TYPE type2, std::string op);

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
