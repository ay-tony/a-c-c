module visitor;

std::string visitor::expression::to_string() {
  if (is_const)
    switch (type) {
    case variable::TYPE::INT32:
      return std::format("{}", std::get<std::int32_t>(value));
    case variable::TYPE::FLOAT:
      union union_val {
        std::uint64_t val_i;
        double val_f;
      } tmp_value{.val_f = std::get<float>(value)};
      return std::format("0x{:x}", tmp_value.val_i);
    }
  else
    return std::format("%{}", ir_cnt);
}

visitor::expression visitor::expression::to_i1(this expression &&self, visitor &vis) {
  if (self.is_const)
    switch (self.type) {
    case variable::TYPE::INT32:
      return {true, variable::TYPE::INT32, 0, std::get<std::int32_t>(self.value) != 0};
    case variable::TYPE::FLOAT:
      return {true, variable::TYPE::INT32, 0, std::get<float>(self.value) != 0};
    }
  else
    switch (self.type) {
    case variable::TYPE::INT32:
      vis.pl("%{} = icmp ne i32 {}, 0", vis.new_ir_cnt(), self.to_string());
      return {false, variable::TYPE::INT32, vis.m_ir_cnt, {}};
    case variable::TYPE::FLOAT:
      vis.pl("%{} = fcmp one float {}, 0.", vis.new_ir_cnt(), self.to_string());
      return {false, variable::TYPE::INT32, vis.m_ir_cnt, {}};
    }
}

void visitor::pd() {
  for (std::size_t i = 0; i < m_indent; i++)
    m_ir += "    ";
}

template <class... Args> void visitor::p(std::format_string<Args...> fmt, Args &&...args) {
  m_ir += std::format(fmt, std::forward<Args>(args)...);
}

template <class... Args> void visitor::pl(std::format_string<Args...> fmt, Args &&...args) {
  pd();
  p(fmt, std::forward<Args>(args)...);
  m_ir += '\n';
}

variable visitor::resolve_variable(const std::string &name) {
  /*for (auto &s : m_scopes | std::views::reverse)*/ // TODO: 使用这行代码会使用弃置的 iterator
  for (std::size_t i = m_scopes.size() - 1; i >= 0; i--) {
    auto &s = m_scopes[i];
    if (auto ret = s.resolve_variable(name))
      return ret.value();
  }
  throw std::system_error(internal_error::invalid_variable_name, std::format("{}", name));
}

function visitor::resolve_function(const std::string &name) {
  /*for (auto &scope : m_scopes | std::views::reverse)*/ // TODO: 同上 visitor.cpp:19
  for (std::size_t i = m_scopes.size() - 1; i >= 0; i--) {
    auto &s = m_scopes[i];
    if (auto ret = s.resolve_function(name))
      return ret.value();
  }
  throw std::system_error(internal_error::invalid_function_name, std::format("{}", name));
}

visitor::expression visitor::expression_cast(expression raw_expression, variable::TYPE target_type, bool to_non_const) {
  auto result_expression{expression{}};
  std::map<std::pair<variable::TYPE, variable::TYPE>, std::function<void()>> operations[2]{
      {
          {{variable::TYPE::INT32, variable::TYPE::INT32}, [&] { result_expression = raw_expression; }},
          {{variable::TYPE::INT32, variable::TYPE::FLOAT},
           [&] {
             pl("%{} = sitofp {} %{} to {}", new_ir_cnt(), "i32", raw_expression.ir_cnt, "float");
             result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
           }},
          {{variable::TYPE::FLOAT, variable::TYPE::INT32},
           [&] {
             pl("%{} = fptosi {} %{} to {}", new_ir_cnt(), "float", raw_expression.ir_cnt, "i32");
             result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
           }},
          {{variable::TYPE::FLOAT, variable::TYPE::FLOAT}, [&] { result_expression = raw_expression; }},
      },
      {
          {{variable::TYPE::INT32, variable::TYPE::INT32}, [&] { result_expression = raw_expression; }},
          {{variable::TYPE::INT32, variable::TYPE::FLOAT},
           [&] {
             result_expression = {true, variable::TYPE::FLOAT, 0,
                                  static_cast<float>(std::get<std::int32_t>(raw_expression.value))};
           }},
          {{variable::TYPE::FLOAT, variable::TYPE::INT32},
           [&] {
             result_expression = {true, variable::TYPE::INT32, 0,
                                  static_cast<std::int32_t>(std::get<float>(raw_expression.value))};
           }},
          {{variable::TYPE::FLOAT, variable::TYPE::FLOAT}, [&] { result_expression = raw_expression; }},
      }};

  operations[raw_expression.is_const][{raw_expression.type, target_type}]();

  if (to_non_const && result_expression.is_const) {
    std::unordered_map<variable::TYPE, std::function<void()>> const_operations{
        {variable::TYPE::INT32,
         [&] {
           pl("%{} = add i32 0, {}", new_ir_cnt(), result_expression.to_string());
           result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
         }},
        {variable::TYPE::FLOAT, [&] {
           pl("%{} = fadd float 0., {}", new_ir_cnt(), result_expression.to_string());
           result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
         }}};
    const_operations[result_expression.type]();
  }

  return result_expression;
}

variable::TYPE visitor::get_common_type(variable::TYPE type1, variable::TYPE type2, std::string op) {
  std::set<std::string> arithmetic_operators{"+", "-", "*", "/", "%"};
  std::set<std::string> relation_operators{">=", ">", "<=", "<", "==", "!="};
  std::set<std::string> logic_operators{"&&", "||"};

  if (arithmetic_operators.contains(op))
    if (type1 == variable::TYPE::FLOAT || type2 == variable::TYPE::FLOAT)
      return variable::TYPE::FLOAT;
    else
      return variable::TYPE::INT32;
  else if (relation_operators.contains(op))
    return variable::TYPE::INT32;
  else if (logic_operators.contains(op))
    if (type1 == variable::TYPE::FLOAT || type2 == variable::TYPE::FLOAT)
      return variable::TYPE::FLOAT;
    else
      return variable::TYPE::INT32;
  else
    throw std::system_error(internal_error::unrecognized_operator, op);
}

std::any visitor::visitFunctionDefinition(sysy_parser::FunctionDefinitionContext *ctx) {
  auto raw_type_name{std::any_cast<std::string>(visit(ctx->functionType()))};
  auto type{function::to_type(raw_type_name)};
  auto type_name{function::to_string(type)};
  auto function_name{std::any_cast<std::string>(visit(ctx->IDENTIFIER()))};

  m_current_function_name = function_name;
  current_scope().insert_function(function_name, function{type});

  pl("define {} @{}() {{", type_name, function_name);
  m_indent++;
  visit(ctx->block());
  m_indent--;
  pl("}}");

  m_current_function_name.reset();
  return defaultResult();
}

std::any visitor::visitTerminal(antlr4::tree::terminal_node *ctx) {
  switch (ctx->getSymbol()->getType()) {
  case sysy_parser::INTEGER_CONSTANT:
    return static_cast<std::int32_t>(std::stoi(ctx->getText(), nullptr, 0));
  case sysy_parser::FLOATING_CONSTANT:
    return static_cast<float>(std::stof(ctx->getText()));
  case sysy_parser::IDENTIFIER:
    return ctx->getText();
  default:
    return ctx->getText();
  }
}

std::any visitor::visitLeftValue(sysy_parser::LeftValueContext *ctx) {
  // TODO: 加上对 expression 的处理（加入数组之后）
  return resolve_variable(std::any_cast<std::string>(visit(ctx->IDENTIFIER())));
}

std::any visitor::visitBlock(sysy_parser::BlockContext *ctx) {
  m_scopes.push_back(scope()); // TODO: 这里报错可能是因为编译器原因导致的，等待更新编译器
  for (auto item : ctx->blockItem())
    visit(item);
  m_scopes.pop_back();
  return defaultResult();
}

std::any visitor::visitLeftValueExpression(sysy_parser::LeftValueExpressionContext *ctx) {
  auto var{std::any_cast<variable>(visit(ctx->leftValue()))};
  if (var.is_const())
    return expression{true, var.type(), 0, var.value()};
  else {
    pl("%{} = load {}, ptr %{}", new_ir_cnt(), variable::to_string(var.type()), var.ir_cnt());
    return expression{false, var.type(), m_ir_cnt, {}};
  }
}

std::any visitor::visitIntegerConstantExpression(sysy_parser::IntegerConstantExpressionContext *ctx) {
  return expression{true, variable::TYPE::INT32, 0, std::any_cast<std::int32_t>(visit(ctx->INTEGER_CONSTANT()))};
}

std::any visitor::visitFloatingConstantExpression(sysy_parser::FloatingConstantExpressionContext *ctx) {
  return expression{true, variable::TYPE::FLOAT, 0, std::any_cast<float>(visit(ctx->FLOATING_CONSTANT()))};
}

std::any visitor::visitBraceExpression(sysy_parser::BraceExpressionContext *ctx) { return visit(ctx->expression()); }

std::any visitor::visitUnaryExpression(sysy_parser::UnaryExpressionContext *ctx) {
  auto op{ctx->op->getText()};

  auto raw_expression{std::any_cast<expression>(visit(ctx->expression()))};
  auto result_expression{expression{}};

  std::unordered_map<variable::TYPE, std::unordered_map<std::string, std::function<void()>>> operations[2]{
      {{variable::TYPE::INT32,
        {{"+",
          [&] {
            pl("%{} = mul i32 1, %{}", new_ir_cnt(), raw_expression.ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"-",
          [&] {
            pl("%{} = mul i32 -1, %{}", new_ir_cnt(), raw_expression.ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"!",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = icmp eq i32 0, %{}", tmp_ir_cnt, raw_expression.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }}}},
       {variable::TYPE::FLOAT,
        {{"+",
          [&] {
            pl("%{} = fmul float 1., %{}", new_ir_cnt(), raw_expression.ir_cnt);
            result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
          }},
         {"-",
          [&] {
            pl("%{} = fmul float -1., %{}", new_ir_cnt(), raw_expression.ir_cnt);
            result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
          }},
         {"!",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = fcmp oeq float 0., %{}", tmp_ir_cnt, raw_expression.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }}}}},
      {{variable::TYPE::INT32,
        {{"+", [&] { result_expression = raw_expression; }},
         {"-",
          [&] { result_expression = {true, variable::TYPE::INT32, 0, -std::get<std::int32_t>(raw_expression.value)}; }},
         {"!",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0, !std::get<std::int32_t>(raw_expression.value)};
          }}}},
       {variable::TYPE::FLOAT,
        {{"+", [&] { result_expression = raw_expression; }},
         {"-", [&] { result_expression = {true, variable::TYPE::FLOAT, 0, -std::get<float>(raw_expression.value)}; }},
         {"!",
          [&] { result_expression = {true, variable::TYPE::FLOAT, 0, !std::get<float>(raw_expression.value)}; }}}}}};

  if (auto search{operations[raw_expression.is_const][raw_expression.type].find(op)};
      search != operations[raw_expression.is_const][raw_expression.type].end())
    search->second();
  else
    throw std::system_error(internal_error::unrecognized_operator, op);

  return result_expression;
}

std::any visitor::visitBinaryArithmeticExpression(sysy_parser::BinaryArithmeticExpressionContext *ctx) {
  auto raw_expression_l{std::any_cast<expression>(visit(ctx->lhs))};
  auto raw_expression_r{std::any_cast<expression>(visit(ctx->rhs))};
  auto op{ctx->op->getText()};
  auto common_type{get_common_type(raw_expression_l.type, raw_expression_r.type, op)};
  auto result_expression{expression{.is_const = raw_expression_l.is_const && raw_expression_r.is_const}};

  raw_expression_l = expression_cast(raw_expression_l, common_type, !result_expression.is_const);
  raw_expression_r = expression_cast(raw_expression_r, common_type, !result_expression.is_const);

  std::unordered_map<variable::TYPE, std::unordered_map<std::string, std::function<void()>>> operations[2]{
      {{variable::TYPE::INT32,
        {{"+",
          [&] {
            pl("%{} = add i32 %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"-",
          [&] {
            pl("%{} = sub i32 %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"*",
          [&] {
            pl("%{} = mul i32 %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"/",
          [&] {
            pl("%{} = sdiv i32 %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"%",
          [&] {
            pl("%{} = srem i32 %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }}}},
       {variable::TYPE::FLOAT,
        {{"+",
          [&] {
            pl("%{} = fadd float %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
          }},
         {"-",
          [&] {
            pl("%{} = fsub float %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
          }},
         {"*",
          [&] {
            pl("%{} = fmul float %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
          }},
         {"/",
          [&] {
            pl("%{} = fdiv float %{}, %{}", new_ir_cnt(), raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            result_expression = {false, variable::TYPE::FLOAT, m_ir_cnt, {}};
          }}}}},
      {{variable::TYPE::INT32,
        {{"+",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) +
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"-",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) -
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"*",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) *
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"/",
          [&] {
            if (std::get<std::int32_t>(raw_expression_r.value) == 0)
              throw std::system_error(internal_error::divide_by_zero);
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) /
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"%",
          [&] {
            if (std::get<std::int32_t>(raw_expression_r.value) == 0)
              throw std::system_error(internal_error::modulo_by_zero);
            else
              result_expression = {true, variable::TYPE::INT32, 0,
                                   std::get<std::int32_t>(raw_expression_l.value) %
                                       std::get<std::int32_t>(raw_expression_r.value)};
          }}}},
       {variable::TYPE::FLOAT,
        {{"+",
          [&] {
            result_expression = {true, variable::TYPE::FLOAT, 0,
                                 std::get<float>(raw_expression_l.value) + std::get<float>(raw_expression_r.value)};
          }},
         {"-",
          [&] {
            result_expression = {true, variable::TYPE::FLOAT, 0,
                                 std::get<float>(raw_expression_l.value) - std::get<float>(raw_expression_r.value)};
          }},
         {"*",
          [&] {
            result_expression = {true, variable::TYPE::FLOAT, 0,
                                 std::get<float>(raw_expression_l.value) * std::get<float>(raw_expression_r.value)};
          }},
         {"/", [&] {
            if (std::get<float>(raw_expression_r.value) == 0)
              throw std::system_error(internal_error::divide_by_zero);
            result_expression = {true, variable::TYPE::FLOAT, 0,
                                 std::get<float>(raw_expression_l.value) / std::get<float>(raw_expression_r.value)};
          }}}}}};

  if (auto search{operations[result_expression.is_const][common_type].find(op)};
      search != operations[result_expression.is_const][common_type].end())
    search->second();
  else
    throw std::system_error(internal_error::unrecognized_operator, op);

  return result_expression;
}

std::any visitor::visitBinaryLogicExpression(sysy_parser::BinaryLogicExpressionContext *ctx) {

  auto raw_expression_l{std::any_cast<expression>(visit(ctx->lhs)).to_i1(*this)};
  auto op{ctx->op->getText()};
  auto result_expression{expression{}};

  std::unordered_map<std::string, std::function<void()>> operations{
      {"||",
       [&] {
         if (raw_expression_l.is_const)
           result_expression = std::get<std::int32_t>(raw_expression_l.value)
                                   ? expression{true, variable::TYPE::INT32, 0, true}
                                   : std::any_cast<expression>(visit(ctx->rhs));
         else {
           auto tmp_ir_cnt{new_ir_cnt()};
           auto label_true{new_label()}, label_false{new_label()}, label_end{new_label()};

           pl("br i1 %{}, label %{}, label %{}", raw_expression_l.to_string(), label_true, label_false);
           pl("");

           pl("{}:", label_true);
           pl("%{} = or i1 0, %{}", tmp_ir_cnt, raw_expression_l.to_string());
           pl("br label {}", label_end);
           pl("");

           pl("{}:", label_false);
           auto raw_expression_r{std::any_cast<expression>(visit(ctx->rhs)).to_i1(*this)};
           pl("%{} = or i1 {}, {}", tmp_ir_cnt, raw_expression_l.to_string(), raw_expression_r.to_string());
           pl("br label %{}", label_end);
           pl("");

           pl("{}:", label_end);
           pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);

           result_expression = {false, variable::TYPE::INT32, tmp_ir_cnt, {}};
         }
       }},
      {"&&", [&] {
         if (raw_expression_l.is_const)
           result_expression = std::get<std::int32_t>(raw_expression_l.value)
                                   ? std::any_cast<expression>(visit(ctx->rhs))
                                   : expression{true, variable::TYPE::INT32, 0, false};
         else {
           auto tmp_ir_cnt{new_ir_cnt()};
           auto label_true{new_label()}, label_false{new_label()}, label_end{new_label()};

           pl("br i1 %{}, label %{}, label %{}", raw_expression_l.to_string(), label_true, label_false);
           pl("");

           pl("{}:", label_true);
           pl("%{} = or i1 0, %{}", tmp_ir_cnt, raw_expression_l.to_string());
           pl("br label %{}", label_end);
           pl("");

           pl("{}:", label_false);
           auto raw_expression_r{std::any_cast<expression>(visit(ctx->rhs)).to_i1(*this)};
           pl("%{} = and i1 {}, {}", tmp_ir_cnt, raw_expression_l.to_string(), raw_expression_r.to_string());
           pl("br label %{}", label_end);
           pl("");

           pl("{}:", label_end);
           pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);

           result_expression = {false, variable::TYPE::INT32, tmp_ir_cnt, {}};
         }
       }}};
  if (auto search{operations.find(op)}; search != operations.end())
    search->second();
  else
    throw std::system_error(internal_error::unrecognized_operator, op);
  return result_expression;
}

std::any visitor::visitBinaryRelationExpression(sysy_parser::BinaryRelationExpressionContext *ctx) {
  auto raw_expression_l{std::any_cast<expression>(visit(ctx->lhs))};
  auto raw_expression_r{std::any_cast<expression>(visit(ctx->rhs))};
  auto op{ctx->op->getText()};
  auto common_type{get_common_type(raw_expression_l.type, raw_expression_r.type, op)};
  auto result_expression{expression{.is_const = raw_expression_l.is_const && raw_expression_r.is_const}};

  raw_expression_l = expression_cast(raw_expression_l, common_type, !result_expression.is_const);
  raw_expression_r = expression_cast(raw_expression_r, common_type, !result_expression.is_const);

  std::unordered_map<variable::TYPE, std::unordered_map<std::string, std::function<void()>>> operations[2]{
      {{variable::TYPE::INT32,
        {{">=",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = icmp sge i32 %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {">",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = icmp sgt i32 %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"<=",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = icmp sle i32 %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"<",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = icmp slt i32 %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"==",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = icmp eq i32 %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"!=",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = icmp ne i32 %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }}}},
       {variable::TYPE::FLOAT,
        {{">=",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = fcmp oge float %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {">",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = fcmp ogt float %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"<=",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = fcmp ole float %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"<",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = fcmp olt float %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"==",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = fcmp oeq float %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }},
         {"!=",
          [&] {
            auto tmp_ir_cnt{new_ir_cnt()};
            pl("%{} = fcmp one float %{}, %{}", tmp_ir_cnt, raw_expression_l.ir_cnt, raw_expression_r.ir_cnt);
            pl("%{} = sext i1 %{} to i32", new_ir_cnt(), tmp_ir_cnt);
            result_expression = {false, variable::TYPE::INT32, m_ir_cnt, {}};
          }}}}},
      {{variable::TYPE::INT32,
        {{">=",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) >=
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {">",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) >
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"<",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) <
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"<=",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) <=
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"==",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) ==
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }},
         {"!=",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<std::int32_t>(raw_expression_l.value) !=
                                     std::get<std::int32_t>(raw_expression_r.value)};
          }}}},
       {variable::TYPE::FLOAT,
        {{">=",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<float>(raw_expression_l.value) >= std::get<float>(raw_expression_r.value)};
          }},
         {">",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<float>(raw_expression_l.value) > std::get<float>(raw_expression_r.value)};
          }},
         {"<=",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<float>(raw_expression_l.value) < std::get<float>(raw_expression_r.value)};
          }},
         {"==",
          [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<float>(raw_expression_l.value) == std::get<float>(raw_expression_r.value)};
          }},
         {"!=", [&] {
            result_expression = {true, variable::TYPE::INT32, 0,
                                 std::get<float>(raw_expression_l.value) != std::get<float>(raw_expression_r.value)};
          }}}}}};

  if (auto search{operations[result_expression.is_const][common_type].find(op)};
      search != operations[result_expression.is_const][common_type].end())
    search->second();
  else
    throw std::system_error(internal_error::unrecognized_operator, op);

  return result_expression;
}

std::any visitor::visitExpressionStatement(sysy_parser::ExpressionStatementContext *ctx) {
  visit(ctx->expression()); // TODO: 测试如果没有 expression 会怎么样，即空语句
  return defaultResult();
}

std::any visitor::visitBlockStatement(sysy_parser::BlockStatementContext *ctx) {
  visit(ctx->block());
  return defaultResult();
}

std::any visitor::visitAssignmentStatement(sysy_parser::AssignmentStatementContext *ctx) {
  auto var{std::any_cast<variable>(visit(ctx->leftValue()))};
  auto value{std::any_cast<expression>(visit(ctx->expression()))};

  value = expression_cast(value, var.type());
  pl("store {} {}, ptr %{}", variable::to_string(var.type()), value.to_string(), var.ir_cnt());

  return defaultResult();
}

std::any visitor::visitReturnStatement(sysy_parser::ReturnStatementContext *ctx) {
  auto raw_expression{std::any_cast<expression>(visit(ctx->expression()))};
  auto return_type{resolve_function(m_current_function_name.value()).return_type()};
  if (return_type != function::TYPE::VOID) {
    auto target_type = function::to_variable_type(return_type);
    raw_expression = expression_cast(raw_expression, target_type);
    pl("ret {} {}", variable::to_string(target_type), raw_expression.to_string());
  }
  return defaultResult();
}

std::any visitor::visitIfStatement(sysy_parser::IfStatementContext *ctx) {
  bool have_else{ctx->statement().size() == 2};
  auto exp{std::any_cast<expression>(visit(ctx->expression())).to_i1(*this)};

  if (exp.is_const) {
    if (std::get<std::int32_t>(exp.value))
      visit(ctx->statement(0));
    else if (!std::get<std::int32_t>(exp.value) && have_else)
      visit(ctx->statement(1));
  } else {
    auto label_true{new_label()}, label_false{have_else ? new_label() : ""}, label_end{new_label()};

    pl("br i1 {}, label %{}, label %{}", exp.to_string(), label_true, have_else ? label_false : label_end);
    pl("");

    pl("{}:", label_true);
    visit(ctx->statement(0));
    pl("br label %{}", label_end);
    pl("");

    if (have_else) {
      pl("{}:", label_false);
      visit(ctx->statement(1));
      pl("br label %{}", label_end);
      pl("");
    }

    pl("{}:", label_end);
  }

  return defaultResult();
}

std::any visitor::visitConstDeclaration(sysy_parser::ConstDeclarationContext *ctx) {
  auto type{variable::to_type(std::any_cast<std::string>(visit(ctx->basicType())))};

  for (auto child : ctx->constDefinition()) {
    auto [name, value]{std::any_cast<std::tuple<std::string, expression>>(visit(child))};
    value = expression_cast(value, type);
    current_scope().insert_variable(name, {type, value.value});
  }

  return defaultResult();
}

std::any visitor::visitConstDefinition(sysy_parser::ConstDefinitionContext *ctx) {
  auto identifier_name{std::any_cast<std::string>(visit(ctx->IDENTIFIER()))};
  // TODO: 此处返回的不一定是 expression 类型（加入数组之后）
  auto value{std::any_cast<expression>(visit(ctx->constInitializeValue()))};
  return std::tuple{identifier_name, value};
}

std::any visitor::visitConstInitializeValue(sysy_parser::ConstInitializeValueContext *ctx) {
  auto value{std::any_cast<expression>(visit(ctx->expression()))};
  if (!value.is_const)
    throw std::system_error(internal_error::expect_const_expression, ctx->getText());
  return value;
}

std::any visitor::visitVariableDeclaration(sysy_parser::VariableDeclarationContext *ctx) {
  auto type{variable::to_type(std::any_cast<std::string>(visit(ctx->basicType())))};

  for (auto child : ctx->variableDefinition()) {
    auto [name, value]{std::any_cast<std::tuple<std::string, expression>>(visit(child))};
    value = expression_cast(value, type);
    current_scope().insert_variable(name, {type, new_ir_cnt()});
    pl("%{} = alloca {}", m_ir_cnt, variable::to_string(type));
    pl("store {} {}, ptr %{}", variable::to_string(type), value.to_string(), m_ir_cnt);
  }

  return defaultResult();
}

std::any visitor::visitVariableDefinition(sysy_parser::VariableDefinitionContext *ctx) {
  auto identifier_name{std::any_cast<std::string>(visit(ctx->IDENTIFIER()))};
  // TODO: 此处返回的不一定是 expression 类型（加入数组之后）
  auto value{std::any_cast<expression>(visit(ctx->initializeValue()))};
  return std::tuple{identifier_name, value};
}

std::any visitor::visitInitializeValue(sysy_parser::InitializeValueContext *ctx) { return visit(ctx->expression()); }
