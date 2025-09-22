#ifndef AST_H
#define AST_H

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>

namespace GameLang {

// Forward declarations
class ASTVisitor;

// Value types for runtime (forward declaration)
struct Value;

// Base AST node
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

// Expression base class
class Expression : public ASTNode {
public:
    virtual ~Expression() = default;
};

// Statement base class  
class Statement : public ASTNode {
public:
    virtual ~Statement() = default;
};

// Value types for runtime
struct Value {
    std::variant<
        double,                                    // numbers
        std::string,                              // strings
        bool,                                     // booleans
        std::nullptr_t,                           // nil
        std::vector<Value>,                       // lists
        std::unordered_map<std::string, Value>    // objects/dicts
    > data;
    
    Value(double v) : data(v) {}
    Value(const std::string& v) : data(v) {}
    Value(bool v) : data(v) {}
    Value(std::nullptr_t) : data(nullptr) {}
    Value(const std::vector<Value>& v) : data(v) {}
    Value(const std::unordered_map<std::string, Value>& v) : data(v) {}
};

// Literals
class NumberLiteral : public Expression {
public:
    double value;
    explicit NumberLiteral(double value) : value(value) {}
    void accept(ASTVisitor& visitor) override;
};

class StringLiteral : public Expression {
public:
    std::string value;
    explicit StringLiteral(const std::string& value) : value(value) {}
    void accept(ASTVisitor& visitor) override;
};

class BooleanLiteral : public Expression {
public:
    bool value;
    explicit BooleanLiteral(bool value) : value(value) {}
    void accept(ASTVisitor& visitor) override;
};

class NilLiteral : public Expression {
public:
    NilLiteral() = default;
    void accept(ASTVisitor& visitor) override;
};

// Identifiers and variables
class Identifier : public Expression {
public:
    std::string name;
    explicit Identifier(const std::string& name) : name(name) {}
    void accept(ASTVisitor& visitor) override;
};

// Binary expressions
class BinaryExpression : public Expression {
public:
    std::unique_ptr<Expression> left;
    std::string operator_;
    std::unique_ptr<Expression> right;
    
    BinaryExpression(std::unique_ptr<Expression> left, 
                    const std::string& op, 
                    std::unique_ptr<Expression> right)
        : left(std::move(left)), operator_(op), right(std::move(right)) {}
    void accept(ASTVisitor& visitor) override;
};

// Unary expressions
class UnaryExpression : public Expression {
public:
    std::string operator_;
    std::unique_ptr<Expression> operand;
    
    UnaryExpression(const std::string& op, std::unique_ptr<Expression> operand)
        : operator_(op), operand(std::move(operand)) {}
    void accept(ASTVisitor& visitor) override;
};

// Assignment expressions
class AssignmentExpression : public Expression {
public:
    std::string name;
    std::unique_ptr<Expression> value;
    
    AssignmentExpression(const std::string& name, std::unique_ptr<Expression> value)
        : name(name), value(std::move(value)) {}
    void accept(ASTVisitor& visitor) override;
};

// Function calls
class FunctionCall : public Expression {
public:
    std::unique_ptr<Expression> function;
    std::vector<std::unique_ptr<Expression>> arguments;
    
    FunctionCall(std::unique_ptr<Expression> function,
                std::vector<std::unique_ptr<Expression>> arguments)
        : function(std::move(function)), arguments(std::move(arguments)) {}
    void accept(ASTVisitor& visitor) override;
};

// Lambda expressions
class LambdaExpression : public Expression {
public:
    std::vector<std::string> parameters;
    std::unique_ptr<Expression> body;
    
    LambdaExpression(std::vector<std::string> parameters,
                    std::unique_ptr<Expression> body)
        : parameters(std::move(parameters)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

// List literals and comprehensions
class ListLiteral : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elements;
    
    explicit ListLiteral(std::vector<std::unique_ptr<Expression>> elements)
        : elements(std::move(elements)) {}
    void accept(ASTVisitor& visitor) override;
};

class ListComprehension : public Expression {
public:
    std::unique_ptr<Expression> element;
    std::string variable;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<Expression> condition; // optional filter
    
    ListComprehension(std::unique_ptr<Expression> element,
                     const std::string& variable,
                     std::unique_ptr<Expression> iterable,
                     std::unique_ptr<Expression> condition = nullptr)
        : element(std::move(element)), variable(variable), 
          iterable(std::move(iterable)), condition(std::move(condition)) {}
    void accept(ASTVisitor& visitor) override;
};

// Dictionary literals
class DictionaryLiteral : public Expression {
public:
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> pairs;
    
    explicit DictionaryLiteral(
        std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> pairs)
        : pairs(std::move(pairs)) {}
    void accept(ASTVisitor& visitor) override;
};

// Game theory specific expressions
class PlayerExpression : public Expression {
public:
    std::string name;
    std::vector<std::string> strategies;
    std::unique_ptr<Expression> utility_function; // optional
    
    PlayerExpression(const std::string& name,
                    std::vector<std::string> strategies,
                    std::unique_ptr<Expression> utility_function = nullptr)
        : name(name), strategies(std::move(strategies)), 
          utility_function(std::move(utility_function)) {}
    void accept(ASTVisitor& visitor) override;
};

class GameExpression : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> players;
    std::unique_ptr<Expression> payoff_matrix;
    std::string game_type; // "normal", "extensive", "population"
    
    GameExpression(std::vector<std::unique_ptr<Expression>> players,
                  std::unique_ptr<Expression> payoff_matrix,
                  const std::string& game_type = "normal")
        : players(std::move(players)), payoff_matrix(std::move(payoff_matrix)),
          game_type(game_type) {}
    void accept(ASTVisitor& visitor) override;
};

// Pattern matching
class MatchExpression : public Expression {
public:
    std::unique_ptr<Expression> value;
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> cases;
    
    MatchExpression(std::unique_ptr<Expression> value,
                   std::vector<std::pair<std::unique_ptr<Expression>, 
                                       std::unique_ptr<Expression>>> cases)
        : value(std::move(value)), cases(std::move(cases)) {}
    void accept(ASTVisitor& visitor) override;
};

// Pipeline expression
class PipelineExpression : public Expression {
public:
    std::unique_ptr<Expression> input;
    std::vector<std::unique_ptr<Expression>> functions;
    bool parallel; // true for |>>, false for |>
    
    PipelineExpression(std::unique_ptr<Expression> input,
                      std::vector<std::unique_ptr<Expression>> functions,
                      bool parallel = false)
        : input(std::move(input)), functions(std::move(functions)), parallel(parallel) {}
    void accept(ASTVisitor& visitor) override;
};

// Statements
class ExpressionStatement : public Statement {
public:
    std::unique_ptr<Expression> expression;
    
    explicit ExpressionStatement(std::unique_ptr<Expression> expression)
        : expression(std::move(expression)) {}
    void accept(ASTVisitor& visitor) override;
};

class VariableDeclaration : public Statement {
public:
    std::vector<std::string> names; // for unpacking: a, b, c := [1, 2, 3]
    std::unique_ptr<Expression> initializer;
    
    VariableDeclaration(std::vector<std::string> names,
                       std::unique_ptr<Expression> initializer)
        : names(std::move(names)), initializer(std::move(initializer)) {}
    void accept(ASTVisitor& visitor) override;
};

class FunctionDeclaration : public Statement {
public:
    std::string name;
    std::vector<std::string> parameters;
    std::unique_ptr<Statement> body;
    
    FunctionDeclaration(const std::string& name,
                       std::vector<std::string> parameters,
                       std::unique_ptr<Statement> body)
        : name(name), parameters(std::move(parameters)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

class BlockStatement : public Statement {
public:
    std::vector<std::unique_ptr<Statement>> statements;
    
    explicit BlockStatement(std::vector<std::unique_ptr<Statement>> statements)
        : statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
};

class IfStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> then_statement;
    std::unique_ptr<Statement> else_statement; // optional
    
    IfStatement(std::unique_ptr<Expression> condition,
               std::unique_ptr<Statement> then_statement,
               std::unique_ptr<Statement> else_statement = nullptr)
        : condition(std::move(condition)), then_statement(std::move(then_statement)),
          else_statement(std::move(else_statement)) {}
    void accept(ASTVisitor& visitor) override;
};

class WhileStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> body;
    
    WhileStatement(std::unique_ptr<Expression> condition,
                  std::unique_ptr<Statement> body)
        : condition(std::move(condition)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

class ForStatement : public Statement {
public:
    std::string variable;
    std::unique_ptr<Expression> iterable;
    std::unique_ptr<Statement> body;
    
    ForStatement(const std::string& variable,
                std::unique_ptr<Expression> iterable,
                std::unique_ptr<Statement> body)
        : variable(variable), iterable(std::move(iterable)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;
};

class ReturnStatement : public Statement {
public:
    std::unique_ptr<Expression> value; // optional
    
    explicit ReturnStatement(std::unique_ptr<Expression> value = nullptr)
        : value(std::move(value)) {}
    void accept(ASTVisitor& visitor) override;
};

// Program (top-level)
class Program : public ASTNode {
public:
    std::vector<std::unique_ptr<Statement>> statements;
    
    explicit Program(std::vector<std::unique_ptr<Statement>> statements)
        : statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
};

} // namespace GameLang

#endif // AST_H