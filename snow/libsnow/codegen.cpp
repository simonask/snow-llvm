#include "codegen.hpp"
#include "vm.hpp"

namespace snow {
	Codegen::Codegen(llvm::LLVMContext& context) : _context(context), _module(NULL), _function(NULL) {
		_module = new llvm::Module("Compiled Snow Code", _context);
	}
	
	Codegen::Codegen(llvm::LLVMContext& context, llvm::Module* module) : _context(context), _module(module), _function(NULL) {}
	
	bool Codegen::compile_ast(const SnAST* ast) {
		ASSERT(_module != NULL);
		_function = new JITFunction;
		_function->signature.return_type = SnAnyType;
		_function->signature.num_params = 0;
		_function->signature.param_types = NULL;
		_function->signature.param_names = NULL;
		_function->function = llvm::Function::Create(snow::get_function_type(), llvm::GlobalValue::InternalLinkage, "snow_module_entry", _module);
		
		return compile_function_body(ast->_root);
	}
	
	bool Codegen::compile_function_body(const SnAstNode* seq) {
		using namespace llvm;
		
		ASSERT(seq->type == SN_AST_SEQUENCE);
		
		Function* F = _function->function;
		ASSERT(F);
		
		// Prepare to compile function
		BasicBlock* entry_bb = BasicBlock::Create(_context, "entry", F);
		BasicBlock* exit_bb = BasicBlock::Create(_context, "exit", F);
		
		FunctionCompilerInfo info;
		// function arguments are the first locals
		for (size_t i = 0; i < _function->signature.num_params; ++i) {
			info.local_names.push_back(_function->signature.param_names[i]);
		}
		info.function_exit = exit_bb;
		info.current_block = entry_bb;
		Function::arg_iterator arg_it = F->arg_begin();
		info.here = arg_it++;
		info.self = arg_it++;
		info.it = arg_it++;
		info.here->setName("here");
		info.self->setName("self");
		info.it->setName("it");
		
		// Function entry
		IRBuilder<> builder(entry_bb);
		info.last_value = value_constant(builder, SN_NIL);
		
		// Function exit
		builder.SetInsertPoint(exit_bb);
		PHINode* return_value = builder.CreatePHI(builder.getInt8PtrTy());
		
		std::map<BasicBlock*, Value*>::iterator it = info.return_points.find(entry_bb);
		if (it != info.return_points.end()) {
			// entry block has no return statement
			return_value->addIncoming(info.last_value, info.current_block);
		}
		
		for (std::map<BasicBlock*, Value*>::iterator it = info.return_points.begin(); it != info.return_points.end(); ++it) {
			return_value->addIncoming(it->second, it->first);
		}
		builder.CreateRet(return_value);
		
		return compile_ast_node(seq, info);
	}
	
	bool Codegen::compile_ast_node(const SnAstNode* node, FunctionCompilerInfo& info) {
		using namespace llvm;
		IRBuilder<> builder(info.current_block);
		
		switch (node->type) {
			case SN_AST_SEQUENCE: {
				for (const SnAstNode* x = node->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x, info)) return false;
				}
				return true;
			}
			case SN_AST_LITERAL: {
				// TODO: Handle GC objects (Strings)
				info.last_value = value_constant(builder, node->literal.value);
				break;
			}
			case SN_AST_CLOSURE: {
				break;
			}
			case SN_AST_PARAMETER: {
				break;
			}
			case SN_AST_RETURN: {
				// If the current BasicBlock already has a return statement, then 
				std::map<BasicBlock*, Value*>::iterator it = info.return_points.find(info.current_block);
				if (it != info.return_points.end()) {
					if (node->return_expr.value) {
						if (!compile_ast_node(node->return_expr.value, info)) return false;
					} else {
						info.last_value = value_constant(builder, SN_NIL);
					}
					info.return_points[info.current_block] = info.last_value;
					builder.CreateBr(info.function_exit);
				}
				return true;
			}
			case SN_AST_IDENTIFIER: {
				break;
			}
			case SN_AST_SELF: {
				info.last_value = info.self;
				break;
			}
			case SN_AST_HERE: {
				info.last_value = builder.CreateBitCast(info.here, builder.getInt8PtrTy());
				break;
			}
			case SN_AST_IT: {
				info.last_value = info.it;
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
			case SN_AST_BREAK: {
				break;
			}
			case SN_AST_CONTINUE: {
				break;
			}
			case SN_AST_IF_ELSE: {
				break;
			}
		}
		return false;
	}
	
	llvm::Value* Codegen::value_constant(llvm::IRBuilder<>& builder, const VALUE constant) {
		return builder.CreateBitCast(builder.getInt64(reinterpret_cast<const int64_t>(constant)), builder.getInt8PtrTy());
	}
}