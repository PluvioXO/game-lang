#ifndef GAMELANG_EXPRESSION_EVALUATOR_H
#define GAMELANG_EXPRESSION_EVALUATOR_H

#include "runtime_value.h"
#include "lexer/token.h"

#include <vector>

namespace GameLang {

class Interpreter;

RuntimeValue evaluateExpression(Interpreter& interpreter, const std::vector<Token>& tokens);

} // namespace GameLang

#endif // GAMELANG_EXPRESSION_EVALUATOR_H
