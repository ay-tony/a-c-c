module visitor;

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

void visitor::convert_expression(variable::TYPE from_type, variable::ir_cnt_t from_ir_cnt, variable::TYPE to_type,
                                 variable::ir_cnt_t to_ir_cnt) {
  auto from_type_name = variable::to_string(from_type);
  auto to_type_name = variable::to_string(to_type);
  switch (from_type) {
  case variable::TYPE::INT32:
    switch (to_type) {
    case variable::TYPE::INT32:
      pl("%{} = %{}", to_ir_cnt, from_ir_cnt);
      return;
    case variable::TYPE::FLOAT:
      pl("%{} = sitofp {} %{} to {}", to_ir_cnt, from_type_name, from_ir_cnt, to_type_name);
      return;
    }
  case variable::TYPE::FLOAT:
    switch (to_type) {
    case variable::TYPE::INT32:
      pl("%{} = fptosi {} %{} to {}", to_ir_cnt, from_type_name, from_ir_cnt, to_type_name);
      return;
    case variable::TYPE::FLOAT:
      pl("%{} = %{}", to_ir_cnt, from_ir_cnt);
      return;
    }
  }
}

variable::TYPE visitor::get_common_type(variable::TYPE type1, variable::TYPE type2) {
  if (type1 == variable::TYPE::FLOAT || type2 == variable::TYPE::FLOAT)
    return variable::TYPE::FLOAT;
  return variable::TYPE::INT32;
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

std::any visitor::visitIntegerConstantExpression(sysy_parser::IntegerConstantExpressionContext *ctx) {
  pl("%{} = add i32 0, {}", m_ir_cnt++, std::any_cast<std::int32_t>(visit(ctx->INTEGER_CONSTANT())));
  return expression{m_ir_cnt - 1, variable::TYPE::INT32};
}

std::any visitor::visitBraceExpression(sysy_parser::BraceExpressionContext *ctx) { return visit(ctx->expression()); }

std::any visitor::visitUnaryExpression(sysy_parser::UnaryExpressionContext *ctx) {
  auto op{ctx->op->getText()};

  auto [ir_cnt, type] = std::any_cast<expression>(visit(ctx->expression()));
  auto cur_ir_cnt = m_ir_cnt++;

  std::unordered_map<std::string, std::function<void()>> operations_int32{
      {"+", [&] { pl("%{} = mul i32 1, %{}", cur_ir_cnt, ir_cnt); }},
      {"-", [&] { pl("%{} = mul i32 -1, %{}", cur_ir_cnt, ir_cnt); }},
      {"!", [&] {
         auto tmp_ir_cnt{cur_ir_cnt};
         cur_ir_cnt = m_ir_cnt++;
         pl("%{} = icmp eq i32 0, %{}", tmp_ir_cnt, ir_cnt);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt, tmp_ir_cnt);
       }}};

  std::unordered_map<std::string, std::function<void()>> operations_float{
      {"+", [&] { pl("%{} = fmul float 1., %{}", cur_ir_cnt, ir_cnt); }},
      {"-", [&] { pl("%{} = fmul float -1., %{}", cur_ir_cnt, ir_cnt); }},
      {"!", [&] {
         auto tmp_ir_cnt{cur_ir_cnt};
         cur_ir_cnt = m_ir_cnt++;
         pl("%{} = fcmp oeq float 0., %{}", tmp_ir_cnt, ir_cnt);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt, tmp_ir_cnt);
         type = variable::TYPE::INT32;
       }}};

  switch (type) {
  case variable::TYPE::INT32:
    if (auto search{operations_int32.find(op)}; search != operations_int32.end())
      search->second();
    else
      throw std::system_error(internal_error::unrecognized_operator, op);
    break;

  case variable::TYPE::FLOAT:
    if (auto search{operations_int32.find(op)}; search != operations_float.end())
      search->second();
    else
      throw std::system_error(internal_error::unrecognized_operator, op);
    break;
  }

  return expression{cur_ir_cnt, type};
}

std::any visitor::visitBinaryExpression(sysy_parser::BinaryExpressionContext *ctx) {
  auto [ir_cnt_l, type_l]{std::any_cast<expression>(visit(ctx->lhs))};
  auto [ir_cnt_r, type_r]{std::any_cast<expression>(visit(ctx->rhs))};
  auto new_ir_cnt_l{ir_cnt_l}, new_ir_cnt_r{ir_cnt_r};
  auto common_type{get_common_type(type_l, type_r)};
  auto op{ctx->op->getText()};

  if (type_l != common_type) {
    new_ir_cnt_l = m_ir_cnt++;
    convert_expression(type_l, ir_cnt_l, common_type, new_ir_cnt_l);
  }
  if (type_r != common_type) {
    new_ir_cnt_r = m_ir_cnt++;
    convert_expression(type_r, ir_cnt_r, common_type, new_ir_cnt_r);
  }

  auto cur_ir_cnt{m_ir_cnt++};

  std::unordered_map<std::string, std::function<void()>> operations_int32{
      {"+", [&] { pl("%{} = add i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {"-", [&] { pl("%{} = sub i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {"*", [&] { pl("%{} = mul i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {"/", [&] { pl("%{} = sdiv i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {"%", [&] { pl("%{} = srem i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {">=",
       [&] {
         pl("%{} = icmp sge i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {">",
       [&] {
         pl("%{} = icmp sgt i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"<=",
       [&] {
         pl("%{} = icmp sle i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"<",
       [&] {
         pl("%{} = icmp slt i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"==",
       [&] {
         pl("%{} = icmp eq i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"!=",
       [&] {
         pl("%{} = icmp ne i32 %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
  };

  std::unordered_map<std::string, std::function<void()>> operations_float{
      {"+", [&] { pl("%{} = fadd float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {"-", [&] { pl("%{} = fsub float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {"*", [&] { pl("%{} = fmul float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {"/", [&] { pl("%{} = fdiv float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r); }},
      {">=",
       [&] {
         pl("%{} = fcmp oge float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {">",
       [&] {
         pl("%{} = fcmp ogt float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"<=",
       [&] {
         pl("%{} = fcmp ole float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"<",
       [&] {
         pl("%{} = fcmp olt float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"==",
       [&] {
         pl("%{} = fcmp oeq float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
      {"!=",
       [&] {
         pl("%{} = fcmp one float %{}, %{}", cur_ir_cnt, new_ir_cnt_l, new_ir_cnt_r);
         pl("%{} = sext i1 %{} to i32", cur_ir_cnt + 1, cur_ir_cnt);
         cur_ir_cnt = m_ir_cnt++;
         common_type = variable::TYPE::INT32;
       }},
  };

  switch (common_type) {
  case variable::TYPE::INT32:
    if (auto search{operations_int32.find(op)}; search != operations_int32.end())
      search->second();
    else
      throw std::system_error(internal_error::unrecognized_operator, op);
    break;

  case variable::TYPE::FLOAT:
    if (auto search{operations_float.find(op)}; search != operations_float.end())
      search->second();
    else
      throw std::system_error(internal_error::unrecognized_operator, op);
    break;
  }

  return expression{cur_ir_cnt, common_type};
}

std::any visitor::visitReturnStatement(sysy_parser::ReturnStatementContext *ctx) {
  auto [ir_cnt, type]{std::any_cast<expression>(visit(ctx->expression()))};
  auto return_type{resolve_function(m_current_function_name.value()).return_type()};
  if (return_type != function::TYPE::VOID) {
    auto target_type = function::to_variable_type(return_type);
    if (target_type != type) {
      auto target_ir_cnt = m_ir_cnt++;
      convert_expression(type, ir_cnt, target_type, target_ir_cnt);
      ir_cnt = target_ir_cnt;
    }
    auto variable_type_name{variable::to_string(target_type)};
    pl("ret {} %{}", variable_type_name, ir_cnt);
  }
  return defaultResult();
}
