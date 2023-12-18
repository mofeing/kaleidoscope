module;

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

#include <map>
#include <memory>
#include <ranges>
#include <typeindex>
#include <variant>

export module codegen;

import parser;

export namespace codegen {

using namespace parser;

struct Codegen {
    llvm::LLVMContext context;
    llvm::IRBuilder<> builder;
    llvm::Module mod;
    std::map<std::string, llvm::Value*> named_values;

    Codegen(const std::string& mod_name) : context{}, builder{context}, mod{mod_name, context} {}

    auto operator()(Expr& expr) -> llvm::Value*;
};

using CodeGenerator = std::function<llvm::Value*(Codegen&, Expr&)>;
std::unordered_map<std::type_index, CodeGenerator> dispatcher{
    {typeid(Number),
     [](Codegen& cdg, Expr& expr) -> llvm::Value* {
         auto& number_expr = static_cast<Number&>(expr);
         return llvm::ConstantFP::get(cdg.context, llvm::APFloat{number_expr.value});
     }},
    {typeid(BinaryOperator),
     [](Codegen& cdg, Expr& expr) -> llvm::Value* {
         auto& binop_expr = static_cast<BinaryOperator&>(expr);
         llvm::Value* l = cdg(*binop_expr.lhs);
         llvm::Value* r = cdg(*binop_expr.rhs);
         if (l == nullptr || r == nullptr) return nullptr;

         if (binop_expr.op == "+") {
             return cdg.builder.CreateFAdd(l, r, "addtmp");
         } else if (binop_expr.op == "-") {
             return cdg.builder.CreateFSub(l, r, "subtmp");
         } else if (binop_expr.op == "*") {
             return cdg.builder.CreateFMul(l, r, "multmp");
         } else if (binop_expr.op == "<") {
             l = cdg.builder.CreateFCmpULT(l, r, "cmptmp");
             return cdg.builder.CreateUIToFP(l, llvm::Type::getDoubleTy(cdg.context), "booltmp");
         } else {
             throw std::runtime_error{"invalid binary operator"};
         }
     }},
    {typeid(Call),
     [](Codegen& cdg, Expr& expr) -> llvm::Value* {
         // look up the name in the global module table
         auto& call_expr = static_cast<Call&>(expr);
         llvm::Function* callee = cdg.mod.getFunction(call_expr.callee);
         if (callee == nullptr) throw std::runtime_error{"unknown function referenced"};

         // if argument mismatch error
         if (callee->arg_size() != call_expr.args.size()) throw std::runtime_error{"incorrect # arguments passed"};

         std::vector<llvm::Value*> args;
         for (auto& arg : call_expr.args) {
             args.push_back(cdg(*arg));
             if (args.back() == nullptr) return nullptr;
         }

         return cdg.builder.CreateCall(callee, args, "calltmp");
     }},
    {typeid(Prototype),
     [](Codegen& cdg, Expr& expr) -> llvm::Value* {
         auto& proto_expr = static_cast<Prototype&>(expr);

         // make the function type
         std::vector<llvm::Type*> arg_types{proto_expr.args.size(), llvm::Type::getDoubleTy(cdg.context)};
         llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getDoubleTy(cdg.context), arg_types, false);
         llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, proto_expr.name(), cdg.mod);

         // set names for all arguments
         int index = 0;
         for (auto& arg : f->args()) arg.setName(proto_expr.args[index++]);

         return f;
     }},
    {typeid(Function),
     [](Codegen& cdg, Expr& expr) -> llvm::Value* {
         auto& func_expr = static_cast<Function&>(expr);

         // check for an existing function from a previous 'extern' declaration
         llvm::Function* function = cdg.mod.getFunction(func_expr.proto->name());
         if (function == nullptr) function = reinterpret_cast<llvm::Function*>(cdg(*func_expr.proto));
         if (function == nullptr) return nullptr;

         // create basic block
         llvm::BasicBlock* bb = llvm::BasicBlock::Create(cdg.context, "entry", function);
         cdg.builder.SetInsertPoint(bb);

         // record function arguments into the `named_values` map
         cdg.named_values.clear();
         for (auto& arg : function->args()) cdg.named_values[std::string(arg.getName())] = &arg;

         llvm::Value* return_value = cdg(*func_expr.body);
         if (return_value == nullptr) {
             function->eraseFromParent();
             return nullptr;
         }

         cdg.builder.CreateRet(return_value);

         // validate generated code
         llvm::verifyFunction(*function);
         return function;
     }},
};

auto Codegen::operator()(Expr& expr) -> llvm::Value* {
    auto it = dispatcher.find(typeid(expr));
    if (it == dispatcher.end()) throw std::runtime_error{"unknown expression type"};
    return it->second(*this, expr);
}

}  // namespace codegen