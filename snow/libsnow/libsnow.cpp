#include "basic.hpp"
#include "libsnow.hpp"
#include "codemanager.hpp"
#include "symbol.hpp"

static SnVM vm;
static snow::CodeManager* code_manager = NULL;

namespace snow {
	namespace {
		bool compile_ast(void* vm_state, const char* module_name, const char* source, const SnAST* ast, SnCompilationResult* out_result) {
			CodeManager* cm = (CodeManager*)vm_state;
			return cm->compile_ast(ast, source, module_name, *out_result);
		}
		
		SnModuleInitFunc load_bitcode_module(void* vm_state, const char* path) {
			CodeManager* cm = (CodeManager*)vm_state;
			return cm->load_bitcode_module(path);
		}
	}
	
	void init(const char* runtime_bitcode_path) {
		CodeManager::init();
		code_manager = new CodeManager;
		
		vm.vm_state = code_manager;
		vm.compile_ast = compile_ast;
		vm.load_bitcode_module = load_bitcode_module;
		vm.symbol = snow::symbol;
		vm.symbol_to_cstr = snow::symbol_to_cstr;
		
		code_manager->load_runtime(runtime_bitcode_path, &vm);
	}
	
	void finish() {
		delete code_manager;
		code_manager = NULL;
		CodeManager::finish();
	}
	
	int main(int argc, const char** argv) {
		return code_manager->run_main(argc, argv);
	}
}