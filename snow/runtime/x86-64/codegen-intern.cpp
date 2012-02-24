#include "codegen-intern.hpp"
#include "../ccall-bindings.hpp"
#include "dwarf.hpp"

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
		return AsmValue<VALUE>(address(RBP, -(countof(REG_PRESERVE)+idx+1)*sizeof(VALUE)));
	}
	
	AsmValue<VALUE> Codegen::Function::compile_function_body(const ASTNode* function_node, const ASTNode* body_seq) {
		if (function_node != NULL)
			record_source_location(function_node);
		/*
			STACK LAYOUT:
			
			return address
			caller %rbp            <-- %rbp
			<preserved registers>
			<temporaries>
			<allocas>              <-- %rsp
		*/
		
		materialized_code_offset = get_current_offset();
		
		// Emit basic exception handling info, so everything is set up when we start keeping track
		// of the frame pointers.
		eh.last_label_offset = materialized_code_offset;
		emit_eh_cie();
		
		// Emit the beginning of the eh_frame structure.
		eh.eh_frame_offset = eh.buffer.size();
		Fixup& eh_frame_length = eh.buffer.emit_relative_s32();
		eh_frame_length.displacement = -4;               // FDE length does not include the length field itself
		eh.fde_cie_pointer = &eh.buffer.emit_u32();      // FDE CIE offset
		eh.fde_function_pointer = &eh.buffer.emit_u64(); // FDE function offset (calculated in materialize_at)
		eh.fde_function_length  = &eh.buffer.emit_u64(); // FDE function length
		eh.buffer.emit_uleb128(0);                       // length of FDE augmentation data??
		
		size_t current_stack_frame_size = 8; // rip
		pushq(RBP); current_stack_frame_size += 8;
		eh_label();
		eh.buffer.emit_u8(dwarf::DW_CFA_def_cfa_offset);
		eh.buffer.emit_uleb128(-STACK_GROWTH*2); // CFA is %rsp+8, but pushq(%rbp) subtracts from %rsp, so we must compensate to make CFA <- %rsp+16.
		emit_eh_define_location(RBP, 2);
		
		movq(RSP, RBP);
		eh_label();
		eh.buffer.emit_u8(dwarf::DW_CFA_def_cfa_register);  // we are about to allocate the stack frame from %rsp, and %rbp is the base pointer now, so redefine CFA in terms of %rbp.
		eh.buffer.emit_uleb128(dwarf::reg(RBP));
		
		// Back up preserved registers before allocating the stack frame
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			pushq(REG_PRESERVE[i]);
			current_stack_frame_size += 8;
		}
		eh_label();
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			emit_eh_define_location(REG_PRESERVE[i], i+1); // CFA is %rbp
		}
		
		// Allocate the stack frame (temporaries)
		Fixup& stack_size_offset = subq(RSP);
		stack_size_offset.type = CodeBuffer::FixupAbsolute;
		
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
		
		label(*return_label);
		// Deallocate stack frame.
		// We have to do this because we save preserved registers *before* allocating the stack frame.
		// Alternatively, we would have to fix up the generated DWARF code to be able to access the preserved
		// registers in terms of [%rbp+(size_of_stack_frame)+i]. Not impossible; not trivial (because DWARF
		// uses variable-length LEB128 encoding for DW_CFA_def_cfa_offset).
		Fixup& stack_size_offset2 = addq(RSP);
		stack_size_offset2.type = CodeBuffer::FixupAbsolute;
		
		// Restore preserved registers and return
		for (size_t i = 0; i < countof(REG_PRESERVE); ++i) {
			popq(REG_PRESERVE[countof(REG_PRESERVE)-i-1]);
		}
		leave();		
		ret();
		
		// Fix up stack size
		size_t temporary_alloc = num_temporaries * sizeof(VALUE);
		current_stack_frame_size += temporary_alloc;
		if (current_stack_frame_size % 16 != 0) {
			temporary_alloc += sizeof(VALUE); // allocate an extra temporary to align the stack
		}
		stack_size_offset.value = temporary_alloc;
		stack_size_offset2.value = temporary_alloc;
		
		materialized_code_length = get_current_offset() - materialized_code_offset;
		align_to(sizeof(void*));
		compile_function_descriptor();
		align_to(sizeof(void*));
		eh.materialized_eh_offset = get_current_offset();
		
		// Fixup EH frame header
		eh.buffer.align_to(sizeof(void*), dwarf::DW_CFA_nop);
		eh_frame_length.value = eh.buffer.size();
		// LLVM does this, not sure why:
		// Double zeroes for the unwind runtime.
		eh.buffer.emit_u64(0);
		eh.buffer.emit_u64(0); 
		
		return result;
	}
	
	AsmValue<VALUE> Codegen::Function::compile_ast_node(const ASTNode* node) {
		record_source_location(node);
		
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
					record_source_location(node);
					movq(r, result);
				}
				addq(alloca_total, RSP);
				jmp(*return_label);
				return result;
			}
			case ASTNodeTypeIdentifier: {
				auto local = compile_get_local(node->identifier.name);
				if (!local.is_valid()) {
					auto c_local_missing = call(ccall::local_missing);
					c_local_missing.set_arg<0>(get_call_frame());
					c_local_missing.set_arg<1>(node->identifier.name);
					return c_local_missing.call();
				}
				return local;
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
				record_source_location(node);
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
					auto c_get_property = call(ccall::get_property);
					c_get_property.set_arg<0>(method);
					c_get_property.set_arg<1>(self);
					c_get_property.set_arg<2>(node->method.name);
					c_get_property.set_arg<3>(method_type);
					movq(c_get_property.call(), result);
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
				record_source_location(node);
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
				record_source_location(node);
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
				record_source_location(node->association.object);
				return compile_method_call(self, snow::sym("get"), num_args, args_ptr);
			}
			case ASTNodeTypeAnd: {
				Label& left_true = declare_label();
				Label& after = declare_label();
				
				auto left = compile_ast_node(node->logic_and.left);
				record_source_location(node);
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
				record_source_location(node);
				movq(r, result);

				label(after);
				return result;
			}
			case ASTNodeTypeOr: {
				Label& left_false = declare_label("or_left_is_false");
				Label& after = declare_label("or_after");
				
				auto left = compile_ast_node(node->logic_or.left);
				record_source_location(node);
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
				record_source_location(node);
				movq(r, result);

				label(after);
				return result;
			}
			case ASTNodeTypeXor: {
				Label& equal_truth = declare_label("xor_equal_truth");
				Label& after = declare_label("xor_after");
				Label& left_is_true = declare_label("xor_left_is_true");
				
				auto left = compile_ast_node(node->logic_xor.left);
				record_source_location(node);
				Temporary<VALUE> left_value(*this);
				movq(left, left_value); // save left value
				
				auto right = compile_ast_node(node->logic_xor.right);
				record_source_location(node);
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
				record_source_location(node);
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
				record_source_location(node);
				movq(result, ret);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(result);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, after);
				
				auto body_result = compile_ast_node(node->loop.body);
				record_source_location(node);
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
				record_source_location(node);
				movq(cond, ret);
				auto c_is_truthy = call(snow::is_truthy);
				c_is_truthy.set_arg<0>(cond);
				auto truthy = c_is_truthy.call();
				cmpq(0, truthy);
				j(CC_EQUAL, else_body);
				auto body_result = compile_ast_node(node->if_else.body);
				record_source_location(node);
				movq(body_result, ret);
				
				if (node->if_else.else_body) {
					jmp(after);
					label(else_body);
					auto else_result = compile_ast_node(node->if_else.else_body);
					record_source_location(node);
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
					record_source_location(target);
					auto r = compile_method_call(object, snow::sym("set"), num_args, args_ptr);
					movq(r, ret);
					break;
				}
				case ASTNodeTypeInstanceVariable: {
					auto object = compile_ast_node(target->instance_variable.object);
					record_source_location(target);
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
					record_source_location(target);
					compile_set_local(target->identifier.name, values[i], ret);
					break;
				}
				case ASTNodeTypeMethod: {
					auto object = compile_ast_node(target->method.object);
					record_source_location(target);
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
		f->compile_function_body(function, function->closure.body);
		Function* final_function = f.release();
		codegen._functions.push_back(final_function);
		return final_function;
	}
	
	bool Codegen::Function::find_local(Symbol name, LocalLocation& out_location) const {
		// First, look for old-fashioned local variables in lexical scopes
		const Function* f = this;
		out_location.level = 0;
		out_location.index = -1;
		while (f) {
			out_location.index = index_of(f->local_names, name);
			if (out_location.index >= 0) break;
			f = f->parent;
			++out_location.level;
		}
		
		if (out_location.index < 0) {
			out_location.level = -1;
			out_location.index = index_of(codegen.module_globals, name);
		}
		
		return out_location.index >= 0;
	}
	
	AsmValue<VALUE> Codegen::Function::compile_get_local(Symbol name, Register result_hint) {
		LocalLocation location;
		if (find_local(name, location)) {
			AsmValue<VALUE> result(result_hint);
			if (location.is_global()) {
				auto c_get_global = call(ccall::get_global);
				c_get_global.set_arg<0>(get_call_frame());
				c_get_global.set_arg<1>(name);
				movq(c_get_global.call(), result);
				return result;
			} else {
				if (location.level == 0) {
					auto c_get_locals = call(ccall::call_frame_get_locals);
					c_get_locals.set_arg<0>(get_call_frame());
					auto locals = c_get_locals.call();
					movq(address(locals, location.index * sizeof(Value)), result);
					return result;
				} else {
					auto c_get_locals = call(snow::get_locals_from_higher_lexical_scope);
					c_get_locals.set_arg<0>(get_call_frame());
					c_get_locals.set_arg<1>(location.level);
					auto locals = c_get_locals.call();
					movq(address(locals, location.index * sizeof(Value)), result);
					return result;
				}
			}
		}
		return AsmValue<VALUE>(); // invalid
	}
	
	AsmValue<VALUE> Codegen::Function::compile_set_local(Symbol name, AsmValue<VALUE> value, Register result_hint) {
		LocalLocation location;
		if (!find_local(name, location)) {
			if (parent == nullptr) {
				location.level = -1;
				location.index = codegen.module_globals.size();
				codegen.module_globals.push_back(name);
			} else {
				// define a regular local variable
				location.level = 0;
				location.index = local_names.size();
				local_names.push_back(name);
			}
		}
		
		AsmValue<VALUE> result(result_hint);
		if (location.is_global()) {
			auto c_set_global = call(ccall::set_global);
			c_set_global.set_arg<0>(get_call_frame());
			c_set_global.set_arg<1>(name);
			c_set_global.set_arg<2>(value);
			movq(c_set_global.call(), result);
		} else {
			AsmValue<Value*> locals;
			if (location.level == 0) {
				auto c_get_locals = call(ccall::call_frame_get_locals);
				c_get_locals.set_arg<0>(get_call_frame());
				locals = c_get_locals.call();
			} else {
				auto c_get_locals = call(snow::get_locals_from_higher_lexical_scope);
				c_get_locals.set_arg<0>(get_call_frame());
				c_get_locals.set_arg<1>(location.level);
				locals = c_get_locals.call();
			}
			movq(value, REG_SCRATCH[0]);
			movq(REG_SCRATCH[0], address(locals, location.index * sizeof(Value)));
			movq(REG_SCRATCH[0], result);
		}
		
		return result;
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
			record_source_location(node->call.object);
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
		bind_label_references();
		
		// Fix up eh_frame values:
		eh.fde_cie_pointer->type = FixupAbsolute;
		eh.fde_cie_pointer->value = eh.fde_cie_pointer->position - eh.fde_cie_offset; // weird DWARF-way of referring to CIE: the number of bytes to subtract from the current position in order to find the CIE.
		eh.fde_function_pointer->type = FixupAbsolute;
		byte* eh_frame_fde_pointer = destination + eh.materialized_eh_offset + eh.fde_function_pointer->position;
		materialized_code = destination + materialized_code_offset;
		eh.fde_function_pointer->value = materialized_code - eh_frame_fde_pointer;
		eh.fde_function_length->type = FixupAbsolute;
		eh.fde_function_length->value = materialized_code_length;
		
		// Render:
		render_at(destination, max_size);
		eh.buffer.render_at(destination + eh.materialized_eh_offset, max_size - eh.materialized_eh_offset);
		materialized_descriptor = reinterpret_cast<FunctionDescriptor*>(destination + materialized_descriptor_offset);
		eh.materialized_eh_frame = destination + eh.materialized_eh_offset + eh.eh_frame_offset;
		eh.materialized_fde_cie = destination + eh.materialized_eh_offset + eh.fde_cie_offset;
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
		
		align_to(sizeof(void*));
		materialized_descriptor_offset = get_current_offset();
		Fixup& fixup_function_ptr = emit_pointer_to_offset();
		emit_u64(name);
		emit_u32(AnyType);
		emit_u64(param_names.size());
		Fixup& fixup_param_types_ptr = emit_pointer_to_offset();
		Fixup& fixup_param_names_ptr = emit_pointer_to_offset();
		Fixup& fixup_local_names_ptr = emit_pointer_to_offset();
		emit_u32(local_names.size());
		emit_u64(0); // num_variable_references (unused!)
		emit_u64(0); // variable_references (unused!)
		emit_u64(num_method_calls);
		emit_u64(num_instance_variable_accesses);
		
		align_to(sizeof(void*));
		
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
		
		align_to(sizeof(void*));
		
		fixup_function_ptr.value = materialized_code_offset; // Code is at the beginning of the buffer.
	}
	
	void Codegen::Function::eh_label() {
		int32_t diff = get_current_offset() - eh.last_label_offset;
		if (diff != 0) {
			if (diff < 64) {
				eh.buffer.emit_u8(dwarf::DW_CFA_advance_loc + diff);
			} else if (diff < 256) {
				eh.buffer.emit_u8(dwarf::DW_CFA_advance_loc1);
				eh.buffer.emit_u8(diff);
			} else {
				eh.buffer.emit_u8(dwarf::DW_CFA_advance_loc4);
				eh.buffer.emit_s32(diff);
			}
		}
	}
	
	void Codegen::Function::emit_eh_cie() {
		// Emit CIE header
		eh.buffer.align_to(4);
		Fixup& cie_length = eh.buffer.emit_relative_s32();
		eh.fde_cie_offset = cie_length.position;
		eh.buffer.emit_u32(dwarf::DW_CIE_ID);
		eh.buffer.emit_u8(dwarf::DW_CIE_VERSION);
		// personality: (none for now)
		eh.buffer.emit_u8('z');
		eh.buffer.emit_u8('R');
		eh.buffer.emit_u8(0);
		eh.buffer.emit_uleb128(1);            // code alignment
		eh.buffer.emit_sleb128(STACK_GROWTH); // data alignment = stack growth direction
		eh.buffer.emit_u8(dwarf::reg(RIP));   // return address register
		eh.buffer.emit_uleb128(0);            // augmentation size ('z')
		eh.buffer.emit_uleb128(dwarf::DW_EH_PE_absptr | dwarf::DW_EH_PE_pcrel); // FDE encoding ('R')
		
		// Emit CIE frame moves (standard C calling convention)
		emit_eh_define_cfa(RSP, -STACK_GROWTH);     // CFA  <- %rsp+8
		emit_eh_define_location(RIP, 1);            // %rip <- [CFA-8] == [%rsp]
		
		eh.buffer.align_to(sizeof(void*), dwarf::DW_CFA_nop);
		cie_length.value = eh.buffer.size();
		cie_length.displacement = -4; // cie_length does not include the length itself
	}
	
	void Codegen::Function::emit_eh_define_location(Register reg, ssize_t offset) {
		if (offset < 0) {
			eh.buffer.emit_u8(dwarf::DW_CFA_offset_extended_sf);
			eh.buffer.emit_uleb128(dwarf::reg(reg));
			eh.buffer.emit_sleb128(offset);
		} else {
			eh.buffer.emit_u8(dwarf::DW_CFA_offset + dwarf::reg(reg));
			eh.buffer.emit_uleb128(offset);
		}
	}
	
	void Codegen::Function::emit_eh_define_cfa(Register reg, size_t offset) {
		eh.buffer.emit_u8(dwarf::DW_CFA_def_cfa);
		eh.buffer.emit_uleb128(dwarf::reg(reg));
		eh.buffer.emit_uleb128(offset);
	}
	
	void Codegen::Function::record_source_location(const ASTNode* node) {
		size_t offset = get_current_offset();
		ASSERT(offset <= UINT32_MAX);
		SourceLocation location;
		location.code_offset = offset;
		location.line = node->location.line;
		location.column = node->location.column;
		
		// Try to not record more locations than necessary.
		if (source_locations.size()) {
			SourceLocation& last = source_locations.back();
			ASSERT(last.code_offset <= location.code_offset);
			if (last.code_offset == offset) {
				last = location;
			} else if (last.line == location.line && last.column == location.column) {
				// do nothing
			} else {
				source_locations.push_back(location);
			}
		} else {
			source_locations.push_back(location);
		}
	}
}
}