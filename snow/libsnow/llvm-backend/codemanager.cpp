#include "basic.hpp"
#include "codemanager.hpp"
#include "llvm-backend/symbol-inline-pass.hpp"
#include "llvm-backend/codegen.hpp"

#include "snow/ast.hpp"

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>

namespace snow {
	using namespace llvm;
	
	class CodeManager::Impl : public llvm::JITEventListener {
	public:
		enum OptimizationLevel {
			OPTIMIZATION_NONE,
			OPTIMIZATION_NORMAL,
			OPTIMIZATION_AGGRESSIVE,
		};
		
		Impl() : _default_optimization_level(OPTIMIZATION_NORMAL), _engine(NULL), _runtime(NULL) {}
		~Impl();
		void set_default_optimization_level(OptimizationLevel level) { _default_optimization_level = level; }
		OptimizationLevel default_optimization_level() const { return _default_optimization_level; }
		int run_main(int argc, const char** argv, const char* main_func);
		void print_disassembly(const SnFunctionDescriptor* function_ptr);
		bool load_runtime(const char* path, SnVM* vm);
		bool load_precompiled_snow_code(const char* path);
		SnModuleInitFunc load_bitcode_module(const char* path);
		bool compile_ast(const SnAST* ast, const char* source, const char* module_name, SnCompilationResult& out_result);
	private:
		struct FunctionInfo {
			byte* code_begin;
			byte* code_end;
		};
		
		OptimizationLevel _default_optimization_level;
		llvm::ExecutionEngine* _engine;
		llvm::Module* _runtime;
		std::map<const llvm::Function*, FunctionInfo> _function_map;
		
		SymbolInlinePass* _symbol_inline_pass;
		
		void link_with_runtime(llvm::Module* incoming_module);
		void optimize(llvm::Module* module, OptimizationLevel level);
		void optimize(llvm::Function* function, OptimizationLevel level);
		const llvm::Function* get_function_for_address(void* addr);
		
		// llvm::JITEventListener
		void NotifyFunctionEmitted(const llvm::Function& f, void* code, size_t code_size, const llvm::JITEvent_EmittedFunctionDetails& details);
		void NotifyFreeingMachineCode(void* old_ptr);
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
	
	void CodeManager::print_disassembly(const SnFunctionDescriptor* descriptor) {
		_impl->print_disassembly(descriptor);
	}
	
	const llvm::Function* CodeManager::Impl::get_function_for_address(void* _addr) {
		// You would think this could be done by _engine->getGlobalValueAtAddress, but
		// that causes an assertion to get hit within LLVM, so we need our own bookkeeping.
		byte* addr = (byte*)_addr;
		std::map<const llvm::Function*, FunctionInfo>::const_iterator it = _function_map.begin();
		while (it != _function_map.end()) {
			if (it->second.code_begin <= addr && it->second.code_end > addr) {
				return it->first;
			}
			++it;
		}
		return NULL;
	}
	
	void CodeManager::Impl::print_disassembly(const SnFunctionDescriptor* descriptor) {
		const llvm::Function* function = (llvm::Function*)descriptor->jit_info;
		if (!function) function = get_function_for_address((void*)descriptor->ptr);
		if (!function) {
			// XXX: Epic sin. Attempts to find the real address of the function through the
			// LLVM jump nest, which usually looks like: 49 ba XX XX XX XX XX XX XX XX
			void* guess = *((void**)((byte*)descriptor->ptr + 2));
			function = get_function_for_address(guess);
		}
		
		if (function) {
			llvm::outs() << *function << '\n';
		} else {
			llvm::errs() << "ERROR: Descriptor " << descriptor << " does not describe a JIT'ed function!\n";
		}
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
		_engine->DisableLazyCompilation();
		_engine->RegisterJITEventListener(this);
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
			// llvm::outs() << *m << '\n';
			_engine->runStaticConstructorsDestructors(m, false);
			_engine->runJITOnFunction(m->getFunction("snow_module_entry"));
			llvm::GlobalVariable* entry = codegen.get_entry_descriptor();
			SnFunctionDescriptor* descriptor = (SnFunctionDescriptor*)_engine->getPointerToGlobal(entry);
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
					decl->replaceAllUsesWith(ConstantExpr::getBitCast(impl, decl->getType()));
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
	
	void CodeManager::Impl::NotifyFunctionEmitted(const llvm::Function& f, void* code, size_t code_size, const llvm::JITEvent_EmittedFunctionDetails& details) {
		FunctionInfo info;
		info.code_begin = (byte*)code;
		info.code_end = info.code_begin + code_size;
		// llvm::outs() << " at " << info.code_begin << " - " << info.code_end << " Emitting " << f.getName() << '\n';
		_function_map[&f] = info;
	}
	
	void CodeManager::Impl::NotifyFreeingMachineCode(void* old_code) {
		byte* ptr = (byte*)old_code;
		std::map<const llvm::Function*, FunctionInfo>::iterator it = _function_map.begin();
		while (it != _function_map.end()) {
			if (it->second.code_begin <= ptr && it->second.code_end > ptr) {
				_function_map.erase(it);
				break;
			} else {
				++it;
			}
		}
	}
	
	CodeManager::CodeManager()  { _impl = new Impl; }
	CodeManager::~CodeManager() { delete _impl; }
	
	CodeManager::Impl::~Impl() {
		delete _symbol_inline_pass;
		_engine->runStaticConstructorsDestructors(true);
		_engine->UnregisterJITEventListener(this);
		//delete _engine; // TODO/XXX: Figure out a way to safely destroy modules without hitting the "no-use" assert.
	}
	
	void CodeManager::init() {
		llvm::InitializeNativeTarget();
		llvm::llvm_start_multithreaded();
	}
	
	void CodeManager::finish() {
	}
}