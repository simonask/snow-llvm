#define __STDC_LIMIT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

#include "debugger.hpp"

#include "snow/basic.h"
#include "../runtime/vm-intern.hpp"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <llvm/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/CodeGen/MachineFunction.h>
#include <llvm/ExecutionEngine/JITEventListener.h>

#include <vector>
#include <map>
#include <bits/stl_pair.h>
#include <cxxabi.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>


// Integrate with crash reporter.
#ifdef __APPLE__
extern "C" const char* __crashreporter_info__;
const char* __crashreporter_info__ = 0;
#endif

extern int main(int, char**);

namespace snow {
	namespace debugger {
		using namespace llvm;
		
		static SnVM* _vm = NULL;
		
		void start(SnVM* vm) {
			_vm = vm;
		}
		
		const char* demangle_cxx_name(const char* mangled_name) {
			static char* ptr = NULL;
			static size_t demangled_len = 0;
			int demangling_status = -1;
			ptr = abi::__cxa_demangle(mangled_name, ptr, &demangled_len, &demangling_status);
			return ptr ? ptr : mangled_name;
		}
		
		struct InstructionRange {
			const byte* start;
			const byte* end;
			InstructionRange(const byte* s) : start(s), end(s) {}
			InstructionRange(const byte* s, const byte* e) : start(s), end(e) {}
			bool operator<(const InstructionRange& other) const { return start < other.start; }
			bool contains(const byte* ptr) const { return start <= ptr && end >= ptr; }
		};
		typedef std::map<InstructionRange, const Function*> FunctionMap;
		typedef std::map<uintptr_t, MDNode*> LineStartMap;
		typedef std::map<const Function*, LineStartMap> FunctionLineStartMap;
		
		static FunctionMap _function_map;
		static FunctionLineStartMap _function_line_start_map;
		
		FunctionMap::iterator find_function_for_instruction_pointer(const byte* ptr) {
			for (FunctionMap::iterator it = _function_map.begin(); it != _function_map.end(); ++it) {
				if (it->first.contains(ptr))
					return it;
			}
			return _function_map.end();
		}
		
		class SnowJITEventListener : public llvm::JITEventListener {
		public:
			void NotifyFunctionEmitted(const Function& f, void* code, size_t code_size, const JITEvent_EmittedFunctionDetails& details) {
				//fprintf(stderr, "Emitting function %s\n", demangle_cxx_name(f.getName().data()));
				/*const MachineFunction* mf = details.MF;
				byte* bytes = (byte*)code;
				_function_map[InstructionRange(bytes, bytes+code_size)] = &f;
				
				FunctionLineStartMap::iterator inserted = _function_line_start_map.insert(FunctionLineStartMap::value_type(&f, LineStartMap())).first;
				LineStartMap& lsm = inserted->second;
				for (size_t i = 0; i < details.LineStarts.size(); ++i) {
					lsm[details.LineStarts[i].Address] = mf->getDILocation(details.LineStarts[i].Loc).getNode();
					//fprintf(stderr, "registering DebugLoc index %u\n", details.LineStarts[i].Loc.getIndex());
				}*/
			}
			void NotifyFreeingMachineCode(void* old_ptr) {
				fprintf(stderr, "Freeing machine code: %p\n", old_ptr);
				/*byte* bytes = (byte*)old_ptr;
				FunctionMap::iterator f_it = _function_map.find(bytes);
				if (f_it != _function_map.end())
				{
					const Function* f = f_it->second;
					FunctionLineStartMap::iterator fls_it = _function_line_start_map.find(f);
					
					_function_map.erase(f_it);
					if (fls_it != _function_line_start_map.end())
						_function_line_start_map.erase(fls_it);
				}*/
			}
		};
		
		llvm::JITEventListener* jit_event_listener() {
			static SnowJITEventListener el;
			return &el;
		}
		
		void print_callsite(byte* ip) {
			FunctionMap::iterator it = find_function_for_instruction_pointer(ip);
			if (it != _function_map.end()) {
				const Function* f = it->second;
				FunctionLineStartMap::iterator flsm_it = _function_line_start_map.find(f);
				MDNode* dbg = NULL;
				if (flsm_it != _function_line_start_map.end()) {
					LineStartMap& lsm = flsm_it->second;
					
					uintptr_t address = (uintptr_t)ip;
					for (LineStartMap::iterator lsm_it = lsm.begin(); lsm_it != lsm.end(); ++lsm_it) {
						if (address < lsm_it->first) {
							break;
						}
						dbg = lsm_it->second;
					}
				}
				
				if (dbg != NULL) {
					DILocation l = DILocation(dbg);
					fprintf(stderr, "%s (%s:%u+%u)\n", demangle_cxx_name(f->getNameStr().c_str()), l.getFilename().str().c_str(), l.getLineNumber(), l.getColumnNumber());
				} else {
					fprintf(stderr, "%s (no debug info)\n", demangle_cxx_name(f->getNameStr().c_str()));
				}
			} else {
				fprintf(stderr, "%p (no debug info)\n", ip);
			}
		}
		
		void print_backtrace(ucontext_t* context) {
			fprintf(stderr, "BACKTRACE:\n");
			byte* rip = (byte*)context->uc_mcontext->__ss.__rip;
			void** rbp = (void**)context->uc_mcontext->__ss.__rbp;
			int lines = 0;
			while (rip != (byte*)main && rbp && lines < 25) {
				print_callsite(rip);
				rip = (byte*)rbp[1];
				rbp = (void**)rbp[0];
				++lines;
			}
		}
		
		void handle_signal(int sig, siginfo_t* si, void* ctx) {
			static bool handling_signal = false;
			
			if (handling_signal) {
				fprintf(stderr, "Another signal caught while handling a signal! Aborting.\n");
				exit(1);
			}
			
			handling_signal = true;
			printf("HANDLING SIGNAL!\n");
			ucontext_t* context = reinterpret_cast<ucontext_t*>(ctx);
			
			print_callsite((byte*)context->uc_mcontext->__ss.__rip);
			
			print_backtrace(context);
			
			switch (sig) {
				case SIGTRAP:
				{
					fprintf(stderr, "TRAP CAUGHT! OMG!\n");
					exit(1);
				}
				case SIGABRT:
				{
					fprintf(stderr, "ABORT! OMG!\n");
					exit(1);
				}
				case SIGBUS:
				{
					fprintf(stderr, "BUS ERROR! OMG!\n");
					exit(1);
				}
				case SIGSEGV:
				{
					fprintf(stderr, "SEGMENTATION FAULT! OMG!\n");
					exit(1);
				}
				default: {
					fprintf(stderr, "UKNOWN SIGNAL! OMG!\n");
					exit(1);
				}
			}
		}
		
		void install_signal_handlers() {
			struct sigaction sa;
			sigemptyset(&sa.sa_mask);
			sigaddset(&sa.sa_mask, SIGTRAP);
			sigaddset(&sa.sa_mask, SIGABRT);
			sigaddset(&sa.sa_mask, SIGBUS);
			sigaddset(&sa.sa_mask, SIGSEGV);
			sa.sa_flags = SA_64REGSET | SA_SIGINFO | SA_NODEFER;
			sa.sa_sigaction = handle_signal;
			sigaction(SIGTRAP, &sa, NULL);
			sigaction(SIGABRT, &sa, NULL);
			sigaction(SIGBUS, &sa, NULL);
			sigaction(SIGSEGV, &sa, NULL);
		}
	}
}