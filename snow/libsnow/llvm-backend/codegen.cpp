#include "codegen.hpp"
#include "symbol.hpp"
#include "snow/type.h"
#include "snow/numeric.h"
#include "snow/boolean.h"

#include <algorithm>

namespace {
	template <typename A, typename B, int N> struct PairComparison;
	template <typename A, typename B> struct PairComparison<A, B, 1> {
		typedef typename std::pair<A, B> Pair;
		bool operator()(const Pair& a, const Pair& b) const { return a.first < b.first; }
	};
	template <typename A, typename B> struct PairComparison<A, B, 2> {
		typedef typename std::pair<A, B> Pair;
		bool operator()(const Pair& a, const Pair& b) const { return a.second < b.second; }
	};
	
	typedef PairComparison<llvm::Value*, SnSymbol, 2> ValueSymbolPairCompare;
	
	template <typename C, typename T>
	int index_of(const C& container, const T& element) {
		typename C::const_iterator it = container.begin();
		int i = 0;
		while (it != container.end()) {
			if (element == *it) return i;
			++i;
			++it;
		}
		return -1;
	}
	
	template <typename C, typename T>
	typename C::const_iterator find(const C& container, const T& element) {
		for (typename C::const_iterator it = container.begin(); it != container.end(); ++it) {
			if (*it == element) return it;
		}
		return container.end();
	}
	
	template <typename C, typename T>
	typename C::iterator find(C& container, const T& element) {
		for (typename C::iterator it = container.begin(); it != container.end(); ++it) {
			if (*it == element) return it;
		}
		return container.end();
	}
}

namespace snow {
	Codegen::Codegen(llvm::Module* runtime_module, const llvm::StringRef& module_name) :
		_runtime_module(runtime_module),
		_module(NULL),
		_module_name(module_name),
		_entry_descriptor(NULL),
		_error_string(NULL)
	{
		_module = new llvm::Module(module_name, runtime_module->getContext());
	}
	
	Codegen::~Codegen() {
		for (size_t i = 0; i < _functions.size(); ++i) {
			delete _functions[i];
		}
	}
	
	bool Codegen::compile_ast(const SnAST* ast) {
		ASSERT(_module != NULL);
		ASSERT(_entry_descriptor == NULL);
		
		Function* function = new Function;
		_functions.push_back(function);
		function->parent = NULL;
		function->function = llvm::Function::Create(get_function_type(), llvm::GlobalValue::InternalLinkage, "snow_module_entry", _module);
		function->name = snow::symbol(_module_name.str().c_str());
		function->needs_environment = true;
		function->locals_array = NULL;
		function->variable_references_array = NULL;
		
		Value last_val = compile_closure_body(ast->_root, *function);
		if (!last_val) return false;

		_entry_descriptor = get_function_descriptor_for(function);
		return true;
	}
	
	
	Function* Codegen::compile_closure(const SnAstNode* closure, Function& declared_in) {
		ASSERT(closure->type == SN_AST_CLOSURE);
		
		static int function_count = 0;
		
		Function* function = new Function;
		_functions.push_back(function);
		function->parent = &declared_in;
		function->function = llvm::Function::Create(get_function_type(), llvm::GlobalValue::InternalLinkage, llvm::StringRef("function_") + current_assignment_name(), _module);
		function->name = current_assignment_symbol();
		function->entry_point = NULL;
		function->exit_point = NULL;
		function->unwind_point = NULL;
		function->self_type = SnAnyType; // TODO: Consider how type of self should be inferred.
		function->return_type = SnAnyType;
		function->locals_array = NULL;
		function->variable_references_array = NULL;
		function->needs_environment = true;
		
		std::vector<std::pair<SnSymbol, SnAstNode*> > params;
		if (closure->closure.parameters) {
			params.reserve(closure->closure.parameters->sequence.length);
			for (SnAstNode* x = closure->closure.parameters->sequence.head; x; x = x->next) {
				params.push_back(std::pair<SnSymbol, SnAstNode*>(x->parameter.name, x->parameter.id_type));
			}
		}
		
		if (params.size() > 0) {
			// Parameters need to be sorted by symbol value
			std::pair<SnSymbol, SnAstNode*> first_name = params[0];
			std::sort(params.begin(), params.end(), PairComparison<SnSymbol, SnAstNode*, 1>());
			function->it_index = index_of(params, first_name);
		} else {
			function->it_index = 0;
		}
		
		function->local_names.reserve(params.size());
		function->param_names.reserve(params.size());
		function->param_types.reserve(params.size());
		for (size_t i = 0; i < params.size(); ++i) {
			// function arguments are the first locals
			function->local_names.push_back(params[i].first);
			function->param_names.push_back(params[i].first);
			function->param_types.push_back(SnAnyType); // TODO: 'as'-expressions in parser
		}
		
		Value last_val = compile_closure_body(closure->closure.body, *function);
		if (last_val) {
			return function;
		} else {
			std::vector<Function*>::iterator it = find(_functions, function);
			if (it != _functions.end()) _functions.erase(it);
			delete function;
			return NULL;
		}
	}
	
	Value Codegen::compile_closure_body(const SnAstNode* seq, Function& current_function) {
		ASSERT(seq->type == SN_AST_SEQUENCE);
		
		llvm::Function* F = current_function.function;
		llvm::LLVMContext& context = _module->getContext();
		
		// Prepare to compile function
		llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(context, "entry", F);
		llvm::BasicBlock* exit_bb = llvm::BasicBlock::Create(context, "exit", F);
		llvm::BasicBlock* unwind_bb = llvm::BasicBlock::Create(context, "unwind", F);
		current_function.entry_point = entry_bb;
		current_function.exit_point = exit_bb;
		current_function.unwind_point = unwind_bb;
		
		// Function entry
		Builder builder(entry_bb);
		
		llvm::Function::arg_iterator arg_it = F->arg_begin();
		current_function.function_value = &*arg_it++;
		current_function.here = &*arg_it++;
		current_function.self = &*arg_it++;
		current_function.it = &*arg_it++;
		current_function.function_value->setName("function");
		current_function.here.value->setName("here");
		current_function.self.value->setName("self");
		current_function.it.value->setName("it");
		builder.CreateCall(get_runtime_function("_snow_push_call_frame"), current_function.here);
		if (current_function.parent) {
			// locals_array = here->locals; // locals is the 5th member of SnCallFrame
			current_function.locals_array = builder.CreateLoad(builder.CreateStructGEP(current_function.here, 4), ":locals");
			// variable_references_array = here->function->variable_references;
			llvm::Value* function = builder.CreateLoad(builder.CreateStructGEP(current_function.here, 1), ":function");
			current_function.variable_references_array = builder.CreateLoad(builder.CreateStructGEP(function, 3), ":function:variable_references");
		}
		// module = here->module; // module is the 7th member of SnCallFrame
		current_function.module = builder.CreateLoad(builder.CreateStructGEP(current_function.here, 6), ":module");
		
		// Function body
		Value last_value = compile_ast_node(builder, seq, current_function);
		if (!last_value) return last_value;
		builder.CreateBr(exit_bb);
		llvm::BasicBlock* last_block = builder.GetInsertBlock();
		
		// Function exit
		builder.SetInsertPoint(exit_bb);
		llvm::PHINode* return_value = builder.CreatePHI(get_value_type());
		
		Function::ReturnPoints::iterator it = current_function.return_points.find(last_block);
		if (it == current_function.return_points.end()) {
			// last block has no return statement
			return_value->addIncoming(last_value, last_block);
			current_function.possible_return_types.push_back(last_value.type);
		}
		for (std::map<llvm::BasicBlock*, Value>::iterator it = current_function.return_points.begin();
		     it != current_function.return_points.end();
		     ++it)
		{
			return_value->addIncoming(it->second, it->first);
		}
		builder.CreateCall(get_runtime_function("_snow_pop_call_frame"), current_function.here);
		builder.CreateRet(return_value);
		
		current_function.return_type = current_function.possible_return_types[0];
		for (size_t i = 1; i < current_function.possible_return_types.size(); ++i) {
			current_function.return_type = current_function.return_type.combine(current_function.possible_return_types[i]);
		}
		
		// Function unwind
		// This block is executed when an exception has been thrown, and the
		// runtime is looking for a handler.
		builder.SetInsertPoint(unwind_bb);
		builder.CreateCall(get_runtime_function("_snow_pop_call_frame"), current_function.here);
		builder.CreateUnwind(); // "rethrow"
		
		return Value(return_value, current_function.return_type);
	}
	
	Value Codegen::compile_ast_node(Builder& builder, const SnAstNode* node, Function& current_function) {
		llvm::Function* F = current_function.function;
		llvm::LLVMContext& context = _module->getContext();
		
		switch (node->type) {
			case SN_AST_SEQUENCE: {
				Value last = nil(builder);
				for (const SnAstNode* x = node->sequence.head; x; x = x->next) {
					last = compile_ast_node(builder, x, current_function);
					if (!last) break;
				}
				return last; // value of sequences is the value of the last expression
			}
			case SN_AST_LITERAL: {
				VALUE val = node->literal.value;
				switch (snow_type_of(val)) {
					case SnIntegerType: return constant_integer(builder, snow_value_to_integer(val));
					case SnNilType:     return nil(builder);
					case SnFalseType:
					case SnTrueType:    return constant_bool(builder, snow_value_to_boolean(val));
					case SnSymbolType:  return constant_symbol(builder, snow_value_to_symbol(val));
					case SnFloatType:   return constant_float(builder, snow_value_to_float(val));
					case SnStringType:  return constant_string(builder, (SnString*)val);
					default: {
						llvm::Value* c = builder.getInt64((uint64_t)val);
						return Value(builder.CreateIntToPtr(c, get_value_type()));
					}
				}
			}
			case SN_AST_CLOSURE: {
				Function* closure = compile_closure(node, current_function);
				if (!closure) return Value();
				llvm::Value* descriptor = get_function_descriptor_for(closure);
				llvm::Value* instantiated_function = builder.CreateCall2(get_runtime_function("snow_create_function"), descriptor, current_function.here, "closure");
				llvm::Value* casted = builder.CreatePointerCast(instantiated_function, get_value_type(), instantiated_function->getName()+":value");
				return Value(casted, SnFunctionType);
			}
			case SN_AST_RETURN: {
				// If the current BasicBlock doesn't already have a return statement...
				Function::ReturnPoints::iterator it = current_function.return_points.find(builder.GetInsertBlock());
				if (it == current_function.return_points.end()) {
					Value result;
					if (node->return_expr.value) {
						result = compile_ast_node(builder, node->return_expr.value, current_function);
						if (!result) return result;
					} else {
						result = nil(builder);
					}
					current_function.return_points[builder.GetInsertBlock()] = result;
					builder.CreateBr(current_function.exit_point);
					current_function.possible_return_types.push_back(result.type);
					return result;
				}
				return nil(builder);
			}
			case SN_AST_IDENTIFIER:  return compile_get_local(builder, current_function, node->identifier.name);
			case SN_AST_SELF: return current_function.self;
			case SN_AST_HERE: return cast_to_value(builder, current_function.here);
			case SN_AST_IT:   return current_function.it;
			case SN_AST_ASSIGN: {
				ASSERT(node->assign.value->type == SN_AST_SEQUENCE);
				ASSERT(node->assign.target->type == SN_AST_SEQUENCE);
				std::vector<Value> values;
				std::vector<SnAstNode*> targets;
				values.reserve(node->assign.value->sequence.length);
				targets.reserve(node->assign.target->sequence.length);
				
				for (SnAstNode* x = node->assign.target->sequence.head; x; x = x->next) {
					targets.push_back(x);
				}
				
				static SnSymbol anon_sym = snow::symbol("<anonymous>");
				int i = 0;
				for (SnAstNode* x = node->assign.value->sequence.head; x; x = x->next, ++i) {
					if (i < targets.size()) {
						// See if we can deduce a name for the compiled value from the assignment target.
						SnAstNode* target = targets[i];
						switch (target->type) {
							case SN_AST_IDENTIFIER: _assignment_name_stack.push(target->identifier.name); break;
							case SN_AST_MEMBER:     _assignment_name_stack.push(target->member.name); break;
							default:                _assignment_name_stack.push(anon_sym); break;
						}
						Value value = compile_ast_node(builder, x, current_function);
						values.push_back(value);
						_assignment_name_stack.pop();
						if (!value) return value;
					} else {
						// No name, just go with anonymous.
						_assignment_name_stack.push(anon_sym);
						Value value = compile_ast_node(builder, x, current_function);
						values.push_back(value);
						_assignment_name_stack.pop();
						if (!value) return value;
					}
				}
				
				while (targets.size() > values.size()) {
					// More targets than values. Assign nil to the remaining targets.
					values.push_back(nil(builder));
				}
				
				Value value;
				for (size_t i = 0; i < targets.size(); ++i) {
					// If this is the last target, and there are several remaining
					// values, put the rest of them in an array, and assign that.
					if (i == targets.size()-1 && values.size() > targets.size()) {
						size_t num_remaining = values.size() - targets.size() + 1;
						value = builder.CreateCall(get_runtime_function("snow_create_array_with_size"), builder.getInt64(num_remaining));
						for (size_t j = i; j < values.size(); ++j) {
							builder.CreateCall2(get_runtime_function("snow_array_push"), value, values[j]);
						}
						value.value = builder.CreatePointerCast(value.value, get_value_type());
						value.type = SnArrayType;
					} else {
						value = values[i];
					}
					
					SnAstNode* target = targets[i];
					switch (target->type) {
						case SN_AST_ASSOCIATION: compile_association_assignment(builder, current_function, target, value); break;
						case SN_AST_MEMBER:      compile_member_assignment(builder, current_function, target, value); break;
						case SN_AST_IDENTIFIER:  compile_local_assignment(builder, current_function, target, value); break;
						default:
							asprintf(&_error_string, "CODEGEN: Invalid target for assignment! (type=%d)", target->type);
							return Value();
					}
				}
				
				return value;
			}
			case SN_AST_MEMBER: {
				Value object = compile_ast_node(builder, node->member.object, current_function);
				if (!object) return object;
				llvm::Value* args[2] = {object, symbol(builder, node->member.name)};
				Value member = invoke_call(builder, current_function, get_runtime_function("snow_get_member"), args, args+2, (object.value->getName() + "." + snow::symbol_to_cstr(node->member.name)).str());
				return member;
			}
			case SN_AST_CALL: {
				std::vector<std::pair<Value, SnAstNode*> > values_and_nodes;
				size_t num_named_arguments = 0;
				size_t num_arguments = 0;
				
				if (node->call.args) {
					// Compile all argument values in order.
					num_arguments = node->call.args->sequence.length;
					values_and_nodes.reserve(num_arguments);
					for (SnAstNode* x = node->call.args->sequence.head; x; x = x->next) {
						SnAstNode* expr = x;
						if (expr->type == SN_AST_NAMED_ARGUMENT) {
							expr = expr->named_argument.expr;
							++num_named_arguments;
						}
						Value value = compile_ast_node(builder, expr, current_function);
						if (!value) return value;
						values_and_nodes.push_back(std::pair<Value, SnAstNode*>(value, x));
					}
				}
				
				
				// Split the arguments into named and unnamed
				std::vector<std::pair<Value, SnSymbol> > named_args;
				std::vector<Value> unnamed_args;
				std::vector<Value> splat_args;
				named_args.reserve(num_named_arguments);
				unnamed_args.reserve(num_arguments - num_named_arguments);
				splat_args.reserve(num_arguments - num_named_arguments);
				
				CallArguments args;
				if (values_and_nodes.size()) args.it = values_and_nodes[0].first;
				
				for (std::vector<std::pair<Value, SnAstNode*> >::iterator it = values_and_nodes.begin(); it != values_and_nodes.end(); ++it) {
					if (it->second->type == SN_AST_NAMED_ARGUMENT) {
						named_args.push_back(std::pair<Value, SnSymbol>(it->first, it->second->named_argument.name));
					} else {
						SnAstNode* n = it->second;
						// Recognize splat arguments, which look like this:
						// call(member(expr, '*'), 0 args)
						if (n->type == SN_AST_CALL
						    && n->call.object->type == SN_AST_MEMBER
						    && (!n->call.args || (n->call.args->sequence.length == 0))
						    && n->call.object->member.name == snow::symbol("*")) {
							args.splat_values.push_back(it->first);
						} else {
							unnamed_args.push_back(it->first);
						}
					}
				}
				
				// Sort the named arguments by symbol value
				std::sort(named_args.begin(), named_args.end(), ValueSymbolPairCompare());
				
				args.values.reserve(values_and_nodes.size());
				args.names.reserve(named_args.size());
				
				for (size_t i = 0; i < named_args.size(); ++i) {
					args.values.push_back(named_args[i].first);
					args.names.push_back(named_args[i].second);
				}
				
				args.values.insert(args.values.end(), unnamed_args.begin(), unnamed_args.end());
				
				// Find callee, and call it!
				if (node->call.object->type == SN_AST_MEMBER) {
					SnAstNode* member_node = node->call.object;
					SnSymbol method = member_node->member.name;
					Value object = compile_ast_node(builder, member_node->member.object, current_function);
					if (!object) return object;
					return compile_method_call(builder, current_function, object, method, args);
				} else {
					Value callee = compile_ast_node(builder, node->call.object, current_function);
					if (!callee) return callee;
					return compile_call(builder, current_function, callee, NULL, args);
				}
				break;
			}
			case SN_AST_ASSOCIATION: {
				Value object = compile_ast_node(builder, node->association.object, current_function);
				if (!object) return object;
				
				CallArguments args;
				args.values.reserve(node->association.args->sequence.length);
				for (SnAstNode* x = node->association.args->sequence.head; x; x = x->next) {
					Value arg = compile_ast_node(builder, x, current_function);
					if (!arg) return arg;
					args.values.push_back(arg);
				}
				if (args.values.size()) args.it = args.values[0];
				return compile_method_call(builder, current_function, object, snow::symbol("get"), args);
			}
			case SN_AST_AND: {
				SnAstNode* left = node->logic_and.left;
				SnAstNode* right = node->logic_and.right;
				
				llvm::BasicBlock* left_bb = builder.GetInsertBlock();
				llvm::BasicBlock* right_bb = llvm::BasicBlock::Create(context, "and_right", F);
				llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "and_merge", F);
				
				Value left_value = compile_ast_node(builder, left, current_function);
				if (!left_value) return left_value;
				left_bb = builder.GetInsertBlock();
				llvm::Value* left_truth = builder.CreateCall(get_runtime_function("snow_eval_truth"), left_value);
				builder.CreateCondBr(left_truth, right_bb, merge_bb);
				
				builder.SetInsertPoint(right_bb);
				Value right_value = compile_ast_node(builder, right, current_function);
				if (!right_value) return right_value;
				right_bb = builder.GetInsertBlock();
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				llvm::PHINode* phi = builder.CreatePHI(get_value_type());
				phi->addIncoming(left_value, left_bb);
				phi->addIncoming(right_value, right_bb);
				
				return Value(phi, left_value.type.combine(right_value.type));
			}
			case SN_AST_OR: {
				SnAstNode* left = node->logic_or.left;
				SnAstNode* right = node->logic_or.right;
				
				llvm::BasicBlock* left_bb = builder.GetInsertBlock();
				llvm::BasicBlock* right_bb = llvm::BasicBlock::Create(context, "or_right", F);
				llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "or_merge", F);
				
				Value left_value = compile_ast_node(builder, left, current_function);
				if (!left_value) return left_value;
				left_bb = builder.GetInsertBlock();
				llvm::Value* left_truth = builder.CreateCall(get_runtime_function("snow_eval_truth"), left_value);
				builder.CreateCondBr(left_truth, merge_bb, right_bb);
				
				builder.SetInsertPoint(right_bb);
				Value right_value = compile_ast_node(builder, right, current_function);
				if (!right_value) return right_value;
				right_bb = builder.GetInsertBlock();
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				llvm::PHINode* phi = builder.CreatePHI(get_value_type());
				phi->addIncoming(left_value, left_bb);
				phi->addIncoming(right_value, right_bb);
				
				Type type = left_value.type.combine(right_value.type);
				if (left_value.type.option && !right_value.type.option) {
					// if left is maybe falsy, and right is definitely not falsy,
					// then we know that the result is definitely not falsy.
					type.option = false;
				}
				
				return Value(phi, type);
			}
			case SN_AST_XOR: {
				SnAstNode* left = node->logic_xor.left;
				SnAstNode* right = node->logic_xor.right;
				
				llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "xor_merge", F);
				llvm::BasicBlock* test_truth_bb = llvm::BasicBlock::Create(context, "xor_test_truth", F);
				llvm::BasicBlock* left_false_right_true_bb = llvm::BasicBlock::Create(context, "xor_left_false_true_false", F);
				llvm::BasicBlock* both_true_or_false_bb = llvm::BasicBlock::Create(context, "xor_both_true_or_false", F);
				
				Value left_value = compile_ast_node(builder, left, current_function);
				if (!left_value) return left_value;
				llvm::Value* left_truth = builder.CreateCall(get_runtime_function("snow_eval_truth"), left_value);
				
				Value right_value = compile_ast_node(builder, right, current_function);
				if (!right_value) return right_value;
				llvm::Value* right_truth = builder.CreateCall(get_runtime_function("snow_eval_truth"), right_value);
				
				builder.CreateCondBr(builder.CreateXor(left_truth, right_truth, "xor"), test_truth_bb, both_true_or_false_bb);
				
				builder.SetInsertPoint(test_truth_bb);
				builder.CreateCondBr(left_truth, merge_bb, left_false_right_true_bb);
				
				builder.SetInsertPoint(left_false_right_true_bb);
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(both_true_or_false_bb);
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				llvm::PHINode* phi = builder.CreatePHI(get_value_type(), "xor_phi");
				phi->addIncoming(constant_bool(builder, false), both_true_or_false_bb);
				phi->addIncoming(left_value, test_truth_bb);
				phi->addIncoming(right_value, left_false_right_true_bb);
				return Value(phi, Type::Option(left_value.type.combine(right_value.type)));
			}
			case SN_AST_NOT: {
				Value value = compile_ast_node(builder, node->logic_not.expr, current_function);
				if (!value) return value;
				llvm::Value* truth = builder.CreateCall(get_runtime_function("snow_eval_truth"), value);
				
				llvm::BasicBlock* false_bb = builder.GetInsertBlock();
				llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(context, "not_true", F);
				llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "not_merge", F);
				
				builder.CreateCondBr(truth, true_bb, merge_bb);
				
				builder.SetInsertPoint(true_bb);
				builder.CreateBr(merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				llvm::PHINode* phi = builder.CreatePHI(get_value_type());
				phi->addIncoming(constant_bool(builder, false), true_bb);
				phi->addIncoming(constant_bool(builder, true), false_bb);
				return Value(phi, SnAnyType); // TODO: Consider if we can do something about type inference here.
			}
			case SN_AST_LOOP: {
				llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(context, "loop_cond", F);
				llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(context, "loop_body", F);
				llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "loop_merge", F);
				
				llvm::BasicBlock* incoming_bb = builder.GetInsertBlock();
				builder.CreateBr(cond_bb);
				
				builder.SetInsertPoint(body_bb);
				Value body_value = compile_ast_node(builder, node->loop.body, current_function);
				if (!body_value) return body_value;
				builder.CreateBr(cond_bb);
				llvm::BasicBlock* body_end_bb = builder.GetInsertBlock();
				
				builder.SetInsertPoint(cond_bb);
				llvm::PHINode* loop_value = builder.CreatePHI(get_value_type());
				loop_value->addIncoming(constant_bool(builder, false), incoming_bb);
				loop_value->addIncoming(body_value, body_end_bb);
				Value cond_value = compile_ast_node(builder, node->loop.cond, current_function);
				if (!cond_value) return cond_value;
				llvm::BasicBlock* cond_end_bb = builder.GetInsertBlock();
				llvm::Value* cond_truth = builder.CreateCall(get_runtime_function("snow_eval_truth"), cond_value);
				builder.CreateCondBr(cond_truth, body_bb, merge_bb);
				
				builder.SetInsertPoint(merge_bb);
				return Value(loop_value, SnAnyType); // TODO: Consider if we can do something about type inference here.
			}
			case SN_AST_BREAK: {
				// TODO: Implement break.
				break;
			}
			case SN_AST_CONTINUE: {
				// TODO: Implement continue.
				break;
			}
			case SN_AST_IF_ELSE: {
				llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(context, "if_body", F);
				llvm::BasicBlock* else_bb = node->if_else.else_body ? llvm::BasicBlock::Create(context, "else_body", F) : NULL;
				llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "if_merge", F);
				
				Value cond_value = compile_ast_node(builder, node->if_else.cond, current_function);
				if (!cond_value) return cond_value;
				llvm::BasicBlock* cond_bb = builder.GetInsertBlock();
				llvm::Value* cond_truth = builder.CreateCall(get_runtime_function("snow_eval_truth"), cond_value);
				builder.CreateCondBr(cond_truth, body_bb, else_bb ? else_bb : merge_bb);
				
				builder.SetInsertPoint(body_bb);
				Value body_value = compile_ast_node(builder, node->if_else.body, current_function);
				if (!body_value) return body_value;
				llvm::BasicBlock* body_end_bb = builder.GetInsertBlock();
				builder.CreateBr(merge_bb);
				
				Value else_value;
				llvm::BasicBlock* else_end_bb = NULL;
				if (else_bb) {
					builder.SetInsertPoint(else_bb);
					else_value = compile_ast_node(builder, node->if_else.else_body, current_function);
					if (!else_value) return else_value;
					else_end_bb = builder.GetInsertBlock();
					builder.CreateBr(merge_bb);
				}
				
				builder.SetInsertPoint(merge_bb);
				llvm::PHINode* phi = builder.CreatePHI(get_value_type());
				phi->addIncoming(body_value, body_end_bb);
				if (else_end_bb) {
					phi->addIncoming(else_value, else_end_bb);
				} else {
					phi->addIncoming(cond_value, cond_bb);
				}
				
				return Value(phi); // TODO: Consider if we can do something about type inference here.
			}
			default: {
				asprintf(&_error_string, "Inappropriate AST node in tree (type %d).", node->type);
				return Value();
			}
		}
		return Value();
	}
	
	llvm::GlobalVariable* Codegen::get_function_descriptor_for(const Function* function) {
		std::map<const Function*, llvm::GlobalVariable*>::iterator it = _function_descriptors.find(function);
		if (it != _function_descriptors.end()) {
			return it->second;
		}
		
		llvm::LLVMContext& context = _module->getContext();
		
		const llvm::IntegerType* itype32 = llvm::IntegerType::get(context, 32);
		const llvm::IntegerType* symbol_type = llvm::IntegerType::get(context, sizeof(SnSymbol)*8);
		const llvm::IntegerType* size_type = llvm::IntegerType::get(context, sizeof(void*)*8);
		const llvm::IntegerType* type_type /* eh heh heh */ = llvm::IntegerType::get(context, sizeof(SnType)*8);
		const llvm::IntegerType* bool_type = llvm::IntegerType::get(context, sizeof(bool)*8);
		llvm::Constant* zero32 = llvm::ConstantInt::get(itype32, 0);
		llvm::Constant* zeroes[2] = { zero32, zero32 };
		
		llvm::GlobalVariable* param_types = get_constant_array(function->param_types_as_simple_types(), function->function->getName() + "_param_types");
		llvm::GlobalVariable* param_names = get_constant_array(function->param_names, function->function->getName() + "_param_names");
		llvm::GlobalVariable* local_names = get_constant_array(function->local_names, function->function->getName() + "_local_names");
		
		/*
			typedef struct SnFunctionDescriptor {
				SnFunctionPtr ptr;
				SnSymbol name;
				SnType return_type;
				size_t num_params;
				SnType* param_types;
				SnSymbol* param_names;
				int it_index;
				SnSymbol* local_names;
				uint32_t num_locals;
				bool needs_context;
				void* jit_info;
				size_t num_variable_references;
				SnVariableReference* variable_references;
			} SnFunctionDescriptor;
		*/		
		const llvm::StructType* descriptor_type = get_struct_type("struct.SnFunctionDescriptor");
		std::vector<llvm::Constant*> struct_values;
		struct_values.reserve(descriptor_type->getNumElements());
		struct_values.push_back(function->function);
		struct_values.push_back(llvm::ConstantInt::get(symbol_type, function->name));
		struct_values.push_back(llvm::ConstantInt::get(type_type, function->return_type));
		struct_values.push_back(llvm::ConstantInt::get(size_type, function->param_names.size()));
		struct_values.push_back(llvm::ConstantExpr::getInBoundsGetElementPtr(param_types, zeroes, 2));
		struct_values.push_back(llvm::ConstantExpr::getInBoundsGetElementPtr(param_names, zeroes, 2));
		struct_values.push_back(llvm::ConstantInt::get(itype32, function->it_index)); // it_index
		struct_values.push_back(llvm::ConstantExpr::getInBoundsGetElementPtr(local_names, zeroes, 2));
		struct_values.push_back(llvm::ConstantInt::get(itype32, function->local_names.size()));
		struct_values.push_back(llvm::ConstantInt::get(bool_type, function->needs_environment));
		struct_values.push_back(llvm::ConstantExpr::getIntToPtr(llvm::ConstantInt::get(size_type, (size_t)function->function), get_value_type())); // VALUE is void*
		// Variable references
		struct_values.push_back(llvm::ConstantInt::get(size_type, function->variable_references.size()));
		// TODO: Make this a globalvariable instead of an embedded pointer
		SnVariableReference* references = new SnVariableReference[function->variable_references.size()];
		for (size_t i = 0; i < function->variable_references.size(); ++i) references[i] = function->variable_references[i];
		struct_values.push_back(llvm::ConstantExpr::getIntToPtr(llvm::ConstantInt::get(size_type, (size_t)references), get_pointer_to_struct_type("struct.SnVariableReference")));
		
		llvm::Constant* descriptor_value = llvm::ConstantStruct::get(descriptor_type, struct_values);
		llvm::GlobalVariable* var = new llvm::GlobalVariable(*_module, descriptor_value->getType(), true, llvm::GlobalValue::InternalLinkage, descriptor_value, function->function->getName() + "_descriptor");
		_function_descriptors[function] = var;
		return var;
	}
	
	template <typename C>
	llvm::GlobalVariable* Codegen::get_constant_array(const C& container, const llvm::Twine& name) {
		typedef typename C::value_type ValueType;
		typedef typename C::const_iterator ConstIterator;
		const llvm::IntegerType* element_type = llvm::IntegerType::get(_module->getContext(), sizeof(ValueType)*8);
		std::vector<llvm::Constant*> result;
		result.reserve(container.size());
		for (ConstIterator it = container.begin(); it != container.end(); ++it) {
			result.push_back(llvm::ConstantInt::get(element_type, *it));
		}
		llvm::Constant* array = llvm::ConstantArray::get(llvm::ArrayType::get(element_type, result.size()), result);
		llvm::GlobalVariable* var = new llvm::GlobalVariable(*_module, array->getType(), true, llvm::GlobalVariable::InternalLinkage, array, name);
		return var;
	}
	
	Value Codegen::get_direct_method_call(Builder& builder, const Value& self, SnSymbol method_name, const CallArguments& args) {
		if (self.type.type == SnAnyType)
			return Value();
		
		static SnSymbol* method_names_table = NULL;
		if (!method_names_table) {
			method_names_table = new SnSymbol[NumInlinableMethods];
			method_names_table[MethodPlus] = snow::symbol("+");
			method_names_table[MethodMinus] = snow::symbol("-");
			method_names_table[MethodMultiply] = snow::symbol("*");
			method_names_table[MethodDivide] = snow::symbol("/");
			method_names_table[MethodModulo] = snow::symbol("%");
			method_names_table[MethodComplement] = snow::symbol("~");
		}
		
		InlinableMethod method = NumInlinableMethods;
		for (int i = 0; i < NumInlinableMethods; ++i) {
			if (method_names_table[i] == method_name) {
				method = (InlinableMethod)i;
				break;
			}
		}
		if (method == NumInlinableMethods)
			return Value();
		
		// Handle numeric types
		if (self.type.is_numeric()) {
			if (args.values.size()) {
				if (args.values[0].type.is_numeric()) {
					return get_binary_numeric_operation(builder, self, args.values[0], method);
				}
			} else {
				return get_unary_numeric_operation(builder, self, method);
			}
		} else if (self.type.is_collection()) {
			return get_collection_operation(builder, self, args, method);
		}
		
		// TODO: Other inlinable types
		
		return Value(); // could not be inlined
	}
	
	Value Codegen::get_binary_numeric_operation(Builder& builder, const Value& a, const Value& b, InlinableMethod method) {
		if (a.type.option || b.type.option) return Value(); // TODO: Insert inlined nil-check
		
		if (a.type == SnFloatType || b.type == SnFloatType) {
			// one of operands is float, so convert both to floats
			llvm::Value* a_unshifted = a.type == SnFloatType ? get_value_to_float(builder, a) : get_integer_to_float(builder, get_value_to_integer(builder, a));
			llvm::Value* b_unshifted = b.type == SnFloatType ? get_value_to_float(builder, b) : get_integer_to_float(builder, get_value_to_integer(builder, b));
			llvm::Value* result;
			switch (method) {
				case MethodPlus:     result = builder.CreateFAdd(a_unshifted, b_unshifted); break;
				case MethodMinus:    result = builder.CreateFSub(a_unshifted, b_unshifted); break;
				case MethodMultiply: result = builder.CreateFMul(a_unshifted, b_unshifted); break;
				case MethodDivide:   result = builder.CreateFDiv(a_unshifted, b_unshifted); break;
				default: return Value(); // no inlinable method
			}
			return get_float_to_value(builder, result);
		} else {
			llvm::Value* a_unshifted = get_value_to_integer(builder, a);
			llvm::Value* b_unshifted = get_value_to_integer(builder, b);
			llvm::Value* result;
			switch (method) {
				case MethodPlus:     result = builder.CreateAdd(a_unshifted, b_unshifted); break;
				case MethodMinus:    result = builder.CreateSub(a_unshifted, b_unshifted); break;
				case MethodMultiply: result = builder.CreateMul(a_unshifted, b_unshifted); break;
				case MethodDivide:   result = builder.CreateSDiv(a_unshifted, b_unshifted); break;
				case MethodModulo:   result = builder.CreateSRem(a_unshifted, b_unshifted); break;
				default: {
					ASSERT(false && "Invalid operation for inlinable float operation.");
					return Value();
				}
			}
			return get_integer_to_value(builder, result);
		}
	}
	
	Value Codegen::get_unary_numeric_operation(Builder& builder, const Value& self, InlinableMethod operation) {
		return Value(); // TODO: Integer complement, float-to-int conversion, int-to-float conversion, etc.
	}
	
	Value Codegen::get_collection_operation(Builder& builder, const Value& self, const CallArguments& args, InlinableMethod method) {
		if (self.type.option) return Value(); // TODO: Insert inlined nil-check
		

		if (self.type == SnArrayType && args.values.size()) {
			llvm::Value* array_index = NULL;
			if (args.values[0].type == SnIntegerType) {
				array_index = builder.CreateIntCast(get_value_to_integer(builder, args.values[0]), builder.getInt32Ty(), true);
			} else
				return Value();
			
			llvm::Value* array_pointer = self.unmasked_value ? self.unmasked_value : builder.CreatePointerCast(self, get_pointer_to_struct_type("struct.SnArray"));
			
			if (method == MethodGet) {
				return builder.CreateCall2(get_runtime_function("snow_array_get"), array_pointer, array_index);
			} else if (method == MethodSet && args.values.size() >= 2) {
				return builder.CreateCall3(get_runtime_function("snow_array_set"), array_pointer, array_index, args.values[1]);
			}
		} else if (self.type == SnMapType && args.values.size()) {
			llvm::Value* map_pointer = self.unmasked_value ? self.unmasked_value : builder.CreatePointerCast(self, get_pointer_to_struct_type("struct.SnMap"));
			if (method == MethodGet) {
				return builder.CreateCall2(get_runtime_function("snow_map_get"), map_pointer, args.values[0]);
			} else if (method == MethodSet && args.values.size() >= 2) {
				return builder.CreateCall3(get_runtime_function("snow_map_set"), map_pointer, args.values[0], args.values[1]);
			}
		}
		
		return Value();
	}
	
	llvm::Value* Codegen::get_value_to_integer(Builder& builder, const Value& n) {
		if (n.unmasked_value) {
			ASSERT(n.unmasked_value->getType()->isIntegerTy());
			return n.unmasked_value;
		}
		llvm::Value* casted = builder.CreatePtrToInt(n, builder.getInt64Ty());
		llvm::Value* shifted = builder.CreateAShr(casted, 1);
		return shifted;
	}
	
	llvm::Value* Codegen::get_value_to_float(Builder& builder, const Value& f) {
		if (f.unmasked_value) {
			ASSERT(f.unmasked_value->getType()->isFloatTy());
			return f.unmasked_value;
		}
		llvm::Value* casted = builder.CreatePtrToInt(f, builder.getInt64Ty());
		llvm::Value* shifted = builder.CreateLShr(casted, 16);
		llvm::Value* converted = builder.CreateBitCast(shifted, builder.getFloatTy());
		return converted;
	}
	
	Value Codegen::get_integer_to_value(Builder& builder, llvm::Value* n) {
		ASSERT(n->getType()->isIntegerTy());
		llvm::Value* shifted = builder.CreateShl(n, 1);
		llvm::Value* masked = builder.CreateIntToPtr(builder.CreateOr(shifted, 0x1), get_value_type());
		return Value(masked, n, SnIntegerType);
	}
	
	Value Codegen::get_float_to_value(Builder& builder, llvm::Value* f) {
		ASSERT(f->getType()->isFloatTy());
		llvm::Value* casted = builder.CreateBitCast(f, builder.getInt32Ty());
		llvm::Value* casted_extended = builder.CreateZExt(casted, builder.getInt64Ty());
		llvm::Value* shifted = builder.CreateShl(casted_extended, 16);
		llvm::Value* masked = builder.CreateIntToPtr(builder.CreateOr(shifted, SnFloatType), get_value_type());
		return Value(masked, f, SnFloatType);
	}
	
	llvm::Value* Codegen::get_integer_to_float(Builder& builder, llvm::Value* n) {
		ASSERT(n->getType()->isIntegerTy());
		return builder.CreateSIToFP(n, builder.getFloatTy());
	}
	
	llvm::Value* Codegen::symbol(Builder& builder, SnSymbol k) {
		return builder.getInt64(k);
	}
	
	Value Codegen::constant_integer(Builder& builder, int64_t integer) {
		llvm::Value* c = builder.getInt64(integer);
		return get_integer_to_value(builder, c);
	}
	
	Value Codegen::constant_symbol(Builder& builder, SnSymbol sym) {
		llvm::Value* c = symbol(builder, sym);
		llvm::Value* shifted = builder.CreateShl(c, 4);
		llvm::Value* masked = builder.CreateIntToPtr(builder.CreateOr(shifted, SnSymbolType), get_value_type());
		return Value(masked, c, SnSymbolType);
	}
	
	Value Codegen::constant_float(Builder& builder, float number) {
		llvm::Value* c = llvm::ConstantFP::get(builder.getContext(), llvm::APFloat(number));
		return get_float_to_value(builder, c);
	}
	
	Value Codegen::constant_bool(Builder& builder, bool boolean) {
		VALUE val = boolean ? SN_TRUE : SN_FALSE;
		llvm::Value* c = builder.CreateIntToPtr(builder.getInt64((uint64_t)val), get_value_type());
		return Value(c, NULL, boolean ? SnTrueType : SnFalseType);
	}
	
	Value Codegen::constant_string(Builder& builder, SnString* string) {
		// TODO: Do something more clever for this.
		llvm::Value* c = builder.getInt64((uint64_t)string);
		llvm::Value* v = builder.CreateIntToPtr(c, get_value_type());
		return Value(v, SnStringType);
	}
	
	Value Codegen::nil(Builder& builder) {
		llvm::Value* c = builder.getInt64((uint64_t)SN_NIL);
		llvm::Value* v = builder.CreateIntToPtr(c, get_value_type());
		return Value(v, SnNilType);
	}
	
	Value Codegen::cast_to_value(Builder& builder, const Value& value) {
		ASSERT(value.value->getType()->isPointerTy());
		if (!value.value->getType()->getContainedType(0)->isIntegerTy(8)) {
			ASSERT(value.value->getType()->getContainedType(0)->isStructTy());
			return Value(builder.CreatePointerCast(value, get_value_type()), value.value, value.type);
		}
		return value;
	}
	
	llvm::Value* Codegen::get_pointer_to_variable_reference(Builder& builder, Function& current_function, SnSymbol name) {
		VariableReference ref;
		ref.level = 1;
		ref.index = -1;
		const Function* function = current_function.parent;
		while (function) {
			ref.index = index_of(function->local_names, name);
			if (ref.index >= 0) break;
			++ref.level;
			function = function->parent;
		}
		if (ref.index >= 0) {
			int ref_index = index_of(current_function.variable_references, ref);
			if (ref_index < 0) {
				ref_index = (int)current_function.variable_references.size();
				current_function.variable_references.push_back(ref);
			}
			
			return builder.CreateLoad(builder.CreateConstGEP1_32(current_function.variable_references_array, ref_index));
		}
		return NULL;
	}
	
	Value Codegen::compile_get_local(Builder& builder, Function& current_function, SnSymbol name) {
		std::string sname = snow::symbol_to_cstr(name);
		int local_index = index_of(current_function.local_names, name);
		if (local_index >= 0) {
			llvm::Value* local = builder.CreateLoad(builder.CreateConstGEP1_32(current_function.locals_array, local_index), sname);
			Value result;
			if (local_index < current_function.param_names.size()) {
				// It's a parameter, so we might have some type information
				result = Value(local, current_function.param_types[local_index]);
			} else {
				result = Value(local);
			}
			current_function.local_cache[name] = result;
			return result;
		}
		
		// Nothing was found in the current scope. It's time to check parent scopes.
		llvm::Value* ref_ptr = get_pointer_to_variable_reference(builder, current_function, name);
		if (ref_ptr) return Value(builder.CreateLoad(ref_ptr));
		
		// Nothing was found at all! *sigh* Check the module, and throw an error if nothing is found.
		// TODO: Provide source/line information to snow_get_module_value.
		llvm::Value* args[2] = { current_function.module, symbol(builder, name) };
		llvm::Value* v = invoke_call(builder, current_function, get_runtime_function("snow_get_module_value"), args, args+2, sname);
		Value result(v);
		current_function.local_cache[name] = result;
		return result;
	}
	
	Value Codegen::compile_association_assignment(Builder& builder, Function& current_function, const SnAstNode* target, const Value& value) {
		ASSERT(target->type == SN_AST_ASSOCIATION);
		ASSERT(target->association.args->type == SN_AST_SEQUENCE);
		
		CallArguments args;
		args.values.reserve(target->association.args->sequence.length + 1);
		for (SnAstNode* x = target->association.args->sequence.head; x; x = x->next) {
			Value arg = compile_ast_node(builder, x, current_function);
			if (!arg) return arg;
			args.values.push_back(arg);
		}
		args.values.push_back(value);
		args.it = args.values[0];
		
		Value object = compile_ast_node(builder, target->association.object, current_function);
		if (!object) return object;
		return compile_method_call(builder, current_function, object, snow::symbol("set"), args);
	}
	
	Value Codegen::compile_member_assignment(Builder& builder, Function& current_function, const SnAstNode* target, const Value& value) {
		ASSERT(target->type == SN_AST_MEMBER);
		
		Value object = compile_ast_node(builder, target->member.object, current_function);
		llvm::Value* args[3] = { object, symbol(builder, target->member.name), value };
		return invoke_call(builder, current_function, get_runtime_function("snow_set_member"), args, args+3, (object.value->getName() + "." + snow::symbol_to_cstr(target->member.name)).str());
	}
	
	Value Codegen::compile_local_assignment(Builder& builder, Function& current_function, const SnAstNode* target, const Value& value) {
		ASSERT(target->type == SN_AST_IDENTIFIER);
		SnSymbol name = target->identifier.name;
		//current_function.local_cache[name] = value;
		std::string sname = snow::symbol_to_cstr(name);
		
		int global_index = index_of(_module_globals, name);
		if (global_index < 0 && current_function.parent == NULL) {
			global_index == (int)_module_globals.size();
			_module_globals.push_back(name);
		}
		
		if (global_index >= 0) {
			// Toplevel scope, set module member.
			llvm::Value* args[2] = { current_function.module, cast_to_value(builder, current_function.module) };
			return invoke_call(builder, current_function, get_runtime_function("snow_object_set_member"), args, args+2, sname);
		}
		
		int local_index = index_of(current_function.local_names, name);
		if (local_index >= 0) {
			builder.CreateStore(value, builder.CreateConstGEP1_32(current_function.locals_array, local_index, sname + ":ptr"));
			return value;
		}
		
		// Nothing was found in the current scope. It's time to check parent scopes.
		llvm::Value* ref_ptr = get_pointer_to_variable_reference(builder, current_function, name);
		if (ref_ptr) {
			builder.CreateStore(value, ref_ptr);
			return value;
		}
		
		// No variable in parent scopes was found, so declare one here.
		if (current_function.parent) {
			// Not toplevel scope, add it to the locals list.
			local_index = (int)current_function.local_names.size();
			current_function.local_names.push_back(name);
			builder.CreateStore(value, builder.CreateConstGEP1_32(current_function.locals_array, local_index, sname + ":ptr"));
		} else {
			// Toplevel scope, set module member.
			llvm::Value* args[4] = { current_function.module, cast_to_value(builder, current_function.module), symbol(builder, name), value };
			invoke_call(builder, current_function, get_runtime_function("snow_object_set_member"), args, args+4, sname);
			_module_globals.push_back(name);
		}
		return value;
	}
	
	Value Codegen::compile_method_call(Builder& builder, Function& caller, const Value& object, SnSymbol method, const CallArguments& args) {
		// First, check if this call can/should be inlined.
		Value direct_method_call = get_direct_method_call(builder, object, method, args);
		if (direct_method_call) return direct_method_call;
		
		// Nope, make regular call.
		llvm::Twine function_name = object.value->getName() + "." + snow::symbol_to_cstr(method);
		llvm::Value* real_self_ptr = builder.CreateAlloca(get_value_type(), builder.getInt64(1), function_name + ":self_ptr");
		builder.CreateStore(object, real_self_ptr);
		llvm::Value* va[3] = { object, symbol(builder, method), real_self_ptr };
		llvm::Value* function = invoke_call(builder, caller, get_runtime_function("snow_get_method"), va, va+3, (function_name + ":method").str());
		llvm::Value* real_self = builder.CreateLoad(real_self_ptr, function_name + ":self");
		return compile_call(builder, caller, function, object, args);
	}
	
	Value Codegen::compile_call(Builder& builder, Function& caller, const Value& object, const Value& self, const CallArguments& args) {
		ASSERT(args.names.size() <= args.values.size());
		
		llvm::Value* null_value = get_null_ptr(get_value_type());
		llvm::Value* real_self = self ? cast_to_value(builder, self).value : null_value;
		
		Value function;
		const llvm::Type* function_type = get_pointer_to_struct_type("struct.SnFunction");
		llvm::Twine function_name = object.value->getName() + ":as_function";
		if (object.type != SnFunctionType && object.value->getType() != function_type) {
			llvm::Value* new_self_ptr = builder.CreateAlloca(get_value_type(), builder.getInt64(1), function_name + ":real_self_ptr");
			builder.CreateStore(real_self, new_self_ptr);
			llvm::Value* va[2] = { cast_to_value(builder, object), new_self_ptr };
			function = invoke_call(builder, caller, get_runtime_function("snow_value_to_function"), va, va+2, function_name.str());
			real_self = builder.CreateLoad(new_self_ptr, function_name + ":real_self");
		} else {
			function = builder.CreatePointerCast(object.value, function_type, function_name);
		}
		function.type = SnFunctionType;
		
		llvm::Value* names;
		if (args.names.size()) {
			names = builder.CreatePointerCast(get_constant_array(args.names, function.value->getName() + ":arg_names"), get_pointer_to_int_type(64));
		} else {
			names = get_null_ptr(get_pointer_to_int_type(64));
		}
		
		llvm::Value* values = NULL;
		if (args.values.size()) {
			values = builder.CreateAlloca(get_value_type(), builder.getInt64(args.values.size()), function_name + ":args");
			for (size_t i = 0; i < args.values.size(); ++i) {
				builder.CreateStore(args.values[i], builder.CreateConstGEP1_32(values, i));
			}
		} else {
			values = get_null_ptr(llvm::PointerType::getUnqual(get_value_type()));
		}
		
		llvm::Value* create_context_args[5] = {
			function,
			builder.getInt64(args.names.size()),
			names,
			builder.getInt64(args.values.size()),
			values
		};
		
		llvm::Value* context = builder.CreateCall(get_runtime_function("snow_create_call_frame"), create_context_args, create_context_args + 5, function_name + ":environment");
		
		// merge splat arguments
		for (size_t i = 0; i < args.splat_values.size(); ++i) {
			llvm::Value* va[2] = { context, args.splat_values[i] };
			invoke_call(builder, caller, get_runtime_function("snow_merge_splat_arguments"), va, va+2, "");
		}
		
		llvm::Value* it = args.it ? args.it.value : get_null_ptr(get_value_type());
		llvm::Value* va[4] = { function, context, real_self, it };
		return invoke_call(builder, caller, get_runtime_function("snow_function_call"), va, va+4, "");
	}
	
	SnSymbol Codegen::current_assignment_symbol() const {
		if (_assignment_name_stack.size() > 0) {
			return _assignment_name_stack.top();
		}
		return snow::symbol("<anonymous>");
	}
	
	llvm::StringRef Codegen::current_assignment_name() const {
		return llvm::StringRef(snow::symbol_to_cstr(current_assignment_symbol()));
	}
	
	const llvm::StructType* Codegen::get_struct_type(const char* name) const {
		return llvm::cast<const llvm::StructType>(_runtime_module->getTypeByName(name));
	}
	
	const llvm::PointerType* Codegen::get_pointer_to_struct_type(const char* name) const {
		const llvm::StructType* struct_type = get_struct_type(name);
		if (struct_type) {
			return llvm::PointerType::getUnqual(struct_type);
		}
		return NULL;
	}
	
	const llvm::PointerType* Codegen::get_pointer_to_int_type(uint64_t size) const {
		const llvm::IntegerType* int_type = llvm::IntegerType::get(_runtime_module->getContext(), size);
		return llvm::PointerType::getUnqual(int_type);
	}
	
	llvm::Value* Codegen::get_null_ptr(const llvm::Type* type) const {
		llvm::ConstantInt* zero = llvm::ConstantInt::get(llvm::IntegerType::get(_runtime_module->getContext(), 64), 0);
		return llvm::ConstantExpr::getIntToPtr(zero, type);
	}
	
	const llvm::PointerType* Codegen::get_value_type() const {
		return get_pointer_to_int_type(8);
	}
	
	llvm::InvokeInst* Codegen::invoke_call(Builder& builder, Function& caller, llvm::Function* callee, llvm::Value** arg_begin, llvm::Value** arg_end, const llvm::StringRef& result_name) {
		llvm::BasicBlock* normal = llvm::BasicBlock::Create(builder.getContext(), result_name.str() + ".i", caller.function);
		llvm::BasicBlock* unwind = caller.unwind_point;
		llvm::InvokeInst* result = builder.CreateInvoke(callee, normal, unwind, arg_begin, arg_end, result_name);
		builder.SetInsertPoint(normal);
		return result;
	}
	
	llvm::FunctionType* Codegen::get_function_type() const {
		static llvm::FunctionType* FT = NULL;
		if (!FT) {
			ASSERT(_runtime_module != NULL); // runtime must be loaded!
			const llvm::Type* value_type = llvm::Type::getInt8PtrTy(_runtime_module->getContext());
			std::vector<const llvm::Type*> param_types(4, NULL);
			param_types[0] = get_pointer_to_struct_type("struct.SnFunction");
			ASSERT(param_types[0]);
			param_types[1] = get_pointer_to_struct_type("struct.SnCallFrame");
			ASSERT(param_types[1]);
			param_types[2] = value_type;
			param_types[3] = value_type;
			FT = llvm::FunctionType::get(value_type, param_types, false);
		}
		return FT;
	}
	
	llvm::Function* Codegen::get_runtime_function(const char* name) const {
		ASSERT(_runtime_module != NULL); // runtime must be loaded
		llvm::Function* F = _runtime_module->getFunction(name);
		if (!F) {
			fprintf(stderr, "ERROR: Function '%s' not found in runtime! Codegen wants it.\n", name);
			return NULL;
		}
		return F;
	}
}
