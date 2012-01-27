#pragma once
#ifndef ASM_HPP_MY11KBZ
#define ASM_HPP_MY11KBZ

#include "snow/basic.h"
#include "../linkheap.hpp"

#include <vector>
#include <list>

namespace snow {
	enum RM_MODE {
		RM_ADDRESS        = 0,
		RM_ADDRESS_DISP8  = 1,
		RM_ADDRESS_DISP32 = 2,
		RM_REGISTER       = 3,
		RM_INVALID
	};
	
	enum REX_FLAGS {
		NO_REX               = 0,
		REX_EXTEND_RM        = 1,
		REX_EXTEND_SIB_INDEX = 2,
		REX_EXTEND_REG       = 4,
		REX_WIDE_OPERAND     = 8
	};
	static const byte REX_BASE = 0x40;
	
	enum COND {
		CC_OVERFLOW = 0,
		CC_NOT_OVERFLOW = 1,
		CC_BELOW = 2,
		CC_CARRY = 2,
		CC_NOT_BELOW = 3,
		CC_NOT_CARRY = 3,
		CC_ZERO = 4,
		CC_EQUAL = 4,
		CC_NOT_ZERO = 5,
		CC_NOT_EQUAL = 5,
		CC_NOT_ABOVE = 6,
		CC_ABOVE = 7,
		CC_SIGN = 8,
		CC_NOT_SIGN = 9,
		CC_PARITY_EVEN = 0xA,
		CC_PARITY_ODD = 0xB,
		CC_LESS = 0xC,
		CC_GREATER_EQUAL = 0xD,
		CC_LESS_EQUAL = 0xE,
		CC_GREATER = 0xF
	};
	
	enum SIB_SCALE {
		SIB_SCALE_INVALID = -1,
		SIB_SCALE_1    = 0,
		SIB_SCALE_2    = 1,
		SIB_SCALE_4    = 2,
		SIB_SCALE_8    = 3,
	};
	
	struct Register {
		uint8_t reg;
		bool ext;
		
		bool operator==(const Register& other) const { return reg == other.reg && ext == other.ext; }
		bool operator!=(const Register& other) const { return !(*this == other); }
		bool is_valid() const { return reg != 0xff; }
	};
	
	struct Label {
		ssize_t offset;
		Label(ssize_t offset = -1) : offset(offset) {}
		bool is_bound() const { return offset >= 0; }
	};
	
	enum OperandType {
		OP_INVALID = -1,
		OP_REGISTER,
		OP_ADDRESS,
		OP_SIB,
		OP_RIP_RELATIVE,
		OP_GLOBAL,
	};
	
	struct Operand {
		int8_t type;
		union {
			Register reg;

			struct {
				Register reg;
				int32_t disp;
				uint8_t disp_size;
			} address;

			struct {
				Register index;
				Register base;
				SIB_SCALE scale;
				int32_t disp;
				uint8_t disp_size;
			} sib;
			
			struct {
				const Label* label;
				int32_t adjust;
			} rip_relative;
			
			struct {
				int32_t idx;
			} global;
		};
		
		Operand() : type(OP_INVALID) {}
		Operand(const Register& reg) : type(OP_REGISTER) {
			this->reg = reg;
		}
		explicit Operand(const Label* label, int32_t adjust = 0) : type(OP_RIP_RELATIVE) {
			rip_relative.label = label;
			rip_relative.adjust = adjust;
		}
		Operand(const Operand& other) {
			memcpy(this, &other, sizeof(*this));
		}
		Operand& operator=(const Operand& other) {
			memcpy(this, &other, sizeof(*this));
			return *this;
		}
		
		bool operator==(const Operand& other) const {
			if (type == other.type) {
				switch (type) {
					case OP_INVALID: return true;
					case OP_REGISTER: return reg == other.reg;
					case OP_ADDRESS: return address.reg == other.address.reg && address.disp == other.address.disp && address.disp_size == other.address.disp_size;
					case OP_SIB: return sib.index == other.sib.index && sib.base == other.sib.base && sib.scale == other.sib.scale;
					case OP_RIP_RELATIVE: return rip_relative.label == other.rip_relative.label;
					case OP_GLOBAL: return global.idx == other.global.idx;
				}
			}
			return false;
		}
		
		bool is_valid() const { return type != OP_INVALID; }
		bool is_address() const { return type == OP_ADDRESS; }
		bool is_memory() const { return (type == OP_ADDRESS) || (type == OP_SIB) || (type == OP_RIP_RELATIVE) || (type == OP_GLOBAL); }
	};
	
	struct LabelReference {
		size_t disp_offset;
		const Label* label;
		int32_t adjust;
		LabelReference(size_t disp_offset, const Label* label, int32_t adjust = 0) : disp_offset(disp_offset), label(label), adjust(adjust) {}
	};
	
	struct LabelLabelReference {
		// references to labels that reference other labels (!!)
		size_t mov_imm_offset;   // where the reference is
		const Label* jmp_target; // what the data should be
		const Label* jmp_disp;
		LabelLabelReference(size_t imm_offset, const Label* jmp_target, const Label* jmp_disp)
			: mov_imm_offset(imm_offset),
			jmp_target(jmp_target),
			jmp_disp(jmp_disp)
			{}
	};
	
	static const Register INVALID_REGISTER = {0xff, 0};
	static const Register RAX = {0, 0};
	static const Register RCX = {1, 0};
	static const Register RDX = {2, 0};
	static const Register RBX = {3, 0};
	static const Register RSP = {4, 0};
	static const Register RBP = {5, 0};
	static const Register RSI = {6, 0};
	static const Register RDI = {7, 0};
	static const Register R8  = {0, 1};
	static const Register R9  = {1, 1};
	static const Register R10 = {2, 1};
	static const Register R11 = {3, 1};
	static const Register R12 = {4, 1};
	static const Register R13 = {5, 1};
	static const Register R14 = {6, 1};
	static const Register R15 = {7, 1};
	
	class Asm {
	public:
		static const uint32_t PLACEHOLDER_IMM32 = UINT32_MAX;
		static const uint64_t PLACEHOLDER_IMM64 = UINT64_MAX;
		static const uint8_t PLACEHOLDER_IMM8 = UINT8_MAX;
		
		const std::vector<byte>& code() const { return _code; }
		std::vector<byte>& code() { return _code; }
		void link_labels();
		void link_globals(size_t code_offset, size_t globals_offset = 0);
		void clear();
		Label* label();
		void bind_label(Label* label) const;
		Label* label_at(size_t offset);
		void bind_label_at(Label* label, size_t offset) const;
		Operand address(const Register& reg, int32_t disp = 0) const;
		Operand sib(SIB_SCALE scale, const Register& index, const Register& base, int32_t disp = 0) const;
		Operand sib(const Register& base, int32_t disp = 0) const;
		Operand global(int32_t idx) const;
		
		size_t addl(uint32_t immediate, const Operand& target);
		size_t addq(uint32_t immediate, const Operand& target);
		void addl(const Operand& source, const Operand& target);
		void addq(const Operand& source, const Operand& target);
		size_t subl(uint32_t immediate, const Operand& target);
		size_t subq(uint32_t immediate, const Operand& target);
		void subl(const Operand& source, const Operand& target);
		void subq(const Operand& source, const Operand& target);
		void imul_i(const Operand& reg, const Operand& rm, int32_t immediate);
		void andq(const Operand& source, const Operand& target);
		void xorq(const Operand& source, const Operand& target);
		void xorb(const Operand& source, const Operand& target);
		void orq(const Operand& source, const Operand& target);
		void orq(uint32_t immediate, const Operand& target);
		void shr(uint32_t immediate, const Operand& target);
		
		size_t movb(uint8_t immediate, const Operand& target);
		size_t movl(uint32_t immediate, const Operand& target);
		size_t movq(uint64_t immediate, const Register& target);
		void movl(const Operand& source, const Operand& target);
		void movq(const Operand& source, const Operand& target);
		void movl_label_to_jmp(const Label* new_jmp_target, const Label* jmp_disp);
		void leaq(const Operand& address, const Register& target);
		void setb(COND cc, const Operand& target);
		
		size_t pushq(uint64_t immediate);
		void pushq(const Operand& source);
		void popq(const Operand& target);
		
		size_t cmpb(uint8_t immediate, const Operand& other);
		size_t cmpl(uint32_t immediate, const Operand& other);
		size_t cmpq(uint32_t immediate, const Operand& other);
		void cmpb(const Operand& left, const Operand& right);
		void cmpl(const Operand& left, const Operand& right);
		void cmpq(const Operand& left, const Operand& right);
		void call(const Operand& target);
		size_t call(uint32_t displacement);
		size_t j(COND cc, const Label* label);
		size_t jmp(const Label* label);
		void jmp(const Operand& target);
		void ret();
		void leave();
		void int3(); // debug break
		void nop();
	private:
		void emit(byte b);
		void emit_rex(byte rex);
		size_t emit_immediate(const void* data, size_t num_bytes);
		void emit_modrm(byte mod, byte reg, byte rm);
		void emit_sib(SIB_SCALE scale, byte index, byte base);
		ssize_t emit_operands(const Operand& reg, const Operand& rm);
		void emit_instruction(byte opcode, const Operand& reg, const Operand& rm, bool wide = false);
		void emit_instruction(const char* opcode, const Operand& reg, const Operand& rm, bool wide = false);
		void choose_and_emit_instruction(byte opcode1, byte opcode2, const Operand& source, const Operand& target, bool wide = false);
		size_t emit_label_reference(const Label* label, int32_t adjust = 0);
		byte rex_for_operands(const Operand& reg, const Operand& rm) const;
		Operand opcode_ext(byte ext) const;
		size_t get_current_offset() const { return _code.size(); }

		std::vector<byte> _code;
		LinkHeap<Label> _labels;
		std::vector<LabelReference> _label_references;
		std::vector<LabelLabelReference> _label_label_references;
		struct GlobalReference { int32_t global; size_t offset; };
		std::vector<GlobalReference> _global_references;
	};
	
	inline Operand Asm::address(const Register& reg, int32_t disp) const {
		if (reg.reg == 4) {
			// RSP/R12 can only be represented in SIB form as memory operands
			return sib(reg, disp);
		} else {
			Operand op;
			op.type = OP_ADDRESS;
			op.address.reg = reg;
			op.address.disp = disp;
			if (disp) {
				if (disp < -127 || disp >= 128) {
					op.address.disp_size = 4;
				} else {
					op.address.disp_size = 1;
				}
			} else {
				op.address.disp_size = 0;
			}
			return op;
		}
	}
	
	inline Operand Asm::sib(const Register& reg, int32_t disp) const {
		Operand op;
		op.type = OP_SIB;
		op.sib.scale = SIB_SCALE_1;
		op.sib.base = reg;
		op.sib.index = RSP; // ignored index
		op.sib.disp = disp;
		if (disp < -127 || disp > 128) {
			op.sib.disp_size = 4;
		} else if (disp || reg.reg == 4) {
			op.sib.disp_size = 1;
		} else {
			op.sib.disp_size = 0;
		}
		return op;
	}
	
	/*
	// XXX: Ever needed?
	inline Operand Asm::sib(SIB_SCALE scale, const Register& index, int32_t disp) {
		ASSERT(index.reg != 4); // RBP cannot be index
		Operand op;
		op.type = OP_SIB;
		op.sib.scale = scale;
		op.sib.index = index;
		op.sib.base = RBP; // ignored base
		op.sib.disp_size = 4;
		op.sib.disp = disp;
		return op;
	}
	*/
	
	inline Operand Asm::sib(SIB_SCALE scale, const Register& index, const Register& base, int32_t disp) const {
		ASSERT(index != RSP);
		Operand op;
		op.type = OP_SIB;
		op.sib.scale = scale;
		op.sib.index = index;
		op.sib.base = base;
		if (disp < -127 || disp > 128) {
			op.sib.disp_size = 4;
		} else if (disp || base.reg == 5) {
			op.sib.disp_size = 1;
		} else {
			op.sib.disp_size = 0;
		}
		return op;
	}
	
	inline Operand Asm::global(int32_t idx) const {
		ASSERT(idx >= 0);
		Operand op;
		op.type = OP_GLOBAL;
		op.global.idx = idx;
		return op;
	}
	
	inline Operand Asm::opcode_ext(byte reg) const {
		Register r = {reg, 0};
		return Operand(r);
	}
	
	inline size_t Asm::addl(uint32_t immediate, const Operand& target) {
		emit_instruction(0x81, opcode_ext(0), target, false);
		return emit_immediate(&immediate, 4);
	}
	
	inline size_t Asm::addq(uint32_t immediate, const Operand& target) {
		emit_instruction(0x81, opcode_ext(0), target, true);
		return emit_immediate(&immediate, 4);
	}
	
	inline void Asm::addl(const Operand& source, const Operand& target) {
		choose_and_emit_instruction(0x01, 0x03, source, target, false);
	}
	
	inline void Asm::addq(const Operand& source, const Operand& target) {
		choose_and_emit_instruction(0x01, 0x03, source, target, true);
	}
	
	inline size_t Asm::subl(uint32_t immediate, const Operand& target) {
		if (immediate <= UINT8_MAX) {
			emit_instruction(0x83, opcode_ext(5), target, true);
			return emit_immediate(&immediate, 1);
		} else {
			emit_instruction(0x81, opcode_ext(5), target, false);
			return emit_immediate(&immediate, 4);
		}
	}
	
	inline size_t Asm::subq(uint32_t immediate, const Operand& target) {
		if (immediate <= UINT8_MAX) {
			emit_instruction(0x83, opcode_ext(5), target, true);
			return emit_immediate(&immediate, 1);
		} else {
			emit_instruction(0x81, opcode_ext(5), target, true);
			return emit_immediate(&immediate, 4);
		}
	}
	
	inline void Asm::subl(const Operand& source, const Operand& target) {
		choose_and_emit_instruction(0x29, 0x2b, source, target, false);
	}
	
	inline void Asm::subq(const Operand& source, const Operand& target) {
		choose_and_emit_instruction(0x29, 0x2b, source, target, true);
	}
	
	inline void Asm::andq(const Operand& source, const Operand& target) {
		choose_and_emit_instruction(0x21, 0x23, source, target, true);
	}
	
	inline void Asm::xorq(const Operand& source, const Operand& target) {
		choose_and_emit_instruction(0x31, 0x33, source, target, true);
	}
	
	inline void Asm::xorb(const Operand& source, const Operand& target) {
		choose_and_emit_instruction(0x30, 0x32, source, target, false);
	}
	
	inline void Asm::orq(uint32_t mask, const Operand& target) {
		if (mask <= UINT8_MAX) {
			emit_instruction(0x83, opcode_ext(1), target, true);
			emit_immediate(&mask, 1);
		} else {
			emit_instruction(0x81, opcode_ext(1), target, true);
			emit_immediate(&mask, 4);
		}
	}
	
	inline void Asm::shr(uint32_t immediate, const Operand& target) {
		emit_instruction(0xc1, opcode_ext(5), target);
		emit_immediate(&immediate, 4);
	}
	
	inline size_t Asm::movb(uint8_t immediate, const Operand& target) {
		if (!target.is_memory()) {
			emit_rex(target.reg.ext ? REX_EXTEND_RM : NO_REX);
			emit(0xb0 + target.reg.reg);
		} else {
			emit_instruction(0xc6, opcode_ext(0), target, false);
		}
		return emit_immediate(&immediate, 1);
	}
	
	inline size_t Asm::movl(uint32_t immediate, const Operand& target) {
		if (!target.is_memory()) {
			emit_rex(target.reg.ext ? REX_EXTEND_RM : NO_REX);
			emit(0xb8 + target.reg.reg);
		} else {
			emit_instruction(0xc7, opcode_ext(0), target, false);
		}
		return emit_immediate(&immediate, 4);
	}
	
	inline size_t Asm::movq(uint64_t immediate, const Register& target) {
		emit_rex((target.ext ? REX_EXTEND_RM : NO_REX) | REX_WIDE_OPERAND);
		emit(0xb8 + target.reg);
		return emit_immediate(&immediate, 8);
	}
	
	inline void Asm::movl_label_to_jmp(const Label* new_jmp_target, const Label* jmp_disp) {
		emit_instruction(0xc7, opcode_ext(0), Operand(jmp_disp, -4), false);
		static const int32_t zero = 0;
		size_t imm_offset = emit_immediate(&zero, 4);
		_label_label_references.push_back(LabelLabelReference(imm_offset, new_jmp_target, jmp_disp));
	}
	
	inline void Asm::movl(const Operand& source, const Operand& target) {
		if (source == target) return;
		choose_and_emit_instruction(0x89, 0x8b, source, target, false);
	}
	
	inline void Asm::movq(const Operand& source, const Operand& target) {
		if (source == target) return;
		choose_and_emit_instruction(0x89, 0x8b, source, target, true);
	}
	
	inline void Asm::leaq(const Operand& address, const Register& target) {
		ASSERT(address.is_memory());
		if (address.is_address() && address.address.disp == 0) {
			movq(address.address.reg, target);
		} else {
			emit_instruction(0x8d, target, address, true);
		}
	}
	
	inline void Asm::setb(COND cc, const Operand& target) {
		auto reg = opcode_ext(0);
		byte rex = rex_for_operands(reg, target);
		emit_rex(rex);
		emit(0x0f);
		emit(0x90 + cc);
		emit_operands(reg, target);
	}
	
	inline size_t Asm::pushq(uint64_t immediate) {
		emit(0x68);
		return emit_immediate(&immediate, 8);
	}
	
	inline void Asm::pushq(const Operand& source) {
		if (source.is_memory()) {
			emit_instruction(0xff, opcode_ext(6), source, false);
		} else {
			emit_rex(source.reg.ext ? REX_EXTEND_RM : NO_REX); // push doesn't need REX.W in 64-bit mode
			emit(0x50 + source.reg.reg);
		}
	}
	
	inline void Asm::popq(const Operand& target) {
		if (target.is_memory()) {
			emit_instruction(0x8f, opcode_ext(0), target, false);
		} else {
			emit_rex(target.reg.ext ? REX_EXTEND_RM : NO_REX); // pop doesn't need REX.W in 64-bit mode
			emit(0x58 + target.reg.reg);
		}
	}
	
	inline size_t Asm::cmpb(uint8_t immediate, const Operand& source) {
		emit_instruction(0x80, opcode_ext(7), source, false);
		return emit_immediate(&immediate, 1);
	}
	
	inline size_t Asm::cmpl(uint32_t immediate, const Operand& source) {
		emit_instruction(0x81, opcode_ext(7), source, false);
		return emit_immediate(&immediate, 4);
	}
	
	inline size_t Asm::cmpq(uint32_t immediate, const Operand& source) {
		emit_instruction(0x81, opcode_ext(7), source, true);
		return emit_immediate(&immediate, 4);
	}
	
	inline void Asm::cmpb(const Operand& left, const Operand& right) {
		choose_and_emit_instruction(0x38, 0x3a, left, right, false);
	}
	
	inline void Asm::cmpl(const Operand& left, const Operand& right) {
		choose_and_emit_instruction(0x39, 0x3b, left, right, false);
	}
	
	inline void Asm::cmpq(const Operand& left, const Operand& right) {
		choose_and_emit_instruction(0x39, 0x3b, left, right, true);
	}
	
	inline void Asm::call(const Operand& target) {
		emit_instruction(0xff, opcode_ext(2), target, true);
	}
	
	inline size_t Asm::call(uint32_t displacement) {
		emit(0xe8);
		return emit_immediate(&displacement, 4);
	}
	
	inline size_t Asm::j(COND cc, const Label* label) {
		emit(0x0f);
		emit(0x80 + cc);
		return emit_label_reference(label);
	}
	
	inline size_t Asm::jmp(const Label* label) {
		emit(0xe9);
		return emit_label_reference(label);
	}
	
	inline void Asm::jmp(const Operand& target) {
		emit_instruction(0xff, opcode_ext(4), target, true);
	}
	
	inline void Asm::ret() { emit(0xc3); }
	inline void Asm::leave() { emit(0xc9); }
	inline void Asm::int3() { emit(0xcc); }
	inline void Asm::nop() { emit(0x90); }
	
	inline void Asm::emit(byte b) { _code.push_back(b); }
	inline void Asm::emit_rex(byte rex) { if (rex) emit(REX_BASE | rex); }
	inline size_t Asm::emit_immediate(const void* data, size_t num_bytes) {
		size_t offset = get_current_offset();
		const byte* d = (const byte*)data;
		for (size_t i = 0; i < num_bytes; ++i) {
			emit(d[i]);
		}
		return offset;
	}
	
	inline byte Asm::rex_for_operands(const Operand& reg, const Operand& rm) const {
		ASSERT(!reg.is_memory());
		byte flags = NO_REX;
		if (reg.reg.ext) flags |= REX_EXTEND_REG;
		switch (rm.type) {
			case OP_REGISTER: {
				if (rm.reg.ext) flags |= REX_EXTEND_RM;
				break;
			} 
			case OP_ADDRESS: {
				if (rm.address.reg.ext) flags |= REX_EXTEND_RM;
				break;
			}
			case OP_SIB: {
				if (rm.sib.base.ext) flags |= REX_EXTEND_RM;
				if (rm.sib.index.ext) flags |= REX_EXTEND_SIB_INDEX;
				break;
			}
			case OP_RIP_RELATIVE:
			default:
				break;
		}
		return flags;
	}
	
	inline void Asm::emit_modrm(byte mod, byte reg, byte rm) {
		// masking to avoid any spill
		byte mod_shifted = (mod << 6) & 0xc0;
		byte reg_shifted = (reg << 3) & 0x38;
		byte rm_masked = rm & 0x07;
		emit(mod_shifted | reg_shifted | rm_masked);
	}
	
	inline void Asm::emit_sib(SIB_SCALE scale, byte index, byte base) {
		byte scale_shifted = ((uint8_t)scale << 6) & 0xc0;
		byte index_shifted = (index << 3) & 0x38;
		byte base_masked = base & 0x07;
		emit(scale_shifted | index_shifted | base_masked);
	}
	
	inline ssize_t Asm::emit_operands(const Operand& reg, const Operand& rm) {
		ASSERT(!reg.is_memory());
		
		RM_MODE mod;
		switch (rm.type) {
			case OP_REGISTER: {
				mod = RM_REGISTER;
				emit_modrm(mod, reg.reg.reg, rm.reg.reg);
				break;
			}
			case OP_ADDRESS: {
				switch (rm.address.disp_size) {
					case 0: mod = RM_ADDRESS; break;
					case 1: mod = RM_ADDRESS_DISP8; break;
					case 4: mod = RM_ADDRESS_DISP32; break;
					default: ASSERT(false && "Invalid displacement size!"); break;
				}
				
				bool force_disp8 = false;
				if (mod == RM_ADDRESS && rm.address.reg.reg == 5) {
					// RBP and R13 need special treatment
					mod = RM_ADDRESS_DISP8;
					force_disp8 = true;
				}
				emit_modrm(mod, reg.reg.reg, rm.address.reg.reg);
				return emit_immediate(&rm.address.disp, force_disp8 ? 1 : rm.address.disp_size);
			}
			case OP_SIB: {
				switch (rm.sib.disp_size) {
					case 0: mod = RM_ADDRESS; break;
					case 1: mod = RM_ADDRESS_DISP8; break;
					case 4: mod = RM_ADDRESS_DISP32; break;
					default: ASSERT(false && "Invalid displacement size!"); break;
				}
				
				bool force_disp8 = false;
				if (mod == RM_ADDRESS && rm.sib.base == R12) {
					// R12 needs special treatment
					mod = RM_ADDRESS_DISP8;
					force_disp8 = true;
				}
				
				if (mod == RM_ADDRESS_DISP32 && rm.sib.base.reg == 5) {
					mod = RM_ADDRESS;
				}
				
				emit_modrm(mod, reg.reg.reg, 4);
				emit_sib(rm.sib.scale, rm.sib.index.reg, rm.sib.base.reg);
				return emit_immediate(&rm.sib.disp, force_disp8 ? 1 : rm.sib.disp_size);
			}
			case OP_RIP_RELATIVE: {
				mod = RM_ADDRESS;
				emit_modrm(mod, reg.reg.reg, 5);
				return emit_label_reference(rm.rip_relative.label, rm.rip_relative.adjust);
			}
			case OP_GLOBAL: {
				mod = RM_ADDRESS;
				emit_modrm(mod, reg.reg.reg, 5);
				static const int32_t zero = 0;
				size_t offset = emit_immediate(&zero, 4);
				GlobalReference ref;
				ref.global = rm.global.idx;
				ref.offset = offset;
				_global_references.push_back(ref);
				return offset;
			}
			default: {
				ASSERT(false && "Invalid operand type!");
			}
		}
		return -1;
	}
	
	inline void Asm::emit_instruction(byte opcode, const Operand& reg, const Operand& rm, bool wide) {
		byte rex = rex_for_operands(reg, rm);
		if (wide) rex |= REX_WIDE_OPERAND;
		emit_rex(rex);
		emit(opcode);
		emit_operands(reg, rm);
	}
	
	inline void Asm::choose_and_emit_instruction(byte opcode1, byte opcode2, const Operand& source, const Operand& target, bool wide) {
		ASSERT(!(source.is_memory() && target.is_memory()));
		if (source.is_memory()) {
			emit_instruction(opcode2, target, source, wide);
		} else {
			emit_instruction(opcode1, source, target, wide);
		}
	}
	
	inline Label* Asm::label() {
		return _labels.alloc();
	}
	
	inline Label* Asm::label_at(size_t offset) {
		Label* l = label();
		l->offset = offset;
		return l;
	}
	
	inline void Asm::bind_label(Label* label) const {
		label->offset = get_current_offset();
	}
	
	inline void Asm::bind_label_at(Label* label, size_t offset) const {
		label->offset = offset;
	}
	
	inline size_t Asm::emit_label_reference(const Label* label, int32_t adjust) {
		if (label->is_bound()) {
			int32_t diff = (int32_t)((ssize_t)label->offset - (get_current_offset() + 4) + adjust);
			return emit_immediate(&diff, 4);
		} else {
			static const int32_t zero = 0;
			size_t disp_offset = emit_immediate(&zero, 4);
			_label_references.push_back(LabelReference(disp_offset, label, adjust));
			return disp_offset;
		}
	}
	
	inline void Asm::link_labels() {
		// link remaining labelreferences
		for (std::vector<LabelReference>::iterator it = _label_references.begin(); it != _label_references.end(); ++it) {
			LabelReference* ref = &*it;
			ASSERT(ref->label->is_bound() && "Could not link an unbound label!");
			int32_t diff = (int32_t)((ssize_t)ref->label->offset - (ref->disp_offset + 4 + ref->adjust));
			for (size_t i = 0; i < 4; ++i) {
				_code[ref->disp_offset+i] = reinterpret_cast<byte*>(&diff)[i];
			}
		}
		_label_references.clear();
		
		// link labellabelreferences
		for (std::vector<LabelLabelReference>::iterator it = _label_label_references.begin(); it != _label_label_references.end(); ++it) {
			LabelLabelReference* ref = &*it;
			ASSERT(ref->jmp_disp->is_bound() && "Could not link an unbound label!");
			ASSERT(ref->jmp_target->is_bound() && "Could not link an unbound label!");
			int32_t diff = (int32_t)((ssize_t)ref->jmp_target->offset - ((ssize_t)ref->jmp_disp->offset + 4));
			for (size_t i = 0; i < 4; ++i) {
				_code[ref->mov_imm_offset+i] = reinterpret_cast<byte*>(&diff)[i];
			}
		}
		_label_label_references.clear();
	}
	
	inline void Asm::link_globals(size_t code_offset, size_t globals_offset) {
		for (size_t i = 0; i < _global_references.size(); ++i) {
			ssize_t g = _global_references[i].global * sizeof(VALUE) + globals_offset;
			ssize_t o = _global_references[i].offset;
			int32_t diff = g - (code_offset + o + 4);
			for (size_t j = 0; j < 4; ++j) {
				_code[o + j] = reinterpret_cast<byte*>(&diff)[j];
			}
		}
	}
	
	inline void Asm::clear() {
		_code.clear();
		_labels.clear();
		_label_references.clear();
	}
}

#endif /* end of include guard: ASM_HPP_MY11KBZ */
