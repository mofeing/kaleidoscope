add_rules("mode.debug", "mode.release")
set_languages("c++20")

add_requires("brew::llvm", {alias = "llvm", configs = {shared = true}})

target("kaleidoscope")
    set_kind("shared")
    add_files(
        "src/generator.cppm",
        "src/lexer.cppm",
        "src/parser.cppm",
        "src/codegen.cppm"
        )
    add_packages("llvm", {public=true})
    add_links("c++")
    add_links("termcap")

target("repl")
    set_kind("binary")
    add_files("src/main.cpp")
    add_deps("kaleidoscope")
