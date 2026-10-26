#include "parser/lexer.hpp"
#include <cctype>
#include <algorithm>
#include <unordered_map>

namespace minidb {

Lexer::Lexer(std::string sql) : sql_(std::move(sql)) {}

char Lexer::Peek() const {
    if (IsAtEnd()) return '\0';
    return sql_[cursor_];
}

char Lexer::Advance() {
    if (IsAtEnd()) return '\0';
    char c = sql_[cursor_++];
    col_++;
    return c;
}

bool Lexer::IsAtEnd() const {
    return cursor_ >= sql_.size();
}

void Lexer::SkipWhitespace() {
    while (!IsAtEnd()) {
        char c = Peek();
        if (c == ' ' || c == '\t' || c == '\r') {
            Advance();
        } else if (c == '\n') {
            line_++;
            col_ = 1;
            cursor_++;
        } else if (c == '-' && cursor_ + 1 < sql_.size() && sql_[cursor_ + 1] == '-') {
            // SQL comment -- ...
            while (!IsAtEnd() && Peek() != '\n') {
                Advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::IdentifierOrKeyword() {
    int start_col = col_;
    std::string text;
    while (!IsAtEnd() && (std::isalnum(Peek()) || Peek() == '_')) {
        text += Advance();
    }

    std::string upper = text;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"CREATE", TokenType::CREATE},
        {"TABLE", TokenType::TABLE},
        {"INDEX", TokenType::INDEX},
        {"ON", TokenType::ON},
        {"DROP", TokenType::DROP},
        {"INSERT", TokenType::INSERT},
        {"INTO", TokenType::INTO},
        {"VALUES", TokenType::VALUES},
        {"SELECT", TokenType::SELECT},
        {"FROM", TokenType::FROM},
        {"WHERE", TokenType::WHERE},
        {"AND", TokenType::AND},
        {"OR", TokenType::OR},
        {"NOT", TokenType::NOT},
        {"UPDATE", TokenType::UPDATE},
        {"SET", TokenType::SET},
        {"DELETE", TokenType::DELETE},
        {"INT", TokenType::INTEGER},
        {"INTEGER", TokenType::INTEGER},
        {"BIGINT", TokenType::BIGINT},
        {"FLOAT", TokenType::FLOAT},
        {"DOUBLE", TokenType::DOUBLE},
        {"VARCHAR", TokenType::VARCHAR},
        {"BOOLEAN", TokenType::BOOLEAN},
        {"BOOL", TokenType::BOOLEAN},
        {"TRUE", TokenType::TRUE_KW},
        {"FALSE", TokenType::FALSE_KW},
        {"NULL", TokenType::NULL_KW},
    };

    auto it = keywords.find(upper);
    if (it != keywords.end()) {
        return Token{it->second, text, line_, start_col};
    }
    return Token{TokenType::IDENTIFIER, text, line_, start_col};
}

Token Lexer::Number() {
    int start_col = col_;
    std::string text;
    bool has_dot = false;

    if (Peek() == '-') {
        text += Advance();
    }

    while (!IsAtEnd() && (std::isdigit(Peek()) || Peek() == '.')) {
        if (Peek() == '.') {
            if (has_dot) break;
            has_dot = true;
        }
        text += Advance();
    }
    return Token{TokenType::NUMBER, text, line_, start_col};
}

Token Lexer::String() {
    int start_col = col_;
    Advance(); // Consume opening quote '
    std::string text;

    while (!IsAtEnd() && Peek() != '\'') {
        if (Peek() == '\\' && cursor_ + 1 < sql_.size()) {
            Advance();
        }
        text += Advance();
    }

    if (!IsAtEnd() && Peek() == '\'') {
        Advance(); // Consume closing quote '
    }

    return Token{TokenType::STRING_LITERAL, text, line_, start_col};
}

std::vector<Token> Lexer::Tokenize() {
    std::vector<Token> tokens;

    while (true) {
        SkipWhitespace();
        if (IsAtEnd()) {
            tokens.push_back(Token{TokenType::END_OF_FILE, "", line_, col_});
            break;
        }

        char c = Peek();
        int cur_col = col_;

        if (std::isalpha(c) || c == '_') {
            tokens.push_back(IdentifierOrKeyword());
        } else if (std::isdigit(c) || (c == '-' && cursor_ + 1 < sql_.size() && std::isdigit(sql_[cursor_ + 1]))) {
            tokens.push_back(Number());
        } else if (c == '\'') {
            tokens.push_back(String());
        } else if (c == '=') {
            Advance();
            tokens.push_back(Token{TokenType::EQUALS, "=", line_, cur_col});
        } else if (c == '!' && cursor_ + 1 < sql_.size() && sql_[cursor_ + 1] == '=') {
            Advance(); Advance();
            tokens.push_back(Token{TokenType::NOT_EQUALS, "!=", line_, cur_col});
        } else if (c == '<') {
            Advance();
            if (Peek() == '=') {
                Advance();
                tokens.push_back(Token{TokenType::LESS_EQUALS, "<=", line_, cur_col});
            } else if (Peek() == '>') {
                Advance();
                tokens.push_back(Token{TokenType::NOT_EQUALS, "<>", line_, cur_col});
            } else {
                tokens.push_back(Token{TokenType::LESS_THAN, "<", line_, cur_col});
            }
        } else if (c == '>') {
            Advance();
            if (Peek() == '=') {
                Advance();
                tokens.push_back(Token{TokenType::GREATER_EQUALS, ">=", line_, cur_col});
            } else {
                tokens.push_back(Token{TokenType::GREATER_THAN, ">", line_, cur_col});
            }
        } else if (c == ',') {
            Advance();
            tokens.push_back(Token{TokenType::COMMA, ",", line_, cur_col});
        } else if (c == ';') {
            Advance();
            tokens.push_back(Token{TokenType::SEMICOLON, ";", line_, cur_col});
        } else if (c == '(') {
            Advance();
            tokens.push_back(Token{TokenType::LPAREN, "(", line_, cur_col});
        } else if (c == ')') {
            Advance();
            tokens.push_back(Token{TokenType::RPAREN, ")", line_, cur_col});
        } else if (c == '*') {
            Advance();
            tokens.push_back(Token{TokenType::STAR, "*", line_, cur_col});
        } else {
            Advance(); // unknown char
        }
    }

    return tokens;
}

} // namespace minidb
