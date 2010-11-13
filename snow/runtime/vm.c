#include "snow/vm.h"
#include "snow/snow.h"
#include "snow/process.h"
#include "snow/str.h"

bool snow_vm_compile_ast(const struct SnAST* ast, SnCompilationResult* out_result) {
	SnProcess* p = snow_get_process();
	return p->vm->compile_ast(p->vm->vm_state, ast, out_result);
}

SnString* snow_vm_get_name_of(void* ptr) {
	SnProcess* p = snow_get_process();
	char buffer[100];
	int n = p->vm->get_name_of(p->vm->vm_state, ptr, buffer, 100);
	return snow_create_string_with_size(buffer, n);
}