LEVEL = ..
TARGET := driver

SOURCES := $(wildcard *.cpp *.c)
OBJECTS = $(patsubst %,%.o,$(SOURCES))

include $(LEVEL)/Makefile.rules

LLVM_LIBS := LLVMAnalysis LLVMArchive LLVMBitReader LLVMBitWriter LLVMCodeGen \
			LLVMCore LLVMExecutionEngine LLVMInstCombine LLVMInstrumentation \
			LLVMInterpreter LLVMJIT LLVMLinker LLVMMC LLVMMCParser LLVMScalarOpts LLVMSelectionDAG \
			LLVMSupport LLVMSystem LLVMTarget LLVMTransformUtils LTO 
LLVM_PLATFORM_LIBS := LLVMX86CodeGen LLVMX86AsmPrinter LLVMX86Disassembler LLVMX86Info
LLVM_LIBS := $(LLVM_LIBS) $(LLVM_PLATFORM_LIBS)
LLVM_LIBS := $(patsubst %,$(LLVM_LIB_PATH)/lib%.a,$(LLVM_LIBS))
LLVM_LIBS := $(wildcard $(LLVM_LIB_PATH)/*.a)

driver: $(OBJECTS) $(LLVM_LIBS)
	$(LD) $(LDFLAGS) $(OBJECTS) $(LLVM_LIBS) -o $@
