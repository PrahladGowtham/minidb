#pragma once

#include "parser/lexer.hpp"
#include "parser/ast.hpp"
#include <memory>
#include <string>

namespace minidb {

class Parser {
public:
    explicit Parser(std::string sql);

    std::unique_ptr<SQLStatement> ParseStatement();

private:
    const Token& Current() const;
    const Token& Advance();
    bool Match(TokenType type);
    const Token& Expect(TokenType type, const std::string& err_msg);
    bool IsAtEnd() const;

    std::unique_ptr<SQLStatement> ParseCreateTable();
    std::unique_ptr<SQLStatement> ParseCreateIndex();
    std::unique_ptr<SQLStatement> ParseInsert();
    std::unique_ptr<SQLStatement> ParseSelect();
    std::unique_ptr<SQLStatement> ParseUpdate();
    std::unique_ptr<SQLStatement> ParseDelete();

    std::unique_ptr<AbstractExpression> ParseExpression();
    std::unique_ptr<AbstractExpression> ParsePrimary();
    Value ParseLiteralValue();

    std::vector<Token> tokens_;
    size_t cursor_{0};
};

} // namespace minidb
