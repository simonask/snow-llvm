#define __STDC_LIMIT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

#include "snow/vm.h"
#include "snow/vm-intern.hpp"
#include "snow/process.h"
#include "snow/ast.hpp"

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Constants.h>
#include <llvm/Instructions.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

#include <google/dense_hash_map>

using namespace llvm;
using namespace snow;

struct FunctionBuilderInfo {
	SnVM* vm;
	SnFunctionRef snow_function;
	llvm::Function* llvm_function;
	google::dense_hash_map<SnSymbol, Value*> locals;
	
	Value* process;   // SN_P
	Value* here;      // SnFunctionCallContext
	Value* arguments; // SnArguments
	Value* self;
	Value* it;
};

struct Builder : public llvm::IRBuilder<> {
	FunctionBuilderInfo& info;
	
	Value* last_value;
	
	Builder(llvm::BasicBlock* bb, FunctionBuilderInfo& info) : IRBuilder<>(bb) , info(info) , last_value(NULL) {}
	Builder(const Builder& parent, llvm::BasicBlock* bb) : llvm::IRBuilder<>(bb), info(parent.info), last_value(parent.last_value) {}
};

#define VM_SET_FUNC(VM, NAME) do {                         \
	llvm::Module* m = (VM)->module;                     \
	llvm::Function* f = m->getFunction("snow_" #NAME);  \
	if (!f) { fprintf(stderr, "Function snow_" #NAME " could not be found!\n"); }  \
	(VM)->NAME = f;                                     \
} while (0)

CAPI void snow_vm_init(SN_P p) {
	SnVM* vm = reinterpret_cast<SnVM*>(p->vm);
	// get functions from main module, for fast compilation later
	VM_SET_FUNC(vm, call);
	VM_SET_FUNC(vm, call_with_self);
	VM_SET_FUNC(vm, call_method);
	VM_SET_FUNC(vm, get_member);
	VM_SET_FUNC(vm, set_member);
	VM_SET_FUNC(vm, get_global);
	VM_SET_FUNC(vm, set_global);
	VM_SET_FUNC(vm, array_get);
	VM_SET_FUNC(vm, array_set);
	VM_SET_FUNC(vm, arguments_init);
	VM_SET_FUNC(vm, argument_get);
	VM_SET_FUNC(vm, argument_push);
	VM_SET_FUNC(vm, argument_set_named);
	VM_SET_FUNC(vm, get_local);
	VM_SET_FUNC(vm, set_local);
	
	vm->value_type = sizeof(VALUE) == 8 ? Type::getInt64Ty(*vm->context) : Type::getInt32Ty(*vm->context);
	
	vm->null_constant = ConstantInt::get(vm->value_type, 0);
	vm->nil_constant = ConstantInt::get(vm->value_type, (uintptr_t)SN_NIL);
	vm->true_constant = ConstantInt::get(vm->value_type, (uintptr_t)SN_TRUE);
	vm->false_constant = ConstantInt::get(vm->value_type, (uintptr_t)SN_FALSE);
	
	// signature: VALUE func(SN_P, SnFunctionCallContext* here)
	const Type* voidPtrTy = Type::getInt8PtrTy(*vm->context);
	std::vector<const Type*> params;
	params.push_back(PointerType::getUnqual(vm->module->getTypeByName("SnProcess")));
	params.push_back(PointerType::getUnqual(vm->module->getTypeByName("SnFunctionCallContext")));
	vm->function_type = FunctionType::get(voidPtrTy, params, false);
}

CAPI SnFunctionRef snow_vm_load_precompiled_image(SN_P p, const char* file) {
	std::string error;
	Module* m = NULL;
	MemoryBuffer* buffer = NULL;
	if ((buffer = MemoryBuffer::getFile(file, &error))) {
		m = getLazyBitcodeModule(buffer, *(LLVMContext*)p->vm->context, &error);
		if (!m)
			delete buffer;
	}
	
	Function* hello = m->getFunction("hello");
	
	ExecutionEngine* engine = (ExecutionEngine*)p->vm->engine;
	engine->runStaticConstructorsDestructors(m, false);
	std::vector<GenericValue> args;
	args.push_back(llvm::PTOGV((void*)"world"));
	GenericValue gv = engine->runFunction(hello, args);
	
	//engine->runStaticConstructorsDestructors(m, true);
	//engine->freeMachineCodeForFunction(hello);
	return snow_create_function(p, hello);
}

static void vm_compile_recursive(SN_P p, const SnAstNode* node, Builder& builder) {
	SnVM* vm = builder.info.vm;
	FunctionBuilderInfo& info = builder.info;
	
	switch (node->type) {
		case SN_AST_SEQUENCE: {
			for (SnAstNode* x = node->sequence.head; x; x = x->next) {
				vm_compile_recursive(p, x, builder);
			}
			break;
		}
		case SN_AST_LITERAL: {
			builder.last_value = ConstantInt::get(vm->value_type, (intptr_t)node->literal.value);
			break;
		}
		case SN_AST_CLOSURE: {
			break;
		}
		case SN_AST_PARAMETER: {
			ASSERT(false); // Invalid parameter at this point
			break;
		}
		case SN_AST_RETURN: {
			if (node->return_expr.value) {
				vm_compile_recursive(p, node->return_expr.value, builder);
			}
			builder.CreateRet(builder.last_value);
			break;
		}
		case SN_AST_IDENTIFIER: {
			Value* sym = ConstantInt::get(vm->value_type, (intptr_t)node->identifier.name);
			builder.last_value = builder.CreateCall3(vm->get_local, info.process, info.here, sym);
			break;
		}
		case SN_AST_BREAK: {
			ASSERT(false); // NIY
			break;
		}
		case SN_AST_CONTINUE: {
			ASSERT(false); // NIY
			break;
		}
		case SN_AST_SELF: {
			builder.last_value = builder.CreateLoad(builder.CreateGEP(builder.info.here, ConstantInt::get(vm->value_type, offsetof(SnFunctionCallContext, self))));
			break;
		}
		case SN_AST_HERE: {
			builder.last_value = info.here;
			break;
		}
		case SN_AST_IT: {
			builder.last_value = info.it;
			break;
		}
		case SN_AST_ASSIGN: {
			break;
		}
		case SN_AST_MEMBER: {
			break;
		}
		case SN_AST_CALL: {
			break;
		}
		case SN_AST_ASSOCIATION: {
			break;
		}
		case SN_AST_NAMED_ARGUMENT: {
			break;
		}
		case SN_AST_AND: {
			break;
		}
		case SN_AST_OR: {
			break;
		}
		case SN_AST_XOR: {
			break;
		}
		case SN_AST_NOT: {
			break;
		}
		case SN_AST_LOOP: {
			break;
		}
		case SN_AST_IF_ELSE: {
			break;
		}
	}
}
	
CAPI SnFunctionRef snow_vm_compile_ast(SN_P p, const SnAST* ast, const char* source) {
	SnVM* vm = reinterpret_cast<SnVM*>(p->vm);
	
	Module* m = new Module("<module compiled by Snow>", *vm->context);
	
	FunctionBuilderInfo info;
	info.vm = vm;
	info.llvm_function = Function::Create(vm->function_type, Function::ExternalLinkage, "__snow_entry", m);
	Function::arg_iterator ai = info.llvm_function->arg_begin();
	info.process = ai++;
	info.process->setName("p");
	info.here = ai++;
	info.here->setName("here");

	BasicBlock* entry = BasicBlock::Create(*vm->context, "entry", info.llvm_function);
	Builder builder(entry, info);
	builder.last_value = vm->nil_constant;
	
	/*
		VALUE arguments = here->arguments;
		VALUE self = here->self;
	*/
	info.arguments = builder.CreateLoad(builder.CreateGEP(info.here, ConstantInt::get(vm->value_type, offsetof(SnFunctionCallContext, arguments))));
	info.self = builder.CreateLoad(builder.CreateGEP(info.here, ConstantInt::get(vm->value_type, offsetof(SnFunctionCallContext, self))));
	
	/*
		VALUE it = args ? argument_get(args, 0) : SN_NIL;
	*/
	BasicBlock* args_is_null = BasicBlock::Create(*vm->context, "args_is_null", info.llvm_function);
	BasicBlock* args_is_not_null = BasicBlock::Create(*vm->context, "args_is_not_null", info.llvm_function);
	Builder builder_args_is_not_null(args_is_not_null, info);
	outs() << *vm->argument_get << '\n';
	Value* it = builder_args_is_not_null.CreateCall3(vm->argument_get, info.process, info.arguments, vm->null_constant);
	builder.CreateCondBr(builder.CreateIsNull(info.arguments), args_is_null, args_is_not_null);
	llvm::PHINode* it_phi = builder.CreatePHI(vm->value_type);
	it_phi->addIncoming(vm->nil_constant, args_is_null);
	it_phi->addIncoming(it, args_is_not_null);
	info.it = it_phi;
	
	
	info.snow_function = snow_create_function(p, info.llvm_function);
	
	/*ASSERT(ast->type == SN_AST_FUNCTION);
	const SnAstFunction* ast_function = &ast->function;
	vm_compile_recursive(p, vm, ast_function->seq_body, info);*/
	
	//ReturnInst::Create(*vm->context, info.last_value, info.block);
	
	printf("COMPILED MODULE:\n");
	outs() << *m << '\n';
	
	vm->engine->addModule(m);
	
	return info.snow_function;
}

CAPI VALUE snow_vm_call_function(SN_P p, void* jit_func, SnFunctionRef function, VALUE self, struct SnArguments* args) {
	SnVM* vm = reinterpret_cast<SnVM*>(p->vm);
	Function* f = reinterpret_cast<Function*>(jit_func);
	
	std::vector<GenericValue> a;
	a.reserve(4);
	a.push_back(PTOGV(p));
	a.push_back(PTOGV(snow_function_as_object(function)));
	a.push_back(PTOGV(self));
	a.push_back(PTOGV(args));
	
	GenericValue result = vm->engine->runFunction(f, a);
	return GVTOP(result);
}