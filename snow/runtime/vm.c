#include "snow/vm.h"
#include "snow/snow.h"
#include "snow/process.h"
#include "snow/str.h"
#include "snow/function.h"

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

void snow_vm_print_disassembly(const SnFunction* function) {
	SnProcess* p = snow_get_process();
	p->vm->print_disassembly(p->vm->vm_state, function->descriptor);
}