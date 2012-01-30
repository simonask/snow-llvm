#include "codegen.hpp"
#include "asm.hpp"
#include "snow/util.hpp"
#include "../function-internal.hpp"
#include "../inline-cache.hpp"
#include "../internal.h"

#include "snow/snow.hpp"
#include "snow/array.hpp"
#include "snow/class.hpp"

#include "codegen-intern.hpp"

#include <map>
#include <algorithm>
#include <tuple>

namespace snow {
	bool Codegen::compile_ast(const ASTBase* ast) {
		Function* function = new Function(*this);
		_functions.push_back(function);
		_entry = function;
		return function->compile_function_body(ast->_root);
	}
	
	size_t Codegen::compiled_size() const {
		size_t accum = module_globals.size() * sizeof(VALUE);
		for (size_t i = 0; i < _functions.size(); ++i) {
			accum += sizeof(FunctionDescriptor);
			accum += _functions[i]->code().size();
		}
		return accum;
	}
	
	size_t Codegen::materialize_at(byte* destination) {
		size_t offset = 0;
		
		// materialize globals
		memset(destination, 0, module_globals.size() * sizeof(VALUE));
		offset += module_globals.size() * sizeof(VALUE);
		
		// materialize function descriptors
		for (size_t i = 0; i < _functions.size(); ++i) {
			Function* f = _functions[i];
			f->materialized_descriptor_offset = offset;
			
			FunctionDescriptor* descriptor = (FunctionDescriptor*)(destination + offset);
			offset += sizeof(FunctionDescriptor);
			
			descriptor->ptr = 0; // linked later
			descriptor->name = f->name;
			descriptor->return_type = AnyType;
			size_t num_params = f->param_names.size();
			descriptor->num_params = num_params;
			descriptor->param_types = (ValueType*)(destination + offset);
			offset += sizeof(ValueType) * num_params;
			descriptor->param_names = (Symbol*)(destination + offset);
			offset += sizeof(Symbol) * num_params;
			for (size_t j = 0; j < num_params; ++j) {
				descriptor->param_types[j] = AnyType; // TODO
				descriptor->param_names[j] = f->param_names[j];
			}
			
			size_t num_locals = f->local_names.size();
			descriptor->num_locals = num_locals;
			descriptor->local_names = (Symbol*)(destination + offset);
			offset += sizeof(Symbol) * num_locals;
			for (size_t j = 0; j < num_locals; ++j) {
				descriptor->local_names[j] = f->local_names[j];
			}
			
			descriptor->num_variable_references = 0; // TODO
			descriptor->variable_references = NULL;
			
			descriptor->num_method_calls = f->num_method_calls;
			descriptor->num_instance_variable_accesses = f->num_instance_variable_accesses;
		}
		
		std::vector<Function::FunctionDescriptorReference> descriptor_references;
		std::vector<Function::DirectCall> direct_calls;
		
		// materialize code
		for (size_t i = 0; i < _functions.size(); ++i) {
			Function* f = _functions[i];
			f->link_globals(offset);
			f->materialize_at(destination + offset);
			f->materialized_code_offset = offset;
			FunctionDescriptor* descriptor = (FunctionDescriptor*)(destination + f->materialized_descriptor_offset);
			descriptor->ptr = (FunctionPtr)(destination + offset);
			
			// collect descriptor references
			for (size_t j = 0; j < f->function_descriptor_references.size(); ++j) {
				Function::FunctionDescriptorReference ref;
				ref.function = f->function_descriptor_references[j].function;
				ref.offset = offset + f->function_descriptor_references[j].offset;
				descriptor_references.push_back(ref);
			}
			
			// collect direct calls
			for (size_t j = 0; j < f->direct_calls.size(); ++j) {
				direct_calls.push_back(f->direct_calls[j]);
				direct_calls.back().offset += offset;
			}
			
			offset += f->code().size();
		}
		
		// link descriptor references
		for (size_t i = 0; i < descriptor_references.size(); ++i) {
			uint64_t* site = (uint64_t*)(destination + descriptor_references[i].offset);
			*site = (uint64_t)(destination + descriptor_references[i].function->materialized_descriptor_offset);
		}
		
		// link direct calls
		for (size_t i = 0; i < direct_calls.size(); ++i) {
			size_t o = direct_calls[i].offset;
			byte* callee = (byte*)direct_calls[i].callee;
			static const size_t direct_call_instr_len = 4;
			intptr_t diff = callee - (destination + o + direct_call_instr_len);
			ASSERT(diff <= INT_MAX && diff >= INT_MIN);
			for (size_t j = 0; j < 4; ++j) {
				destination[o + j] = reinterpret_cast<byte*>(&diff)[j];
			}
		}
		
		return offset;
	}
	
	size_t Codegen::get_offset_for_entry_descriptor() const {
		return _entry->materialized_descriptor_offset;
	}
}