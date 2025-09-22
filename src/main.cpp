#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include "lexer/lexer.h"
#include "lexer/token.h"

using namespace GameLang;

void printTokens(const std::vector<Token>& tokens) {
    std::cout << "Tokens:\n";
    std::cout << "-------\n";
    for (const auto& token : tokens) {
        std::cout << tokenTypeToString(token.type) << " ";
        if (!token.lexeme.empty()) {
            std::cout << "'" << token.lexeme << "' ";
        }
        if (token.type == TokenType::NUMBER) {
            std::cout << "(" << token.numberValue << ") ";
        }
        std::cout << "[" << token.line << ":" << token.column << "]\n";
    }
    std::cout << "\n";
}

void runREPL() {
    std::cout << "GameLang REPL v1.0 - Game Theory Programming Language\n";
    std::cout << "Type 'exit' to quit, 'help' for commands\n\n";
    
    Lexer lexer("");
    std::string input;
    
    while (true) {
        std::cout << "gl> ";
        std::getline(std::cin, input);
        
        if (input == "exit" || input == "quit") {
            break;
        }
        
        if (input == "help") {
            std::cout << "GameLang Commands:\n";
            std::cout << "  help     - Show this help\n";
            std::cout << "  tokens   - Show lexical analysis\n";
            std::cout << "  exit     - Exit REPL\n";
            std::cout << "\nExample syntax:\n";
            std::cout << "  x := 42\n";
            std::cout << "  players := [player(\"Alice\"), player(\"Bob\")]\n";
            std::cout << "  payoffs := [[3,3|0,5], [5,0|1,1]]\n";
            std::cout << "  equilibria := solve_nash(game)\n\n";
            continue;
        }
        
        if (input.empty()) {
            continue;
        }
        
        try {
            lexer.reset(input);
            auto tokens = lexer.scanTokens();
            
            // For now, just show tokens (parser/interpreter will be added later)
            std::cout << "Lexical analysis:\n";
            for (const auto& token : tokens) {
                if (token.type != TokenType::EOF_TOKEN) {
                    std::cout << "  " << tokenTypeToString(token.type);
                    if (!token.lexeme.empty()) {
                        std::cout << " '" << token.lexeme << "'";
                    }
                    if (token.type == TokenType::NUMBER) {
                        std::cout << " (" << token.numberValue << ")";
                    }
                    std::cout << "\n";
                }
            }
            std::cout << "\n";
            
        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n\n";
        }
    }
    
    std::cout << "Goodbye!\n";
}

void runFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "'\n";
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    std::cout << "Running GameLang file: " << filename << "\n";
    std::cout << "=====================================\n\n";
    
    try {
        Lexer lexer(source);
        auto tokens = lexer.scanTokens();
        
        // Show lexical analysis
        std::cout << "Lexical Analysis:\n";
        printTokens(tokens);
        
        // TODO: Add parser and interpreter
        std::cout << "Parser and interpreter will be implemented next.\n";
        std::cout << "For now, showing lexical analysis only.\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::cout << "GameLang - Declarative Game Theory Programming Language\n";
    std::cout << "======================================================\n\n";
    
    if (argc == 1) {
        // No arguments - start REPL
        runREPL();
    } else if (argc == 2) {
        // One argument - run file
        runFile(argv[1]);
    } else {
        std::cout << "Usage:\n";
        std::cout << "  " << argv[0] << "           # Start interactive REPL\n";
        std::cout << "  " << argv[0] << " <file>    # Run GameLang file\n\n";
        std::cout << "Examples:\n";
        std::cout << "  " << argv[0] << " examples/prisoners_dilemma.gl\n";
        std::cout << "  " << argv[0] << " examples/auction_theory.gl\n";
        return 1;
    }
    
    return 0;
}