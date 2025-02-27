#include "parser.hpp"

using namespace cfp;
using namespace cfp::parse;
using namespace cfp::scan;
using namespace cfp::ast;
using enum token::tokenType;

/*!
 * @param tokens stream of tokens (scanner output)
 * @param reporter reporter
 */
parser::parser(std::vector<token> &tokens, const io::reporter &reporter) : reporter(reporter), tokens(tokens){
}


/*!
 * @brief execute a parser
 * @return root of AST
 */
std::vector<std::unique_ptr<stmt::statement>> parser::parse() {
    std::vector<std::unique_ptr<stmt::statement>> statements;
    while (!isAtEnd()) {
        statements.push_back(declaration());
    }
    return statements;
}


/*!
 * @return true if current token type is END_OF_FILE
 */
bool parser::isAtEnd() const {
    return peek().type==END_OF_FILE;
}


/*!
 * @brief check current type
 * @param type expected type
 * @return true if a current type same as a expected type
 */
bool parser::check(const token::tokenType type) const {
    if(isAtEnd()) return false;
    return type == peek().type;
}


/*!
 * @brief return current token and advance
 * @return current token
 */
token &parser::advance() {
    if(!isAtEnd()) ++current;
    return previous();
}


/*!
 * @brief advance if a current token is expected else throw parse_error
 * @param type expected type
 * @param msg message in parse_error
 * @return current token
 */
token &parser::consume(const token::tokenType type, const std::string &msg) {
    if(check(type)) return advance();
    throw error(peek(), msg);
}


/*!
 * @brief make a report and create parse_error
 * @param token token which has an error
 * @param msg message in parse_error
 * @return parse_error
 */
parse_error parser::error(const token &token, const std::string &msg) const {
	parse_error err(msg, token.line, token.type==END_OF_FILE ?
				" at end" :
				std::format(" at '{}'", token.lexeme));
	reporter.parse_error(err);
    return err;
}


/*!
 * @return current token
 */
token &parser::peek() const {
    return tokens[current];
}


/*!
 * @return previously token
 */
token &parser::previous() const {
    return tokens[current-1];
}


/*!
 * @brief parse expr productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::expr() {
    return assignment();
}


/*!
 * @brief parse assignment productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::assignment() {
	auto expr = logical_or();

	if(match(EQUAL)) {
		decltype(auto) equals = previous();
		auto value = this->expr();

		if(const auto var = dynamic_cast<expr::variable*>(expr.get())) {
			return std::make_unique<expr::assign>(var->name, std::move(value));
		}

		throw error(equals, "Invalid assignment target.");
	}

	return expr;
}


/*!
 * @brief parse logical_or production
 * @return expression
 */
std::unique_ptr<expr::expression> parser::logical_or() {
	auto expr = logical_and();

	if(match(OR)) {
		const auto& oper = previous();
		auto right = logical_and();
		return std::make_unique<expr::logical>(std::move(expr), std::move(right), oper);
	}

	return expr;
}


/*!
 * @brief parse logical_and production
 * @return expression
 */
std::unique_ptr<expr::expression> parser::logical_and() {
	auto expr = equality();

	if(match(AND)) {
		const auto& oper = previous();
		auto right = equality();
		return std::make_unique<expr::logical>(std::move(expr), std::move(right), oper);
	}

	return expr;
}


/*!
 * @brief parse equality productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::equality() {
    decltype(auto) expr = comparison();

    while (match(EQUAL_EQUAL, BANG_EQUAL)) {
        decltype(auto) oper = previous();
        decltype(auto) right = comparison();
        expr = std::make_unique<expr::binary>(std::move(expr), std::move(right), oper);
    }

    return expr;
}


/*!
 * @brief parse comparison productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::comparison() {
    decltype(auto) expr = term();

    while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
        decltype(auto) oper = previous();
        decltype(auto) right = term();
        expr = std::make_unique<expr::binary>(std::move(expr), std::move(right), oper);
    }

    return expr;
}


/*!
 * @brief parse term productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::term() {
    decltype(auto) expr = factor();

    while (match(PLUS, MINUS)) {
        auto oper = previous();
        auto right = factor();
        expr = std::make_unique<expr::binary>(std::move(expr), std::move(right), std::move(oper));
    }

    return expr;
}


/*!
 * @brief parse factor productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::factor() {
    decltype(auto) expr = binary_assignment();

    while (match(STAR, SLASH)) {
        auto oper = previous();
        auto right = unary();
        expr = std::make_unique<expr::binary>(std::move(expr), std::move(right), std::move(oper));
    }

    return expr;
}


/*!
 * @brief parse binary assignment productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::binary_assignment() {
	auto expr = this->unary();

	if(match(PLUS_EQUAL) || match(MINUS_EQUAL) || match(STAR_EQUAL) || match(SLASH_EQUAL)) {
		auto equals= previous();
		auto value = this->unary();

		if(const auto var = dynamic_cast<expr::variable*>(expr.get())) {
			return std::make_unique<expr::binary_assign>(var->name, std::move(value), std::move(equals));
		}

		throw error(equals, "Invalid assignment target.");
	}

	return expr;
}


/*!
 * @brief parse unary productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::unary() {
    if(match(BANG, MINUS)) {
        auto oper = previous();
        return std::make_unique<expr::unary>(call(), std::move(oper));
    }

    return call();
}


/*!
 * @brief parse call productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::call() {
	auto expr = member();

	while(match(LEFT_PAREN)) {
		expr = finishCall(std::move(expr));
	}

	return expr;
}


/*!
 * @brief parse sub productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::member() {
	decltype(auto) expr = primary();

	while (match(DOT)) {
		auto mem = primary();
		expr = std::make_unique<expr::member>(std::move(expr), std::move(mem));
	}

	return expr;
}


/*!
 * @brief parse primary productions
 * @return expression
 */
std::unique_ptr<expr::expression> parser::primary() {
    if(match(TRUE)) return std::make_unique<expr::literal>(literal{true});
    if(match(FALSE)) return std::make_unique<expr::literal>(literal{false});
    if(match(NIL)) return std::make_unique<expr::literal>(literal{});

    if(match(NUMBER, STRING)) {
        return std::make_unique<expr::literal>(previous().literal);
    }

	if(match(IDENTIFIER)) {
		return std::make_unique<expr::variable>(previous());
	}

    if(match(LEFT_PAREN)) {
        auto expr = std::make_unique<expr::grouping>(this->expr());
        consume(RIGHT_PAREN, "Expect \')\' after expression.");
        return expr;
    }

    throw error(peek(), "Expect expression.");
}




std::unique_ptr<expr::expression> parser::finishCall(std::unique_ptr<expr::expression> &&callee) {
	std::vector<std::unique_ptr<expr::expression>> arguments;

	if(!check(RIGHT_PAREN)) {
		do {
			if (arguments.size() >= types::callable::max_arity) {
				error(peek(), std::format("Can't have more than {} arguments.", types::callable::max_arity));
			}
			arguments.push_back(this->expr());
		} while (match(COMMA));
	}

	decltype(auto) paren = consume(RIGHT_PAREN, "Expect ')' after arguments.");

	return std::make_unique<expr::call>(std::move(callee), paren, std::move(arguments));
}


/*!
 * @brief synchronize in panic mode
 */
void parser::synchronize() {
    advance();

    while (!isAtEnd()) {
        if (previous().type == SEMICOLON) return;

        switch (peek().type) {
            case CLASS:
            case FN:
            case LET:
            case FOR:
            case IF:
            case WHILE:
            case RETURN:
              return;
          default: break;
        }

        advance();
    }
}


/*!
 * @brief parse current declaration
 * @return declaration statement
 */
std::unique_ptr<stmt::statement> parser::declaration() {
	try {
		if(match(LET)) {
			return letDeclaration();
		}
		return statement();
	} catch (const parse_error& ex) {
		synchronize();
		return nullptr;
	}
}


/*!
 * @brief parse variable declaration
 * @return variable declaration statement
 */
std::unique_ptr<stmt::let> parser::letDeclaration() {
	token name = consume(IDENTIFIER, "Expect variable name.");

	std::unique_ptr<expr::expression> initializer;
	if(match(EQUAL)) {
		initializer = expr();
	}

	consume(SEMICOLON, "Expect ; after variable declaration.");

	return std::make_unique<stmt::let>(std::move(name), std::move(initializer));
}



/*!
 * @brief parse a current statement
 * @return statement
 */
std::unique_ptr<stmt::statement> parser::statement() {
	if(match(LEFT_BRACE)) return block();
	if(match(IF)) return ifStatement();
	if(match(WHILE)) return whileStatement();
	if(match(FOR)) return forStatement();
	if(match(LOOP)) return loopStatement();
	if(match(FN)) return functionStatement("function");
	if(match(RETURN)) return returnStatement();
	if(match(BREAK)) return breakStatement();
	if(match(CLASS)) return classStatement();
    return expressionStatement();
}


/*!
 * @brief parse a current block (lexical scope)
 * @return block
 */
std::unique_ptr<stmt::block> parser::block() {
	std::vector<std::unique_ptr<stmt::statement>> statements;

	while (!check(RIGHT_BRACE) && !isAtEnd()) {
		statements.push_back(declaration());
	}

	consume(RIGHT_BRACE, "Expect '}' after block.");

	return std::make_unique<stmt::block>(std::move(statements));
}


/*!
 * @brief parse expression
 * @return expression statement
 */
std::unique_ptr<stmt::expression> parser::expressionStatement() {
    decltype(auto) expr = this->expr();
    consume(SEMICOLON, "Expect ; after expression.");
    return std::make_unique<stmt::expression>(std::move(expr));
}


/*!
 * @brief parse function
 * @param kind kind of function (class method or function)
 * @return function statement
 */
std::unique_ptr<stmt::function> parser::functionStatement(const std::string &kind) {
	auto name = consume(IDENTIFIER, std::format("Expect {} name.", kind));

	consume(LEFT_PAREN, std::format("Expect '(' after {} name.", kind));

	std::vector<token> params;
	if(!check(RIGHT_PAREN)) {
		do {
			if (params.size() >= types::callable::max_arity) {
				error(peek(), std::format("Can't have more than {} parameters.", types::callable::max_arity));
			}
			params.push_back(consume(IDENTIFIER, "Expect parameter name."));
		} while (match(COMMA));
	}

	consume(RIGHT_PAREN, "Expect ')' after parameters.");

	consume(LEFT_BRACE, "Expect '{' before " + kind + " body.");
	const auto body = block();
	return std::make_unique<stmt::function>(std::move(name), std::move(params), std::move(body->statements));
}


/*!
 * @brief class statement
 * @return class declaration
 */
std::unique_ptr<stmt::class_> parser::classStatement() {
	auto name = consume(IDENTIFIER,"Expect class name.");

	std::vector<token> bases;
	if(match(COLON)) {
		do {
			bases.push_back(consume(IDENTIFIER, "Expect class name."));
		} while(match(COMMA));
	}

	consume(LEFT_BRACE, "Expect '{' before class body.");

	std::vector<std::unique_ptr<stmt::function>> methods;
	std::vector<std::unique_ptr<stmt::let>> variables;
	while(!check(RIGHT_BRACE) && !isAtEnd()) {
		if(match(FN)) {
			methods.push_back(functionStatement("method"));
		} else if(match(LET)) {
			variables.push_back(letDeclaration());
		} else {
			throw error(peek(), "Invalid class member.");
		}
	}

	consume(RIGHT_BRACE, "Expect '}' after class body.");

	return std::make_unique<stmt::class_>(std::move(name), std::move(bases), std::move(methods), std::move(variables));
}



/*!
 * @brief parse if-else
 * @return if-else statement
 */
std::unique_ptr<stmt::if_else> parser::ifStatement() {
	consume(LEFT_PAREN, "Expect '(' after 'if'.");
	auto conditional = expr();
	consume(RIGHT_PAREN, "Expect ')' after 'if'.");

	auto ifBranch = statement();
	std::unique_ptr<stmt::statement> elseBranch = nullptr;
	if(match(ELSE)) {
		elseBranch = statement();
	}

	return std::make_unique<stmt::if_else>(std::move(conditional), std::move(ifBranch), std::move(elseBranch));
}


/*!
 * @brief parse while loop
 * @return while loop statement
 */
std::unique_ptr<stmt::loop> parser::whileStatement() {
	consume(LEFT_PAREN, "Expect '(' after 'while'.");
	auto conditional = expr();
	consume(RIGHT_PAREN, "Expect ')' after 'while'.");

	auto body = statement();

	return std::make_unique<stmt::loop>(nullptr,std::move(conditional), nullptr, std::move(body));
}

/*!
 * @brief parse for-loop
 * @return for-loop statement
 */
std::unique_ptr<stmt::loop> parser::forStatement() {
	consume(LEFT_PAREN, "Expect '(' after 'for'.");

	std::unique_ptr<stmt::statement> initializer;
	if(!match(SEMICOLON)) {
		if(match(LET)) initializer = letDeclaration();
		else initializer = expressionStatement();
	}

	std::unique_ptr<expr::expression> conditional;
	if(check(SEMICOLON)) conditional = std::make_unique<expr::literal>(literal{true});
	else conditional = expr();
	consume(SEMICOLON, "Expect ';' after conditional.");

	std::unique_ptr<expr::expression> increment;
	if(!check(RIGHT_PAREN)) {
		increment = expr();
	}

	consume(RIGHT_PAREN, "Expect ')' after 'for'.");

	auto body = statement();

	return std::make_unique<stmt::loop>(std::move(initializer), std::move(conditional), std::move(increment), std::move(body));
}


/*!
 * @brief parse loop
 */
std::unique_ptr<stmt::loop> parser::loopStatement() {
	auto body = statement();

	return std::make_unique<stmt::loop>(nullptr, std::make_unique<expr::literal>(literal{true}), nullptr, std::move(body));
}



/*!
 * @brief parse return
 */
std::unique_ptr<stmt::return_fn> parser::returnStatement() {
	auto keyword = previous();

	std::unique_ptr<expr::expression> value;
	if(!check(SEMICOLON)) {
		value = expr();
	}

	consume(SEMICOLON, "Expect ';' after return value.");

	return std::make_unique<stmt::return_fn>(std::move(keyword), std::move(value));
}



/*!
 * @brief parse 'break'
 */
std::unique_ptr<stmt::break_loop> parser::breakStatement() {
	consume(SEMICOLON, "Expect ; after 'break'.");
	return std::make_unique<stmt::break_loop>();
}
