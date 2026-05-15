#include "interpreter.h"

#include "expression_evaluator.h"
#include "lexer/lexer.h"
#include "runtime_support.h"
#include "token_utils.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

namespace GameLang {
namespace {

std::vector<std::string> parseAssignmentTargets(const std::vector<Token>& tokens);

} // namespace

Interpreter::Interpreter()
    : randomEngine(std::random_device{}()) {
    variables["T"] = RuntimeValue(true);
    variables["F"] = RuntimeValue(false);
    variables["nil"] = RuntimeValue();
}

std::mt19937& Interpreter::rng() {
    return randomEngine;
}

bool Interpreter::hasVariable(const std::string& name) const {
    return variables.find(name) != variables.end();
}

RuntimeValue Interpreter::getVariable(const std::string& name) const {
    auto found = variables.find(name);
    if (found == variables.end()) return RuntimeValue();
    return found->second;
}

void Interpreter::setVariable(const std::string& name, const RuntimeValue& value) {
    variables[name] = value;
}

std::vector<RuntimeValue> Interpreter::execute(const std::string& source, bool echoExpressionResults) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scanTokens();
    std::vector<RuntimeValue> results;
    std::vector<Token> statement;
    int depth = 0;

    auto flush = [&]() {
        std::vector<Token> trimmed = trimStatement(statement);
        statement.clear();
        if (!trimmed.empty()) {
            results.push_back(executeStatement(trimmed, echoExpressionResults));
        }
    };

    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];
        if (token.type == TokenType::ERROR) {
            throw std::runtime_error("Cannot execute source with lexer errors");
        }
        if (token.type == TokenType::EOF_TOKEN) {
            flush();
            break;
        }

        if (depth == 0 && (token.type == TokenType::NEWLINE || token.type == TokenType::SEMICOLON)) {
            TokenType next = nextSignificantType(tokens, i);
            if (token.type == TokenType::NEWLINE && !statement.empty() &&
                (next == TokenType::PIPELINE || next == TokenType::PARALLEL_MAP)) {
                statement.push_back(token);
                continue;
            }
            flush();
            continue;
        }

        statement.push_back(token);
        depth += topLevelDepthDelta(token.type);
        if (depth < 0) depth = 0;
    }

    return results;
}

RuntimeValue Interpreter::evaluateExpressionSource(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scanTokens();
    std::vector<Token> expressionTokens;
    for (const Token& token : tokens) {
        if (token.type == TokenType::EOF_TOKEN) break;
        expressionTokens.push_back(token);
    }
    return evaluateTokens(expressionTokens);
}

std::string Interpreter::debugStatements(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scanTokens();
    std::ostringstream out;
    std::vector<Token> statement;
    int depth = 0;
    int index = 1;

    auto flush = [&]() {
        std::vector<Token> trimmed = trimStatement(statement);
        statement.clear();
        if (trimmed.empty()) return;
        out << "statement " << index++ << ":";
        for (const Token& token : trimmed) {
            out << " " << tokenTypeToString(token.type);
            if (!token.lexeme.empty()) out << "('" << token.lexeme << "')";
        }
        out << "\n";
    };

    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];
        if (token.type == TokenType::EOF_TOKEN) {
            flush();
            break;
        }
        if (depth == 0 && (token.type == TokenType::NEWLINE || token.type == TokenType::SEMICOLON)) {
            TokenType next = nextSignificantType(tokens, i);
            if (token.type == TokenType::NEWLINE && !statement.empty() &&
                (next == TokenType::PIPELINE || next == TokenType::PARALLEL_MAP)) {
                statement.push_back(token);
                continue;
            }
            flush();
            continue;
        }
        statement.push_back(token);
        depth += topLevelDepthDelta(token.type);
        if (depth < 0) depth = 0;
    }

    return out.str();
}

RuntimeValue Interpreter::executeStatement(const std::vector<Token>& rawStatement, bool echoExpressionResults) {
    std::vector<Token> statement = trimStatement(rawStatement);
    if (statement.empty()) return RuntimeValue();

    if (statement[0].type == TokenType::IDENTIFIER && statement[0].lexeme == "assert") {
        std::vector<Token> expressionTokens(statement.begin() + 1, statement.end());
        RuntimeValue condition = evaluateTokens(expressionTokens);
        if (!isTruthy(condition)) {
            throw std::runtime_error("Assertion failed: " + valueToString(condition));
        }
        return RuntimeValue(true);
    }

    size_t assignIndex = findTopLevelToken(statement, TokenType::ASSIGN_OP);
    if (assignIndex != std::string::npos) {
        std::vector<Token> left(statement.begin(), statement.begin() + static_cast<long>(assignIndex));
        std::vector<Token> right(statement.begin() + static_cast<long>(assignIndex + 1), statement.end());
        RuntimeValue value = evaluateTokens(right);

        std::vector<std::string> names = parseAssignmentTargets(left);
        if (names.empty()) throw std::runtime_error("Assignment is missing a target");

        if (names.size() == 1) {
            setVariable(names[0], value);
        } else {
            RuntimeList values = asList(value, "unpacking assignment");
            if (values.size() != names.size()) {
                throw std::runtime_error("Unpacking assignment expected " + std::to_string(names.size()) +
                                         " values but got " + std::to_string(values.size()));
            }
            for (size_t i = 0; i < names.size(); ++i) setVariable(names[i], values[i]);
        }
        return value;
    }

    RuntimeValue value = evaluateTokens(statement);
    if (echoExpressionResults && !value.isNil()) {
        std::cout << valueToString(value) << "\n";
    }
    return value;
}

RuntimeValue Interpreter::evaluateTokensWithTemporaryBindings(
    const std::vector<Token>& expressionTokens,
    const std::vector<std::pair<std::string, RuntimeValue>>& bindings) {
    std::vector<std::pair<std::string, RuntimeValue>> oldValues;
    std::vector<std::string> missing;
    for (const auto& [name, value] : bindings) {
        auto found = variables.find(name);
        if (found == variables.end()) missing.push_back(name);
        else oldValues.push_back({name, found->second});
        variables[name] = value;
    }

    try {
        RuntimeValue result = evaluateTokens(expressionTokens);
        for (const std::string& name : missing) variables.erase(name);
        for (const auto& [name, value] : oldValues) variables[name] = value;
        return result;
    } catch (...) {
        for (const std::string& name : missing) variables.erase(name);
        for (const auto& [name, value] : oldValues) variables[name] = value;
        throw;
    }
}

RuntimeValue Interpreter::evaluateTokens(const std::vector<Token>& rawTokens) {
    std::vector<Token> tokens = trimStatement(rawTokens);
    if (tokens.empty()) return RuntimeValue();

    size_t whereIndex = findTopLevelToken(tokens, TokenType::WHERE);
    if (whereIndex != std::string::npos) {
        std::vector<Token> left(tokens.begin(), tokens.begin() + static_cast<long>(whereIndex));
        std::vector<Token> right(tokens.begin() + static_cast<long>(whereIndex + 1), tokens.end());
        std::vector<std::pair<std::string, RuntimeValue>> bindings;

        for (const std::vector<Token>& part : splitTopLevel(right, TokenType::COMMA)) {
            if (part.empty()) continue;
            size_t assignIndex = findTopLevelToken(part, TokenType::ASSIGN_OP);
            if (assignIndex == std::string::npos || assignIndex == 0 || part[0].type != TokenType::IDENTIFIER) {
                throw std::runtime_error("where bindings must look like name := expression");
            }
            std::string name = part[0].lexeme;
            std::vector<Token> expression(part.begin() + static_cast<long>(assignIndex + 1), part.end());
            RuntimeValue value = evaluateTokensWithTemporaryBindings(expression, bindings);
            bindings.push_back({name, value});
        }

        return evaluateTokensWithTemporaryBindings(left, bindings);
    }

    return evaluateExpression(*this, tokens);
}

namespace {

std::vector<std::string> parseAssignmentTargets(const std::vector<Token>& tokens) {
    std::vector<std::string> names;
    std::vector<Token> currentName;
    for (const Token& token : tokens) {
        if (token.type == TokenType::COMMA) {
            if (currentName.size() != 1 || !isNameToken(currentName[0].type)) {
                throw std::runtime_error("Invalid assignment target");
            }
            names.push_back(currentName[0].lexeme);
            currentName.clear();
        } else if (token.type != TokenType::NEWLINE) {
            currentName.push_back(token);
        }
    }
    if (!currentName.empty()) {
        if (currentName.size() != 1 || !isNameToken(currentName[0].type)) {
            throw std::runtime_error("Invalid assignment target");
        }
        names.push_back(currentName[0].lexeme);
    }
    return names;
}

} // namespace

} // namespace GameLang
