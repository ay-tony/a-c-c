export module error_handler;

import std;
import metadata;
import antlr4_runtime;

export enum class internal_error {
  success = 0,
  invalid_command_line_argument,
  invalid_function_return_value_type_str,
  invalid_variable_type_str,
  variable_redefinition,
  function_redefinition,
  invalid_variable_name,
  invalid_function_name,
  function_return_type_void_conversion,
  unrecognized_operator,
  syntax_error,
  divide_by_zero,
  modulo_by_zero,
  failed
};

export std::error_code make_error_code(internal_error e) noexcept {
  static const struct : std::error_category {
    virtual const char *name() const noexcept override { return metadata::name.c_str(); }

    virtual std::string message(int code) const override {
      switch (static_cast<internal_error>(code)) {
      case internal_error::success:
        return "Success";
      case internal_error::invalid_command_line_argument:
        return "Invalid command line argument";
      case internal_error::invalid_function_return_value_type_str:
        return "Invalid function return value type string conversion";
      case internal_error::invalid_variable_type_str:
        return "Invalid variable type string conversion";
      case internal_error::variable_redefinition:
        return "Variable redefinition";
      case internal_error::function_redefinition:
        return "Function redefinition";
      case internal_error::invalid_variable_name:
        return "Invalid variable name";
      case internal_error::invalid_function_name:
        return "Invalid function name";
      case internal_error::function_return_type_void_conversion:
        return "Function return type is void, cannot convert to variable type";
      case internal_error::unrecognized_operator:
        return "Unrecognized operator";
      case internal_error::syntax_error:
        return "Syntax error";
      case internal_error::divide_by_zero:
        return "Divide by zero";
      case internal_error::modulo_by_zero:
        return "Modulo by zero";
      case internal_error::failed:
        return "Failed";
      default:
        break;
      }
      return "Unknown";
    }

    virtual std::error_condition default_error_condition(int code) const noexcept override {
      switch (static_cast<internal_error>(code)) {
      case internal_error::invalid_command_line_argument:
        return std::errc::invalid_argument;
        break;

      default:
        return {};
      }
    }

    virtual bool equivalent(int code, const std::error_condition &condition) const noexcept override {
      return default_error_condition(code) == condition;
    }

    virtual bool equivalent(const std::error_code &code, int condition) const noexcept override {
      return (*this == code.category()) && (code.value() == condition);
    }
  } category;
  return std::error_code(static_cast<int>(e), category);
};

namespace std {
export template <> struct is_error_code_enum<internal_error> : true_type {};
}; // namespace std

export class lgcc_error_listener : public antlr4::base_error_listener {
  std::string m_file_name;

  void syntaxError(antlr4::recognizer *recognizer, antlr4::token *offendingSymbol, std::size_t line, std::size_t charPositionInLine,
                   const std::string &msg, std::exception_ptr e) override {
    auto error_message{std::format("{}:{}:{}: {}", m_file_name, line, charPositionInLine, msg)};
    throw std::system_error(internal_error::syntax_error, error_message);
  }

public:
  lgcc_error_listener(std::string file_name) : m_file_name(file_name) {};
};
