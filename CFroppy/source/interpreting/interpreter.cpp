#include "interpreter.hpp"
#include "iostream"

using namespace cfp;
using namespace cfp::interpreting;
using namespace scan::types;
using enum scan::token::tokenType;

void interpreter::interpret(const std::vector<std::unique_ptr<ast::stmt::statement>> &stmts) {
    for(decltype(auto) stmt : stmts) {
        execute(stmt);
    }
}

void interpreter::execute(const std::unique_ptr<ast::stmt::statement> &stmt) {
    stmt->accept(*this);
}



scan::literal interpreter::visit(ast::expr::binary &expr) {
    const auto left = evaluate(expr.left);
    const auto right = evaluate(expr.right);

    switch (expr.oper.type) {
        case PLUS: return left+right;
        case MINUS: return left-right;
        case STAR: return left*right;
        case SLASH: return left/right;
        case GREATER: return scan::literal(left>right);
        case GREATER_EQUAL: return scan::literal(left>=right);
        case LESS: return scan::literal(left<right);
        case LESS_EQUAL: return scan::literal(left<=right);
        case EQUAL_EQUAL: return scan::literal(left==right);
        case BANG_EQUAL: return scan::literal(left!=right);
        default: return {};
    }
}

scan::literal interpreter::visit(ast::expr::grouping &expr) {
    return evaluate(expr.expr);
}

scan::literal interpreter::visit(ast::expr::literal &expr) {
    return expr.value;
}

scan::literal interpreter::visit(ast::expr::unary &expr) {
    const auto right = evaluate(expr.expr);

    switch (expr.oper.type) {
        case MINUS:
            return -right;
        case BANG:
            return scan::literal(!right);
        default: return {};
    }
}


void interpreter::visit(ast::stmt::expression &stmt) {
    evaluate(stmt.expr);
}

void interpreter::visit(ast::stmt::print &stmt) {
    const auto literal = evaluate(stmt.expr);
    std::cout << literal.stringify() << std::endl;
}


scan::literal interpreter::evaluate(const std::unique_ptr<ast::expr::expression> &expr) {
    return expr->accept(*this);
}

bool interpreter::isTruthy(const scan::literal &value) {
    if(value.has<boolean>()) return value.getBoolean();
    return !value.has<nil>();
}