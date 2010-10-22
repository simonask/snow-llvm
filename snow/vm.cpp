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

#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace snow;

struct SnCompilationInfo {
	const char* source;
	SnFunctionRef ref;
	Function* function;
	const Type* value_type;
	
	BasicBlock* block;
	Value* last_value;
	
	Value* p;
	Value* this_function;
	Value* self;
	Value* args;
};

#define VM_SET_FUNC(VM, NAME) {                         \
	llvm::Module* m = (VM)->module;                     \
	llvm::Function* f = m->getFunction("snow_" #NAME);  \
	if (!f) { fprintf(stderr, "Function snow_" #NAME " could not be found!\n"); }  \
	(VM)->NAME = f;                                     \
}

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
	
	vm->value_type = sizeof(VALUE) == 8 ? Type::getInt64Ty(*vm->context) : Type::getInt32Ty(*vm->context);
	
	vm->null_constant = ConstantInt::get(vm->value_type, 0);
	vm->nil_constant = ConstantInt::get(vm->value_type, (uintptr_t)SN_NIL);
	vm->true_constant = ConstantInt::get(vm->value_type, (uintptr_t)SN_TRUE);
	vm->false_constant = ConstantInt::get(vm->value_type, (uintptr_t)SN_FALSE);
	
	// signature: VALUE func(SN_P, SnObject* function, VALUE self, SnArguments* args)
	const Type* voidPtrTy = Type::getInt8PtrTy(*vm->context);
	std::vector<const Type*> params(4, voidPtrTy);
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

static void vm_compile_recursive(SN_P p, SnVM* vm, const SnAstNode* ast, SnCompilationInfo& info) {
	/*switch (ast->type) {
		case SN_AST_LITERAL:
		{
			const SnAstLiteral* lit = &ast->literal;
			printf("LITERAL: %p\n", lit->value);
			if (!snow_is_object(lit->value)) {
				info.last_value = ConstantInt::get(vm->value_type, (uintptr_t)lit->value);
			} else {
				// TODO!!
				TRAP();
			}
			break;
		}
		case SN_AST_SEQUENCE: {
			const SnAstSequence* seq = &ast->sequence;
			printf("SEQUENCE: %p (%lu nodes)\n", seq, seq->length);
			for (size_t i = 0; i < seq->length; ++i) {
				vm_compile_recursive(p, vm, seq->nodes[i], info);
			}
			break;
		}
		case SN_AST_FUNCTION: {
			const SnAstFunction* ast_f = &ast->function;
			const SnAstSequence* args = &ast_f->seq_args->sequence;
			const SnAstSequence* body = &ast_f->seq_body->sequence;
			break;
		}
		case SN_AST_RETURN: {
			const SnAstReturn* r = &ast->ast_return;
			
			BasicBlock* old = info.block;
			info.block = BasicBlock::Create(*vm->context, "return", info.function);
			if (r->value) {
				vm_compile_recursive(p, vm, r->value, info);
			}
			ReturnInst::Create(*vm->context, info.last_value, info.block);
			info.block = old;
		}
		case SN_AST_BREAK: {
			break;
		}
		case SN_AST_CONTINUE: {
			break;
		}
		case SN_AST_SELF: {
			info.last_value = info.self;
			break;
		}
		case SN_AST_HERE: {
			info.last_value = info.this_function;
			break;
		}
		case SN_AST_IT: {
			Value* args[3];
			args[0] = info.p;
			args[1] = info.args;
			args[2] = ConstantInt::get(Type::getInt32Ty(*vm->context), 0);
			info.last_value = CallInst::Create(vm->argument_get, args, args+3, "it", info.block);
			break;
		}
		case SN_AST_LOCAL: {
			break;
		}
		case SN_AST_MEMBER:
		case SN_AST_LOCAL_ASSIGNMENT:
		case SN_AST_MEMBER_ASSIGNMENT:
		case SN_AST_IF_ELSE:
		case SN_AST_CALL:
		case SN_AST_LOOP:
		case SN_AST_TRY:
		case SN_AST_CATCH:
		case SN_AST_AND:
		case SN_AST_OR:
		case SN_AST_XOR:
		case SN_AST_NOT:
		case SN_AST_PARALLEL_THREAD:
		case SN_AST_PARALLEL_FORK:
		default: {
			ASSERT(false && "Unknown AST node type.");
			break;
		}
	}*/
}
	
CAPI SnFunctionRef snow_vm_compile_ast(SN_P p, const SnAST* ast, const char* source) {
	SnVM* vm = reinterpret_cast<SnVM*>(p->vm);
	
	Module* m = new Module("<module compiled by Snow>", *vm->context);
	
	SnCompilationInfo info;
	info.value_type = Type::getInt8PtrTy(*vm->context);
	info.last_value = vm->nil_constant;
	
	info.function = Function::Create(vm->function_type, Function::ExternalLinkage, "__snow_entry", m);
	Function::arg_iterator ai = info.function->arg_begin();
	info.p = ai++;
	info.p->setName("p");
	info.this_function = ai++;
	info.this_function->setName("this_function");
	info.self = ai++;
	info.self->setName("self");
	info.args = ai++;
	info.args->setName("args");
	
	info.block = BasicBlock::Create(*vm->context, "entry", info.function);
	
	info.ref = snow_create_function(p, info.function);
	info.source = source;
	
	/*ASSERT(ast->type == SN_AST_FUNCTION);
	const SnAstFunction* ast_function = &ast->function;
	vm_compile_recursive(p, vm, ast_function->seq_body, info);*/
	
	ReturnInst::Create(*vm->context, info.last_value, info.block);
	
	printf("COMPILED MODULE:\n");
	outs() << *m << '\n';
	
	vm->engine->addModule(m);
	
	return info.ref;
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