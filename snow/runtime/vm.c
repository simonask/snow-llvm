#include "snow/vm.h"
#include "snow/snow.h"
#include "snow/process.h"

bool snow_vm_compile_ast(const struct SnAST* ast, SnCompilationResult* out_result) {
	SnProcess* p = snow_get_process();
	return p->vm->compile_ast(p->vm->vm_state, ast, out_result);
}