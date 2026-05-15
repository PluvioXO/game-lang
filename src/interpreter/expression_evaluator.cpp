#include "expression_evaluator.h"

#include "interpreter.h"
#include "runtime_support.h"
#include "token_utils.h"

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace GameLang {
namespace {

class ExpressionEvaluator {
public:
    ExpressionEvaluator(Interpreter& interpreter, std::vector<Token> tokens)
        : interpreter(interpreter), tokens(withEOF(std::move(tokens))), current(0) {}

    RuntimeValue evaluate() {
        RuntimeValue value = expression();
        if (!isAtEnd()) {
            throw error(peek(), "Unexpected token after expression");
        }
        return value;
    }

private:
    Interpreter& interpreter;
    std::vector<Token> tokens;
    size_t current;

    RuntimeValue expression() { return ternary(); }

    RuntimeValue ternary() {
        RuntimeValue condition = pipeline();
        if (match(TokenType::QUESTION)) {
            RuntimeValue thenValue = expression();
            consume(TokenType::COLON, "Expected ':' in conditional expression");
            RuntimeValue elseValue = expression();
            return isTruthy(condition) ? thenValue : elseValue;
        }
        return condition;
    }

    RuntimeValue pipeline() {
        RuntimeValue value = logicalOr();
        while (match(TokenType::PIPELINE) || match(TokenType::PARALLEL_MAP)) {
            bool parallel = previous().type == TokenType::PARALLEL_MAP;
            value = parsePipelineStep(value, parallel);
        }
        return value;
    }

    RuntimeValue parsePipelineStep(const RuntimeValue& input, bool parallel) {
        skipNewlines();
        if (!isNameToken(peek().type)) {
            throw error(peek(), "Expected a function name after pipeline operator");
        }
        std::string functionName = advance().lexeme;
        std::vector<CallArg> args;
        args.push_back({"", input});
        if (match(TokenType::LEFT_PAREN)) {
            parseArgumentList(args, TokenType::RIGHT_PAREN);
        }
        return interpreter.callFunction(functionName, args, parallel);
    }

    RuntimeValue logicalOr() {
        RuntimeValue value = logicalAnd();
        while (match(TokenType::OR)) {
            RuntimeValue right = logicalAnd();
            value = RuntimeValue(isTruthy(value) || isTruthy(right));
        }
        return value;
    }

    RuntimeValue logicalAnd() {
        RuntimeValue value = equality();
        while (match(TokenType::AND)) {
            RuntimeValue right = equality();
            value = RuntimeValue(isTruthy(value) && isTruthy(right));
        }
        return value;
    }

    RuntimeValue equality() {
        RuntimeValue value = comparison();
        while (match(TokenType::EQUAL) || match(TokenType::NOT_EQUAL)) {
            Token op = previous();
            RuntimeValue right = comparison();
            bool equal = valuesEqual(value, right);
            value = RuntimeValue(op.type == TokenType::EQUAL ? equal : !equal);
        }
        return value;
    }

    RuntimeValue comparison() {
        RuntimeValue value = range();
        while (match(TokenType::LESS) || match(TokenType::LESS_EQUAL) ||
               match(TokenType::GREATER) || match(TokenType::GREATER_EQUAL)) {
            Token op = previous();
            RuntimeValue right = range();
            double leftNumber = asNumber(value, "comparison");
            double rightNumber = asNumber(right, "comparison");
            switch (op.type) {
                case TokenType::LESS: value = RuntimeValue(leftNumber < rightNumber); break;
                case TokenType::LESS_EQUAL: value = RuntimeValue(leftNumber <= rightNumber); break;
                case TokenType::GREATER: value = RuntimeValue(leftNumber > rightNumber); break;
                case TokenType::GREATER_EQUAL: value = RuntimeValue(leftNumber >= rightNumber); break;
                default: break;
            }
        }
        return value;
    }

    RuntimeValue range() {
        RuntimeValue value = term();
        if (match(TokenType::RANGE)) {
            double start = asNumber(value, "range start");
            double end = asNumber(term(), "range end");
            double step = start <= end ? 1.0 : -1.0;
            if (match(TokenType::STEP)) {
                step = asNumber(term(), "range step");
            }
            if (std::abs(step) < EPSILON) {
                throw std::runtime_error("range step cannot be zero");
            }
            RuntimeList values;
            if (step > 0) {
                for (double number = start; number <= end + EPSILON; number += step) values.emplace_back(number);
            } else {
                for (double number = start; number >= end - EPSILON; number += step) values.emplace_back(number);
            }
            return RuntimeValue::list(values);
        }
        return value;
    }

    RuntimeValue term() {
        RuntimeValue value = factor();
        while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
            Token op = previous();
            RuntimeValue right = factor();
            if (op.type == TokenType::PLUS && (value.isString() || right.isString())) {
                value = RuntimeValue(asString(value) + asString(right));
            } else {
                double leftNumber = asNumber(value, "arithmetic");
                double rightNumber = asNumber(right, "arithmetic");
                value = RuntimeValue(op.type == TokenType::PLUS ? leftNumber + rightNumber : leftNumber - rightNumber);
            }
        }
        return value;
    }

    RuntimeValue factor() {
        RuntimeValue value = power();
        while (match(TokenType::MULTIPLY) || match(TokenType::DIVIDE) || match(TokenType::MODULO)) {
            Token op = previous();
            RuntimeValue right = power();
            double leftNumber = asNumber(value, "arithmetic");
            double rightNumber = asNumber(right, "arithmetic");
            if (op.type == TokenType::MULTIPLY) value = RuntimeValue(leftNumber * rightNumber);
            if (op.type == TokenType::DIVIDE) value = RuntimeValue(leftNumber / rightNumber);
            if (op.type == TokenType::MODULO) value = RuntimeValue(std::fmod(leftNumber, rightNumber));
        }
        return value;
    }

    RuntimeValue power() {
        RuntimeValue value = unary();
        if (match(TokenType::POWER)) {
            RuntimeValue right = power();
            value = RuntimeValue(std::pow(asNumber(value, "power"), asNumber(right, "power")));
        }
        return value;
    }

    RuntimeValue unary() {
        if (match(TokenType::NOT)) {
            return RuntimeValue(!isTruthy(unary()));
        }
        if (match(TokenType::MINUS)) {
            return RuntimeValue(-asNumber(unary(), "unary minus"));
        }
        return call();
    }

    RuntimeValue call() {
        RuntimeValue value = primary();
        while (true) {
            if (match(TokenType::LEFT_PAREN)) {
                value = finishCall(value);
            } else if (match(TokenType::DOT)) {
                if (!isNameToken(peek().type)) throw error(peek(), "Expected property name after '.'");
                std::string key = advance().lexeme;
                value = objectGetOrNil(value, key);
            } else if (match(TokenType::LEFT_BRACKET)) {
                RuntimeValue index = expression();
                consume(TokenType::RIGHT_BRACKET, "Expected ']' after index");
                value = indexValue(value, index);
            } else {
                break;
            }
        }
        return value;
    }

    RuntimeValue finishCall(const RuntimeValue& callee) {
        std::string functionName = asString(callee);
        std::vector<CallArg> args;
        parseArgumentList(args, TokenType::RIGHT_PAREN);
        return interpreter.callFunction(functionName, args);
    }

    void parseArgumentList(std::vector<CallArg>& args, TokenType terminator) {
        skipNewlines();
        if (match(terminator)) return;
        do {
            skipNewlines();
            if (isNameToken(peek().type) && nextNonNewlineType() == TokenType::COLON) {
                std::string name = advance().lexeme;
                consume(TokenType::COLON, "Expected ':' after named argument");
                args.push_back({name, expression()});
            } else {
                args.push_back({"", expression()});
            }
            skipNewlines();
        } while (match(TokenType::COMMA));
        consume(terminator, "Expected end of argument list");
    }

    RuntimeValue indexValue(const RuntimeValue& value, const RuntimeValue& index) {
        if (value.isList()) {
            const auto& list = *std::get<RuntimeValue::ListPtr>(value.data);
            int rawIndex = static_cast<int>(asNumber(index, "list index"));
            if (rawIndex < 0) rawIndex = static_cast<int>(list.size()) + rawIndex;
            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= list.size()) return RuntimeValue();
            return list[static_cast<size_t>(rawIndex)];
        }
        if (value.isObject()) {
            return objectGetOrNil(value, asString(index));
        }
        if (value.isString()) {
            const std::string& text = std::get<std::string>(value.data);
            int rawIndex = static_cast<int>(asNumber(index, "string index"));
            if (rawIndex < 0) rawIndex = static_cast<int>(text.size()) + rawIndex;
            if (rawIndex < 0 || static_cast<size_t>(rawIndex) >= text.size()) return RuntimeValue();
            return RuntimeValue(std::string(1, text[static_cast<size_t>(rawIndex)]));
        }
        return RuntimeValue();
    }

    RuntimeValue primary() {
        if (match(TokenType::NUMBER)) return RuntimeValue(previous().numberValue);
        if (match(TokenType::STRING)) return RuntimeValue(previous().lexeme);
        if (match(TokenType::TRUE)) return RuntimeValue(true);
        if (match(TokenType::FALSE)) return RuntimeValue(false);
        if (match(TokenType::NIL)) return RuntimeValue();

        if (match(TokenType::IDENTIFIER) || match(TokenType::WILDCARD) || matchKeywordSymbol()) {
            std::string name = previous().lexeme;
            if (interpreter.hasVariable(name)) return interpreter.getVariable(name);
            return RuntimeValue(name);
        }

        if (match(TokenType::LEFT_PAREN)) {
            skipNewlines();
            if (match(TokenType::RIGHT_PAREN)) return RuntimeValue::list({});
            RuntimeValue first = expression();
            if (match(TokenType::COMMA)) {
                RuntimeList tuple;
                tuple.push_back(first);
                do {
                    tuple.push_back(expression());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected ')' after tuple");
                return RuntimeValue::list(tuple);
            }
            consume(TokenType::RIGHT_PAREN, "Expected ')' after expression");
            return first;
        }

        if (match(TokenType::LEFT_BRACKET)) return listLiteral();
        if (match(TokenType::LEFT_BRACE)) return dictionaryLiteral();

        throw error(peek(), "Expected expression");
    }

    RuntimeValue listLiteral() {
        RuntimeList values;
        skipNewlines();
        if (match(TokenType::RIGHT_BRACKET)) return RuntimeValue::list(values);
        do {
            skipNewlines();
            if (check(TokenType::RIGHT_BRACKET)) break;
            values.push_back(expression());
            skipNewlines();
        } while (match(TokenType::COMMA) || match(TokenType::PATTERN_OR));
        consume(TokenType::RIGHT_BRACKET, "Expected ']' after list");
        return RuntimeValue::list(values);
    }

    RuntimeValue dictionaryLiteral() {
        RuntimeObject object;
        skipNewlines();
        if (match(TokenType::RIGHT_BRACE)) return RuntimeValue::object(object);
        do {
            skipNewlines();
            std::string key;
            if ((check(TokenType::STRING) || isNameToken(peek().type)) && nextNonNewlineType() == TokenType::COLON) {
                key = advance().lexeme;
                consume(TokenType::COLON, "Expected ':' after dictionary key");
            } else {
                RuntimeValue keyValue = expression();
                key = asString(keyValue);
                consume(TokenType::COLON, "Expected ':' after dictionary key");
            }
            RuntimeValue value = expression();
            object[key] = value;
            skipNewlines();
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_BRACE, "Expected '}' after dictionary");
        return RuntimeValue::object(object);
    }

    bool matchKeywordSymbol() {
        skipNewlines();
        if (!isKeywordSymbol(peek().type)) return false;
        advance();
        return true;
    }

    bool match(TokenType type) {
        skipNewlines();
        if (!check(type)) return false;
        advance();
        return true;
    }

    bool check(TokenType type) {
        skipNewlines();
        if (isAtEnd()) return type == TokenType::EOF_TOKEN;
        return peek().type == type;
    }

    Token advance() {
        if (!isAtEnd()) ++current;
        return previous();
    }

    bool isAtEnd() {
        skipNewlines();
        return current >= tokens.size() || tokens[current].type == TokenType::EOF_TOKEN;
    }

    Token peek() {
        skipNewlines();
        if (current >= tokens.size()) return tokens.back();
        return tokens[current];
    }

    Token previous() const {
        return tokens[current - 1];
    }

    void consume(TokenType type, const std::string& message) {
        if (match(type)) return;
        throw error(peek(), message);
    }

    TokenType nextNonNewlineType() const {
        size_t index = current + 1;
        while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
        if (index >= tokens.size()) return TokenType::EOF_TOKEN;
        return tokens[index].type;
    }

    void skipNewlines() {
        while (current < tokens.size() && tokens[current].type == TokenType::NEWLINE) ++current;
    }

    std::runtime_error error(const Token& token, const std::string& message) const {
        std::ostringstream out;
        out << "Parse error at " << token.line << ":" << token.column << ": " << message;
        return std::runtime_error(out.str());
    }
};

} // namespace

RuntimeValue evaluateExpression(Interpreter& interpreter, const std::vector<Token>& tokens) {
    ExpressionEvaluator evaluator(interpreter, tokens);
    return evaluator.evaluate();
}

} // namespace GameLang
