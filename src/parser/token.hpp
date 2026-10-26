#pragma once

#include <string>

namespace minidb {

enum class TokenType {
    END_OF_FILE,
    IDENTIFIER,
    NUMBER,
    STRING_LITERAL,

    // Keywords
    CREATE, TABLE, INDEX, ON, DROP,
    INSERT, INTO, VALUES,
    SELECT, FROM, WHERE, AND, OR, NOT,
    UPDATE, SET, DELETE,
    INTEGER, BIGINT, FLOAT, DOUBLE, VARCHAR, BOOLEAN, TRUE_KW, FALSE_KW, NULL_KW,

    // Operators & Punctuation
    EQUALS, NOT_EQUALS,
    LESS_THAN, LESS_EQUALS,
    GREATER_THAN, GREATER_EQUALS,
    COMMA, SEMICOLON,
    LPAREN, RPAREN,
    STAR,
};

struct Token {
    TokenType type;
    std::string text;
    int line{1};
    int col{1};
};

} // namespace minidb
