#pragma once
#ifndef CODEMANAGER_HPP_JMOI6NFS
#define CODEMANAGER_HPP_JMOI6NFS

#include "snow/ast.hpp"
#include "snow/module.hpp"

#include "function-internal.hpp"

namespace snow {
	struct CodeModule {
		const char* name;
		void* code;
		size_t code_size;
		Symbol* global_names;
		VALUE* globals;
		size_t num_globals;
		const FunctionDescriptor* entry;
		
		CodeModule(const char* name) : name(name), code(NULL), code_size(0), globals(NULL), entry(NULL) {}
		~CodeModule();
	};
	
	class CodeManager {
	public:
		CodeManager();
		~CodeManager();
		
		CodeModule* compile_ast(const ASTBase* ast, const char* source, const char* module_name);
		ModuleInitFunc load_module(const char* path);
		TestSuiteFunc load_test_suite(const char* path);
	private:
		class Impl;
		Impl* _impl;
	};
	
	CodeManager* get_code_manager();
}

#endif /* end of include guard: CODEMANAGER_HPP_JMOI6NFS */
