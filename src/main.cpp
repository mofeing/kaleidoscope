#include <cxxabi.h>
#include <iostream>
#include <sstream>
#include <string>

import lexer;
import parser;
import codegen;

using namespace codegen;

std::string demangle(const char* name) {
    int status = -4;  // some arbitrary value to eliminate the compiler warning
    std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, NULL, NULL, &status), std::free};
    return (status == 0) ? res.get() : name;
}

[[noreturn]] void driver_loop(Codegen& cdg) {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream is{line};
        for (auto& expr : parser::parse(is)) { std::cout << cdg(*expr) << std::endl; }
        std::cout << "repl> ";
    }
    std::exit(0);
}

int main() {
    Codegen cdg{"my cool jit"};
    std::cout << "repl> ";
    driver_loop(cdg);
}