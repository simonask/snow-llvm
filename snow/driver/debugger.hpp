#pragma once
#ifndef DEBUGGER_HPP_LRQWBOEI
#define DEBUGGER_HPP_LRQWBOEI

struct SnVM;

namespace llvm { class JITEventListener; }

namespace snow {
	namespace debugger {
		void start(SnVM* vm);
		void install_signal_handlers();
		
		llvm::JITEventListener* jit_event_listener();
	}
}

#endif /* end of include guard: DEBUGGER_HPP_LRQWBOEI */
