export module symbol;

import std;
import error_handler;

export class variable {
public:
  using ir_cnt_t = std::uint32_t; // TODO: 本语句放在哪里有待商榷
  enum class TYPE { INT32, FLOAT };

  static TYPE to_type(std::string_view str);
  static std::string to_string(TYPE type);

private:
  ir_cnt_t m_ir_cnt;
  TYPE m_type;
  bool m_isconst;

public:
  variable(std::uint32_t ir_cnt, TYPE type, bool isconst) : m_ir_cnt{ir_cnt}, m_type{type}, m_isconst{isconst} {}
  variable(const variable &func) = default;

  ir_cnt_t ir_cnt() const { return m_ir_cnt; }
  TYPE type() const { return m_type; }
  bool is_const() const { return m_isconst; }
};

export class function {
public:
  enum class TYPE { VOID, INT32, FLOAT };

  static TYPE to_type(std::string_view str);
  static std::string to_string(TYPE type);
  static variable::TYPE to_variable_type(TYPE type);

private:
  TYPE m_return_type;

public:
  function(TYPE return_type) : m_return_type{return_type} {}
  function(const function &func) = default;

  TYPE return_type() const { return m_return_type; }
};

export class scope {
private:
  std::map<std::string, std::unique_ptr<variable>> variable_table;
  std::map<std::string, std::unique_ptr<function>> function_table;

public:
  scope() = default;

  void insert_variable(const std::string &name, const variable &sym);
  std::optional<variable> resolve_variable(const std::string &name);

  void insert_function(const std::string &name, const function &sym);
  std::optional<function> resolve_function(const std::string &name);
};
