#include "codemanager.hpp"
#include "x86-64/codegen.hpp"
#include "x86-64/cconv.hpp"

#include <vector>
#include <memory>
#include <algorithm>
#include <sys/mman.h>
#include <libunwind.h>

namespace snow {
	CodeModule::~CodeModule() {
		if (memory) {
			munmap(memory, size);
		}
	}

	CodeModule* CodeManager::compile_ast(const ASTBase* ast, const std::string& source, const std::string& path)
	{
		x86_64::CodegenSettings settings = {
			.use_inline_cache = true,
			.perform_inlining = true,
		};
		
		x86_64::Codegen codegen(settings);
		if (codegen.compile_ast(ast)) {
			std::unique_ptr<CodeModule> mod(new CodeModule);
			mod->source_file.path = path;
			mod->source_file.source = source;
			mod->size = codegen.compiled_size();
			
			mod->memory = (byte*)mmap(NULL, mod->size, PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
			codegen.materialize_in(*mod);
			mprotect(mod->memory, mod->size, PROT_WRITE|PROT_EXEC);
			mod->entry = (const FunctionDescriptor*)(mod->memory + codegen.get_offset_for_entry_descriptor());

			CodeModule* ret = mod.get();
			_modules.push_back(std::move(mod));
			return ret;
		}
		return NULL;
	}
	
	CodeManager* CodeManager::get() {
		static CodeManager* manager = NULL;
		if (!manager) manager = new CodeManager;
		return manager;
	}
	
	bool CodeManager::find_source_location_from_instruction_pointer(void* ip, const SourceFile*& out_file, const SourceLocation*& out_location) {
		byte* p = (byte*)ip;
		for (const std::unique_ptr<CodeModule>& module: _modules) {
			byte* end = module->memory + module->size;
			if (p >= module->memory && p < end) {
				size_t offset = p - module->memory;
				
				struct CompareOffsets {
					bool operator()(const SourceLocation& a, const SourceLocation& b) {
						return a.code_offset < b.code_offset;
					}
					bool operator()(const SourceLocation& a, uint32_t offset) {
						return a.code_offset < offset;
					}
				};
				auto begin = module->locations.begin();
				auto end   = module->locations.end();
				if (begin != end) {
					auto location = std::lower_bound(begin, end, offset, CompareOffsets());
					if (location != begin) {
						// lower_bound finds the first element that is *not* less than offset, which is exactly one past the one we need.
						--location;
					}
					out_file = &module->source_file;
					out_location = &*location;
					return true;
				}
				return false;
			}
		}
		return false;
	}
	
	const CallFrame* CodeManager::find_call_frame(unw_cursor_t* cursor) {
		unw_word_t frame;
		unw_get_reg(cursor, x86_64::regnum(x86_64::REG_CALL_FRAME), &frame);
		return (const CallFrame*)frame;
	}
}