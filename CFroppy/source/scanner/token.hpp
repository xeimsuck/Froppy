#pragma once
#include <optional>
#include <string>
#include "literal.hpp"

namespace cfp::scan {
    struct token {
        enum class tokenType {
            // Single-character tokens.
            LEFT_PAREN, RIGHT_PAREN, LEFT_BRACE, RIGHT_BRACE,
            COMMA, DOT, MINUS, PLUS, SEMICOLON, SLASH, STAR,

            // One or two character tokens.
            BANG, BANG_EQUAL,
            EQUAL, EQUAL_EQUAL,
            GREATER, GREATER_EQUAL,
            LESS, LESS_EQUAL,

            // Literals.
            IDENTIFIER, STRING, NUMBER,

            // Keywords.
            AND, CLASS, ELSE, FALSE, FUN, FOR, IF, NIL, OR,
            PRINT, RETURN, SUPER, THIS, TRUE, VAR, WHILE,

            END_OF_FILE
        };

        tokenType type;
        std::string lexeme;
        std::optional<scan::literal> literal;
        int line;

        token(tokenType type, std::string lexeme, std::optional<scan::literal> literal, int line);
    };
}
