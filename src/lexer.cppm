module;

// #include "generator.hpp"

#include <charconv>
#include <coroutine>
#include <istream>
#include <limits>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

export module lexer;

import generator;

namespace lexer {

export struct Token {
    std::string_view text;
    Token(std::string_view text) : text{text} {}
    virtual ~Token() = default;

    bool operator==(const Token& other) const { return this->text == other.text; }
    bool operator!=(const Token& other) const { return !(*this == other); }
};

export struct Eof : public Token {
    Eof() : Token{""} {}
};

export struct Def : public Token {
    Def() : Token{"def"} {}
};

export struct Extern : public Token {
    Extern() : Token{"extern"} {}
};

export struct Identifier : public Token {
    Identifier(std::string_view text) : Token{text} {}
};

export struct Number : public Token {
    double value;
    Number(std::string_view text) : Token{text} { value = std::stod(std::string{text}); }
};

export struct token_iterator {
    token_iterator(std::istream& is) {}
};

export cppcoro::generator<Token> lex(std::istringstream& is) {
    while (true) {
        // skip whitespace and newlines
        while (std::isspace(is.peek())) { is.get(); }

        // skip comment
        if (static_cast<char>(is.peek()) == '#') {
            // TODO may fail if comment is last line
            is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

        // match identifier
        else if (std::isalpha(is.peek())) {
            std::string match;
            while (std::isalnum(is.peek())) { match += is.get(); }

            if (match == "def") {
                co_yield Def{};
            } else if (match == "extern") {
                co_yield Extern{};
            } else {
                co_yield Identifier{match};
            }
        }

        // match number
        else if (std::isdigit(is.peek()) || static_cast<char>(is.peek()) == '.') {
            std::string match;
            do { match += is.get(); } while (std::isdigit(is.peek()) || static_cast<char>(is.peek()) == '.');

            co_yield Number{match};
        }

        // finish on EOF
        else if (is.peek() == EOF) {
            co_yield Eof{};
            break;
        }

        // default: match single character
        else {
            co_yield Token{std::string{static_cast<char>(is.get())}};
        }
    }
}

}  // namespace lexer
