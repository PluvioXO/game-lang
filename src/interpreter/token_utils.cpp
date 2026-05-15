#include "token_utils.h"

#include <string>

namespace GameLang {

bool isKeywordSymbol(TokenType type) {
    switch (type) {
        case TokenType::PLAYER:
        case TokenType::STRATEGY:
        case TokenType::GAME:
        case TokenType::PAYOFF:
        case TokenType::EQUILIBRIUM:
        case TokenType::OUTCOME:
        case TokenType::NASH:
        case TokenType::DOMINANT:
        case TokenType::MIXED:
        case TokenType::PURE:
        case TokenType::NUM:
        case TokenType::STR:
        case TokenType::BOOL:
        case TokenType::LIST:
        case TokenType::SET:
        case TokenType::DICT:
        case TokenType::MATRIX:
            return true;
        default:
            return false;
    }
}

bool isNameToken(TokenType type) {
    return type == TokenType::IDENTIFIER || type == TokenType::WILDCARD || isKeywordSymbol(type);
}

std::vector<Token> withEOF(std::vector<Token> tokens) {
    tokens.emplace_back(
        TokenType::EOF_TOKEN,
        "",
        tokens.empty() ? 1 : tokens.back().line,
        tokens.empty() ? 1 : tokens.back().column);
    return tokens;
}

std::vector<Token> trimStatement(const std::vector<Token>& tokens) {
    size_t begin = 0;
    size_t end = tokens.size();
    while (begin < end && tokens[begin].type == TokenType::NEWLINE) ++begin;
    while (end > begin && tokens[end - 1].type == TokenType::NEWLINE) --end;
    return std::vector<Token>(tokens.begin() + static_cast<long>(begin), tokens.begin() + static_cast<long>(end));
}

int topLevelDepthDelta(TokenType type) {
    if (type == TokenType::LEFT_PAREN || type == TokenType::LEFT_BRACKET || type == TokenType::LEFT_BRACE) return 1;
    if (type == TokenType::RIGHT_PAREN || type == TokenType::RIGHT_BRACKET || type == TokenType::RIGHT_BRACE) return -1;
    return 0;
}

size_t findTopLevelToken(const std::vector<Token>& tokens, TokenType type) {
    int depth = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (depth == 0 && tokens[i].type == type) return i;
        depth += topLevelDepthDelta(tokens[i].type);
    }
    return std::string::npos;
}

std::vector<std::vector<Token>> splitTopLevel(const std::vector<Token>& tokens, TokenType separator) {
    std::vector<std::vector<Token>> parts;
    std::vector<Token> current;
    int depth = 0;
    for (const Token& token : tokens) {
        if (depth == 0 && token.type == separator) {
            parts.push_back(trimStatement(current));
            current.clear();
            continue;
        }
        current.push_back(token);
        depth += topLevelDepthDelta(token.type);
    }
    parts.push_back(trimStatement(current));
    return parts;
}

TokenType nextSignificantType(const std::vector<Token>& tokens, size_t index) {
    ++index;
    while (index < tokens.size() && tokens[index].type == TokenType::NEWLINE) ++index;
    if (index >= tokens.size()) return TokenType::EOF_TOKEN;
    return tokens[index].type;
}

} // namespace GameLang
