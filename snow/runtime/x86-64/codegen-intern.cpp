#include "codegen-intern.hpp"
#include "../ccall-bindings.hpp"

namespace snow {
	inline int Codegen::Function::alloc_temporary() {
		if (temporaries_freelist.size()) {
			int tmp = temporaries_freelist.back();
			temporaries_freelist.pop_back();
			return tmp;
		}
		return num_temporaries++;
	}
	
	inline void Codegen::Function::free_temporary(int idx) {
		temporaries_freelist.push_back(idx);
	}
	
	inline AsmValue<VALUE> Codegen::Function::temporary(int idx) {
		return AsmValue<VALUE>(address(RBP, -(idx+1)*sizeof(VALUE)));
	}
	
	bool Codegen::Function::compile_function_body(const ASTNode* body_seq) {
		pushq(RBP);
		movq(RSP, RBP);
		size_t stack_size_offset1 = subq(PLACEHOLDER_IMM32, RSP);
		
		// Back up preserved registers
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			pushq(REG_PRESERVE[i]);
		}
		
		// Set up function environment
		movq(REG_ARGS[0], REG_CALL_FRAME);
		
		auto c1 = call(ccall::call_frame_get_method_cache_lines);
		c1.set_arg<0>(get_call_frame());
		auto method_cache = c1.call();
		movq(method_cache, REG_METHOD_CACHE);
		
		auto c2 = call(ccall::call_frame_get_instance_variable_cache_lines);
		c2.set_arg<0>(get_call_frame());
		auto ivar_cache = c2.call();
		movq(ivar_cache, REG_IVAR_CACHE);
		
		xorq(REG_RETURN, REG_RETURN); // always clear return register, so empty functions return nil.
		return_label = label();
		
		if (!compile_ast_node(body_seq)) return false;
		
		// Restore preserved registers and return
		bind_label(return_label);
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			popq(REG_PRESERVE[countof(REG_PRESERVE)-i-1]);
		}
		leave();
		ret();
		
		// set stack size
		int stack_size = (num_temporaries) * sizeof(VALUE);
		size_t pad = stack_size % 16;
		if (pad != 8) stack_size += 8;
		for (size_t i = 0; i < 4; ++i) {
			code()[stack_size_offset1+i] = reinterpret_cast<byte*>(&stack_size)[i];
		}
		
		return true;
	}
	
	bool Codegen::Function::compile_ast_node(const ASTNode* node) {
		switch (node->type) {
			case ASTNodeTypeSequence: {
				for (const ASTNode* x = node->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x)) return false;
				}
				return true;
			}
			case ASTNodeTypeLiteral: {
				uint64_t imm = (uint64_t)node->literal.value;
				movq(imm, REG_RETURN);
				return true;
			}
			case ASTNodeTypeClosure: {
				Function* f = compile_function(node);
				if (!f) return false;
				
				auto c_env = call(ccall::call_frame_environment);
				c_env.set_arg<0>(get_call_frame());
				auto env = c_env.call();
				FunctionDescriptorReference ref;
				AsmValue<const FunctionDescriptor*> desc(REG_ARGS[0]);
				ref.offset = movq(PLACEHOLDER_IMM64, REG_ARGS[0]);
				ref.function = f;
				function_descriptor_references.push_back(ref);
				auto c_func = call(ccall::create_function_for_descriptor);
				c_func.set_arg<0>(desc);
				c_func.set_arg<1>(env);
				c_func.call();
				return true;
			}
			case ASTNodeTypeReturn: {
				if (node->return_expr.value) {
					if (!compile_ast_node(node->return_expr.value)) return false;
				}
				jmp(return_label);
				return true;
			}
			case ASTNodeTypeIdentifier: {
				Operand local_addr = compile_get_address_for_local(REG_ARGS[2], node->identifier.name, false);
				if (local_addr.is_valid()) {
					movq(local_addr, REG_RETURN);
				} else {
					auto c_local_missing = call(ccall::local_missing);
					c_local_missing.set_arg<0>(get_call_frame());
					c_local_missing.set_arg<1>(node->identifier.name);
					c_local_missing.call();
				}
				return true;
			}
			case ASTNodeTypeSelf: {
				movq(address(REG_CALL_FRAME, UNSAFE_OFFSET_OF(CallFrame, self)), REG_RETURN);
				return true;
			}
			case ASTNodeTypeHere: {
				movq(REG_CALL_FRAME, REG_RETURN);
				return true;
			}
			case ASTNodeTypeIt: {
				auto c_it = call(ccall::call_frame_get_it);
				c_it.set_arg<0>(get_call_frame());
				c_it.call();
				return true;
			}
			case ASTNodeTypeAssign: {
				return compile_assignment(node);
			}
			case ASTNodeTypeMethod: {
				if (!compile_ast_node(node->method.object)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				Temporary<VALUE> self(*this);
				movq(result, self);
				
				AsmValue<MethodType> method_type(RAX);
				AsmValue<VALUE> method(REG_ARGS[0]);
				compile_get_method_inline_cache(result, node->method.name, method_type, method);
				Label* get_method = label();
				Label* after = label();
				
				cmpb(MethodTypeFunction, method_type);
				j(CC_EQUAL, get_method);
				
				{
					// get property
					AsmValue<VALUE*> args(REG_ARGS[3]);
					xorq(args, args); // no args!
					compile_call(method, self, 0, args);
					jmp(after);
				}
				
				{
					// get method proxy wrapper
					bind_label(get_method);
					auto c_create_method_proxy = call(ccall::create_method_proxy);
					c_create_method_proxy.set_arg<0>(self);
					c_create_method_proxy.set_arg<1>(method);
					c_create_method_proxy.call();
					// jmp(after);
				}
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeInstanceVariable: {
				if (!compile_ast_node(node->method.object)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				Temporary<VALUE> self(*this);
				movq(result, self);
				AsmValue<int32_t> idx(REG_ARGS[1]);
				compile_get_index_of_field_inline_cache(result, node->instance_variable.name, idx);
				auto c_object_get_ivar_by_index = call(ccall::object_get_instance_variable_by_index);
				c_object_get_ivar_by_index.set_arg<0>(self);
				c_object_get_ivar_by_index.set_arg<1>(idx);
				c_object_get_ivar_by_index.call();
				return true;
			}
			case ASTNodeTypeCall: {
				return compile_call(node);
			}
			case ASTNodeTypeAssociation: {
				if (!compile_ast_node(node->association.object)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				Temporary<VALUE> self(*this);
				movq(result, self);
				
				size_t num_args = node->association.args->sequence.length;
				Temporary<VALUE*> args_ptr(*this);
				Alloca<VALUE> _1(*this, args_ptr, num_args);
				
				size_t i = 0;
				for (ASTNode* x = node->association.args->sequence.head; x; x = x->next) {
					if (!compile_ast_node(x)) return false;
					movq(args_ptr, REG_SCRATCH[0]);
					movq(result, address(REG_SCRATCH[0], sizeof(VALUE) * i++));
				}
				compile_method_call(self, snow::sym("get"), num_args, args_ptr);
				return true;
			}
			case ASTNodeTypeAnd: {
				Label* left_true = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_and.left)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				movq(result, REG_PRESERVED_SCRATCH[0]);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_NOT_EQUAL, left_true);
				movq(REG_PRESERVED_SCRATCH[0], REG_RETURN);
				jmp(after);
				
				bind_label(left_true);
				if (!compile_ast_node(node->logic_and.right)) return false;

				bind_label(after);
				return true;
			}
			case ASTNodeTypeOr: {
				Label* left_false = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_or.left)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				movq(result, REG_PRESERVED_SCRATCH[0]);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, left_false);
				movq(REG_PRESERVED_SCRATCH[0], result);
				jmp(after);
				
				bind_label(left_false);
				if (!compile_ast_node(node->logic_or.right)) return false;

				bind_label(after);
				return true;
			}
			case ASTNodeTypeXor: {
				Label* equal_truth = label();
				Label* after = label();
				Label* left_is_true = label();
				
				if (!compile_ast_node(node->logic_xor.left)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				Temporary<VALUE> left_value(*this);
				movq(result, left_value); // save left value
				
				if (!compile_ast_node(node->logic_xor.right)) return false;
				movq(result, REG_PRESERVED_SCRATCH[0]); // save right value in caller-preserved register
				auto c_is_truthy1 = call(snow::is_truthy);
				c_is_truthy1.set_arg<0>(result);
				auto truthy1 = c_is_truthy1.call();
				Temporary<bool> right_truth(*this);
				movq(truthy1, right_truth);
				auto c_is_truthy2 = call(snow::is_truthy);
				c_is_truthy2.set_arg<0>(left_value);
				auto truthy2 = c_is_truthy2.call();
				cmpb(truthy2, right_truth); // compare left truth to right truth
				j(CC_EQUAL, equal_truth);
				cmpq(0, truthy2); // compare left truth to zero
				j(CC_NOT_EQUAL, left_is_true);
				movq(REG_PRESERVED_SCRATCH[0], result); // previously saved right value
				jmp(after);
				
				bind_label(left_is_true);
				movq(left_value, result);
				jmp(after);
				
				bind_label(equal_truth);
				movq((uintptr_t)SN_FALSE, result);
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeNot: {
				Label* truth = label();
				Label* after = label();
				
				if (!compile_ast_node(node->logic_not.expr)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_NOT_EQUAL, truth);
				movq((uintptr_t)SN_TRUE, result);
				jmp(after);
				
				bind_label(truth);
				movq((uintptr_t)SN_FALSE, result);
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeLoop: {
				Label* cond = label();
				Label* after = label();
				
				bind_label(cond);
				if (!compile_ast_node(node->loop.cond)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, after);
				
				if (!compile_ast_node(node->loop.body)) return false;
				jmp(cond);
				
				bind_label(after);
				return true;
			}
			case ASTNodeTypeBreak: {
				// TODO!
				return true;
			}
			case ASTNodeTypeContinue: {
				// TODO!
				return true;
			}
			case ASTNodeTypeIfElse: {
				Label* else_body = label();
				Label* after = label();
				
				if (!compile_ast_node(node->if_else.cond)) return false;
				AsmValue<VALUE> result(REG_RETURN);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, else_body);
				if (!compile_ast_node(node->if_else.body)) return false;
				
				if (node->if_else.else_body) {
					jmp(after);
					bind_label(else_body);
					if (!compile_ast_node(node->if_else.else_body)) return false;
				} else {
					bind_label(else_body);
				}
				bind_label(after);
				return true;
			}
			default: {
				fprintf(stderr, "CODEGEN: Inappropriate AST node in tree (type %d).\n", node->type);
				return false;
			}
		}
	}
	
	bool Codegen::Function::compile_assignment(const ASTNode* node) {
		ASSERT(node->assign.value->type == ASTNodeTypeSequence);
		ASSERT(node->assign.target->type == ASTNodeTypeSequence);
		
		// gather assign targets
		const size_t num_targets = node->assign.target->sequence.length;
		ASTNode* targets[num_targets];
		size_t i = 0;
		for (ASTNode* x = node->assign.target->sequence.head; x; x = x->next) {
			targets[i++] = x;
		}
		
		// allocate temporaries for this assignment
		const size_t num_values = node->assign.value->sequence.length;
		TemporaryArray<VALUE> values(*this, num_values);
		// compile assignment values
		i = 0;
		for (ASTNode* x = node->assign.value->sequence.head; x; x = x->next) {
			// TODO: Assignment name
			if (!compile_ast_node(x)) return false;
			movq(REG_RETURN, values[i++]);
		}
		
		// assign values to targets
		for (size_t i = 0; i < num_targets; ++i) {
			// If this is the last target, and there are several remaining
			// values, put the rest of them in an array, and assign that.
			if (i == num_targets-1 && num_values > num_targets) {
				size_t num_remaining = num_values - num_targets + 1;
				auto c_create_array_with_size = call(ccall::create_array_with_size);
				c_create_array_with_size.set_arg<0>(num_remaining);
				auto array_r = c_create_array_with_size.call();
				AsmValue<Object*> array(REG_PRESERVED_SCRATCH[0]);
				movq(array_r, REG_PRESERVED_SCRATCH[0]);
				for (size_t j = i; j < num_values; ++j) {
					auto c_array_push = call(ccall::array_push);
					c_array_push.set_arg<0>(array);
					c_array_push.set_arg<1>(values[j]);
					c_array_push.call();
				}
				movq(array, values[i]);
			}
			
			ASTNode* target = targets[i];
			switch (target->type) {
				case ASTNodeTypeAssociation: {
					size_t num_args = target->association.args->sequence.length + 1;
					Temporary<VALUE*> args_ptr(*this);
					Alloca<VALUE> _1(*this, args_ptr, num_args);
					
					size_t j = 0;
					for (ASTNode* x = target->association.args->sequence.head; x; x = x->next) {
						if (!compile_ast_node(x)) return false;
						movq(args_ptr, REG_ARGS[3]);
						movq(REG_RETURN, address(REG_ARGS[3], sizeof(VALUE) * j++));
					}
					if (!j) movq(args_ptr, REG_ARGS[3]); // if j != 0, args_ptr is already in RCX from above
					if (i < num_values)
						movq(values[i], REG_ARGS[2]);
					else
						movq((uint64_t)SN_NIL, REG_ARGS[2]);
					movq(REG_ARGS[2], address(REG_ARGS[3], sizeof(VALUE) * j));
					
					if (!compile_ast_node(target->association.object)) return false;
					compile_method_call(AsmValue<VALUE>(REG_RETURN), snow::sym("set"), num_args, args_ptr);
					break;
				}
				case ASTNodeTypeInstanceVariable: {
					if (!compile_ast_node(target->instance_variable.object)) return false;
					AsmValue<VALUE> result(REG_RETURN);
					Temporary<VALUE> obj(*this);
					movq(result, obj);
					AsmValue<int32_t> idx(REG_ARGS[1]);
					compile_get_index_of_field_inline_cache(result, target->instance_variable.name, idx, true);
					
					auto c_object_set_ivar_by_index = call(ccall::object_set_instance_variable_by_index);
					c_object_set_ivar_by_index.set_arg<0>(obj);
					c_object_set_ivar_by_index.set_arg<1>(idx);
					if (i <= num_values)
						c_object_set_ivar_by_index.set_arg<2>(values[i]);
					else
						c_object_set_ivar_by_index.clear_arg<2>();
					c_object_set_ivar_by_index.call();
					break;
				}
				case ASTNodeTypeIdentifier: {
					AsmValue<VALUE*> local_addr = compile_get_address_for_local(REG_SCRATCH[0], target->identifier.name, true);
					ASSERT(local_addr.is_valid());
					movq(values[i], REG_RETURN);
					movq(REG_RETURN, local_addr);
					break;
				}
				case ASTNodeTypeMethod: {
					if (!compile_ast_node(target->method.object)) return false;
					AsmValue<VALUE> result(REG_RETURN);
					auto c_object_set = call(ccall::object_set_property_or_define_method);
					c_object_set.set_arg<0>(result);
					c_object_set.set_arg<1>(target->method.name);
					if (i <= num_values)
						c_object_set.set_arg<2>(values[i]);
					else
						c_object_set.clear_arg<2>();
					c_object_set.call();
				}
				default:
					fprintf(stderr, "CODEGEN: Invalid target for assignment! (type=%d)", target->type);
					return false;
			}
		}
		return true;
	}
	
	Codegen::Function* Codegen::Function::compile_function(const ASTNode* function) {
		ASSERT(function->type == ASTNodeTypeClosure);
		Function* f = new Function(codegen);
		f->parent = this;
		if (function->closure.parameters) {
			f->param_names.reserve(function->closure.parameters->sequence.length);
			for (const ASTNode* x = function->closure.parameters->sequence.head; x; x = x->next) {
				ASSERT(x->type == ASTNodeTypeParameter);
				f->param_names.push_back(x->parameter.name);
				f->local_names.push_back(x->parameter.name);
			}
		}
		if (!f->compile_function_body(function->closure.body)) {
			delete f;
			return NULL;
		}
		codegen._functions.push_back(f);
		return f;
	}
	
	AsmValue<VALUE*> Codegen::Function::compile_get_address_for_local(const Register& reg, Symbol name, bool can_define) {
		// Look for locals
		Function* f = this;
		int32_t level = 0;
		int32_t index = -1;
		while (f) {
			index = index_of(f->local_names, name);
			if (index >= 0) break;
			f = f->parent;
			++level;
		}
		if (index >= 0) {
			if (level == 0) {
				// local to this scope
				auto c_get_locals = call(ccall::call_frame_get_locals);
				c_get_locals.set_arg<0>(get_call_frame());
				auto locals = c_get_locals.call();
				movq(locals, reg);
				return AsmValue<VALUE*>(address(reg, index * sizeof(Value)));
			} else {
				// local in parent scope
				auto c_get_locals = call(snow::get_locals_from_higher_lexical_scope);
				c_get_locals.set_arg<0>(get_call_frame());
				c_get_locals.set_arg<1>(level);
				auto locals = c_get_locals.call();
				movq(locals, reg);
				return AsmValue<VALUE*>(address(reg, index * sizeof(Value)));
			}
		}
		
		// Look for module globals.
		ssize_t global_idx = index_of(codegen.module_globals, name);
		if (global_idx >= 0) {
			return AsmValue<VALUE*>(global(global_idx));
		}
		
		if (can_define) {
			if (parent) {
				// if we're not in module global scope, define a regular local
				index = local_names.size();
				local_names.push_back(name);
				auto c_get_locals = call(ccall::call_frame_get_locals);
				c_get_locals.set_arg<0>(get_call_frame());
				auto locals = c_get_locals.call();
				movq(locals, reg);
				return AsmValue<VALUE*>(address(reg, index * sizeof(Value)));
			} else {
				global_idx = codegen.module_globals.size();
				codegen.module_globals.push_back(name);
				return AsmValue<VALUE*>(global(global_idx));
			}
		} else {
			return AsmValue<VALUE*>(); // Invalid operand.
		}
	}
	
	bool Codegen::Function::compile_call(const ASTNode* node) {
		std::vector<std::pair<const ASTNode*, int> > args;
		std::vector<Symbol> names;
		
		if (node->call.args) {
			args.reserve(node->call.args->sequence.length);

			size_t num_names = 0;
			for (const ASTNode* x = node->call.args->sequence.head; x; x = x->next) {
				if (x->type == ASTNodeTypeNamedArgument) {
					++num_names;
				}
			}
			
			size_t named_i = 0;
			size_t unnamed_i = num_names;
			names.reserve(num_names);
			for (const ASTNode* x = node->call.args->sequence.head; x; x = x->next) {
				if (x->type == ASTNodeTypeNamedArgument) {
					args.push_back(std::pair<const ASTNode*,int>(x->named_argument.expr, named_i++));
					names.push_back(x->named_argument.name);
				} else {
					args.push_back(std::pair<const ASTNode*,int>(x, unnamed_i++));
				}
			}
			ASSERT(named_i == names.size());
			ASSERT(unnamed_i == args.size());
		}
		
		Temporary<VALUE*> args_ptr(*this);
		Temporary<Symbol*> names_ptr(*this);
		Alloca<VALUE> _1(*this, args_ptr, args.size());
		// TODO: Use static storage for name lists
		Alloca<Symbol> _2(*this, names_ptr, names.size());
		
		// Compile arguments and put them in their places.
		for (size_t i = 0; i < args.size(); ++i) {
			const ASTNode* x = args[i].first;
			int idx = args[i].second;
			if (!compile_ast_node(x)) return false;
			movq(args_ptr, REG_SCRATCH[0]);
			movq(REG_RETURN, address(REG_SCRATCH[0], sizeof(VALUE) * idx));
		}
		
		// Create name list.
		// TODO: Use static storage for name lists, with RIP-addressing.
		movq(names_ptr, REG_ARGS[2]);
		for (size_t i = 0; i < names.size(); ++i) {
			movq(names[i], REG_SCRATCH[0]); // cannot move a 64-bit operand directly to memory
			movq(REG_SCRATCH[0], address(REG_ARGS[2], sizeof(Symbol) * i));
		}
		
		if (node->call.object->type == ASTNodeTypeMethod) {
			if (!compile_ast_node(node->call.object->method.object)) return false;
			AsmValue<VALUE> result(REG_RETURN);
			compile_method_call(result, node->call.object->method.name, args.size(), args_ptr, names.size(), names_ptr);
		} else {
			if (!compile_ast_node(node->call.object)) return false;
			AsmValue<VALUE> result(REG_RETURN);
			AsmValue<VALUE> self(REG_ARGS[1]);
			xorq(self, self); // self = NULL
			compile_call(result, self, args.size(), args_ptr, names.size(), names_ptr);
		}
		return true;
	}
	
	void Codegen::Function::compile_call(const AsmValue<VALUE>& functor, const AsmValue<VALUE>& self, size_t num_args, const AsmValue<VALUE*>& args_ptr, size_t num_names, const AsmValue<Symbol*>& names_ptr) {
		if (num_names) {
			ASSERT(num_args >= num_names);
			auto c_call = call(ccall::call_with_named_arguments);
			c_call.set_arg<0>(functor);
			c_call.set_arg<1>(self);
			c_call.set_arg<2>(num_names);
			c_call.set_arg<3>(names_ptr);
			c_call.set_arg<4>(num_args);
			c_call.set_arg<5>(args_ptr);
			c_call.call();
		} else {
			auto c_call = call(ccall::call);
			c_call.set_arg<0>(functor);
			c_call.set_arg<1>(self);
			if (num_args)
				c_call.set_arg<2>(num_args);
			else
				c_call.clear_arg<2>();
			c_call.set_arg<3>(args_ptr);
			c_call.call();
		}
	}
	
	void Codegen::Function::compile_method_call(const AsmValue<VALUE>& in_self, Symbol method_name, size_t num_args, const AsmValue<VALUE*>& args_ptr, size_t num_names, const AsmValue<Symbol*>& names_ptr) {
		ASSERT(args_ptr.op.is_memory());
		if (num_names) ASSERT(names_ptr.op.is_memory());
		
		AsmValue<VALUE> self(REG_PRESERVED_SCRATCH[1]);
		movq(in_self, self);
		
		AsmValue<VALUE> method(REG_ARGS[0]);
		AsmValue<MethodType> type(REG_ARGS[4]);
		compile_get_method_inline_cache(in_self, method_name, type, method);
		auto c_call = call(ccall::call_method);
		c_call.set_arg<0>(method);
		c_call.set_arg<1>(self);
		c_call.set_arg<2>(num_args);
		c_call.set_arg<3>(args_ptr);
		c_call.set_arg<4>(type);
		c_call.set_arg<5>(method_name);
		c_call.call();
	}
	
	void Codegen::Function::compile_get_method_inline_cache(const AsmValue<VALUE>& object, Symbol name, const AsmValue<MethodType>& out_type, const AsmValue<VALUE>& out_method_getter) {
		if (settings.use_inline_cache) {
			auto c_get_method = call(snow::get_method_inline_cache);
			c_get_method.set_arg<0>(object);
			c_get_method.set_arg<1>(name);
			
			size_t cache_line = num_method_calls++;
			leaq(address(REG_METHOD_CACHE, cache_line * sizeof(MethodCacheLine)), REG_ARGS[2]);
			c_get_method.set_arg<2>(AsmValue<MethodCacheLine*>(REG_ARGS[2]));
			
			AsmValue<MethodQueryResult*> result_ptr(REG_PRESERVED_SCRATCH[0]);
			Alloca<MethodQueryResult> _1(*this, result_ptr, 1);
			c_get_method.set_arg<3>(result_ptr);
			
			c_get_method.call();
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, type)), out_type);
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, result)), out_method_getter);
		} else {
			auto c_get_class = call(ccall::get_class);
			c_get_class.set_arg<0>(object);
			auto cls = c_get_class.call();
			
			auto c_lookup_method = call(ccall::class_lookup_method);
			c_lookup_method.set_arg<0>(cls);
			c_lookup_method.set_arg<1>(name);
			AsmValue<MethodQueryResult*> result_ptr(REG_PRESERVED_SCRATCH[0]);
			Alloca<MethodQueryResult> _1(*this, result_ptr, 1);
			c_lookup_method.set_arg<2>(result_ptr);
			
			c_lookup_method.call();
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, type)), out_type);
			movq(address(result_ptr.op.reg, offsetof(MethodQueryResult, result)), out_method_getter);
		}
	}
	
	void Codegen::Function::compile_get_index_of_field_inline_cache(const AsmValue<VALUE>& object, Symbol name, const AsmValue<int32_t>& target, bool can_define) {
		if (settings.use_inline_cache) {
			auto c_get_ivar_index = call(snow::get_instance_variable_inline_cache);
			c_get_ivar_index.set_arg<0>(object);
			c_get_ivar_index.set_arg<1>(name);
			size_t cache_line = num_instance_variable_accesses++;
			leaq(address(REG_IVAR_CACHE, cache_line * sizeof(InstanceVariableCacheLine)), REG_ARGS[2]);
			c_get_ivar_index.set_arg<2>(AsmValue<InstanceVariableCacheLine*>(REG_ARGS[2]));
			if (can_define)
				c_get_ivar_index.callee = snow::get_or_define_instance_variable_inline_cache;
			auto idx = c_get_ivar_index.call();
			movl(idx, target);
		} else {
			auto c_get_ivar_index = call(ccall::object_get_index_of_instance_variable);
			c_get_ivar_index.set_arg<0>(object);
			c_get_ivar_index.set_arg<1>(name);
			if (can_define)
				c_get_ivar_index.callee = ccall::object_get_or_create_index_of_instance_variable;
			auto idx = c_get_ivar_index.call();
			movl(idx, target);
		}
	}
	
	bool Codegen::Function::perform_inlining(void* callee) {
		if (settings.perform_inlining) {
			if (callee == snow::is_truthy) {
				// val != NULL && val != SN_NIL && val != SN_FALSE;
				auto input = REG_ARGS[0];
				auto scratch = REG_SCRATCH[0];
				Label* done = label();
				
				movq(input, scratch);
				orq((uintptr_t)SN_NIL, scratch);
				xorb(REG_RETURN, REG_RETURN);
				cmpq((uintptr_t)SN_NIL, scratch);
				j(CC_EQUAL, done);
				cmpq((uintptr_t)SN_FALSE, input);
				setb(CC_NOT_EQUAL, REG_RETURN);
				bind_label(done);
				
				return true;
			}
		}
		return false;
	}
	
	void Codegen::Function::call_direct(void* callee) {
		if (!perform_inlining(callee)) {
			DirectCall dc;
			dc.offset = Asm::call(0);
			dc.callee = callee;
			direct_calls.push_back(dc);
		}
	}
	
	void Codegen::Function::call_indirect(void* callee) {
		movq((uintptr_t)callee, REG_SCRATCH[0]);
		Asm::call(REG_SCRATCH[0]);
	}
	
	void Codegen::Function::materialize_at(byte* destination) {
		link_labels();
		
		for (size_t i = 0; i < code().size(); ++i) {
			destination[i] = code()[i];
		}
	}
}
