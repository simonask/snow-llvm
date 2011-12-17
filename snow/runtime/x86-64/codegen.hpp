#pragma once
#ifndef CODEGEN_HPP_IB2E5EKO
#define CODEGEN_HPP_IB2E5EKO

#include "snow/ast.hpp"
#include "snow/function.hpp"
#include <vector>

namespace snow {
	struct CodegenSettings {
		bool use_inline_cache;
	};
	
	class Codegen {
	public:
		Codegen(const CodegenSettings& settings) : _settings(settings) {}
		bool compile_ast(const SnAST* ast);
		size_t compiled_size() const;
		size_t materialize_at(byte* destination);
		size_t get_offset_for_entry_descriptor() const;
		
		class Function;
		std::vector<SnSymbol> module_globals;
	private:
		const CodegenSettings& _settings;
		
		friend class Function;
		Function* _entry;
		std::vector<Function*> _functions;
		
	};
}

#endif /* end of include guard: CODEGEN_HPP_IB2E5EKO */
