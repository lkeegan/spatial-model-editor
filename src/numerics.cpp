#include "numerics.hpp"

#include "logger.hpp"

namespace numerics {

[[noreturn]] static void throwSymbolTableError(
    const exprtk::symbol_table<double> &symbolTable,
    const std::string &symbol) {
  std::string errorMessage = "ExprEval::ExprEval : symbol table error: '";
  errorMessage.append(symbol);
  errorMessage.append("'");
  if (symbolTable.symbol_exists(symbol)) {
    errorMessage.append(" already exists");
  }
  spdlog::critical(errorMessage);
  throw std::invalid_argument(errorMessage);
}

ExprEval::ExprEval(const std::string &expression,
                   const std::vector<std::string> &variableName,
                   std::vector<double> &variableValue,
                   const std::map<std::string, double> &constants) {
  // construct symbol table of variables and constants
  exprtk::symbol_table<double> exprtkSymbolTable;
  spdlog::debug("ExprEval::ExprEval : compiling {}", expression);
  for (std::size_t i = 0; i < variableName.size(); ++i) {
    if (!exprtkSymbolTable.add_variable(variableName[i], variableValue[i])) {
      throwSymbolTableError(exprtkSymbolTable, variableName[i]);
    }
  }
  for (const auto &v : constants) {
    if (!exprtkSymbolTable.add_constant(v.first, v.second)) {
      throwSymbolTableError(exprtkSymbolTable, v.first);
    }
  }
  exprtkExpression.register_symbol_table(exprtkSymbolTable);
  // compile expression
  exprtk::parser<double> exprtkParser;
  exprtkParser.settings().disable_all_control_structures();
  exprtkParser.settings().disable_local_vardef();
  exprtkParser.settings().disable_all_logic_ops();
  exprtkParser.settings().disable_all_inequality_ops();
  exprtkParser.settings().disable_all_assignment_ops();
  // todo: try to make the parser more compatible with libSBML
  // http://sbml.org/Software/libSBML/5.12.0/docs/formatted/cpp-api/libsbml-math.html
  exprtkParser.dec().collect_variables() = true;
  if (!exprtkParser.compile(expression, exprtkExpression)) {
    std::string errorMessage = "ExprEval::ExprEval : compilation error: ";
    errorMessage.append(exprtkParser.error());
    spdlog::critical(errorMessage);
    throw std::invalid_argument(errorMessage);
  }
  // get list of variables the expression depends on
  exprtk::parser<double>::dependent_entity_collector::symbol_t symbolPair;
  std::deque<exprtk::parser<double>::dependent_entity_collector::symbol_t>
      symbolPairList;
  exprtkParser.dec().symbols(symbolPairList);
  std::string strListSymbols = "";
  for (const auto &pair : symbolPairList) {
    if (pair.second == exprtk::parser<double>::e_st_variable) {
      QString value;
      const auto iter = constants.find(pair.first);
      if (iter != constants.cend()) {
        value = QString::number(iter->second);
      }
      strListSymbols.append(
          QString(" %1 (%2),").arg(pair.first.c_str(), value).toStdString());
    }
  }
  spdlog::debug("ExprEval::ExprEval :   - symbols used: {}", strListSymbols);
}

double ExprEval::operator()() const { return exprtkExpression.value(); }

std::vector<std::string> getSymbols(const std::string &expression) {
  std::vector<std::string> symbols;
  exprtk::collect_variables(expression, symbols);
  return symbols;
}

}  // namespace numerics
