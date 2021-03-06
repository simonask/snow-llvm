#include "codegen.hpp"
#include "asm.hpp"
#include "snow/util.hpp"
#include "../function-internal.hpp"
#include "../inline-cache.hpp"
#include "../internal.h"
#include "eh-frame.hpp"

#include "snow/snow.hpp"
#include "snow/array.hpp"
#include "snow/class.hpp"

#include "codegen-intern.hpp"
#include "../codemanager.hpp"

#include <map>
#include <algorithm>
#include <tuple>

namespace snow {
namespace x86_64 {
	bool Codegen::compile_ast(const ASTBase* ast) {
		Function* function = new Function(*this);
		_functions.push_back(function);
		_entry = function;
		function->compile_function_body(NULL, ast->_root);
		return true;
	}
	
	size_t Codegen::compiled_size() const {
		size_t accum = 0;
		for (size_t i = 0; i < _functions.size(); ++i) {
			accum += _functions[i]->compiled_size();
		}
		return accum;
	}
	
	void Codegen::materialize_in(CodeModule& module) {
		std::map<Function*, byte*> function_descriptors;
		
		// Calculate the offsets for function descriptors
		size_t offset = 0;
		for (auto it = _functions.begin(); it != _functions.end(); ++it) {
			function_descriptors[*it] = module.memory + offset + (*it)->materialized_descriptor_offset;
			offset += (*it)->compiled_size();
		}
		
		// Materialize code and function descriptors
		offset = 0;
		for (auto it = _functions.begin(); it != _functions.end(); ++it) {
			size_t sz = (*it)->compiled_size();
			(*it)->fixup_function_references(function_descriptors);
			(*it)->materialize_at(module.memory + offset, sz);
			offset += sz;
		}
		
		// Transform and save debug information
		offset = 0;
		for (const Function* function: _functions) {
			for (const SourceLocation& local: function->source_locations) {
				SourceLocation location;
				location.code_offset = offset + local.code_offset;
				location.line = local.line;
				location.column = local.column;
				module.locations.push_back(location);
			}
			offset += function->compiled_size();
		}
		
		// Register exception handling frames
		for (auto it = _functions.begin(); it != _functions.end(); ++it) {
			byte* frame = (*it)->eh.materialized_eh_frame;
			snow::register_frame(frame);
		}
	}
	
	size_t Codegen::get_offset_for_entry_descriptor() const {
		return _entry->materialized_descriptor_offset;
	}
}
}