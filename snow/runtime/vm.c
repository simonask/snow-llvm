#include "snow/vm.h"
#include "snow/snow.h"
#include "snow/process.h"
#include "snow/str.h"

bool snow_vm_compile_ast(const char* module_name, const char* source, const struct SnAST* ast, SnCompilationResult* out_result) {
	SnProcess* p = snow_get_process();
	return p->vm->compile_ast(p->vm->vm_state, module_name, source, ast, out_result);
}

SnObject* snow_vm_load_bitcode_module(const char* path) {
	SnProcess* p = snow_get_process();
	SnModuleInitFunc init = p->vm->load_bitcode_module(p->vm->vm_state, path);
	if (init) {
		return init();
	}
	return NULL;
}