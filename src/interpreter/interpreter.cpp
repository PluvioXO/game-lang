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
size_t findMatchingBrace(const std::vector<Token>& tokens, size_t openIndex);
size_t findTopLevelLeftBrace(const std::vector<Token>& tokens, size_t start);
RuntimeList iterableItemsForControl(const RuntimeValue& value);

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

RuntimeValue Interpreter::callValue(const RuntimeValue& callable, const std::vector<CallArg>& args, bool parallel) {
    if (callable.isLambda()) {
        const auto& lambda = *std::get<RuntimeValue::LambdaPtr>(callable.data);
        std::vector<std::pair<std::string, RuntimeValue>> bindings;
        for (size_t i = 0; i < lambda.parameters.size(); ++i) {
            RuntimeValue value = argAt(args, i, RuntimeValue());
            const RuntimeValue* named = findNamedArg(args, lambda.parameters[i]);
            if (named) value = *named;
            bindings.push_back({lambda.parameters[i], value});
        }
        return evaluateTokensWithTemporaryBindings(lambda.body, bindings);
    }
    return callFunction(asString(callable), args, parallel);
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

    if (statement[0].type == TokenType::RETURN) {
        std::vector<Token> expressionTokens(statement.begin() + 1, statement.end());
        return evaluateTokens(expressionTokens);
    }

    if (statement[0].type == TokenType::IDENTIFIER && statement[0].lexeme == "assert") {
        std::vector<Token> expressionTokens(statement.begin() + 1, statement.end());
        RuntimeValue condition = evaluateTokens(expressionTokens);
        if (!isTruthy(condition)) {
            throw std::runtime_error("Assertion failed: " + valueToString(condition));
        }
        return RuntimeValue(true);
    }

    if (statement[0].type == TokenType::IF) {
        size_t thenOpen = findTopLevelLeftBrace(statement, 1);
        if (thenOpen == std::string::npos) throw std::runtime_error("if statement expected a '{' block");
        size_t thenClose = findMatchingBrace(statement, thenOpen);
        std::vector<Token> condition(statement.begin() + 1, statement.begin() + static_cast<long>(thenOpen));
        std::vector<Token> thenBlock(statement.begin() + static_cast<long>(thenOpen + 1),
                                     statement.begin() + static_cast<long>(thenClose));
        RuntimeValue result;
        if (isTruthy(evaluateTokens(condition))) {
            std::vector<RuntimeValue> results = executeTokenBlock(thenBlock, echoExpressionResults);
            if (!results.empty()) result = results.back();
            return result;
        }

        size_t next = thenClose + 1;
        while (next < statement.size() && statement[next].type == TokenType::NEWLINE) ++next;
        if (next < statement.size() && statement[next].type == TokenType::ELSE) {
            size_t elseOpen = findTopLevelLeftBrace(statement, next + 1);
            if (elseOpen == std::string::npos) throw std::runtime_error("else statement expected a '{' block");
            size_t elseClose = findMatchingBrace(statement, elseOpen);
            std::vector<Token> elseBlock(statement.begin() + static_cast<long>(elseOpen + 1),
                                         statement.begin() + static_cast<long>(elseClose));
            std::vector<RuntimeValue> results = executeTokenBlock(elseBlock, echoExpressionResults);
            if (!results.empty()) result = results.back();
        }
        return result;
    }

    if (statement[0].type == TokenType::WHILE) {
        size_t bodyOpen = findTopLevelLeftBrace(statement, 1);
        if (bodyOpen == std::string::npos) throw std::runtime_error("while statement expected a '{' block");
        size_t bodyClose = findMatchingBrace(statement, bodyOpen);
        std::vector<Token> condition(statement.begin() + 1, statement.begin() + static_cast<long>(bodyOpen));
        std::vector<Token> body(statement.begin() + static_cast<long>(bodyOpen + 1),
                                statement.begin() + static_cast<long>(bodyClose));
        RuntimeValue result;
        int guard = 0;
        while (isTruthy(evaluateTokens(condition))) {
            std::vector<RuntimeValue> results = executeTokenBlock(body, echoExpressionResults);
            if (!results.empty()) result = results.back();
            if (++guard > 100000) throw std::runtime_error("while loop exceeded 100000 iterations");
        }
        return result;
    }

    if (statement[0].type == TokenType::FOR) {
        if (statement.size() < 5 || !isNameToken(statement[1].type) || statement[2].type != TokenType::IN) {
            throw std::runtime_error("for statement must look like for name in iterable { ... }");
        }
        size_t bodyOpen = findTopLevelLeftBrace(statement, 3);
        if (bodyOpen == std::string::npos) throw std::runtime_error("for statement expected a '{' block");
        size_t bodyClose = findMatchingBrace(statement, bodyOpen);
        std::string loopName = statement[1].lexeme;
        std::vector<Token> iterableTokens(statement.begin() + 3, statement.begin() + static_cast<long>(bodyOpen));
        std::vector<Token> body(statement.begin() + static_cast<long>(bodyOpen + 1),
                                statement.begin() + static_cast<long>(bodyClose));
        RuntimeValue iterable = evaluateTokens(iterableTokens);
        RuntimeValue oldValue;
        bool hadOldValue = hasVariable(loopName);
        if (hadOldValue) oldValue = getVariable(loopName);
        RuntimeValue result;
        for (const RuntimeValue& item : iterableItemsForControl(iterable)) {
            setVariable(loopName, item);
            std::vector<RuntimeValue> results = executeTokenBlock(body, echoExpressionResults);
            if (!results.empty()) result = results.back();
        }
        if (hadOldValue) setVariable(loopName, oldValue);
        else variables.erase(loopName);
        return result;
    }

    size_t assignIndex = findTopLevelToken(statement, TokenType::ASSIGN_OP);
    if (assignIndex != std::string::npos) {
        std::vector<Token> left(statement.begin(), statement.begin() + static_cast<long>(assignIndex));
        std::vector<Token> right(statement.begin() + static_cast<long>(assignIndex + 1), statement.end());
        RuntimeValue value = evaluateTokens(right);

        if (left.size() >= 2 && left.front().type == TokenType::LEFT_BRACE && left.back().type == TokenType::RIGHT_BRACE) {
            RuntimeObject object = asObject(value, "object destructuring assignment");
            std::vector<Token> inner(left.begin() + 1, left.end() - 1);
            for (const std::vector<Token>& part : splitTopLevel(inner, TokenType::COMMA)) {
                std::vector<Token> nameTokens = trimStatement(part);
                if (nameTokens.size() != 1 || !isNameToken(nameTokens[0].type)) {
                    throw std::runtime_error("Invalid object destructuring target");
                }
                setVariable(nameTokens[0].lexeme, objectGetOrNil(value, nameTokens[0].lexeme));
            }
            return value;
        }

        if (left.size() >= 2 && left.front().type == TokenType::LEFT_BRACKET && left.back().type == TokenType::RIGHT_BRACKET) {
            RuntimeList values = asList(value, "list destructuring assignment");
            std::vector<Token> inner(left.begin() + 1, left.end() - 1);
            std::vector<std::vector<Token>> parts = splitTopLevel(inner, TokenType::COMMA);
            size_t valueIndex = 0;
            for (size_t i = 0; i < parts.size(); ++i) {
                std::vector<Token> target = trimStatement(parts[i]);
                bool rest = false;
                if (!target.empty() && target.front().type == TokenType::MULTIPLY) {
                    rest = true;
                    target.erase(target.begin());
                }
                if (target.size() != 1 || !isNameToken(target[0].type)) {
                    throw std::runtime_error("Invalid list destructuring target");
                }
                if (rest) {
                    RuntimeList restValues;
                    while (valueIndex < values.size()) restValues.push_back(values[valueIndex++]);
                    setVariable(target[0].lexeme, RuntimeValue::list(restValues));
                    if (i + 1 != parts.size()) throw std::runtime_error("Rest destructuring target must be last");
                } else {
                    if (valueIndex >= values.size()) throw std::runtime_error("List destructuring assignment ran out of values");
                    setVariable(target[0].lexeme, values[valueIndex++]);
                }
            }
            return value;
        }

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

std::vector<RuntimeValue> Interpreter::executeTokenBlock(const std::vector<Token>& block, bool echoExpressionResults) {
    std::vector<RuntimeValue> results;
    std::vector<Token> statement;
    int depth = 0;

    auto flush = [&]() {
        std::vector<Token> trimmed = trimStatement(statement);
        statement.clear();
        if (!trimmed.empty()) results.push_back(executeStatement(trimmed, echoExpressionResults));
    };

    for (size_t i = 0; i < block.size(); ++i) {
        const Token& token = block[i];
        if (depth == 0 && (token.type == TokenType::NEWLINE || token.type == TokenType::SEMICOLON)) {
            TokenType next = nextSignificantType(block, i);
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
    flush();
    return results;
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

size_t findMatchingBrace(const std::vector<Token>& tokens, size_t openIndex) {
    if (openIndex >= tokens.size() || tokens[openIndex].type != TokenType::LEFT_BRACE) {
        return std::string::npos;
    }
    int depth = 0;
    for (size_t i = openIndex; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::LEFT_BRACE) ++depth;
        if (tokens[i].type == TokenType::RIGHT_BRACE) {
            --depth;
            if (depth == 0) return i;
        }
    }
    throw std::runtime_error("Unterminated block");
}

size_t findTopLevelLeftBrace(const std::vector<Token>& tokens, size_t start) {
    int depth = 0;
    for (size_t i = start; i < tokens.size(); ++i) {
        if (depth == 0 && tokens[i].type == TokenType::LEFT_BRACE) return i;
        depth += topLevelDepthDelta(tokens[i].type);
    }
    return std::string::npos;
}

RuntimeList iterableItemsForControl(const RuntimeValue& value) {
    if (value.isList()) return *std::get<RuntimeValue::ListPtr>(value.data);
    if (objectType(value) == "set") return objectListField(value, "values");
    if (value.isObject()) {
        RuntimeList pairs;
        for (const auto& [key, member] : *std::get<RuntimeValue::ObjectPtr>(value.data)) {
            if (key == "__type") continue;
            pairs.push_back(RuntimeValue::list({RuntimeValue(key), member}));
        }
        return pairs;
    }
    return {};
}

} // namespace

} // namespace GameLang
