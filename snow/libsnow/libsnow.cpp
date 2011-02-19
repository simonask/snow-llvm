#include "basic.hpp"
#include "libsnow.hpp"
#include "codemanager.hpp"
#include "symbol.hpp"

#include "snow/function.h"

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
		
		const char* get_type_name(SnType type) {
			switch (type) {
				case SnAnyType: return "Any";
				case SnIntegerType: return "Integer";
				case SnNilType: return "Nil";
				case SnFalseType:
				case SnTrueType: return "Boolean";
				case SnSymbolType: return "Symbol";
				case SnFloatType: return "Float";
				case SnObjectType: return "Object";
				case SnStringType: return "String";
				case SnArrayType: return "Array";
				case SnMapType: return "Map";
				case SnFunctionType: return "Function";
				case SnFunctionCallContextType: return "FunctionCallContext";
				case SnArgumentsType: return "Arguments";
				case SnPointerType: return "Pointer";
				default: return "<UNKNOWN>";
			}
		}
		
		void print_disassembly(void* cm, const SnFunctionDescriptor* descriptor) {
			CodeManager* code_manager = (CodeManager*)cm;
			printf("Signature: %s(", snow::symbol_to_cstr(descriptor->name));
			for (size_t i = 0; i < descriptor->num_params; ++i) {
				printf("%s AS %s", snow::symbol_to_cstr(descriptor->param_names[i]), get_type_name(descriptor->param_types[i]));
				
				if (i != descriptor->num_params-1) printf(", ");
			}
			printf(")");
			printf(" -> %s\n", get_type_name(descriptor->return_type)); // TODO
			printf("\n");
			if (descriptor->num_locals) {
				printf("Locals:\n");
				for (size_t i = 0; i < descriptor->num_locals; ++i) {
					printf("%s\n", snow::symbol_to_cstr(descriptor->local_names[i]));
				}
				printf("\n");
			}
			
			printf("Disassembly:\n");
			code_manager->print_disassembly(descriptor);
		}
	}
	
	void init(const char* runtime_bitcode_path) {
		CodeManager::init();
		code_manager = new CodeManager;
		
		vm.vm_state = code_manager;
		vm.compile_ast = compile_ast;
		vm.load_bitcode_module = load_bitcode_module;
		vm.print_disassembly = print_disassembly;
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