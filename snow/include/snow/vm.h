#pragma once
#ifndef VM_H_QGO3BPWD
#define VM_H_QGO3BPWD

#include "snow/basic.h"
#include "snow/function.h"

struct SnProcess;
struct SnAST;
struct SnVM;

CAPI void snow_vm_init(SN_P);
CAPI struct SnFunctionRef snow_vm_load_precompiled_image(SN_P, const char* file);
CAPI struct SnFunctionRef snow_vm_compile_ast(SN_P, const struct SnAST* ast, const char* source);
CAPI VALUE snow_vm_call_function(SN_P, void* jit_func, SnFunctionRef function, VALUE self, struct SnArguments* args);

#endif /* end of include guard: VM_H_QGO3BPWD */
