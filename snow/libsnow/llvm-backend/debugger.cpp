#define __STDC_LIMIT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

#include "debugger.hpp"

#include "snow/basic.h"
#include "../runtime/vm-intern.hpp"

#include <list>
#include <string>
#include <algorithm>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>

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
		
		typedef std::pair<intptr_t, std::string> Pair;
		
		typedef std::list<Pair> FunctionMap;
		
		static FunctionMap* function_map() {
			static FunctionMap* fm = NULL;
			if (!fm) fm = new FunctionMap;
			return fm;
		}
		
		struct function_map_cmp {
		public:
			int operator()(const Pair& a, const Pair& b) const {
				return a.first - b.first;
			}
		};
		
		FunctionMap::value_type find_function_for_ip(intptr_t ip) {
			fprintf(stderr, "find_function_for_ip: creating default candidate\n");
			std::pair<intptr_t, std::string> candidate(-1, "<no function>");
			for (FunctionMap::const_iterator it = function_map()->begin(); it != function_map()->end(); ++it) {
				fprintf(stderr, "Looking at function: %s / %lx\n", it->second.c_str(), it->first);
				//if (it->first > ip) return candidate;
				candidate = *it;
			}
			return FunctionMap::value_type(-1, NULL);
		}
		
		class SnowJITEventListener : public llvm::JITEventListener {
		public:
			void NotifyFunctionEmitted(const Function& f, void* code, size_t code_size, const JITEvent_EmittedFunctionDetails& details) {
				//fprintf(stderr, "NotifyFunctionEmitted() at %p\n", code);
				const MachineFunction* mf = details.MF;
				function_map()->push_back(Pair((intptr_t)code, f.getNameStr()));
			}
			void NotifyFreeingMachineCode(void* old_ptr) {
				//fprintf(stderr, "NotifyFreeingMachineCode()\n");
				//fprintf(stderr, "Freeing machine code: %p\n", old_ptr);
				for (FunctionMap::iterator it = function_map()->begin(); it != function_map()->end(); ++it) {
					if (it->first == (intptr_t)old_ptr) {
						function_map()->erase(it);
						break;
					}
				}
			}
		};
		
		llvm::JITEventListener* jit_event_listener() {
			static SnowJITEventListener* el = NULL;
			if (!el) el = new SnowJITEventListener;
			return el;
		}
		
		void print_callsite(intptr_t ip) {
			fprintf(stderr, "finding callsite for rip 0x%lx\n", ip);
			FunctionMap::value_type offset_and_function = find_function_for_ip(ip);
			fprintf(stderr, "found at 0x%lx\n", offset_and_function.first);
			if (offset_and_function.first > 0) {
				const std::string& name = offset_and_function.second;
				intptr_t body_offset = ip - offset_and_function.first;
				fprintf(stderr, "  (0x%lx) %s + 0x%lx\n", offset_and_function.first, name.c_str(), body_offset);
			} else {
				fprintf(stderr, "  (0x%lx)\n", ip);
			}
		}
		
		void print_backtrace(ucontext_t* context) {
			fprintf(stderr, "BACKTRACE:\n");
			void* trace[50];
			size_t n = backtrace(trace, 50);
			for (size_t i = 0; i < n; ++i) {
				print_callsite((intptr_t)trace[i]);
			}
		}
		
		void handle_signal(int sig, siginfo_t* si, void* ctx) {
			static bool handling_signal = false;
			
			if (handling_signal) {
				fprintf(stderr, "Another signal caught while handling a signal! Aborting.\n");
				_Exit(1);
			}
			
			handling_signal = true;
			printf("HANDLING SIGNAL!\n");
			
			switch (sig) {
				case SIGTRAP:
				{
					fprintf(stderr, "TRAP CAUGHT! OMG!\n");
					break;
				}
				case SIGABRT:
				{
					fprintf(stderr, "ABORT! OMG!\n");
					break;
				}
				case SIGBUS:
				{
					fprintf(stderr, "BUS ERROR! OMG!\n");
					break;
				}
				case SIGSEGV:
				{
					fprintf(stderr, "SEGMENTATION FAULT! OMG!\n");
					break;
				}
				default: {
					fprintf(stderr, "UKNOWN SIGNAL! OMG!\n");
					break;
				}
			}
			
			ucontext_t* context = reinterpret_cast<ucontext_t*>(ctx);
			
			//function_map()->sort();
			
			print_callsite(context->uc_mcontext->__ss.__rip);
			
			//print_backtrace(context);
			
			handling_signal = false;
			_Exit(1);
		}
		
		void install_signal_handlers(SnVM* vm) {
			_vm = vm;
			struct sigaction sa;
			sigemptyset(&sa.sa_mask);
			sigaddset(&sa.sa_mask, SIGTRAP);
			sigaddset(&sa.sa_mask, SIGABRT);
			sigaddset(&sa.sa_mask, SIGBUS);
			sigaddset(&sa.sa_mask, SIGSEGV);
			sigaddset(&sa.sa_mask, SIGILL);
			sa.sa_flags = SA_64REGSET | SA_SIGINFO | SA_NODEFER;
			sa.sa_sigaction = handle_signal;
			sigaction(SIGTRAP, &sa, NULL);
			sigaction(SIGABRT, &sa, NULL);
			sigaction(SIGBUS, &sa, NULL);
			sigaction(SIGSEGV, &sa, NULL);
			sigaction(SIGILL, &sa, NULL);
		}
	}
}