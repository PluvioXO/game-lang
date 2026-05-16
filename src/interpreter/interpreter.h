#ifndef GAMELANG_INTERPRETER_H
#define GAMELANG_INTERPRETER_H

#include "runtime_value.h"
#include "lexer/token.h"

#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameLang {

class Interpreter {
public:
    Interpreter();

    std::vector<RuntimeValue> execute(const std::string& source, bool echoExpressionResults = false);
    RuntimeValue evaluateExpressionSource(const std::string& source);
    std::string debugStatements(const std::string& source);

    RuntimeValue callFunction(const std::string& name, const std::vector<CallArg>& args, bool parallel = false);
    RuntimeValue callValue(const RuntimeValue& callable, const std::vector<CallArg>& args, bool parallel = false);

    bool hasVariable(const std::string& name) const;
    RuntimeValue getVariable(const std::string& name) const;
    void setVariable(const std::string& name, const RuntimeValue& value);

    std::mt19937& rng();

    RuntimeValue evaluateTokens(const std::vector<Token>& expressionTokens);
    RuntimeValue evaluateTokensWithTemporaryBindings(
        const std::vector<Token>& expressionTokens,
        const std::vector<std::pair<std::string, RuntimeValue>>& bindings);

private:
    std::unordered_map<std::string, RuntimeValue> variables;
    std::mt19937 randomEngine;

    RuntimeValue executeStatement(const std::vector<Token>& statement, bool echoExpressionResults);
    std::vector<RuntimeValue> executeTokenBlock(const std::vector<Token>& block, bool echoExpressionResults);
};

} // namespace GameLang

#endif // GAMELANG_INTERPRETER_H
