#pragma once
#ifndef CODEMANAGER_HPP_JMOI6NFS
#define CODEMANAGER_HPP_JMOI6NFS

#include "snow/ast.hpp"
#include "snow/module.hpp"
#include "snow/symbol.hpp"

#include "function-internal.hpp"

#include <vector>
#include <string>
#include <tuple>
#include <map>

struct unw_cursor_t;

namespace snow {
	struct SourceLocation : LexerLocation {
		uint32_t code_offset;
	};
	
	struct SourceFile {
		std::string path;
		std::string source;
	};
	
	struct CodeModule {
		typedef std::vector<SourceLocation> LocationList; // sorted by code_offset
		SourceFile source_file;
		LocationList locations;
		byte* memory;
		size_t size;
		const FunctionDescriptor* entry;
		
		CodeModule() : memory(nullptr), size(0) {}
		~CodeModule();
	};
	
	class CodeManager {
	public:
		~CodeManager();
		
		static CodeManager* get();
		
		CodeModule* compile_ast(const ASTBase* ast, const std::string& source, const std::string& path);
		ModuleInitFunc load_module(const char* path);
		TestSuiteFunc load_test_suite(const char* path);
		
		bool find_source_location_from_instruction_pointer(void* ip, const SourceFile*& out_file, const SourceLocation*& out_location);
		CallFrame* find_call_frame(unw_cursor_t* cursor);
		bool find_binding_starting_at(uintptr_t ip, std::string& out_friendly_name);
		void register_binding(Symbol module_name, Symbol function_name, uintptr_t function_start);
	private:
		CodeManager() {}
		std::vector<std::unique_ptr<CodeModule> > _modules;
		std::map<uintptr_t, std::tuple<Symbol, Symbol> > binding_names;
	};
	
	inline void CodeManager::register_binding(Symbol module_name, Symbol function_name, uintptr_t function_start) {
		binding_names[function_start] = std::make_tuple(module_name, function_name);
	}
	
	inline bool CodeManager::find_binding_starting_at(uintptr_t ip, std::string& out_friendly_name) {
		auto it = binding_names.find(ip);
		if (it != binding_names.end()) {
			const char* module_name = sym_to_cstr(std::get<0>(it->second));
			const char* function_name = sym_to_cstr(std::get<1>(it->second));
			out_friendly_name = std::string(module_name ? module_name : "") + "#" + std::string(function_name ? function_name : "<unknown>");
			return true;
		}
		return false;
	}
}

#endif /* end of include guard: CODEMANAGER_HPP_JMOI6NFS */
