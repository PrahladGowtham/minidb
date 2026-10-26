#pragma once

#include "parser/token.hpp"
#include <string>
#include <vector>

namespace minidb {

class Lexer {
public:
    explicit Lexer(std::string sql);

    std::vector<Token> Tokenize();

private:
    char Peek() const;
    char Advance();
    bool IsAtEnd() const;
    void SkipWhitespace();

    Token IdentifierOrKeyword();
    Token Number();
    Token String();

    std::string sql_;
    size_t cursor_{0};
    int line_{1};
    int col_{1};
};

} // namespace minidb
