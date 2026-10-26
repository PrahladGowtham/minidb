#include "parser/parser.hpp"
#include "common/exception.hpp"
#include <algorithm>

namespace minidb {

Parser::Parser(std::string sql) {
    Lexer lexer(std::move(sql));
    tokens_ = lexer.Tokenize();
}

const Token& Parser::Current() const {
    if (cursor_ >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[cursor_];
}

const Token& Parser::Advance() {
    const Token& t = Current();
    if (cursor_ < tokens_.size()) {
        cursor_++;
    }
    return t;
}

bool Parser::Match(TokenType type) {
    if (Current().type == type) {
        Advance();
        return true;
    }
    return false;
}

const Token& Parser::Expect(TokenType type, const std::string& err_msg) {
    if (Current().type != type) {
        throw DBException("Syntax error at line " + std::to_string(Current().line) +
                          ", col " + std::to_string(Current().col) + ": " + err_msg +
                          " (got '" + Current().text + "')");
    }
    return Advance();
}

bool Parser::IsAtEnd() const {
    return Current().type == TokenType::END_OF_FILE;
}

std::unique_ptr<SQLStatement> Parser::ParseStatement() {
    if (Match(TokenType::CREATE)) {
        if (Match(TokenType::TABLE)) {
            return ParseCreateTable();
        } else if (Match(TokenType::INDEX)) {
            return ParseCreateIndex();
        }
        throw DBException("Expected TABLE or INDEX after CREATE");
    } else if (Match(TokenType::INSERT)) {
        Expect(TokenType::INTO, "Expected INTO after INSERT");
        return ParseInsert();
    } else if (Match(TokenType::SELECT)) {
        return ParseSelect();
    } else if (Match(TokenType::UPDATE)) {
        return ParseUpdate();
    } else if (Match(TokenType::DELETE)) {
        Expect(TokenType::FROM, "Expected FROM after DELETE");
        return ParseDelete();
    }

    throw DBException("Unsupported or invalid SQL statement: " + Current().text);
}

std::unique_ptr<SQLStatement> Parser::ParseCreateTable() {
    const Token& tbl_token = Expect(TokenType::IDENTIFIER, "Expected table name");
    Expect(TokenType::LPAREN, "Expected '(' after table name");

    std::vector<Column> columns;
    while (true) {
        const Token& col_token = Expect(TokenType::IDENTIFIER, "Expected column name");
        TypeId type_id = TypeId::INVALID;
        uint32_t len = 0;

        if (Match(TokenType::INTEGER)) {
            type_id = TypeId::INTEGER;
        } else if (Match(TokenType::BIGINT)) {
            type_id = TypeId::BIGINT;
        } else if (Match(TokenType::FLOAT)) {
            type_id = TypeId::FLOAT;
        } else if (Match(TokenType::DOUBLE)) {
            type_id = TypeId::DOUBLE;
        } else if (Match(TokenType::BOOLEAN)) {
            type_id = TypeId::BOOLEAN;
        } else if (Match(TokenType::VARCHAR)) {
            type_id = TypeId::VARCHAR;
            len = 255;
            if (Match(TokenType::LPAREN)) {
                const Token& len_token = Expect(TokenType::NUMBER, "Expected VARCHAR length");
                len = std::stoul(len_token.text);
                Expect(TokenType::RPAREN, "Expected ')' after VARCHAR length");
            }
        } else {
            throw DBException("Unknown type for column: " + col_token.text);
        }

        columns.emplace_back(col_token.text, type_id, true, len);

        if (!Match(TokenType::COMMA)) {
            break;
        }
    }

    Expect(TokenType::RPAREN, "Expected ')' at end of column list");
    Match(TokenType::SEMICOLON);

    return std::make_unique<CreateTableStatement>(tbl_token.text, std::move(columns));
}

std::unique_ptr<SQLStatement> Parser::ParseCreateIndex() {
    const Token& idx_token = Expect(TokenType::IDENTIFIER, "Expected index name");
    Expect(TokenType::ON, "Expected ON after index name");
    const Token& tbl_token = Expect(TokenType::IDENTIFIER, "Expected table name");
    Expect(TokenType::LPAREN, "Expected '(' before index column");
    const Token& col_token = Expect(TokenType::IDENTIFIER, "Expected column name");
    Expect(TokenType::RPAREN, "Expected ')' after index column");
    Match(TokenType::SEMICOLON);

    return std::make_unique<CreateIndexStatement>(idx_token.text, tbl_token.text, col_token.text);
}

Value Parser::ParseLiteralValue() {
    if (Current().type == TokenType::NUMBER) {
        std::string num_str = Advance().text;
        if (num_str.find('.') != std::string::npos) {
            return Value(std::stod(num_str));
        }
        return Value(static_cast<int64_t>(std::stoll(num_str)));
    } else if (Current().type == TokenType::STRING_LITERAL) {
        return Value(Advance().text);
    } else if (Match(TokenType::TRUE_KW)) {
        return Value(true);
    } else if (Match(TokenType::FALSE_KW)) {
        return Value(false);
    } else if (Match(TokenType::NULL_KW)) {
        return Value();
    }
    throw DBException("Expected literal value, got '" + Current().text + "'");
}

std::unique_ptr<SQLStatement> Parser::ParseInsert() {
    const Token& tbl_token = Expect(TokenType::IDENTIFIER, "Expected table name");
    Expect(TokenType::VALUES, "Expected VALUES");

    std::vector<std::vector<Value>> rows;
    while (true) {
        Expect(TokenType::LPAREN, "Expected '(' for row values");
        std::vector<Value> row;
        while (true) {
            row.push_back(ParseLiteralValue());
            if (!Match(TokenType::COMMA)) {
                break;
            }
        }
        Expect(TokenType::RPAREN, "Expected ')' for row values");
        rows.push_back(std::move(row));

        if (!Match(TokenType::COMMA)) {
            break;
        }
    }

    Match(TokenType::SEMICOLON);
    return std::make_unique<InsertStatement>(tbl_token.text, std::move(rows));
}

std::unique_ptr<SQLStatement> Parser::ParseSelect() {
    std::vector<std::string> select_cols;
    if (Match(TokenType::STAR)) {
        select_cols.push_back("*");
    } else {
        while (true) {
            const Token& col_token = Expect(TokenType::IDENTIFIER, "Expected column name");
            select_cols.push_back(col_token.text);
            if (!Match(TokenType::COMMA)) {
                break;
            }
        }
    }

    Expect(TokenType::FROM, "Expected FROM");
    const Token& tbl_token = Expect(TokenType::IDENTIFIER, "Expected table name");

    std::unique_ptr<AbstractExpression> where_expr = nullptr;
    if (Match(TokenType::WHERE)) {
        where_expr = ParseExpression();
    }

    Match(TokenType::SEMICOLON);
    return std::make_unique<SelectStatement>(tbl_token.text, std::move(select_cols), std::move(where_expr));
}

std::unique_ptr<SQLStatement> Parser::ParseUpdate() {
    const Token& tbl_token = Expect(TokenType::IDENTIFIER, "Expected table name");
    Expect(TokenType::SET, "Expected SET");
    const Token& col_token = Expect(TokenType::IDENTIFIER, "Expected column name to update");
    Expect(TokenType::EQUALS, "Expected '=' in SET clause");
    Value new_val = ParseLiteralValue();

    std::unique_ptr<AbstractExpression> where_expr = nullptr;
    if (Match(TokenType::WHERE)) {
        where_expr = ParseExpression();
    }

    Match(TokenType::SEMICOLON);
    return std::make_unique<UpdateStatement>(tbl_token.text, col_token.text, std::move(new_val), std::move(where_expr));
}

std::unique_ptr<SQLStatement> Parser::ParseDelete() {
    const Token& tbl_token = Expect(TokenType::IDENTIFIER, "Expected table name");

    std::unique_ptr<AbstractExpression> where_expr = nullptr;
    if (Match(TokenType::WHERE)) {
        where_expr = ParseExpression();
    }

    Match(TokenType::SEMICOLON);
    return std::make_unique<DeleteStatement>(tbl_token.text, std::move(where_expr));
}

std::unique_ptr<AbstractExpression> Parser::ParsePrimary() {
    if (Current().type == TokenType::IDENTIFIER) {
        return std::make_unique<ColumnRefExpression>(Advance().text);
    }
    return std::make_unique<ConstantExpression>(ParseLiteralValue());
}

std::unique_ptr<AbstractExpression> Parser::ParseExpression() {
    auto left = ParsePrimary();

    ComparisonType comp_type;
    if (Match(TokenType::EQUALS)) {
        comp_type = ComparisonType::EQUAL;
    } else if (Match(TokenType::NOT_EQUALS)) {
        comp_type = ComparisonType::NOT_EQUAL;
    } else if (Match(TokenType::LESS_THAN)) {
        comp_type = ComparisonType::LESS_THAN;
    } else if (Match(TokenType::LESS_EQUALS)) {
        comp_type = ComparisonType::LESS_EQUAL;
    } else if (Match(TokenType::GREATER_THAN)) {
        comp_type = ComparisonType::GREATER_THAN;
    } else if (Match(TokenType::GREATER_EQUALS)) {
        comp_type = ComparisonType::GREATER_EQUAL;
    } else {
        return left;
    }

    auto right = ParsePrimary();
    return std::make_unique<ComparisonExpression>(std::move(left), std::move(right), comp_type);
}

} // namespace minidb
