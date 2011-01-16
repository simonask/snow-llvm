#pragma once
#ifndef CODEMANAGER_HPP_JMOI6NFS
#define CODEMANAGER_HPP_JMOI6NFS

#include "snow/vm.h"
#include "snow/ast.h"

namespace snow {
	class CodeManager {
	public:
		CodeManager();
		~CodeManager();
		
		bool compile_ast(const SnAST* ast, const char* source, const char* module_name, SnCompilationResult& out_result);
		SnModuleInitFunc load_bitcode_module(const char* path);
		bool load_runtime(const char* path, SnVM* vm);
		int run_main(int argc, const char** argv, const char* main_func = "snow_main");
		
		static void init();
		static void finish();
	private:
		class Impl;
		Impl* _impl;
	};
}

#endif /* end of include guard: CODEMANAGER_HPP_JMOI6NFS */
