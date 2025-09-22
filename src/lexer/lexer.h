#ifndef LEXER_H
#define LEXER_H

#include "token.h"
#include <string>
#include <vector>

namespace GameLang {

class Lexer {
private:
    std::string source;
    std::vector<Token> tokens;
    size_t start;
    size_t current;
    size_t line;
    size_t column;
    
    bool isAtEnd() const;
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);
    
    void addToken(TokenType type);
    void addToken(TokenType type, const std::string& lexeme);
    void addToken(TokenType type, const std::string& lexeme, double value);
    
    void scanToken();
    void scanString();
    void scanNumber();
    void scanIdentifier();
    void scanSingleLineComment();
    void scanMultiLineComment();
    
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    
public:
    explicit Lexer(const std::string& source);
    
    std::vector<Token> scanTokens();
    void reset(const std::string& newSource);
};

} // namespace GameLang

#endif // LEXER_H