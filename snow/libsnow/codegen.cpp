#include "codegen.hpp"
#include "vm.hpp"

#include <algorithm>

namespace {
	using namespace llvm;
	
	struct ValueSymbolPairCompare {
		typedef std::pair<Value*, SnSymbol> Pair;
		bool operator()(const Pair& a, const Pair& b) const {
			return a.second < b.second;
		}
	};
}

namespace snow {
	Codegen::Codegen(llvm::LLVMContext& context) : _context(context), _module(NULL), _function(NULL), _error_string(NULL) {
		_module = new llvm::Module("Compiled Snow Code", _context);
	}
	
	Codegen::Codegen(llvm::LLVMContext& context, llvm::Module* module) : _context(context), _module(module), _function(NULL), _error_string(NULL) {}
	
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
	
	bool Codegen::compile_function_body(const SnAstNode* seq, FunctionCompilerInfo* static_parent) {
		using namespace llvm;
		
		ASSERT(seq->type == SN_AST_SEQUENCE);
		
		Function* F = _function->function;
		ASSERT(F);
		
		// Prepare to compile function
		BasicBlock* entry_bb = BasicBlock::Create(_context, "entry", F);
		BasicBlock* exit_bb = BasicBlock::Create(_context, "exit", F);
		
		FunctionCompilerInfo info;
		info.parent = static_parent;
		// function arguments are the first locals
		for (size_t i = 0; i < _function->signature.num_params; ++i) {
			info.local_names.push_back(_function->signature.param_names[i]);
		}
		info.function_exit = exit_bb;
		Function::arg_iterator arg_it = F->arg_begin();
		info.here = arg_it++;
		info.self = arg_it++;
		info.it = arg_it++;
		info.here->setName("here");
		info.self->setName("self");
		info.it->setName("it");
		info.needs_context = false;
		
		// Function entry
		IRBuilder<> builder(entry_bb);
		info.last_value = value_constant(builder, SN_NIL);
		info.locals_array = builder.CreateLoad(builder.CreateConstGEP1_32(info.here, offsetof(SnFunctionCallContext, locals)));
		
		// Function body
		if (!compile_ast_node(seq, builder, info)) return false;
		builder.CreateBr(exit_bb);
		
		BasicBlock* last_block = builder.GetInsertBlock();
		
		// Function exit
		builder.SetInsertPoint(exit_bb);
		PHINode* return_value = builder.CreatePHI(builder.getInt8PtrTy());
		
		std::map<BasicBlock*, Value*>::iterator it = info.return_points.find(last_block);
		if (it == info.return_points.end()) {
			// last block has no return statement
			return_value->addIncoming(info.last_value, last_block);
		}
		
		for (std::map<BasicBlock*, Value*>::iterator it = info.return_points.begin(); it != info.return_points.end(); ++it) {
			return_value->addIncoming(it->second, it->first);
		}
		builder.CreateRet(return_value);
		
		return true;
	}
	
	bool Codegen::compile_ast_node(const SnAstNode* node, llvm::IRBuilder<>& builder, FunctionCompilerInfo& info) {
		using namespace llvm;
		Function* F = _function->function;
		
		switch (node->type) {
			case SN_AST_SEQUENCE: {
				for (const SnAstNode* x = node->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x, builder, info)) return false;
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
			case SN_AST_RETURN: {
				// If the current BasicBlock doesn't already have a return statement...
				std::map<BasicBlock*, Value*>::iterator it = info.return_points.find(builder.GetInsertBlock());
				if (it == info.return_points.end()) {
					if (node->return_expr.value) {
						if (!compile_ast_node(node->return_expr.value, builder, info)) return false;
					} else {
						info.last_value = value_constant(builder, SN_NIL);
					}
					info.return_points[builder.GetInsertBlock()] = info.last_value;
					builder.CreateBr(info.function_exit);
				}
				return true;
			}
			case SN_AST_IDENTIFIER: {
				// get local
				int level;
				int index;
				if (find_local(node->identifier.name, info, level, index)) {
					if (level == 0) {
						// get from current scope
						// last_value = here->locals[index]
						info.last_value = builder.CreateLoad(builder.CreateConstGEP1_32(info.locals_array, index*sizeof(VALUE)));
					} else {
						// get from other scope
						info.last_value = tail_call(builder.CreateCall3(snow::get_runtime_function("snow_get_local"), info.here, builder.getInt32(level), builder.getInt32(index)));
					}
				} else {
					// try globals
					info.last_value = tail_call(builder.CreateCall(snow::get_runtime_function("snow_get_global"), symbol_constant(builder, node->identifier.name)));
				}
				break;
			}
			case SN_AST_SELF: {
				info.last_value = info.self;
				break;
			}
			case SN_AST_HERE: {
				info.needs_context = true;
				info.last_value = builder.CreateBitCast(info.here, builder.getInt8PtrTy());
				break;
			}
			case SN_AST_IT: {
				info.last_value = info.it;
				break;
			}
			case SN_AST_ASSIGN: {
				std::vector<Value*> assign_values;
				std::vector<SnAstNode*> assign_targets;
				size_t num_values = node->assign.value->sequence.length;
				size_t num_targets = node->assign.target->sequence.length;
				assign_values.reserve(num_values);
				assign_targets.reserve(num_targets);
				
				for (SnAstNode* x = node->assign.value->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x, builder, info)) return false;
					assign_values.push_back(info.last_value);
				}
				
				for (SnAstNode* x = node->assign.target->sequence.head; x; x = x->next) {
					assign_targets.push_back(x);
				}
				
				
				// If there are more targets than values, assign nil to the remaining targets
				if (num_values < num_targets) {
					for (size_t i = num_values; i < num_targets; ++i) {
						assign_values.push_back(value_constant(builder, SN_NIL));
					}
				}
				// ... and keep the number updated
				num_values = assign_values.size();
				
				for (size_t i = 0; i < num_targets; ++i) {
					Value* assign_value = NULL;
					// if this is the last assignment target, and there are several remaining
					// values, put the rest of the values into an array and assign that instead.
					if (i == num_targets-1 && num_values > num_targets) {
						size_t num_remaining = num_values - num_targets + 1;
						assign_value = builder.CreateCall(snow::get_runtime_function("snow_create_array_with_size"), builder.getInt64(num_remaining));
						for (size_t j = i; j < num_values; ++j) {
							builder.CreateCall2(snow::get_runtime_function("snow_array_push"), assign_value, assign_values[j]);
						}
						// cast SnArray* to VALUE
						assign_value = builder.CreateBitCast(assign_value, builder.getInt8PtrTy());
					} else {
						assign_value = assign_values[i];
					}
					
					SnAstNode* target = assign_targets[i];
					switch (target->type) {
						case SN_AST_ASSOCIATION: {
							std::vector<Value*> args;
							args.reserve(target->association.args->sequence.length + 1);
							for (SnAstNode* x = target->association.args->sequence.head; x; x = x->next) {
								if (!compile_ast_node(x, builder, info)) return false;
								args.push_back(info.last_value);
							}
							args.push_back(assign_value);
							
							if (!compile_ast_node(target->association.object, builder, info)) return false;
							Value* assoc_target = info.last_value;
							std::vector<SnSymbol> arg_names;
							info.last_value = method_call(builder, info, assoc_target, 0 /* __assoc_set__ */, arg_names, args);
							break;
						}
						case SN_AST_MEMBER: {
							if (!compile_ast_node(target->member.object, builder, info)) return false;
							Value* object = info.last_value;
							info.last_value = tail_call(builder.CreateCall3(snow::get_runtime_function("snow_set_member"), object, symbol_constant(builder, target->member.name), assign_value));
							break;
						}
						case SN_AST_IDENTIFIER: {
							if (info.parent == NULL) {
								// set global
								tail_call(builder.CreateCall2(snow::get_runtime_function("snow_set_global"), symbol_constant(builder, target->identifier.name), assign_value));
							} else {
								// set local
								int level;
								int index;
								if (!find_local(target->identifier.name, info, level, index)) {
									// make a new local in current scope
									level = 0;
									index = info.local_names.size();
									info.local_names.push_back(target->identifier.name);
									info.needs_context = true;
								}
								
								if (level == 0) {
									// set in current scope
									builder.CreateStore(assign_value, builder.CreateConstGEP1_32(info.locals_array, index*sizeof(VALUE)));
								} else {
									tail_call(builder.CreateCall4(snow::get_runtime_function("snow_set_local"), info.here, builder.getInt32(level), builder.getInt32(index), assign_value));
								}
							}
							info.last_value = assign_value;
							break;
						}
						default: {
							asprintf(&_error_string, "CODEGEN: Invalid target for assignment! (type=%d)", target->type);
							return false;
						}
					}
				}
				
				break;
			}
			case SN_AST_MEMBER: {
				if (!compile_ast_node(node->member.object, builder, info)) return false;
				info.last_value = tail_call(builder.CreateCall2(snow::get_runtime_function("snow_get_member"), info.last_value, symbol_constant(builder, node->member.name)));
				break;
			}
			case SN_AST_CALL: {
				std::vector<std::pair<Value*, SnAstNode*> > args;
				size_t num_args = node->call.args->sequence.length;
				args.reserve(num_args);
				
				/*
					Calls must preserve argument order, so that named arguments come before unnamed,
					and so that named arguments are ordered by symbol value.
					Furthermore, the evaluation order must also be preserved. This is the reason for
					the following herp derp.
				*/
				size_t num_named_args = 0;
				for (SnAstNode* x = node->call.args->sequence.head; x; x = x->next) {
					SnAstNode* expr = x;
					if (x->type == SN_AST_NAMED_ARGUMENT) {
						++num_named_args;
						expr = expr->named_argument.expr;
					}
					if (!compile_ast_node(expr, builder, info)) return false;
					args.push_back(std::pair<Value*, SnAstNode*>(info.last_value, x));
				}
				
				// Split the arguments into named and unnamed
				std::vector<std::pair<Value*, SnSymbol> > named_args;
				named_args.reserve(num_named_args);
				std::vector<Value*> unnamed_args;
				unnamed_args.reserve(num_args - num_named_args);
				for (std::vector<std::pair<Value*, SnAstNode*> >::iterator it = args.begin(); it != args.end(); ++it) {
					if (it->second->type == SN_AST_NAMED_ARGUMENT) {
						named_args.push_back(std::pair<Value*, SnSymbol>(it->first, it->second->named_argument.name));
					} else {
						unnamed_args.push_back(it->first);
					}
				}
				
				// Sort the named arguments by symbol value
				std::sort(named_args.begin(), named_args.end(), ValueSymbolPairCompare());
				
				std::vector<SnSymbol> arg_names;
				arg_names.reserve(num_named_args);
				std::vector<Value*> arg_values;
				arg_values.reserve(num_args);
				
				for (std::vector<std::pair<Value*, SnSymbol> >::iterator it = named_args.begin(); it != named_args.end(); ++it) {
					arg_names.push_back(it->second);
					arg_values.push_back(it->first);
				}
				for (std::vector<Value*>::iterator it = unnamed_args.begin(); it != unnamed_args.end(); ++it) {
					arg_values.push_back(*it);
				}
				
				// Find callee, and call it!
				if (node->call.object->type == SN_AST_MEMBER) {
					SnAstNode* member = node->call.object;
					if (!compile_ast_node(member->member.object, builder, info)) return false;
					Value* object = info.last_value;
					return method_call(builder, info, object, member->member.name, arg_names, arg_values);
				} else {
					if (!compile_ast_node(node->call.object, builder, info)) return false;
					Value* object = info.last_value;
					return call(builder, info, object, NULL, arg_names, arg_values);
				}
				break;
			}
			case SN_AST_ASSOCIATION: {
				if (!compile_ast_node(node->association.object, builder, info)) return false;
				Value* object = info.last_value;
				
				std::vector<Value*> args;
				args.reserve(node->association.args->sequence.length);
				for (SnAstNode* x = node->association.args->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x, builder, info)) return false;
					args.push_back(info.last_value);
				}
				std::vector<SnSymbol> arg_names;
				info.last_value = method_call(builder, info, object, 0 /* __index_get__ */, arg_names, args);
				break;
			}
			case SN_AST_AND: {
				SnAstNode* left = node->logic_and.left;
				SnAstNode* right = node->logic_and.right;
				
				BasicBlock* left_bb = builder.GetInsertBlock();
				BasicBlock* right_bb = BasicBlock::Create(_context, "and_right", F);
				BasicBlock* merge_bb = BasicBlock::Create(_context, "and_merge", F);
				
				if (!compile_ast_node(left, builder, info)) return false;
				left_bb = builder.GetInsertBlock();
				Value* left_value = info.last_value;
				Value* left_truth = builder.CreateCall(snow::get_runtime_function("snow_eval_truth"), left_value);
				builder.CreateCondBr(left_truth, right_bb, merge_bb);
				
				builder.SetInsertPoint(right_bb);
				if (!compile_ast_node(right, builder, info)) return false;
				right_bb = builder.GetInsertBlock();
				Value* right_value = info.last_value;
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				PHINode* phi = builder.CreatePHI(builder.getInt8PtrTy());
				phi->addIncoming(left_value, left_bb);
				phi->addIncoming(right_value, right_bb);
				info.last_value = phi;
				
				break;
			}
			case SN_AST_OR: {
				SnAstNode* left = node->logic_or.left;
				SnAstNode* right = node->logic_or.right;
				
				BasicBlock* left_bb = builder.GetInsertBlock();
				BasicBlock* right_bb = BasicBlock::Create(_context, "or_right", F);
				BasicBlock* merge_bb = BasicBlock::Create(_context, "or_merge", F);
				
				if (!compile_ast_node(left, builder, info)) return false;
				left_bb = builder.GetInsertBlock();
				Value* left_value = info.last_value;
				Value* left_truth = builder.CreateCall(snow::get_runtime_function("snow_eval_truth"), left_value);
				builder.CreateCondBr(left_truth, merge_bb, right_bb);
				
				builder.SetInsertPoint(right_bb);
				if (!compile_ast_node(right, builder, info)) return false;
				right_bb = builder.GetInsertBlock();
				Value* right_value = info.last_value;
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				PHINode* phi = builder.CreatePHI(builder.getInt8PtrTy());
				phi->addIncoming(left_value, left_bb);
				phi->addIncoming(right_value, right_bb);
				info.last_value = phi;
				
				break;
			}
			case SN_AST_XOR: {
				SnAstNode* left = node->logic_xor.left;
				SnAstNode* right = node->logic_xor.right;
				
				BasicBlock* merge_bb = BasicBlock::Create(_context, "xor_merge", F);
				BasicBlock* test_truth_bb = BasicBlock::Create(_context, "xor_test_truth", F);
				BasicBlock* left_false_right_true_bb = BasicBlock::Create(_context, "xor_left_false_true_false", F);
				BasicBlock* both_true_or_false_bb = BasicBlock::Create(_context, "xor_both_true_or_false", F);
				
				if (!compile_ast_node(left, builder, info)) return false;
				Value* left_value = info.last_value;
				Value* left_truth = builder.CreateCall(snow::get_runtime_function("snow_eval_truth"), left_value);
				
				if (!compile_ast_node(right, builder, info)) return false;
				Value* right_value = info.last_value;
				Value* right_truth = builder.CreateCall(snow::get_runtime_function("snow_eval_truth"), right_value);
				
				builder.CreateCondBr(builder.CreateXor(left_truth, right_truth, "xor"), test_truth_bb, both_true_or_false_bb);
				
				builder.SetInsertPoint(test_truth_bb);
				builder.CreateCondBr(left_truth, merge_bb, left_false_right_true_bb);
				
				builder.SetInsertPoint(left_false_right_true_bb);
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(both_true_or_false_bb);
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				PHINode* phi = builder.CreatePHI(builder.getInt8PtrTy(), "xor_phi");
				phi->addIncoming(value_constant(builder, SN_FALSE), both_true_or_false_bb);
				phi->addIncoming(left_value, test_truth_bb);
				phi->addIncoming(right_value, left_false_right_true_bb);
				
				info.last_value = phi;
				
				break;
			}
			case SN_AST_NOT: {
				if (!compile_ast_node(node->logic_not.expr, builder, info)) return false;
				Value* truth = builder.CreateCall(snow::get_runtime_function("snow_eval_truth"), info.last_value);
				
				BasicBlock* false_bb = builder.GetInsertBlock();
				BasicBlock* true_bb = BasicBlock::Create(_context, "not_true", F);
				BasicBlock* merge_bb = BasicBlock::Create(_context, "not_merge", F);
				
				builder.CreateCondBr(truth, true_bb, merge_bb);
				
				builder.SetInsertPoint(true_bb);
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				PHINode* phi = builder.CreatePHI(builder.getInt8PtrTy());
				phi->addIncoming(value_constant(builder, SN_FALSE), true_bb);
				phi->addIncoming(value_constant(builder, SN_TRUE), false_bb);
				
				info.last_value = phi;
				break;
			}
			case SN_AST_LOOP: {
				BasicBlock* cond_bb = BasicBlock::Create(_context, "loop_cond", F);
				BasicBlock* body_bb = BasicBlock::Create(_context, "loop_body", F);
				BasicBlock* merge_bb = BasicBlock::Create(_context, "loop_merge", F);
				
				BasicBlock* incoming_bb = builder.GetInsertBlock();
				builder.CreateBr(cond_bb);
				
				builder.SetInsertPoint(body_bb);
				if (!compile_ast_node(node->loop.body, builder, info)) return false;
				builder.CreateBr(cond_bb);
				BasicBlock* body_end_bb = builder.GetInsertBlock();
				Value* body_value = info.last_value;
				
				builder.SetInsertPoint(cond_bb);
				PHINode* loop_value = builder.CreatePHI(builder.getInt8PtrTy());
				loop_value->addIncoming(value_constant(builder, SN_FALSE), incoming_bb);
				loop_value->addIncoming(body_value, body_end_bb);
				if (!compile_ast_node(node->loop.cond, builder, info)) return false;
				BasicBlock* cond_end_bb = builder.GetInsertBlock();
				Value* cond_value = info.last_value;
				Value* cond_truth = builder.CreateCall(snow::get_runtime_function("snow_eval_truth"), cond_value);
				builder.CreateCondBr(cond_truth, body_bb, merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				info.last_value = loop_value;
				
				break;
			}
			case SN_AST_BREAK: {
				// TODO!
				break;
			}
			case SN_AST_CONTINUE: {
				// TODO!
				break;
			}
			case SN_AST_IF_ELSE: {
				BasicBlock* body_bb = BasicBlock::Create(_context, "if_body", F);
				BasicBlock* else_bb = node->if_else.else_body ? BasicBlock::Create(_context, "else_body", F) : NULL;
				BasicBlock* merge_bb = BasicBlock::Create(_context, "if_merge", F);
				
				if (!compile_ast_node(node->if_else.cond, builder, info)) return false;
				BasicBlock* cond_bb = builder.GetInsertBlock();
				Value* cond_value = info.last_value;
				Value* cond_truth = builder.CreateCall(snow::get_runtime_function("snow_eval_truth"), cond_value);
				builder.CreateCondBr(cond_truth, body_bb, else_bb ? else_bb : merge_bb);
				
				builder.SetInsertPoint(body_bb);
				if (!compile_ast_node(node->if_else.body, builder, info)) return false;
				Value* body_value = info.last_value;
				BasicBlock* body_end_bb = builder.GetInsertBlock();
				builder.CreateBr(merge_bb);
				
				Value* else_value = NULL;
				BasicBlock* else_end_bb = NULL;
				if (else_bb) {
					builder.SetInsertPoint(else_bb);
					if (!compile_ast_node(node->if_else.else_body, builder, info)) return false;
					else_value = info.last_value;
					else_end_bb = builder.GetInsertBlock();
					builder.CreateBr(merge_bb);
				}
				
				builder.SetInsertPoint(merge_bb);
				PHINode* phi = builder.CreatePHI(builder.getInt8PtrTy());
				phi->addIncoming(body_value, body_end_bb);
				if (else_bb) {
					phi->addIncoming(else_value, else_end_bb);
				} else {
					phi->addIncoming(cond_value, cond_bb);
				}
				
				info.last_value = phi;
				break;
			}
			default: {
				asprintf(&_error_string, "Inappropriate AST node in tree (type %d).", node->type);
				return false;
			}
		}
		return true;
	}
	
	bool Codegen::find_local(SnSymbol name, FunctionCompilerInfo& info, int& out_level, int& out_index) {
		FunctionCompilerInfo* ci = &info;
		int level = 0;
		while (ci) {
			for (int i = 0; i < (int)ci->local_names.size(); ++i) {
				if (ci->local_names[i] == name) {
					out_level = level;
					out_index = i;
					return true;
				}
			}
			
			++level;
			ci = ci->parent;
		}
		return false;
	}
	
	llvm::Value* Codegen::value_constant(llvm::IRBuilder<>& builder, const VALUE constant) {
		return builder.CreateCast(llvm::Instruction::IntToPtr, builder.getInt64((uint64_t)constant), builder.getInt8PtrTy());
	}
	
	llvm::Value* Codegen::symbol_constant(llvm::IRBuilder<>& builder, const SnSymbol constant) {
		// SnSymbol is uint64_t
		return builder.getInt64(constant);
	}
	
	llvm::CallInst* Codegen::method_call(llvm::IRBuilder<>& builder, FunctionCompilerInfo& info, llvm::Value* object, SnSymbol method, const std::vector<SnSymbol>& arg_names, const std::vector<llvm::Value*>& args) {
		llvm::Value* function = builder.CreateCall2(snow::get_runtime_function("snow_get_method"), object, symbol_constant(builder, method), "method");
		return call(builder, info, function, object, arg_names, args);
	}
	
	llvm::CallInst* Codegen::call(llvm::IRBuilder<>& builder, FunctionCompilerInfo& info, llvm::Value* function, llvm::Value* self, const std::vector<SnSymbol>& arg_names, const std::vector<llvm::Value*>& args) {
		using namespace llvm;
		Value* faux_self = NULL;
		if (function->getType() == builder.getInt8PtrTy()) {
			// function is a value, not a SnFunction*, so we need to find out which function to call
			faux_self = function;
			function = builder.CreateCall(snow::get_runtime_function("snow_value_to_function"), function, "function");
		}
		
		Value* null_array = builder.CreateCast(llvm::Instruction::IntToPtr, builder.getInt64(0), PointerType::getUnqual(builder.getInt8PtrTy()));
		
		size_t num_arg_names = arg_names.size();
		size_t num_args = args.size();
		
		Value* arg_names_array;
		if (num_arg_names == 0) arg_names_array = builder.CreateCast(llvm::Instruction::IntToPtr, builder.getInt64(0), PointerType::getUnqual(builder.getInt64Ty()));
		else {
			arg_names_array = builder.CreateAlloca(builder.getInt64Ty(), builder.getInt64(num_arg_names), "arg_names_array");
			for (size_t i = 0; i < num_arg_names; ++i) {
				builder.CreateStore(symbol_constant(builder, arg_names[i]), builder.CreateConstGEP1_32(arg_names_array, i));
			}
		}
		
		Value* args_array;
		if (num_args == 0) args_array = builder.CreateCast(llvm::Instruction::IntToPtr, builder.getInt64(0), PointerType::getUnqual(builder.getInt8PtrTy()));
		else {
			args_array = builder.CreateAlloca(builder.getInt8PtrTy(), builder.getInt64(num_args), "args_array");
			for (size_t i = 0; i < num_args; ++i) {
				builder.CreateStore(args[i], builder.CreateConstGEP1_32(args_array, i));
			}
		}
		
		Value* v[6];
		v[0] = function;
		v[1] = info.here;
		v[2] = builder.getInt64(num_arg_names);
		v[3] = arg_names_array;
		v[4] = builder.getInt64(num_args);
		v[5] = args_array;
		
		Value* call_context = builder.CreateCall(snow::get_runtime_function("snow_create_function_call_context"), v + 0, v + 6, "call_context");
		if (!self) self = value_constant(builder, NULL);
		Value* it = args.size() ? args[0] : value_constant(builder, NULL);
		return tail_call(builder.CreateCall4(snow::get_runtime_function("snow_function_call"), function, call_context, self, it));
	}
}
