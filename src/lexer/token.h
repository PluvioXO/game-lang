#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <unordered_map>

namespace GameLang {

// Token types enumeration
enum class TokenType {
    // Literals
    NUMBER,
    STRING,
    FSTRING,
    IDENTIFIER,
    
    // Keywords - Core
    LET,        // let (legacy support)
    ASSIGN_OP,  // :=
    FN,         // fn (lambda)
    MATCH,      // match
    IF,
    ELSE,
    FOR,
    IN,
    WHILE,
    RETURN,
    TRUE,       // T
    FALSE,      // F
    NIL,        // nil
    WHERE,      // where
    
    // Game Theory Keywords
    PLAYER,
    STRATEGY,
    GAME,
    PAYOFF,
    EQUILIBRIUM,
    OUTCOME,
    NASH,
    DOMINANT,
    MIXED,
    PURE,
    
    // Type Keywords
    NUM,
    STR,
    BOOL,
    LIST,
    SET,
    DICT,
    MATRIX,
    
    // Operators - Arithmetic
    PLUS,           // +
    MINUS,          // -
    MULTIPLY,       // *
    DIVIDE,         // /
    MODULO,         // %
    POWER,          // ^
    
    // Operators - Assignment
    ASSIGN,         // = (legacy)
    PLUS_ASSIGN,    // +=
    MINUS_ASSIGN,   // -=
    MULTIPLY_ASSIGN,// *=
    DIVIDE_ASSIGN,  // /=
    
    // Operators - Comparison
    EQUAL,          // ==
    NOT_EQUAL,      // !=
    LESS,           // <
    LESS_EQUAL,     // <=
    GREATER,        // >
    GREATER_EQUAL,  // >=
    
    // Operators - Logical
    AND,            // &&
    OR,             // ||
    NOT,            // !
    
    // Operators - Special
    ARROW,          // ->
    PIPELINE,       // |>
    PARALLEL_MAP,   // |>>
    RANGE,          // ..
    STEP,           // step
    UNION,          // ∪
    INTERSECTION,   // ∩
    
    // Pattern Matching
    PATTERN_OR,     // |
    WILDCARD,       // _
    
    // Comprehensions
    COMP_FOR,       // | (in comprehensions)
    
    // Punctuation
    LEFT_PAREN,     // (
    RIGHT_PAREN,    // )
    LEFT_BRACE,     // {
    RIGHT_BRACE,    // }
    LEFT_BRACKET,   // [
    RIGHT_BRACKET,  // ]
    COMMA,          // ,
    DOT,            // .
    SEMICOLON,      // ;
    COLON,          // :
    QUESTION,       // ?
    NEWLINE,        // statement separator
    
    // Special
    EOF_TOKEN,
    ERROR
};

// Token structure
struct Token {
    TokenType type;
    std::string lexeme;
    double numberValue;  // For number literals
    size_t line;
    size_t column;
    
    Token(TokenType type, const std::string& lexeme, size_t line, size_t column)
        : type(type), lexeme(lexeme), numberValue(0.0), line(line), column(column) {}
    
    Token(TokenType type, const std::string& lexeme, double value, size_t line, size_t column)
        : type(type), lexeme(lexeme), numberValue(value), line(line), column(column) {}
};

// Keyword map for identifier lookup
extern const std::unordered_map<std::string, TokenType> keywords;

// Token type to string conversion
std::string tokenTypeToString(TokenType type);

// Check if token is a binary operator
bool isBinaryOperator(TokenType type);

// Check if token is an assignment operator
bool isAssignmentOperator(TokenType type);

// Get operator precedence (higher number = higher precedence)
int getOperatorPrecedence(TokenType type);

// Check if operator is right associative
bool isRightAssociative(TokenType type);

} // namespace GameLang

#endif // TOKEN_H
