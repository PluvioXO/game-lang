#include "ast.h"
#include "visitor.h"

namespace GameLang {

// Implement accept methods for all AST nodes

// Literals
void NumberLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void StringLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void BooleanLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void NilLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

// Identifiers
void Identifier::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

// Expressions
void BinaryExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void UnaryExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void AssignmentExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void FunctionCall::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void LambdaExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void ListLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void ListComprehension::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void DictionaryLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void PlayerExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void GameExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void MatchExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void PipelineExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

// Statements
void ExpressionStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void VariableDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void FunctionDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void BlockStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void IfStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void WhileStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void ForStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void ReturnStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void Program::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

} // namespace GameLang