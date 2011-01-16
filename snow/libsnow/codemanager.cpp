#include "basic.hpp"
#include "codemanager.hpp"
#include "symbol-inline-pass.hpp"
#include "codegen.hpp"

#include "snow/ast.hpp"

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>

namespace snow {
	using namespace llvm;
	
	class CodeManager::Impl {
	public:
		enum OptimizationLevel {
			OPTIMIZATION_NONE,
			OPTIMIZATION_NORMAL,
			OPTIMIZATION_AGGRESSIVE,
		};
		
		Impl() : _default_optimization_level(OPTIMIZATION_NORMAL), _runtime(NULL), _engine(NULL) {}
		~Impl();
		void set_default_optimization_level(OptimizationLevel level) { _default_optimization_level = level; }
		OptimizationLevel default_optimization_level() const { return _default_optimization_level; }
		int run_main(int argc, const char** argv, const char* main_func);
		bool load_runtime(const char* path, SnVM* vm);
		bool load_precompiled_snow_code(const char* path);
		SnModuleInitFunc load_bitcode_module(const char* path);
		bool compile_ast(const SnAST* ast, const char* source, const char* module_name, SnCompilationResult& out_result);
	private:
		OptimizationLevel _default_optimization_level;
		llvm::Module* _runtime;
		llvm::ExecutionEngine* _engine;
		
		SymbolInlinePass* _symbol_inline_pass;
		
		void link_with_runtime(llvm::Module* incoming_module);
		void optimize(llvm::Module* module, OptimizationLevel level);
		void optimize(llvm::Function* function, OptimizationLevel level);
	};
	
	int CodeManager::run_main(int argc, const char** argv, const char* main_func) {
		return _impl->run_main(argc, argv, main_func);
	}
	
	int CodeManager::Impl::run_main(int argc, const char** argv, const char* main_func) {
		llvm::Function* f = _runtime->getFunction(main_func);
		if (!f) {
			fprintf(stderr, "ERROR: Main function '%s' not found in runtime module!\n", main_func);
			return -1;
		}
		
		std::vector<llvm::GenericValue> args(2);
		args[0].IntVal = llvm::APInt(32, argc);
		args[1] = llvm::PTOGV((void*)argv);
		llvm::GenericValue retval = _engine->runFunction(f, args);
		return retval.IntVal.getSExtValue();
	}
	
	bool CodeManager::load_runtime(const char* path, SnVM* vm) {
		return _impl->load_runtime(path, vm);
	}
	
	bool CodeManager::Impl::load_runtime(const char* path, SnVM* vm) {
		if (_engine) {
			fprintf(stderr, "ERROR: Runtime has already been loaded.\n");
			TRAP();
			return false;
		}
		
		std::string error;
		llvm::MemoryBuffer* buffer = NULL;
		if ((buffer = llvm::MemoryBuffer::getFile(path, &error))) {
			_runtime = llvm::getLazyBitcodeModule(buffer, llvm::getGlobalContext(), &error);
			if (!_runtime) {
				fprintf(stderr, "ERROR: Could not load %s.\n", path);
				delete buffer;
				return false;
			}
		}
		
		if (_runtime->MaterializeAllPermanently(&error)) {
			fprintf(stderr, "ERROR: Could not materialize runtime module! (error: %s)", error.c_str());
			return false;
		}
		
		_symbol_inline_pass = new SymbolInlinePass(_runtime->getFunction("snow_sym"));
		
		optimize(_runtime, OPTIMIZATION_AGGRESSIVE);
		
		_engine = llvm::EngineBuilder(_runtime)
			.setErrorStr(&error)
			.setEngineKind(llvm::EngineKind::JIT)
			.create();
		if (!_engine) {
			fprintf(stderr, "ERROR: Error creating ExecutionEngine: %s\n", error.c_str());
			return false;
		}
		_engine->runStaticConstructorsDestructors(_runtime, false);
		
		llvm::Function* snow_init_func = _runtime->getFunction("snow_init");
		if (!snow_init_func) {
			fprintf(stderr, "ERROR: snow_init not found in runtime module!\n");
			return false;
		}
		std::vector<llvm::GenericValue> args(1, llvm::PTOGV((void*)vm));
		_engine->runFunction(snow_init_func, args);
		
		return true;
	}
	
	bool CodeManager::compile_ast(const SnAST* ast, const char* source, const char* module_name, SnCompilationResult& out_result) {
		return _impl->compile_ast(ast, source, module_name, out_result);
	}
	
	bool CodeManager::Impl::compile_ast(const SnAST* ast, const char* source, const char* module_name, SnCompilationResult& out_result) {
		snow::Codegen codegen(_runtime, module_name);
		if (codegen.compile_ast(ast)) {
			llvm::Module* m = codegen.get_module();
			
			link_with_runtime(m);
			optimize(m, _default_optimization_level);
			
			_engine->addModule(m);
			//llvm::outs() << *m << '\n';
			_engine->runStaticConstructorsDestructors(m, false);
			_engine->runJITOnFunction(m->getFunction("snow_module_entry"));
			llvm::GlobalVariable* entry = codegen.get_entry_descriptor();
			SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)_engine->getOrEmitGlobalVariable(entry);
			out_result.entry_descriptor = descriptor;
			out_result.error_str = NULL;
			return true;
		} else {
			out_result.entry_descriptor = NULL;
			out_result.error_str = codegen.get_error_string();
			return false;
		}
	}
	
	SnModuleInitFunc CodeManager::load_bitcode_module(const char* path) {
		return _impl->load_bitcode_module(path);
	}
	
	SnModuleInitFunc CodeManager::Impl::load_bitcode_module(const char* path) {
		std::string error;
		llvm::MemoryBuffer* buffer = NULL;
		llvm::Module* module = NULL;
		if ((buffer = llvm::MemoryBuffer::getFile(path, &error))) {
			module = llvm::getLazyBitcodeModule(buffer, llvm::getGlobalContext(), &error);
			if (!module) {
				fprintf(stderr, "ERROR: Could not load %s.\n", path);
				delete buffer;
				return NULL;
			}
		}
		
		link_with_runtime(module);
		optimize(module, _default_optimization_level);
		
		llvm::Function* init_func = module->getFunction("snow_module_init");
		if (!init_func) {
			fprintf(stderr, "ERROR: Module does not contain a function named 'snow_module_init'.\n");
			return NULL;
		}

		_engine->addModule(module);
		_engine->runStaticConstructorsDestructors(module, false);
		return (SnModuleInitFunc)_engine->getPointerToFunction(init_func);
	}
	
	void CodeManager::Impl::link_with_runtime(llvm::Module* module) {
		for (llvm::Module::iterator it = module->begin(); it != module->end(); ++it) {
			llvm::Function* decl = &*it;
			if (decl->isDeclaration()) {
				llvm::StringRef fname = decl->getName();
				llvm::Function* impl = _runtime->getFunction(fname);
				if (impl != NULL) {
					// replace calls to declaration with call to implementation in runtime
					decl->replaceAllUsesWith(impl);
				}
			}
		}
	}
	
	
	void CodeManager::Impl::optimize(llvm::Module* module, OptimizationLevel level) {
		for (llvm::Module::iterator it = module->begin(); it != module->end(); ++it) {
			llvm::Function* f = &*it;
			if (f->isDeclaration()) continue;
			optimize(f, level);
		}
	}
	
	void CodeManager::Impl::optimize(llvm::Function* function, OptimizationLevel level) {
		if (level > OPTIMIZATION_NONE) {
			_symbol_inline_pass->runOnFunction(*function);
		}
	}
	
	CodeManager::CodeManager()  { _impl = new Impl; }
	CodeManager::~CodeManager() { delete _impl; }
	
	CodeManager::Impl::~Impl() {
		delete _symbol_inline_pass;
		_engine->runStaticConstructorsDestructors(true);
		delete _engine;
	}
	
	void CodeManager::init() {
		llvm::InitializeNativeTarget();
		llvm::llvm_start_multithreaded();
	}
	
	void CodeManager::finish() {
	}
}