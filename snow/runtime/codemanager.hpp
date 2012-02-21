#pragma once
#ifndef CODEMANAGER_HPP_JMOI6NFS
#define CODEMANAGER_HPP_JMOI6NFS

#include "snow/ast.hpp"
#include "snow/module.hpp"

#include "function-internal.hpp"

#include <vector>
#include <string>

struct unw_cursor_t;

namespace snow {
	struct SourceLocation {
		uint32_t code_offset;
		uint32_t line;
		uint32_t column;
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
	private:
		CodeManager() {}
		std::vector<std::unique_ptr<CodeModule> > _modules;
	};
}

#endif /* end of include guard: CODEMANAGER_HPP_JMOI6NFS */
