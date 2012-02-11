#pragma once
#ifndef CODEBUFFER_HPP_9C4S9TMT
#define CODEBUFFER_HPP_9C4S9TMT

#include "snow/basic.h"

#include <deque>
#include <list>

namespace snow {
	class CodeBuffer {
	public:
		typedef std::deque<byte> Storage;
		
		CodeBuffer() : max_alignment(1) {}
		size_t size() const { return buffer.size(); }
		void render_at(byte* location, size_t max_size) const;
		
		void emit_u8(byte b) { buffer.push_back(b); }
		void emit_u32(uint32_t);
		void emit_u64(uint64_t);
		void emit_uleb128(uint64_t);
		void align_to(size_t alignment, byte padding = 0x00);
		
		enum FixupType {
			FixupAbsolute,
			FixupRelative,
			FixupPointerToOffset,
			FixupOffsetToPointer,
		};
		
		struct Fixup {
			FixupType type;
			uint32_t position;
			uint32_t size;
			int32_t displacement;
			int64_t value;
			
			Fixup(FixupType type, uint32_t position, uint32_t size, int32_t displacement, ssize_t value) : type(type), position(position), size(size), displacement(displacement), value(value) {}
		};
		
		Fixup& emit_u8();
		Fixup& emit_u32();
		Fixup& emit_u64();
		Fixup& emit_relative_s32(size_t abs_offset = 0, ssize_t displacement = 0);
		Fixup& emit_relative_s64(size_t abs_offset = 0, ssize_t displacement = 0);
		Fixup& emit_pointer_to_offset(size_t offset = 0);
		Fixup& emit_offset_to_pointer_s32(uintptr_t ptr, ssize_t displacement = 0);
		Fixup& emit_offset_to_pointer_s64(uintptr_t ptr, ssize_t displacement = 0);
	private:
		size_t max_alignment; // for error checking
		Storage buffer;
		std::list<Fixup> fixups;
		
		void perform_fixups(byte* buffer) const;
	};
	
	inline void CodeBuffer::emit_u32(uint32_t n) {
		byte* p = reinterpret_cast<byte*>(&n);
		for (size_t i = 0; i < sizeof(n); ++i)
			emit_u8(p[i]);
	}
	
	inline void CodeBuffer::emit_u64(uint64_t n) {
		byte* p = reinterpret_cast<byte*>(&n);
		for (size_t i = 0; i < sizeof(n); ++i)
			emit_u8(p[i]);
	}
	
	inline void CodeBuffer::emit_uleb128(uint64_t n) {
		do {
			byte b = (byte)(n & 0x7f);
			n >>= 7;
			if (n) b |= 0x80;
			emit_u8(b);
		} while (n);
	}
	
	inline void CodeBuffer::align_to(size_t alignment, byte padding) {
		if (alignment == 0) return;
		if (alignment > max_alignment) max_alignment = alignment;
		while ((size() % alignment) != 0) emit_u8(padding);
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_u8() {
		fixups.emplace_back(FixupAbsolute, size(), sizeof(byte), 0, 0);
		emit_u8(0);
		return fixups.back();
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_u32() {
		fixups.emplace_back(FixupAbsolute, size(), sizeof(uint32_t), 0, 0);
		emit_u32(0);
		return fixups.back();
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_u64() {
		fixups.emplace_back(FixupAbsolute, size(), sizeof(uint64_t), 0, 0);
		emit_u64(0);
		return fixups.back();
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_relative_s32(size_t buffer_offset, ssize_t displacement) {
		fixups.emplace_back(FixupRelative, size(), sizeof(int32_t), displacement, buffer_offset);
		emit_u32(0);
		return fixups.back();
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_relative_s64(size_t buffer_offset, ssize_t displacement) {
		fixups.emplace_back(FixupRelative, size(), sizeof(int64_t), displacement, buffer_offset);
		emit_u64(0);
		return fixups.back();
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_pointer_to_offset(size_t offset) {
		fixups.emplace_back(FixupPointerToOffset, size(), sizeof(void*), 0, offset);
		emit_u64(0); // TODO: Fix for 32-bit?
		return fixups.back();
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_offset_to_pointer_s32(uintptr_t ptr, ssize_t disp) {
		fixups.emplace_back(FixupOffsetToPointer, size(), sizeof(int32_t), disp, (ssize_t)ptr);
		emit_u32(0);
		return fixups.back();
	}
	
	inline CodeBuffer::Fixup& CodeBuffer::emit_offset_to_pointer_s64(uintptr_t ptr, ssize_t disp) {
		fixups.emplace_back(FixupOffsetToPointer, size(), sizeof(int64_t), disp, (ssize_t)ptr);
		emit_u64(0);
		return fixups.back();
	}
}

#endif /* end of include guard: CODEBUFFER_HPP_9C4S9TMT */
