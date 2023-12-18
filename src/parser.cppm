module;

#include <coroutine>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

export module parser;

import generator;
import lexer;

namespace parser {

export struct Expr {
    virtual ~Expr() = default;
};

export struct Symbol : public Expr {
    std::string name;
    Symbol(std::string name) : name{name} {}
};

export struct Number : public Expr {
    double value;

    Number(double value) : value{value} {}
};

export struct BinaryOperator : public Expr {
    std::string op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;

    BinaryOperator(std::string op, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs)
        : op{op}, lhs{std::move(lhs)}, rhs{std::move(rhs)} {}
};

export struct Call : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    Call(const std::string& callee, std::vector<std::unique_ptr<Expr>> args) : callee{callee}, args{std::move(args)} {}
};

export struct Prototype : public Expr {
    std::string _name;
    std::vector<std::string> args;

    Prototype(const std::string& name, std::vector<std::string> args) : _name{name}, args{std::move(args)} {}
    const std::string& name() const { return this->_name; }
};

export struct Function : public Expr {
    std::unique_ptr<Prototype> proto;
    std::unique_ptr<Expr> body;

    Function(std::unique_ptr<Prototype>&& proto, std::unique_ptr<Expr>&& body)
        : proto{std::move(proto)}, body{std::move(body)} {}
};

template <typename Iterator>
std::unique_ptr<Expr> parse_number(Iterator it) {
    auto result = std::make_unique<Number>(std::stod(std::string(it->text)));
    it++;  // eat number
    return std::move(result);
}

template <typename Iterator>
std::unique_ptr<Expr> parse_expr(Iterator it);

template <typename Iterator>
std::unique_ptr<Expr> parse_parenthesis(Iterator it) {
    if (*it != lexer::Token{"("}) { throw std::runtime_error{"expected ')'"}; }
    it++;  // eat '('

    auto expr = parse_expr(it);

    if (*it != lexer::Token{")"}) { throw std::runtime_error{"expected ')'"}; }
    it++;  // eat ')'

    return expr;
}

template <typename Iterator>
std::unique_ptr<Expr> parse_identifier(Iterator it) {
    auto name = std::string(it->text);
    it++;  // eat identifier

    if (*it != lexer::Token{"("}) { return std::make_unique<Symbol>(name); }

    it++;  // eat '('
    std::vector<std::unique_ptr<Expr>> args;
    while (*it != lexer::Token{")"}) {
        if (auto arg = parse_expr(it)) args.push_back(std::move(arg));
        else throw std::runtime_error{"expected expression"};
        if (*it == lexer::Token{","}) { it++; }
    }
    it++;  // eat ')'

    return std::make_unique<Call>(name, std::move(args));
}

template <typename Iterator>
std::unique_ptr<Expr> parse_primary(Iterator it) {
    if (*it == lexer::Token{"("}) {
        parse_parenthesis(it);
    } else if (auto symbol = dynamic_cast<Symbol*>(&*it)) {
        parse_identifier(it);
    } else if (auto number = dynamic_cast<Number*>(&*it)) {
        parse_number(it);
    } else {
        throw std::runtime_error{"unknown token: " + std::string(it->text)};
    }
}

const std::map<std::string, int> operator_precedence_map{
    {"<", 10},
    {"+", 20},
    {"-", 20},
    {"*", 40},
};

template <typename Iterator>
int operator_precedence(Iterator it) {
    return operator_precedence_map.at(std::string(it->text));
}

template <typename Iterator>
std::unique_ptr<Expr> parse_binop_rhs(int expr_precedence, std::unique_ptr<Expr> lhs, Iterator it) {
    auto token_precedence = operator_precedence(it);

    while (true) {
        if (token_precedence < expr_precedence) { return lhs; }

        auto binop = std::string(it->text);
        it++;  // eat binop

        auto rhs = parse_primary(it);
        auto next_precedence = operator_precedence(it);
        if (token_precedence < next_precedence) { rhs = parse_binop_rhs(token_precedence + 1, std::move(rhs), it); }

        lhs = std::make_unique<BinaryOperator>(binop, std::move(lhs), std::move(rhs));
    }
}

template <typename Iterator>
std::unique_ptr<Expr> parse_expr(Iterator it) {
    auto lhs = parse_primary(it);
    return parse_binop_rhs(0, std::move(lhs), it);
}

template <typename Iterator>
std::unique_ptr<Prototype> parse_prototype(Iterator it) {
    it++;  // eat 'def'

    if (auto* name = dynamic_cast<lexer::Identifier*>(&*it); name == nullptr)
        throw std::runtime_error{"expected function name in prototype"};

    auto name = std::string(it->text);
    it++;  // eat identifier

    if (*it != lexer::Token{"("}) throw std::runtime_error{"expected '(' in prototype"};
    it++;  // eat '('

    std::vector<std::string> args;
    while (auto* arg = dynamic_cast<lexer::Identifier*>(&*it)) {
        args.push_back(std::string(arg->text));
        it++;  // eat identifier
    }

    if (*it != lexer::Token{")"}) throw std::runtime_error{"expected ')' in prototype"};
    it++;  // eat ')'

    return std::make_unique<Prototype>(name, std::move(args));
}

export cppcoro::generator<std::unique_ptr<Expr>> parse(std::istringstream& is) {
    auto tokens = lexer::lex(is);
    auto it = tokens.begin();

    auto token = dynamic_cast<lexer::Eof*>(&*it);
    while (token == nullptr) {
        if (auto def = dynamic_cast<lexer::Def*>(&*it)) {
            auto proto = parse_prototype(it);
            auto body = parse_expr(it);
            co_yield std::make_unique<Expr>(Function{std::move(proto), std::move(body)});
        } else if (auto extern_ = dynamic_cast<lexer::Extern*>(&*it)) {
            co_yield parse_prototype(it);
        } else {
            co_yield parse_expr(it);
        }

        token = dynamic_cast<lexer::Eof*>(&*it);
    }
}

}  // namespace parser
