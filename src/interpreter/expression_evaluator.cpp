#include "expression_evaluator.h"

#include "interpreter.h"
#include "runtime_support.h"
#include "token_utils.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace GameLang {
namespace {

struct ComprehensionGenerator {
    std::vector<std::string> names;
    std::vector<Token> iterableTokens;
};

struct PatternResult {
    bool matched = false;
    std::vector<std::pair<std::string, RuntimeValue>> bindings;
};

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

    RuntimeValue expression() { return lambda(); }

    RuntimeValue lambda() {
        if (singleParameterLambdaAhead()) {
            std::vector<std::string> parameters = {advance().lexeme};
            consume(TokenType::ARROW, "Expected '->' after lambda parameter");
            std::vector<Token> body = collectLambdaBody();
            if (body.empty()) throw error(previous(), "Expected lambda body");
            return RuntimeValue::lambda(parameters, body);
        }

        if (parenthesizedLambdaAhead()) {
            consume(TokenType::LEFT_PAREN, "Expected '(' before lambda parameters");
            std::vector<std::string> parameters;
            skipNewlines();
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    if (!isNameToken(peek().type)) throw error(peek(), "Expected lambda parameter name");
                    parameters.push_back(advance().lexeme);
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected ')' after lambda parameters");
            consume(TokenType::ARROW, "Expected '->' after lambda parameters");
            std::vector<Token> body = collectLambdaBody();
            if (body.empty()) throw error(previous(), "Expected lambda body");
            return RuntimeValue::lambda(parameters, body);
        }

        return ternary();
    }

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
            } else if (match(TokenType::LEFT_BRACE)) {
                value = finishBlockCall(value);
            } else if (match(TokenType::DOT)) {
                if (!isNameToken(peek().type)) throw error(peek(), "Expected property name after '.'");
                std::string key = advance().lexeme;
                if (check(TokenType::LEFT_PAREN)) {
                    consume(TokenType::LEFT_PAREN, "Expected '(' for method call");
                    std::vector<CallArg> args;
                    args.push_back({"", value});
                    parseArgumentList(args, TokenType::RIGHT_PAREN);
                    value = interpreter.callFunction(key, args);
                } else {
                    value = objectGetOrNil(value, key);
                }
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
        std::vector<CallArg> args;
        parseArgumentList(args, TokenType::RIGHT_PAREN);
        return interpreter.callValue(callee, args);
    }

    RuntimeValue finishBlockCall(const RuntimeValue& callee) {
        std::vector<CallArg> args;
        skipNewlines();
        if (!match(TokenType::RIGHT_BRACE)) {
            do {
                skipNewlines();
                if (!isNameToken(peek().type) && !check(TokenType::STRING)) {
                    throw error(peek(), "Expected field name in block call");
                }
                std::string name = advance().lexeme;
                consume(TokenType::COLON, "Expected ':' after block field name");
                args.push_back({name, expression()});
                skipNewlines();
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_BRACE, "Expected '}' after block call");
        }

        if (callee.isObject()) {
            RuntimeObject object = *std::get<RuntimeValue::ObjectPtr>(callee.data);
            for (const CallArg& arg : args) object[arg.name] = arg.value;
            return RuntimeValue::object(object);
        }
        return interpreter.callValue(callee, args);
    }

    void parseArgumentList(std::vector<CallArg>& args, TokenType terminator) {
        skipNewlines();
        if (match(terminator)) return;
        do {
            skipNewlines();
            if (isNameToken(peek().type) && nextNonNewlineType() == TokenType::COLON) {
                std::string name = advance().lexeme;
                consume(TokenType::COLON, "Expected ':' after named argument");
                args.push_back({name, parseArgumentValue(terminator)});
            } else {
                args.push_back({"", parseArgumentValue(terminator)});
            }
            skipNewlines();
        } while (match(TokenType::COMMA));
        consume(terminator, "Expected end of argument list");
    }

    RuntimeValue parseArgumentValue(TokenType terminator) {
        if (inlineGeneratorAhead(terminator)) return generatorExpression(terminator);
        return expression();
    }

    RuntimeValue indexValue(const RuntimeValue& value, const RuntimeValue& index) {
        if (objectType(value) == "set") {
            return indexValue(objectGetOrNil(value, "values"), index);
        }
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
        if (match(TokenType::FSTRING)) return RuntimeValue(interpolateFString(previous().lexeme));
        if (match(TokenType::TRUE)) return RuntimeValue(true);
        if (match(TokenType::FALSE)) return RuntimeValue(false);
        if (match(TokenType::NIL)) return RuntimeValue();
        if (match(TokenType::MATCH)) return matchExpression();

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
        if (match(TokenType::LEFT_BRACE)) return braceLiteral();

        throw error(peek(), "Expected expression");
    }

    RuntimeValue listLiteral() {
        RuntimeList values;
        skipNewlines();
        if (match(TokenType::RIGHT_BRACKET)) return RuntimeValue::list(values);

        if (comprehensionAhead(TokenType::RIGHT_BRACKET)) {
            size_t pipeIndex = findComprehensionPipe(TokenType::RIGHT_BRACKET);
            std::vector<Token> elementTokens(tokens.begin() + static_cast<long>(current),
                                             tokens.begin() + static_cast<long>(pipeIndex));
            current = pipeIndex + 1;
            return evaluateComprehension(trimStatement(elementTokens), TokenType::RIGHT_BRACKET, true, false);
        }

        do {
            skipNewlines();
            if (check(TokenType::RIGHT_BRACKET)) break;
            values.push_back(expression());
            skipNewlines();
        } while (match(TokenType::COMMA) || match(TokenType::PATTERN_OR));
        consume(TokenType::RIGHT_BRACKET, "Expected ']' after list");
        return RuntimeValue::list(values);
    }

    RuntimeValue braceLiteral() {
        skipNewlines();
        if (match(TokenType::RIGHT_BRACE)) return RuntimeValue::object({});

        if (comprehensionAhead(TokenType::RIGHT_BRACE)) {
            size_t pipeIndex = findComprehensionPipe(TokenType::RIGHT_BRACE);
            std::vector<Token> elementTokens(tokens.begin() + static_cast<long>(current),
                                             tokens.begin() + static_cast<long>(pipeIndex));
            current = pipeIndex + 1;
            return evaluateComprehension(trimStatement(elementTokens), TokenType::RIGHT_BRACE, true, true);
        }

        if (dictionaryAhead()) return dictionaryLiteralBody();
        return setLiteralBody();
    }

    RuntimeValue dictionaryLiteralBody() {
        RuntimeObject object;
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

    RuntimeValue setLiteralBody() {
        RuntimeList values;
        do {
            skipNewlines();
            if (check(TokenType::RIGHT_BRACE)) break;
            appendUnique(values, expression());
            skipNewlines();
        } while (match(TokenType::COMMA) || match(TokenType::PATTERN_OR));
        consume(TokenType::RIGHT_BRACE, "Expected '}' after set");
        return makeTaggedObject("set", {{"values", RuntimeValue::list(values)}});
    }

    RuntimeValue generatorExpression(TokenType terminator) {
        size_t pipeIndex = findInlineGeneratorPipe(terminator);
        std::vector<Token> elementTokens(tokens.begin() + static_cast<long>(current),
                                         tokens.begin() + static_cast<long>(pipeIndex));
        current = pipeIndex + 1;
        return evaluateComprehension(trimStatement(elementTokens), terminator, false, false, true);
    }

    RuntimeValue evaluateComprehension(
        const std::vector<Token>& elementTokens,
        TokenType terminator,
        bool consumeTerminator,
        bool asSet,
        bool commaMayEnd = false) {
        std::vector<ComprehensionGenerator> generators;
        std::vector<Token> conditionTokens;
        parseComprehensionTail(generators, conditionTokens, terminator, consumeTerminator, commaMayEnd);

        RuntimeList output;
        std::vector<std::pair<std::string, RuntimeValue>> bindings;

        std::function<void(size_t)> visit = [&](size_t index) {
            if (index == generators.size()) {
                if (!conditionTokens.empty() &&
                    !isTruthy(interpreter.evaluateTokensWithTemporaryBindings(conditionTokens, bindings))) {
                    return;
                }
                RuntimeValue value = interpreter.evaluateTokensWithTemporaryBindings(elementTokens, bindings);
                if (asSet) appendUnique(output, value);
                else output.push_back(value);
                return;
            }

            RuntimeValue iterable = interpreter.evaluateTokensWithTemporaryBindings(
                generators[index].iterableTokens,
                bindings);
            RuntimeList items = iterableValues(iterable);
            for (const RuntimeValue& item : items) {
                size_t oldSize = bindings.size();
                bindGeneratorItem(generators[index].names, item, bindings);
                visit(index + 1);
                bindings.resize(oldSize);
            }
        };

        visit(0);
        if (asSet) return makeTaggedObject("set", {{"values", RuntimeValue::list(output)}});
        return RuntimeValue::list(output);
    }

    void parseComprehensionTail(
        std::vector<ComprehensionGenerator>& generators,
        std::vector<Token>& conditionTokens,
        TokenType terminator,
        bool consumeTerminator,
        bool commaMayEnd) {
        while (true) {
            skipNewlines();
            std::vector<std::string> names;
            if (!isNameToken(peek().type)) throw error(peek(), "Expected comprehension variable");
            names.push_back(advance().lexeme);
            while (match(TokenType::COMMA)) {
                if (!isNameToken(peek().type)) throw error(peek(), "Expected comprehension variable");
                names.push_back(advance().lexeme);
            }
            consume(TokenType::IN, "Expected 'in' in comprehension");

            size_t start = current;
            int depth = 0;
            while (current < tokens.size()) {
                TokenType type = tokens[current].type;
                if (depth == 0) {
                    if (type == TokenType::IF || type == terminator) break;
                    if (type == TokenType::COMMA && generatorStart(current + 1, terminator)) break;
                    if (commaMayEnd && type == TokenType::COMMA) break;
                }
                depth += topLevelDepthDelta(type);
                ++current;
            }
            std::vector<Token> iterable(tokens.begin() + static_cast<long>(start),
                                        tokens.begin() + static_cast<long>(current));
            generators.push_back({names, trimStatement(iterable)});

            skipNewlines();
            if (check(TokenType::COMMA) && generatorStart(current + 1, terminator)) {
                advance();
                continue;
            }
            break;
        }

        skipNewlines();
        if (match(TokenType::IF)) {
            size_t start = current;
            int depth = 0;
            while (current < tokens.size()) {
                TokenType type = tokens[current].type;
                if (depth == 0 && (type == terminator || (commaMayEnd && type == TokenType::COMMA))) break;
                depth += topLevelDepthDelta(type);
                ++current;
            }
            std::vector<Token> condition(tokens.begin() + static_cast<long>(start),
                                         tokens.begin() + static_cast<long>(current));
            conditionTokens = trimStatement(condition);
        }

        if (consumeTerminator) consume(terminator, "Expected end of comprehension");
    }

    void bindGeneratorItem(
        const std::vector<std::string>& names,
        const RuntimeValue& item,
        std::vector<std::pair<std::string, RuntimeValue>>& bindings) {
        if (names.size() == 1) {
            bindings.push_back({names.front(), item});
            return;
        }
        RuntimeList values = asList(item, "multi-variable comprehension");
        if (values.size() != names.size()) {
            throw std::runtime_error("comprehension expected " + std::to_string(names.size()) +
                                     " values but got " + std::to_string(values.size()));
        }
        for (size_t i = 0; i < names.size(); ++i) bindings.push_back({names[i], values[i]});
    }

    RuntimeList iterableValues(const RuntimeValue& value) const {
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
        if (value.isString()) {
            RuntimeList chars;
            for (char c : std::get<std::string>(value.data)) chars.emplace_back(std::string(1, c));
            return chars;
        }
        return {};
    }

    void appendUnique(RuntimeList& values, const RuntimeValue& value) const {
        for (const RuntimeValue& existing : values) {
            if (valuesEqual(existing, value)) return;
        }
        values.push_back(value);
    }

    RuntimeValue matchExpression() {
        size_t targetStart = current;
        int depth = 0;
        while (current < tokens.size()) {
            TokenType type = tokens[current].type;
            if (depth == 0 && type == TokenType::LEFT_BRACE) break;
            depth += topLevelDepthDelta(type);
            ++current;
        }
        if (current >= tokens.size() || tokens[current].type != TokenType::LEFT_BRACE) {
            throw error(peek(), "Expected '{' after match value");
        }
        std::vector<Token> targetTokens(tokens.begin() + static_cast<long>(targetStart),
                                        tokens.begin() + static_cast<long>(current));
        RuntimeValue target = interpreter.evaluateTokens(trimStatement(targetTokens));
        consume(TokenType::LEFT_BRACE, "Expected '{' after match value");

        bool found = false;
        RuntimeValue result;
        while (true) {
            skipNewlines();
            if (match(TokenType::RIGHT_BRACE)) break;

            size_t patternStart = current;
            depth = 0;
            while (current < tokens.size()) {
                TokenType type = tokens[current].type;
                if (depth == 0 && type == TokenType::ARROW) break;
                if (depth == 0 && type == TokenType::RIGHT_BRACE) throw error(peek(), "Expected '->' in match case");
                depth += topLevelDepthDelta(type);
                ++current;
            }
            if (current >= tokens.size()) throw error(previous(), "Unterminated match case");
            std::vector<Token> patternTokens(tokens.begin() + static_cast<long>(patternStart),
                                             tokens.begin() + static_cast<long>(current));
            consume(TokenType::ARROW, "Expected '->' in match case");

            size_t resultStart = current;
            depth = 0;
            while (current < tokens.size()) {
                TokenType type = tokens[current].type;
                if (depth == 0 && (type == TokenType::COMMA ||
                                   type == TokenType::NEWLINE ||
                                   type == TokenType::RIGHT_BRACE)) {
                    break;
                }
                depth += topLevelDepthDelta(type);
                ++current;
            }
            std::vector<Token> resultTokens(tokens.begin() + static_cast<long>(resultStart),
                                            tokens.begin() + static_cast<long>(current));

            if (!found) {
                PatternResult match = matchPattern(trimStatement(patternTokens), target, {});
                if (match.matched) {
                    result = interpreter.evaluateTokensWithTemporaryBindings(trimStatement(resultTokens), match.bindings);
                    found = true;
                }
            }

            if (match(TokenType::COMMA)) continue;
            skipNewlines();
            if (check(TokenType::RIGHT_BRACE)) continue;
        }

        return found ? result : RuntimeValue();
    }

    PatternResult matchPattern(
        const std::vector<Token>& rawPattern,
        const RuntimeValue& value,
        std::vector<std::pair<std::string, RuntimeValue>> bindings) {
        std::vector<Token> pattern = trimStatement(rawPattern);
        size_t guardIndex = findTopLevelToken(pattern, TokenType::IF);
        std::vector<Token> guardTokens;
        if (guardIndex != std::string::npos) {
            guardTokens.assign(pattern.begin() + static_cast<long>(guardIndex + 1), pattern.end());
            pattern.erase(pattern.begin() + static_cast<long>(guardIndex), pattern.end());
        }

        PatternResult result = matchPatternBase(trimStatement(pattern), value, std::move(bindings));
        if (!result.matched) return result;
        if (!guardTokens.empty() &&
            !isTruthy(interpreter.evaluateTokensWithTemporaryBindings(trimStatement(guardTokens), result.bindings))) {
            return {};
        }
        return result;
    }

    PatternResult matchPatternBase(
        const std::vector<Token>& pattern,
        const RuntimeValue& value,
        std::vector<std::pair<std::string, RuntimeValue>> bindings) {
        if (pattern.empty()) return {};
        if (pattern.size() == 1 && pattern[0].type == TokenType::WILDCARD) return {true, bindings};
        if (pattern.size() == 1 && pattern[0].type == TokenType::IDENTIFIER) {
            return bindPatternName(pattern[0].lexeme, value, std::move(bindings));
        }

        if (functionPatternAhead(pattern)) {
            std::string expectedType = pattern[0].lexeme;
            std::string actualType = objectType(value);
            if (actualType.empty()) actualType = objectStringField(value, "kind");
            if (actualType != expectedType) return {};
            std::vector<Token> inner(pattern.begin() + 2, pattern.end() - 1);
            return matchFieldPatterns(inner, value, std::move(bindings));
        }

        if (pattern.front().type == TokenType::LEFT_BRACE && pattern.back().type == TokenType::RIGHT_BRACE) {
            std::vector<Token> inner(pattern.begin() + 1, pattern.end() - 1);
            return matchFieldPatterns(inner, value, std::move(bindings));
        }

        if ((pattern.front().type == TokenType::LEFT_PAREN && pattern.back().type == TokenType::RIGHT_PAREN) ||
            (pattern.front().type == TokenType::LEFT_BRACKET && pattern.back().type == TokenType::RIGHT_BRACKET)) {
            if (!value.isList()) return {};
            RuntimeList values = *std::get<RuntimeValue::ListPtr>(value.data);
            std::vector<Token> inner(pattern.begin() + 1, pattern.end() - 1);
            std::vector<std::vector<Token>> parts = splitTopLevel(inner, TokenType::COMMA);
            if (parts.size() != values.size()) return {};
            for (size_t i = 0; i < parts.size(); ++i) {
                PatternResult nested = matchPattern(trimStatement(parts[i]), values[i], std::move(bindings));
                if (!nested.matched) return {};
                bindings = std::move(nested.bindings);
            }
            return {true, bindings};
        }

        RuntimeValue expected = interpreter.evaluateTokensWithTemporaryBindings(pattern, bindings);
        if (!valuesEqual(expected, value)) return {};
        return {true, bindings};
    }

    PatternResult matchFieldPatterns(
        const std::vector<Token>& fieldTokens,
        const RuntimeValue& value,
        std::vector<std::pair<std::string, RuntimeValue>> bindings) {
        if (!value.isObject()) return {};
        for (const std::vector<Token>& part : splitTopLevel(fieldTokens, TokenType::COMMA)) {
            std::vector<Token> field = trimStatement(part);
            if (field.empty()) continue;
            size_t colon = findTopLevelToken(field, TokenType::COLON);
            if (colon == std::string::npos || colon == 0) return {};
            std::string key = field.front().lexeme;
            std::vector<Token> expected(field.begin() + static_cast<long>(colon + 1), field.end());
            RuntimeValue actual = fieldValueForPattern(value, key);
            PatternResult nested = matchPattern(trimStatement(expected), actual, std::move(bindings));
            if (!nested.matched) return {};
            bindings = std::move(nested.bindings);
        }
        return {true, bindings};
    }

    RuntimeValue fieldValueForPattern(const RuntimeValue& value, const std::string& key) const {
        if (key == "type") {
            std::string type = objectType(value);
            if (type.empty()) type = objectStringField(value, "kind");
            return RuntimeValue(type);
        }
        RuntimeValue field = objectGetOrNil(value, key);
        if (key == "players" && field.isList()) {
            return RuntimeValue(static_cast<double>(std::get<RuntimeValue::ListPtr>(field.data)->size()));
        }
        return field;
    }

    PatternResult bindPatternName(
        const std::string& name,
        const RuntimeValue& value,
        std::vector<std::pair<std::string, RuntimeValue>> bindings) const {
        for (const auto& [existingName, existingValue] : bindings) {
            if (existingName == name) return {valuesEqual(existingValue, value), bindings};
        }
        bindings.push_back({name, value});
        return {true, bindings};
    }

    bool functionPatternAhead(const std::vector<Token>& pattern) const {
        return pattern.size() >= 3 &&
               isNameToken(pattern[0].type) &&
               pattern[1].type == TokenType::LEFT_PAREN &&
               pattern.back().type == TokenType::RIGHT_PAREN;
    }

    bool singleParameterLambdaAhead() {
        skipNewlines();
        if (!isNameToken(peek().type)) return false;
        size_t index = current + 1;
        while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
        return index < tokens.size() && tokens[index].type == TokenType::ARROW;
    }

    bool parenthesizedLambdaAhead() {
        skipNewlines();
        if (peek().type != TokenType::LEFT_PAREN) return false;
        size_t index = current + 1;
        while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
        if (index < tokens.size() && tokens[index].type == TokenType::RIGHT_PAREN) {
            ++index;
            while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
            return index < tokens.size() && tokens[index].type == TokenType::ARROW;
        }
        while (index < tokens.size()) {
            while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
            if (index >= tokens.size() || !isNameToken(tokens[index].type)) return false;
            ++index;
            while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
            if (index < tokens.size() && tokens[index].type == TokenType::COMMA) {
                ++index;
                continue;
            }
            if (index < tokens.size() && tokens[index].type == TokenType::RIGHT_PAREN) {
                ++index;
                while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
                return index < tokens.size() && tokens[index].type == TokenType::ARROW;
            }
            return false;
        }
        return false;
    }

    std::vector<Token> collectLambdaBody() {
        size_t start = current;
        int depth = 0;
        while (current < tokens.size()) {
            TokenType type = tokens[current].type;
            if (depth == 0 &&
                (type == TokenType::COMMA ||
                 type == TokenType::RIGHT_PAREN ||
                 type == TokenType::RIGHT_BRACKET ||
                 type == TokenType::RIGHT_BRACE ||
                 type == TokenType::NEWLINE ||
                 type == TokenType::EOF_TOKEN)) {
                break;
            }
            depth += topLevelDepthDelta(type);
            ++current;
        }
        std::vector<Token> body(tokens.begin() + static_cast<long>(start),
                                tokens.begin() + static_cast<long>(current));
        return trimStatement(body);
    }

    bool comprehensionAhead(TokenType terminator) {
        return findComprehensionPipe(terminator) != std::string::npos;
    }

    size_t findComprehensionPipe(TokenType terminator) {
        int depth = 0;
        for (size_t index = current; index < tokens.size(); ++index) {
            TokenType type = tokens[index].type;
            if (depth == 0 && type == terminator) break;
            if (depth == 0 && type == TokenType::PATTERN_OR && generatorStart(index + 1, terminator)) return index;
            depth += topLevelDepthDelta(type);
        }
        return std::string::npos;
    }

    bool inlineGeneratorAhead(TokenType terminator) {
        return findInlineGeneratorPipe(terminator) != std::string::npos;
    }

    size_t findInlineGeneratorPipe(TokenType terminator) {
        int depth = 0;
        for (size_t index = current; index < tokens.size(); ++index) {
            TokenType type = tokens[index].type;
            if (depth == 0 && (type == terminator || type == TokenType::COMMA)) break;
            if (depth == 0 && type == TokenType::PATTERN_OR && generatorStart(index + 1, terminator)) return index;
            depth += topLevelDepthDelta(type);
        }
        return std::string::npos;
    }

    bool generatorStart(size_t index, TokenType terminator) const {
        bool sawName = false;
        while (index < tokens.size()) {
            while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
            if (index >= tokens.size() || tokens[index].type == terminator) return false;
            if (!isNameToken(tokens[index].type)) return false;
            sawName = true;
            ++index;
            while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
            if (index < tokens.size() && tokens[index].type == TokenType::IN) return sawName;
            if (index < tokens.size() && tokens[index].type == TokenType::COMMA) {
                ++index;
                continue;
            }
            return false;
        }
        return false;
    }

    bool dictionaryAhead() const {
        int depth = 0;
        for (size_t index = current; index < tokens.size(); ++index) {
            TokenType type = tokens[index].type;
            if (depth == 0 && type == TokenType::RIGHT_BRACE) return false;
            if (depth == 0 && type == TokenType::COLON) return true;
            if (depth == 0 && (type == TokenType::COMMA || type == TokenType::PATTERN_OR)) return false;
            depth += topLevelDepthDelta(type);
        }
        return false;
    }

    std::string interpolateFString(const std::string& text) {
        std::string output;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '{' && i + 1 < text.size() && text[i + 1] == '{') {
                output += '{';
                ++i;
                continue;
            }
            if (text[i] == '}' && i + 1 < text.size() && text[i + 1] == '}') {
                output += '}';
                ++i;
                continue;
            }
            if (text[i] != '{') {
                output += text[i];
                continue;
            }
            size_t close = text.find('}', i + 1);
            if (close == std::string::npos) {
                output += text[i];
                continue;
            }
            std::string expressionSource = text.substr(i + 1, close - i - 1);
            output += asString(interpreter.evaluateExpressionSource(expressionSource));
            i = close;
        }
        return output;
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
