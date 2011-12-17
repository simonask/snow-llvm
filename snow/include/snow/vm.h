#pragma once
#ifndef VM_H_QGO3BPWD
#define VM_H_QGO3BPWD

#include "snow/basic.h"
#include "snow/symbol.hpp"

struct SnProcess;
struct SnAST;
struct SnFunctionDescriptor;
struct SnString;
struct SnFunction;
struct SnFiber;

typedef VALUE(*SnModuleInitFunc)();
typedef int(*SnTestSuiteFunc)();
typedef SnModuleInitFunc(*SnLoadBitcodeModuleFunc)(void* vm_state, const char* path);



#endif /* end of include guard: VM_H_QGO3BPWD */
