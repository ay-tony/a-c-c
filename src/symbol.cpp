module symbol;

variable::TYPE variable::to_type(std::string_view str) {
  if (str == "int")
    return TYPE::INT32;
  else if (str == "float")
    return TYPE::FLOAT;
  else
    throw std::system_error(internal_error::invalid_variable_type_str);
}

std::string variable::to_string(TYPE type) {
  using namespace std::literals::string_literals;
  switch (type) {
  case TYPE::INT32:
    return "i32"s;
  case TYPE::FLOAT:
    return "float"s;
  }
}

function::TYPE function::to_type(std::string_view str) {
  if (str == "void")
    return TYPE::VOID;
  else if (str == "int")
    return TYPE::INT32;
  else if (str == "float")
    return TYPE::FLOAT;
  else
    throw std::system_error(internal_error::invalid_function_return_value_type_str);
}

std::string function::to_string(TYPE type) {
  using namespace std::literals::string_literals;
  switch (type) {
  case TYPE::VOID:
    return "void"s;
  case TYPE::INT32:
    return "i32"s;
  case TYPE::FLOAT:
    return "float"s;
  }
}

variable::TYPE function::to_variable_type(TYPE type) {
  switch (type) {
  case TYPE::INT32:
    return variable::TYPE::INT32;
  case TYPE::FLOAT:
    return variable::TYPE::FLOAT;
  case TYPE::VOID:
    throw std::system_error(internal_error::function_return_type_void_conversion);
  }
}

void scope::insert_variable(const std::string &name, const variable &sym) {
  if (variable_table.contains(name))
    throw std::system_error(internal_error::variable_redefinition, name);
  variable_table.emplace(name, std::make_unique<variable>(sym));
}

std::optional<variable> scope::resolve_variable(const std::string &name) {
  if (auto it = variable_table.find(name); it != variable_table.end())
    return *(it->second);
  return std::nullopt;
}

void scope::insert_function(const std::string &name, const function &sym) {
  if (function_table.contains(name))
    throw std::system_error(internal_error::function_redefinition, name);
  function_table.emplace(name, std::make_unique<function>(sym));
}

std::optional<function> scope::resolve_function(const std::string &name) {
  if (auto it = function_table.find(name); it != function_table.end())
    return *(it->second);
  return std::nullopt;
}
