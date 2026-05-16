#include "token.h"
#include <unordered_map>

namespace GameLang {

// Initialize keyword map
const std::unordered_map<std::string, TokenType> keywords = {
    // Core keywords
    {"let", TokenType::LET},
    {"fn", TokenType::FN},
    {"match", TokenType::MATCH},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"for", TokenType::FOR},
    {"in", TokenType::IN},
    {"while", TokenType::WHILE},
    {"return", TokenType::RETURN},
    {"T", TokenType::TRUE},
    {"F", TokenType::FALSE},
    {"nil", TokenType::NIL},
    {"where", TokenType::WHERE},
    
    // Game theory keywords
    {"player", TokenType::PLAYER},
    {"strategy", TokenType::STRATEGY},
    {"game", TokenType::GAME},
    {"payoff", TokenType::PAYOFF},
    {"equilibrium", TokenType::EQUILIBRIUM},
    {"outcome", TokenType::OUTCOME},
    {"nash", TokenType::NASH},
    {"dominant", TokenType::DOMINANT},
    {"mixed", TokenType::MIXED},
    {"pure", TokenType::PURE},
    
    // Type keywords
    {"num", TokenType::NUM},
    {"str", TokenType::STR},
    {"bool", TokenType::BOOL},
    {"list", TokenType::LIST},
    {"set", TokenType::SET},
    {"dict", TokenType::DICT},
    {"matrix", TokenType::MATRIX},
    
    // Special keywords
    {"step", TokenType::STEP}
};

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::FSTRING: return "FSTRING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        
        case TokenType::LET: return "LET";
        case TokenType::ASSIGN_OP: return "ASSIGN_OP";
        case TokenType::FN: return "FN";
        case TokenType::MATCH: return "MATCH";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::IN: return "IN";
        case TokenType::RETURN: return "RETURN";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::NIL: return "NIL";
        case TokenType::WHERE: return "WHERE";
        
        case TokenType::PLAYER: return "PLAYER";
        case TokenType::STRATEGY: return "STRATEGY";
        case TokenType::GAME: return "GAME";
        case TokenType::PAYOFF: return "PAYOFF";
        case TokenType::EQUILIBRIUM: return "EQUILIBRIUM";
        case TokenType::OUTCOME: return "OUTCOME";
        case TokenType::NASH: return "NASH";
        case TokenType::DOMINANT: return "DOMINANT";
        case TokenType::MIXED: return "MIXED";
        case TokenType::PURE: return "PURE";
        
        case TokenType::NUM: return "NUM";
        case TokenType::STR: return "STR";
        case TokenType::BOOL: return "BOOL";
        case TokenType::LIST: return "LIST";
        case TokenType::SET: return "SET";
        case TokenType::DICT: return "DICT";
        case TokenType::MATRIX: return "MATRIX";
        
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MULTIPLY: return "MULTIPLY";
        case TokenType::DIVIDE: return "DIVIDE";
        case TokenType::MODULO: return "MODULO";
        case TokenType::POWER: return "POWER";
        
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TokenType::MULTIPLY_ASSIGN: return "MULTIPLY_ASSIGN";
        case TokenType::DIVIDE_ASSIGN: return "DIVIDE_ASSIGN";
        
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        
        case TokenType::ARROW: return "ARROW";
        case TokenType::PIPELINE: return "PIPELINE";
        case TokenType::PARALLEL_MAP: return "PARALLEL_MAP";
        case TokenType::RANGE: return "RANGE";
        case TokenType::STEP: return "STEP";
        case TokenType::UNION: return "UNION";
        case TokenType::INTERSECTION: return "INTERSECTION";
        
        case TokenType::PATTERN_OR: return "PATTERN_OR";
        case TokenType::WILDCARD: return "WILDCARD";
        case TokenType::COMP_FOR: return "COMP_FOR";
        
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COLON: return "COLON";
        case TokenType::QUESTION: return "QUESTION";
        case TokenType::NEWLINE: return "NEWLINE";
        
        case TokenType::EOF_TOKEN: return "EOF";
        case TokenType::ERROR: return "ERROR";
        
        default: return "UNKNOWN";
    }
}

bool isBinaryOperator(TokenType type) {
    switch (type) {
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::MULTIPLY:
        case TokenType::DIVIDE:
        case TokenType::MODULO:
        case TokenType::POWER:
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
        case TokenType::AND:
        case TokenType::OR:
            return true;
        default:
            return false;
    }
}

bool isAssignmentOperator(TokenType type) {
    switch (type) {
        case TokenType::ASSIGN:
        case TokenType::PLUS_ASSIGN:
        case TokenType::MINUS_ASSIGN:
        case TokenType::MULTIPLY_ASSIGN:
        case TokenType::DIVIDE_ASSIGN:
            return true;
        default:
            return false;
    }
}

int getOperatorPrecedence(TokenType type) {
    switch (type) {
        case TokenType::OR:
            return 1;
        case TokenType::AND:
            return 2;
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
            return 3;
        case TokenType::LESS:
        case TokenType::LESS_EQUAL:
        case TokenType::GREATER:
        case TokenType::GREATER_EQUAL:
            return 4;
        case TokenType::PLUS:
        case TokenType::MINUS:
            return 5;
        case TokenType::MULTIPLY:
        case TokenType::DIVIDE:
        case TokenType::MODULO:
            return 6;
        case TokenType::POWER:
            return 7;
        default:
            return 0;
    }
}

bool isRightAssociative(TokenType type) {
    return type == TokenType::POWER;
}

} // namespace GameLang
