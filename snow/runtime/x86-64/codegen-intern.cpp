#include "codegen-intern.hpp"
#include "../ccall-bindings.hpp"

#include "snow/exception.hpp"

#include <memory>

namespace snow {
namespace x86_64 {
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
	
	AsmValue<VALUE> Codegen::Function::compile_function_body(const ASTNode* body_seq) {
		materialized_code_offset = get_current_offset();
		pushq(RBP);
		movq(RSP, RBP);
		Fixup& stack_size_offset = subq(RSP);
		stack_size_offset.type = CodeBuffer::FixupAbsolute;
		
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
		
		AsmValue<VALUE> result(REG_RETURN);
		clear(result); // always clear return register, so empty functions return nil.
		return_label = &declare_label("return");
		
		result = compile_ast_node(body_seq);
		
		// Restore preserved registers and return
		label(*return_label);
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			popq(REG_PRESERVE[countof(REG_PRESERVE)-i-1]);
		}
		leave();
		ret();
		
		// set stack size
		int stack_size = (num_temporaries) * sizeof(VALUE);
		size_t pad = stack_size % 16;
		if (pad != 8) stack_size += 8;
		stack_size_offset.value = stack_size;
		
		return result;
	}
	
	/*void Codegen::Function::emit_eh_table() {
		size_t fstart = 0;
		size_t fend = get_current_offset();
		
		while ((get_current_offset() % 4) != 0) emit(0x90);
		
		size_t cie_start = get_current_offset();
		size_t cie_length_offset = cie_start;
		emit_immediate(PLACEHOLDER_IMM32, 4); // size
		emit(dwarf::DW_CIE_VERSION);          // version
		emit('z');
		emit('R');
	}*/
	
	AsmValue<VALUE> Codegen::Function::compile_ast_node(const ASTNode* node) {
		switch (node->type) {
			case ASTNodeTypeSequence: {
				AsmValue<VALUE> result(REG_RETURN);
				for (const ASTNode* x = node->sequence.head; x; x = x->next) {
					auto r = compile_ast_node(x);
					if (x->next == NULL) {
						movq(r, result);
					}
				}
				return result;
			}
			case ASTNodeTypeLiteral: {
				AsmValue<VALUE> result(REG_ARGS[0]);
				uintptr_t imm = (uintptr_t)node->literal.value;
				movq(imm, result);
				return result;
			}
			case ASTNodeTypeClosure: {
				Function* f = compile_function(node);
				auto c_env = call(ccall::call_frame_environment);
				c_env.set_arg<0>(get_call_frame());
				auto env = c_env.call();
				AsmValue<const FunctionDescriptor*> desc(REG_ARGS[0]);
				Fixup& fixup = movq(desc); // TODO: Use RIP-relative
				fixup.type = CodeBuffer::FixupAbsolute;
				function_descriptor_references.emplace_back(fixup, f);
				auto c_func = call(ccall::create_function_for_descriptor);
				c_func.set_arg<0>(desc);
				c_func.set_arg<1>(env);
				return c_func.call();
			}
			case ASTNodeTypeReturn: {
				AsmValue<VALUE> result(REG_RETURN);
				if (node->return_expr.value) {
					auto r = compile_ast_node(node->return_expr.value);
					movq(r, result);
				}
				jmp(*return_label);
				return result;
			}
			case ASTNodeTypeIdentifier: {
				auto local = compile_get_address_for_local(REG_ARGS[2], node->identifier.name, false);
				if (local.is_valid()) {
					return local;
				} else {
					auto c_local_missing = call(ccall::local_missing);
					c_local_missing.set_arg<0>(get_call_frame());
					c_local_missing.set_arg<1>(node->identifier.name);
					return c_local_missing.call();
				}
			}
			case ASTNodeTypeSelf: {
				auto addr = address(get_call_frame(), UNSAFE_OFFSET_OF(CallFrame, self));
				return AsmValue<VALUE>(addr);
			}
			case ASTNodeTypeHere: {
				auto c_env = call(ccall::call_frame_environment);
				c_env.set_arg<0>(get_call_frame());
				return c_env.call();
			}
			case ASTNodeTypeIt: {
				auto c_it = call(ccall::call_frame_get_it);
				c_it.set_arg<0>(get_call_frame());
				return c_it.call();
			}
			case ASTNodeTypeAssign: {
				return compile_assignment(node);
			}
			case ASTNodeTypeMethod: {
				auto object = compile_ast_node(node->method.object);
				Temporary<VALUE> self(*this);
				movq(object, self);
				
				AsmValue<MethodType> method_type(RAX);
				AsmValue<VALUE> method(REG_ARGS[0]);
				compile_get_method_inline_cache(object, node->method.name, method_type, method);
				Label& get_method = declare_label("get_method");
				Label& after = declare_label("after_get_method");
				
				cmpb(MethodTypeFunction, method_type);
				j(CC_EQUAL, get_method);
				AsmValue<VALUE> result(REG_RETURN);
				
				{
					// get property
					AsmValue<VALUE*> args(REG_ARGS[3]);
					xorq(args, args); // no args!
					auto r = compile_call(method, self, 0, args);
					movq(r, result);
					jmp(after);
				}
				
				{
					// get method proxy wrapper
					label(get_method);
					auto c_create_method_proxy = call(ccall::create_method_proxy);
					c_create_method_proxy.set_arg<0>(self);
					c_create_method_proxy.set_arg<1>(method);
					auto r = c_create_method_proxy.call();
					movq(r, result);
					// jmp(after);
				}
				
				label(after);
				return result;
			}
			case ASTNodeTypeInstanceVariable: {
				auto object = compile_ast_node(node->method.object);
				Temporary<VALUE> self(*this);
				if (object.is_memory()) {
					movq(object, REG_ARGS[0]);
					object.op = REG_ARGS[0];
				}
				movq(object, self);
				AsmValue<int32_t> idx(REG_ARGS[1]);
				compile_get_index_of_field_inline_cache(object, node->instance_variable.name, idx);
				auto c_object_get_ivar_by_index = call(ccall::object_get_instance_variable_by_index);
				c_object_get_ivar_by_index.set_arg<0>(self);
				c_object_get_ivar_by_index.set_arg<1>(idx);
				return c_object_get_ivar_by_index.call();
			}
			case ASTNodeTypeCall: {
				return compile_call(node);
			}
			case ASTNodeTypeAssociation: {
				auto object = compile_ast_node(node->association.object);
				Temporary<VALUE> self(*this);
				movq(object, self);
				
				size_t num_args = node->association.args->sequence.length;
				Temporary<VALUE*> args_ptr(*this);
				Alloca<VALUE> _1(*this, args_ptr, num_args);
				
				size_t i = 0;
				for (ASTNode* x = node->association.args->sequence.head; x; x = x->next) {
					auto result = compile_ast_node(x);
					movq(args_ptr, REG_SCRATCH[0]);
					if (result.is_memory()) {
						movq(result, REG_SCRATCH[1]);
						result.op = REG_SCRATCH[1];
					}
					movq(result, address(REG_SCRATCH[0], sizeof(VALUE) * i++));
				}
				return compile_method_call(self, snow::sym("get"), num_args, args_ptr);
			}
			case ASTNodeTypeAnd: {
				Label& left_true = declare_label();
				Label& after = declare_label();
				
				auto left = compile_ast_node(node->logic_and.left);
				AsmValue<VALUE> preserve_left(REG_PRESERVED_SCRATCH[0]);
				movq(left, preserve_left);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(left);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_NOT_EQUAL, left_true);
				AsmValue<VALUE> result(REG_RETURN);
				movq(preserve_left, result);
				jmp(after);
				
				label(left_true);
				auto r = compile_ast_node(node->logic_and.right);
				movq(r, result);

				label(after);
				return result;
			}
			case ASTNodeTypeOr: {
				Label& left_false = declare_label("or_left_is_false");
				Label& after = declare_label("or_after");
				
				auto left = compile_ast_node(node->logic_or.left);
				AsmValue<VALUE> preserve_left(REG_PRESERVED_SCRATCH[0]);
				movq(left, preserve_left);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(left);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, left_false);
				AsmValue<VALUE> result(REG_RETURN);
				movq(preserve_left, result);
				jmp(after);
				
				label(left_false);
				auto r = compile_ast_node(node->logic_or.right);
				movq(r, result);

				label(after);
				return result;
			}
			case ASTNodeTypeXor: {
				Label& equal_truth = declare_label("xor_equal_truth");
				Label& after = declare_label("xor_after");
				Label& left_is_true = declare_label("xor_left_is_true");
				
				auto left = compile_ast_node(node->logic_xor.left);
				Temporary<VALUE> left_value(*this);
				movq(left, left_value); // save left value
				
				auto right = compile_ast_node(node->logic_xor.right);
				AsmValue<VALUE> preserve_right(REG_PRESERVED_SCRATCH[0]);
				movq(right, preserve_right);
				auto c_is_truthy1 = call(snow::is_truthy);
				c_is_truthy1.set_arg<0>(right);
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
				AsmValue<VALUE> result(REG_RETURN);
				movq(preserve_right, result); // previously saved right value
				jmp(after);
				
				label(left_is_true);
				movq(left_value, result);
				jmp(after);
				
				label(equal_truth);
				movq((uintptr_t)SN_FALSE, result);
				
				label(after);
				return result;
			}
			case ASTNodeTypeNot: {
				Label& truth = declare_label("not_truth");
				Label& after = declare_label("not_after");
				
				auto result = compile_ast_node(node->logic_not.expr);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_NOT_EQUAL, truth);
				AsmValue<VALUE> ret(REG_RETURN);
				movq((uintptr_t)SN_TRUE, ret);
				jmp(after);
				
				label(truth);
				movq((uintptr_t)SN_FALSE, ret);
				
				label(after);
				return ret;
			}
			case ASTNodeTypeLoop: {
				Label& cond = declare_label("loop_cond");
				Label& after = declare_label("loop_after");
				
				label(cond);
				AsmValue<VALUE> ret(REG_RETURN);
				auto result = compile_ast_node(node->loop.cond);
				movq(result, ret);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, after);
				
				auto body_result = compile_ast_node(node->loop.body);
				movq(body_result, ret);
				jmp(cond);
				
				label(after);
				return ret;
			}
			case ASTNodeTypeBreak: {
				throw_exception_with_description("Codegen: Not implemented: break");
				return AsmValue<VALUE>();
			}
			case ASTNodeTypeContinue: {
				// TODO!
				throw_exception_with_description("Codegen: Not implemented: continue");
				return AsmValue<VALUE>();
			}
			case ASTNodeTypeIfElse: {
				Label& else_body = declare_label("if_else_body");
				Label& after = declare_label("if_after");
				
				AsmValue<VALUE> ret(REG_RETURN);
				auto cond = compile_ast_node(node->if_else.cond);
				movq(cond, ret);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(cond);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, else_body);
				auto body_result = compile_ast_node(node->if_else.body);
				movq(body_result, ret);
				
				if (node->if_else.else_body) {
					jmp(after);
					label(else_body);
					auto else_result = compile_ast_node(node->if_else.else_body);
					movq(else_result, ret);
				} else {
					label(else_body);
				}
				label(after);
				return ret;
			}
			default: {
				throw_exception_with_description("Codegen error: Inappropriate AST node in tree (type %@).", node->type);
				return AsmValue<VALUE>(); // unreachable
			}
		}
	}
	
	AsmValue<VALUE> Codegen::Function::compile_assignment(const ASTNode* node) {
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
			auto r = compile_ast_node(x);
			if (r.is_memory()) {
				movq(r, REG_SCRATCH[0]);
				r.op = REG_SCRATCH[0];
			}
			movq(r, values[i++]);
		}
		
		AsmValue<VALUE> ret(REG_RETURN);
		
		// assign values to targets
		for (size_t i = 0; i < num_targets; ++i) {
			// If this is the last target, and there are several remaining
			// values, put the rest of them in an array, and assign that.
			if (i == num_targets-1 && num_values > num_targets) {
				uint64_t num_remaining = num_values - num_targets + 1;
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
					
					auto reg_args_ptr = REG_SCRATCH[0];
					auto reg_load = REG_SCRATCH[1];
					
					size_t j = 0;
					for (ASTNode* x = target->association.args->sequence.head; x; x = x->next) {
						auto r = compile_ast_node(x);
						movq(args_ptr, reg_args_ptr);
						if (r.is_memory()) {
							movq(r, reg_load);
							r.op = reg_load;
						}
						movq(r, address(reg_args_ptr, sizeof(VALUE) * j++));
					}
					if (!j) movq(args_ptr, reg_args_ptr); // if j != 0, args_ptr is already in reg_args_ptr from above
					if (i < num_values)
						movq(values[i], reg_load);
					else
						movq((uint64_t)SN_NIL, reg_load);
					movq(reg_load, address(reg_args_ptr, sizeof(VALUE) * j));
					
					auto object = compile_ast_node(target->association.object);
					auto r = compile_method_call(object, snow::sym("set"), num_args, args_ptr);
					movq(r, ret);
					break;
				}
				case ASTNodeTypeInstanceVariable: {
					auto object = compile_ast_node(target->instance_variable.object);
					if (object.is_memory()) {
						movq(object, REG_ARGS[0]);
						object.op = REG_ARGS[0];
					}
					Temporary<VALUE> obj(*this);
					movq(object, obj);
					AsmValue<int32_t> idx(REG_ARGS[1]);
					compile_get_index_of_field_inline_cache(object, target->instance_variable.name, idx, true);
					
					auto c_object_set_ivar_by_index = call(ccall::object_set_instance_variable_by_index);
					c_object_set_ivar_by_index.set_arg<0>(obj);
					c_object_set_ivar_by_index.set_arg<1>(idx);
					if (i <= num_values)
						c_object_set_ivar_by_index.set_arg<2>(values[i]);
					else
						c_object_set_ivar_by_index.clear_arg<2>();
					auto r = c_object_set_ivar_by_index.call();
					movq(r, ret);
					break;
				}
				case ASTNodeTypeIdentifier: {
					auto local_addr = compile_get_address_for_local(REG_SCRATCH[0], target->identifier.name, true);
					if (local_addr.is_valid()) {
						AsmValue<VALUE> val(REG_RETURN);
						movq(values[i], val);
						movq(val, local_addr);
						movq(val, ret);
					} else {
						// we can define a global!
						int32_t idx = index_of(codegen.module_globals, target->identifier.name);
						if (idx < 0) {
							codegen.module_globals.push_back(target->identifier.name);
						}
						auto c_set_global = call(ccall::set_global);
						c_set_global.set_arg<0>(get_call_frame());
						c_set_global.set_arg<1>(target->identifier.name);
						c_set_global.set_arg<2>(values[i]);
						movq(c_set_global.call(), ret);
					}
					break;
				}
				case ASTNodeTypeMethod: {
					auto object = compile_ast_node(target->method.object);
					auto c_object_set = call(ccall::object_set_property_or_define_method);
					c_object_set.set_arg<0>(object);
					c_object_set.set_arg<1>(target->method.name);
					if (i <= num_values)
						c_object_set.set_arg<2>(values[i]);
					else
						c_object_set.clear_arg<2>();
					auto r = c_object_set.call();
					movq(r, ret);
				}
				default:
					throw_exception_with_description("Codegen: Invalid target for assignment. (type: %@)", target->type);
					return AsmValue<VALUE>(); // unreachable
			}
		}
		return ret;
	}
	
	Codegen::Function* Codegen::Function::compile_function(const ASTNode* function) {
		ASSERT(function->type == ASTNodeTypeClosure);
		std::unique_ptr<Codegen::Function> f(new Function(codegen));
		f->parent = this;
		if (function->closure.parameters) {
			f->param_names.reserve(function->closure.parameters->sequence.length);
			for (const ASTNode* x = function->closure.parameters->sequence.head; x; x = x->next) {
				ASSERT(x->type == ASTNodeTypeParameter);
				f->param_names.push_back(x->parameter.name);
				f->local_names.push_back(x->parameter.name);
			}
		}
		f->compile_function_body(function->closure.body);
		f->compile_function_descriptor();
		Function* final_function = f.release();
		codegen._functions.push_back(final_function);
		return final_function;
	}
	
	AsmValue<VALUE> Codegen::Function::compile_get_address_for_local(const Register& reg, Symbol name, bool can_define) {
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
				return AsmValue<VALUE>(address(reg, index * sizeof(Value)));
			} else {
				// local in parent scope
				auto c_get_locals = call(snow::get_locals_from_higher_lexical_scope);
				c_get_locals.set_arg<0>(get_call_frame());
				c_get_locals.set_arg<1>(level);
				auto locals = c_get_locals.call();
				movq(locals, reg);
				return AsmValue<VALUE>(address(reg, index * sizeof(Value)));
			}
		}
		
		// Look for module globals.
		ssize_t global_idx = index_of(codegen.module_globals, name);
		if (global_idx >= 0) {
			// a module-level variable was found, but we can't find the address of those,
			// but we also don't want to define a local in this scope, so return invalid operand.
			return AsmValue<VALUE>();
		}
		
		if (can_define && parent != NULL) {
			// if we're not in module global scope, define a regular local
			index = local_names.size();
			local_names.push_back(name);
			auto c_get_locals = call(ccall::call_frame_get_locals);
			c_get_locals.set_arg<0>(get_call_frame());
			auto locals = c_get_locals.call();
			movq(locals, reg);
			return AsmValue<VALUE>(address(reg, index * sizeof(Value)));
		}
		return AsmValue<VALUE>(); // Invalid operand.
	}
	
	AsmValue<VALUE> Codegen::Function::compile_call(const ASTNode* node) {
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
			auto r = compile_ast_node(x);
			movq(args_ptr, REG_SCRATCH[0]);
			if (r.is_memory()) {
				movq(r, REG_SCRATCH[1]);
				r.op = REG_SCRATCH[1];
			}
			movq(r, address(REG_SCRATCH[0], sizeof(VALUE) * idx));
		}
		
		// Create name list.
		// TODO: Use static storage for name lists, with RIP-addressing.
		movq(names_ptr, REG_ARGS[2]);
		for (size_t i = 0; i < names.size(); ++i) {
			movq(names[i], REG_SCRATCH[0]); // cannot move a 64-bit operand directly to memory
			movq(REG_SCRATCH[0], address(REG_ARGS[2], sizeof(Symbol) * i));
		}
		
		if (node->call.object->type == ASTNodeTypeMethod) {
			auto object = compile_ast_node(node->call.object->method.object);
			return compile_method_call(object, node->call.object->method.name, args.size(), args_ptr, names.size(), names_ptr);
		} else {
			auto functor = compile_ast_node(node->call.object);
			AsmValue<VALUE> self(REG_ARGS[1]);
			clear(self); // self = NULL
			return compile_call(functor, self, args.size(), args_ptr, names.size(), names_ptr);
		}
	}
	
	AsmValue<VALUE> Codegen::Function::compile_call(const AsmValue<VALUE>& functor, const AsmValue<VALUE>& self, size_t num_args, const AsmValue<VALUE*>& args_ptr, size_t num_names, const AsmValue<Symbol*>& names_ptr) {
		if (num_names) {
			ASSERT(num_args >= num_names);
			auto c_call = call(ccall::call_with_named_arguments);
			c_call.set_arg<0>(functor);
			c_call.set_arg<1>(self);
			c_call.set_arg<2>(num_names);
			c_call.set_arg<3>(names_ptr);
			c_call.set_arg<4>(num_args);
			c_call.set_arg<5>(args_ptr);
			return c_call.call();
		} else {
			auto c_call = call(ccall::call);
			c_call.set_arg<0>(functor);
			c_call.set_arg<1>(self);
			if (num_args)
				c_call.set_arg<2>(num_args);
			else
				c_call.clear_arg<2>();
			c_call.set_arg<3>(args_ptr);
			return c_call.call();
		}
	}
	
	AsmValue<VALUE> Codegen::Function::compile_method_call(const AsmValue<VALUE>& in_self, Symbol method_name, size_t num_args, const AsmValue<VALUE*>& args_ptr, size_t num_names, const AsmValue<Symbol*>& names_ptr) {
		ASSERT(args_ptr.op.is_memory());
		if (num_names) ASSERT(names_ptr.op.is_memory());
		
		AsmValue<VALUE> self(REG_PRESERVED_SCRATCH[1]);
		movq(in_self, self);
		
		AsmValue<VALUE> method(REG_ARGS[0]);
		AsmValue<MethodType> type(REG_ARGS[4]);
		compile_get_method_inline_cache(self, method_name, type, method);
		auto c_call = call(ccall::call_method);
		c_call.set_arg<0>(method);
		c_call.set_arg<1>(self);
		c_call.set_arg<2>(num_args);
		c_call.set_arg<3>(args_ptr);
		c_call.set_arg<4>(type);
		c_call.set_arg<5>(method_name);
		return c_call.call();
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
				Label& done = declare_label("is_truthy_after");
				
				movq(input, scratch);
				orq((uintptr_t)SN_NIL, scratch);
				xorb(REG_RETURN, REG_RETURN);
				cmpq((uintptr_t)SN_NIL, scratch);
				j(CC_EQUAL, done);
				cmpq((uintptr_t)SN_FALSE, input);
				setb(CC_NOT_EQUAL, REG_RETURN);
				label(done);
				
				return true;
			}
		}
		return false;
	}
	
	void Codegen::Function::call_or_inline(void* callee) {
		if (!perform_inlining(callee)) {
			Asm::call(callee);
		}
	}
	
	void Codegen::Function::materialize_at(byte* destination, size_t max_size) {
		render_at(destination, max_size);
		materialized_descriptor = reinterpret_cast<FunctionDescriptor*>(destination + materialized_descriptor_offset);
		materialized_code = destination + materialized_code_offset;
	}
	
	void Codegen::Function::compile_function_descriptor() {
		/*
		struct FunctionDescriptor {
			FunctionPtr ptr;
			Symbol name;
			ValueType return_type;
			size_t num_params;
			ValueType* param_types;
			Symbol* param_names;
			Symbol* local_names;
			uint32_t num_locals; // num_locals >= num_params (locals include arguments)
			size_t num_variable_references;
			VariableReference* variable_references;

			// for inline cache management
			size_t num_method_calls;
			size_t num_instance_variable_accesses;
		};
		*/
		
		align_to(8);
		materialized_descriptor_offset = get_current_offset();
		auto fixup_function_ptr = emit_pointer_to_offset();
		emit_u64(name);
		emit_u32(AnyType);
		emit_u64(param_names.size());
		auto fixup_param_types_ptr = emit_pointer_to_offset();
		auto fixup_param_names_ptr = emit_pointer_to_offset();
		auto fixup_local_names_ptr = emit_pointer_to_offset();
		emit_u32(local_names.size());
		emit_u64(0); // num_variable_references (unused!)
		emit_u64(0); // variable_references (unused!)
		emit_u64(num_method_calls);
		emit_u64(num_instance_variable_accesses);
		
		align_to(8);
		
		fixup_param_names_ptr.value = get_current_offset();
		for (auto it = param_names.begin(); it != param_names.end(); ++it) {
			emit_u64(*it);
		}
		
		fixup_local_names_ptr.value = get_current_offset();
		for (auto it = local_names.begin(); it != local_names.end(); ++it) {
			emit_u64(*it);
		}
		
		fixup_param_types_ptr.value = get_current_offset();
		for (auto it = param_names.begin(); it != param_names.end(); ++it) {
			emit_u32(AnyType);
		}
		
		align_to(8);
		
		fixup_function_ptr.value = materialized_code_offset; // Code is at the beginning of the buffer.
	}
}
}