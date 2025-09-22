#ifndef AST_VISITOR_H
#define AST_VISITOR_H

namespace GameLang {

// Forward declarations
class NumberLiteral;
class StringLiteral;
class BooleanLiteral;
class NilLiteral;
class Identifier;
class BinaryExpression;
class UnaryExpression;
class AssignmentExpression;
class FunctionCall;
class LambdaExpression;
class ListLiteral;
class ListComprehension;
class DictionaryLiteral;
class PlayerExpression;
class GameExpression;
class MatchExpression;
class PipelineExpression;
class ExpressionStatement;
class VariableDeclaration;
class FunctionDeclaration;
class BlockStatement;
class IfStatement;
class WhileStatement;
class ForStatement;
class ReturnStatement;
class Program;

// Visitor pattern for AST traversal
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    // Expression visitors
    virtual void visit(NumberLiteral& node) = 0;
    virtual void visit(StringLiteral& node) = 0;
    virtual void visit(BooleanLiteral& node) = 0;
    virtual void visit(NilLiteral& node) = 0;
    virtual void visit(Identifier& node) = 0;
    virtual void visit(BinaryExpression& node) = 0;
    virtual void visit(UnaryExpression& node) = 0;
    virtual void visit(AssignmentExpression& node) = 0;
    virtual void visit(FunctionCall& node) = 0;
    virtual void visit(LambdaExpression& node) = 0;
    virtual void visit(ListLiteral& node) = 0;
    virtual void visit(ListComprehension& node) = 0;
    virtual void visit(DictionaryLiteral& node) = 0;
    virtual void visit(PlayerExpression& node) = 0;
    virtual void visit(GameExpression& node) = 0;
    virtual void visit(MatchExpression& node) = 0;
    virtual void visit(PipelineExpression& node) = 0;
    
    // Statement visitors
    virtual void visit(ExpressionStatement& node) = 0;
    virtual void visit(VariableDeclaration& node) = 0;
    virtual void visit(FunctionDeclaration& node) = 0;
    virtual void visit(BlockStatement& node) = 0;
    virtual void visit(IfStatement& node) = 0;
    virtual void visit(WhileStatement& node) = 0;
    virtual void visit(ForStatement& node) = 0;
    virtual void visit(ReturnStatement& node) = 0;
    virtual void visit(Program& node) = 0;
};

} // namespace GameLang

#endif // AST_VISITOR_H