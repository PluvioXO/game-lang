#include "lexer.h"
#include <iostream>
#include <cctype>

namespace GameLang {

Lexer::Lexer(const std::string& source)
    : source(source), start(0), current(0), line(1), column(1) {}

std::vector<Token> Lexer::scanTokens() {
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    
    tokens.emplace_back(TokenType::EOF_TOKEN, "", line, column);
    return tokens;
}

void Lexer::reset(const std::string& newSource) {
    source = newSource;
    tokens.clear();
    start = 0;
    current = 0;
    line = 1;
    column = 1;
}

bool Lexer::isAtEnd() const {
    return current >= source.length();
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    
    advance();
    return true;
}

void Lexer::addToken(TokenType type) {
    std::string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, line, column - (current - start));
}

void Lexer::addToken(TokenType type, const std::string& lexeme) {
    tokens.emplace_back(type, lexeme, line, column - (current - start));
}

void Lexer::addToken(TokenType type, const std::string& lexeme, double value) {
    tokens.emplace_back(type, lexeme, value, line, column - (current - start));
}

void Lexer::scanToken() {
    char c = advance();
    
    switch (c) {
        // Whitespace
        case ' ':
        case '\r':
        case '\t':
            break;
        case '\n':
            break;
            
        // Single character tokens
        case '(': addToken(TokenType::LEFT_PAREN); break;
        case ')': addToken(TokenType::RIGHT_PAREN); break;
        case '{': addToken(TokenType::LEFT_BRACE); break;
        case '}': addToken(TokenType::RIGHT_BRACE); break;
        case '[': addToken(TokenType::LEFT_BRACKET); break;
        case ']': addToken(TokenType::RIGHT_BRACKET); break;
        case ',': addToken(TokenType::COMMA); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case '?': addToken(TokenType::QUESTION); break;
        case '^': addToken(TokenType::POWER); break;
        case '%': addToken(TokenType::MODULO); break;
        
        // Operators that might be followed by '=' or other characters
        case '+':
            addToken(match('=') ? TokenType::PLUS_ASSIGN : TokenType::PLUS);
            break;
        case '-':
            if (match('=')) {
                addToken(TokenType::MINUS_ASSIGN);
            } else if (match('>')) {
                addToken(TokenType::ARROW);
            } else {
                addToken(TokenType::MINUS);
            }
            break;
        case '*':
            addToken(match('=') ? TokenType::MULTIPLY_ASSIGN : TokenType::MULTIPLY);
            break;
        case '/':
            if (match('/')) {
                scanSingleLineComment();
            } else if (match('*')) {
                scanMultiLineComment();
            } else if (match('=')) {
                addToken(TokenType::DIVIDE_ASSIGN);
            } else {
                addToken(TokenType::DIVIDE);
            }
            break;
            
        // New operators for game theory syntax
        case ':':
            if (match('=')) {
                addToken(TokenType::ASSIGN_OP);
            } else {
                addToken(TokenType::COLON);
            }
            break;
            
        // Pipeline operators
        case '|':
            if (match('|')) {
                addToken(TokenType::OR);
            } else if (match('>')) {
                if (match('>')) {
                    addToken(TokenType::PARALLEL_MAP);  // |>>
                } else {
                    addToken(TokenType::PIPELINE);      // |>
                }
            } else {
                addToken(TokenType::PATTERN_OR);        // | (for pattern matching)
            }
            break;
            
        // Range operator
        case '.':
            if (match('.')) {
                addToken(TokenType::RANGE);
            } else {
                addToken(TokenType::DOT);
            }
            break;
            
        // Underscore for wildcards
        case '_':
            addToken(TokenType::WILDCARD);
            break;
            
        // Comparison and logical operators
        case '!':
            addToken(match('=') ? TokenType::NOT_EQUAL : TokenType::NOT);
            break;
        case '=':
            addToken(match('=') ? TokenType::EQUAL : TokenType::ASSIGN);
            break;
        case '<':
            addToken(match('=') ? TokenType::LESS_EQUAL : TokenType::LESS);
            break;
        case '>':
            addToken(match('=') ? TokenType::GREATER_EQUAL : TokenType::GREATER);
            break;
            
        // Logical operators
        case '&':
            if (match('&')) {
                addToken(TokenType::AND);
            } else {
                std::cerr << "Error: Unexpected character '&' at line " << line 
                          << ", column " << column << std::endl;
                addToken(TokenType::ERROR);
            }
            break;
            
        // String literals
        case '"':
            scanString();
            break;
            
        // Comments
        case '#':
            scanSingleLineComment();
            break;
            
        default:
            if (isDigit(c)) {
                scanNumber();
            } else if (isAlpha(c)) {
                scanIdentifier();
            } else {
                std::cerr << "Error: Unexpected character '" << c << "' at line " 
                          << line << ", column " << column << std::endl;
                addToken(TokenType::ERROR);
            }
            break;
    }
}

void Lexer::scanString() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') {
            line++;
            column = 1;
        }
        advance();
    }
    
    if (isAtEnd()) {
        std::cerr << "Error: Unterminated string at line " << line << std::endl;
        addToken(TokenType::ERROR);
        return;
    }
    
    // Consume the closing "
    advance();
    
    // Extract the string value (excluding quotes)
    std::string value = source.substr(start + 1, current - start - 2);
    
    // Process escape sequences
    std::string processed;
    for (size_t i = 0; i < value.length(); i++) {
        if (value[i] == '\\' && i + 1 < value.length()) {
            switch (value[i + 1]) {
                case 'n': processed += '\n'; break;
                case 't': processed += '\t'; break;
                case 'r': processed += '\r'; break;
                case '\\': processed += '\\'; break;
                case '"': processed += '"'; break;
                default: 
                    processed += value[i];
                    processed += value[i + 1];
                    break;
            }
            i++; // Skip the next character
        } else {
            processed += value[i];
        }
    }
    
    addToken(TokenType::STRING, processed);
}

void Lexer::scanNumber() {
    while (isDigit(peek())) {
        advance();
    }
    
    // Look for fractional part
    if (peek() == '.' && isDigit(peekNext())) {
        advance(); // Consume the '.'
        
        while (isDigit(peek())) {
            advance();
        }
    }
    
    std::string numberStr = source.substr(start, current - start);
    double value = std::stod(numberStr);
    addToken(TokenType::NUMBER, numberStr, value);
}

void Lexer::scanIdentifier() {
    while (isAlphaNumeric(peek())) {
        advance();
    }
    
    std::string text = source.substr(start, current - start);
    
    // Check if it's a keyword
    auto it = keywords.find(text);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;
    
    addToken(type, text);
}

void Lexer::scanSingleLineComment() {
    // Comment goes until end of line
    while (peek() != '\n' && !isAtEnd()) {
        advance();
    }
}

void Lexer::scanMultiLineComment() {
    while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') {
            advance(); // Consume '*'
            advance(); // Consume '/'
            return;
        }
        advance();
    }
    
    std::cerr << "Error: Unterminated comment at line " << line << std::endl;
    addToken(TokenType::ERROR);
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

} // namespace GameLang