#ifndef GAMELANG_TOKEN_UTILS_H
#define GAMELANG_TOKEN_UTILS_H

#include "lexer/token.h"

#include <cstddef>
#include <vector>

namespace GameLang {

bool isKeywordSymbol(TokenType type);
bool isNameToken(TokenType type);

std::vector<Token> withEOF(std::vector<Token> tokens);
std::vector<Token> trimStatement(const std::vector<Token>& tokens);
int topLevelDepthDelta(TokenType type);
size_t findTopLevelToken(const std::vector<Token>& tokens, TokenType type);
std::vector<std::vector<Token>> splitTopLevel(const std::vector<Token>& tokens, TokenType separator);
TokenType nextSignificantType(const std::vector<Token>& tokens, size_t index);

} // namespace GameLang

#endif // GAMELANG_TOKEN_UTILS_H
