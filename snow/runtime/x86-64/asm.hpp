#pragma once
#ifndef ASM_HPP_MY11KBZ
#define ASM_HPP_MY11KBZ

#include "snow/basic.h"
#include "../linkheap.hpp"
#include "../codebuffer.hpp"

#include <vector>
#include <list>

namespace snow {
	namespace x86_64 {
		enum RmMode {
			RM_ADDRESS    = 0,
			RM_ADDRESS_DISP8 = 1,
			RM_ADDRESS_DISP32 = 2,
			RM_REGISTER = 3,
			RM_INVALID
		};
		
		enum RexFlags {
			REX_NONE = 0,
			REX_EXTEND_RM = 1,
			REX_EXTEND_SIB_INDEX = 2,
			REX_EXTEND_REG = 4,
			REX_WIDE_OPERAND = 8
		};
		
		static const byte REX_BASE = 0x40;
		
		enum Condition {
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
		
		enum SibScale : char {
			SibScale_INVALID = -1,
			SibScale_1 = 0,
			SibScale_2 = 1,
			SibScale_4 = 2,
			SibScale_8 = 3
		};
		
		struct Register {
			unsigned reg : 7;
			unsigned ext : 1;
			
			static const byte INVALID = 0x7f;
			bool is_valid() const { return reg != INVALID; }
			bool operator==(Register other) const { return reg == other.reg && ext == other.ext; }
			bool operator!=(Register other) const { return !(*this == other); }
		};
		
		struct Label {
			const char* name;
			ssize_t offset;
			Label(const char* name, ssize_t offset = -1) : name(name), offset(offset) {}
			bool is_bound() const { return offset >= 0; }
		};
		
		enum OperandType : char {
			OP_INVALID = -1,
			OP_REGISTER,
			OP_ADDRESS,
			OP_SIB,
			OP_RIP_RELATIVE,
		};
		
		struct SIB {
			Register index;
			Register base;
			SibScale scale;
			uint8_t disp_size;
			int32_t disp;
			
			bool operator==(const SIB& other) const {
				return index == other.index &&
				       base == other.base &&
				       scale == other.scale &&
				       disp_size == other.disp_size &&
				       disp == other.disp;
			}
		};
		
		struct Operand {
			OperandType type;
			union {
				Register reg;
				
				struct {
					Register reg;
					uint8_t disp_size;
					int32_t disp;
				} address;
				
				SIB sib;
					
				/*struct { // unused?
					const Label* label;
					int32_t adjust;
				} rip_relative;*/
			};
			
		
			Operand() : type(OP_INVALID) {}
			Operand(Register reg) : type(OP_REGISTER) {
				this->reg = reg;
			}
			/*explicit Operand(const Label* label, int32_t adjust = 0) : type(OP_RIP_RELATIVE) {
				rip_relative.label = label;
				rip_relative.adjust = adjust;
			}*/
			Operand(const Operand& other) {
				memcpy(this, &other, sizeof(*this));
			}
			Operand& operator=(Operand other) {
				memcpy(this, &other, sizeof(*this));
				return *this;
			}
		
			bool operator==(Operand other) const {
				if (type == other.type) {
					switch (type) {
						case OP_INVALID: return true;
						case OP_REGISTER: return reg == other.reg;
						case OP_ADDRESS: return address.reg == other.address.reg && address.disp == other.address.disp && address.disp_size == other.address.disp_size;
						case OP_SIB: return sib == other.sib;
						//case OP_RIP_RELATIVE: return rip_relative.label == other.rip_relative.label;
						default: break;
					}
				}
				return false;
			}
		
			bool is_valid() const { return type != OP_INVALID; }
			bool is_address() const { return type == OP_ADDRESS; }
			bool is_memory() const { return (type == OP_ADDRESS) || (type == OP_SIB) || (type == OP_RIP_RELATIVE); }
		};
		
		struct LabelReference {
			CodeBuffer::Fixup& fixup;
			const Label& label;
			LabelReference(CodeBuffer::Fixup& fixup, const Label& label) : fixup(fixup), label(label) {}
			
			void bind() {
				ASSERT(label.is_bound());
				fixup.value = label.offset;
			}
		};
		
		static const Register INVALID_REGISTER = {Register::INVALID, 0};
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
		static const Register RIP = {8, 0}; // invalid for regular operations
		
		class Asm : public CodeBuffer {
		public:
			size_t get_current_offset() const { return size(); }
			
			Label& declare_label(const char* name = NULL);
			void label(Label&);
			Operand address(Register reg, int32_t disp = 0) const;
			Operand address_disp8(Register reg) const;
			Operand address_disp32(Register reg) const;
			Operand sib(Register base, int32_t disp = 0) const;
			Operand sib(SibScale scale, Register index, Register base, int32_t disp = 0) const;
			
			void addl(uint32_t immediate, Operand target);
			void addl(Operand source, Operand target);
			void addq(uint32_t immediate, Operand target);
			void addq(Operand source, Operand target);
			Fixup& addq(Operand target);
			void subl(uint32_t immediate, Operand target);
			void subl(Operand source, Operand target);
			void subq(uint32_t immediate, Operand target);
			void subq(Operand source, Operand target);
			Fixup& subq(Operand target, bool single_byte = false);
			void andq(Operand source, Operand target);
			void xorq(Operand source, Operand target);
			void xorb(Operand source, Operand target);
			void orq(Operand source, Operand target);
			void orq(uint32_t immediate, Operand target);
			void shr(uint32_t immediate, Operand target);
			
			void movb(uint8_t imm, Operand target);
			void movl(uint32_t imm, Operand target);
			void movl(int32_t imm, Operand target);
			void movq(uint32_t imm, Operand target);
			void movq(uintptr_t imm, Register target);
			void movq(uint64_t imm, Register target);
			Fixup& movq(Register target);
			void movl(Operand source, Operand target);
			void movq(Operand source, Operand target);
			void leaq(Operand address, Register target);
			void setb(Condition cc, Operand target);
			
			void pushq(uint64_t immediate);
			Fixup& pushq();
			void pushq(Operand source);
			void popq(Operand target);
			
			void cmpb(uint8_t imm, Operand other);
			void cmpl(uint32_t imm, Operand other);
			void cmpq(uint32_t imm, Operand other);
			void cmpb(Operand left, Operand right);
			void cmpl(Operand left, Operand right);
			void cmpq(Operand left, Operand right);
			
			void call(Operand target);
			void call(void* function);
			void j(Condition cc, const Label& label);
			void jmp(const Label& label);
			void jmp(Operand target);
			void ret();
			void leave();
			void int3();
			void nop();
		private:
			std::list<Label> labels;
			std::list<LabelReference> label_references;
			
			void emit(byte b) { emit_u8(b); }
			void emit_rex(byte b) { if ((b | REX_BASE) != REX_BASE) emit(b | REX_BASE); }
			void emit_imm8(uint64_t imm)  { emit_u8(imm); }
			void emit_imm32(uint64_t imm) { emit_u32(imm); }
			void emit_imm64(uint64_t imm) { emit_u64(imm); }
			Fixup& emit_imm8() { return emit_u8(); }
			Fixup& emit_imm32() { return emit_u32(); }
			Fixup& emit_imm64() { return emit_u64(); }
			
			void emit_modrm(byte mod, byte reg, byte rm);
			void emit_sib(SibScale scale, byte index, byte base);
			ssize_t emit_operands(Register reg, Operand rm);
			void emit_instruction(byte opcode, Register reg, Operand rm, bool wide = false);
			void emit_instruction(const char* opcode, Register reg, Operand rm, bool wide = false);
			void choose_and_emit_instruction(byte reg_to_rm, byte rm_to_reg, Operand source, Operand target, bool wide = false);
			void emit_label_reference(const Label& label, int32_t adjust = 0);
			byte rex_for_operands(Register reg, Operand rm) const;
			Register opcode_ext(byte ext) const;
			void bind_label_references();
		};
		
		inline Operand Asm::address(Register reg, int32_t disp) const {
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
		
		inline Operand Asm::sib(Register reg, int32_t disp) const {
			ASSERT(reg != RSP);
			Operand op;
			op.type = OP_SIB;
			op.sib.scale = SibScale_1;
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
		
		inline Operand Asm::sib(SibScale scale, Register index, Register base, int32_t disp) const {
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
		
		inline Register Asm::opcode_ext(byte reg) const {
			Register r = {reg, 0};
			return r;
		}
	
		inline void Asm::addl(uint32_t immediate, Operand target) {
			emit_instruction(0x81, opcode_ext(0), target, false);
			emit_imm32(immediate);
		}
	
		inline void Asm::addq(uint32_t immediate, Operand target) {
			emit_instruction(0x81, opcode_ext(0), target, true);
			emit_imm32(immediate);
		}
	
		inline void Asm::addl(Operand source, Operand target) {
			choose_and_emit_instruction(0x01, 0x03, source, target, false);
		}
	
		inline void Asm::addq(Operand source, Operand target) {
			choose_and_emit_instruction(0x01, 0x03, source, target, true);
		}
	
		inline void Asm::subl(uint32_t immediate, Operand target) {
			if (immediate <= UINT8_MAX) {
				emit_instruction(0x83, opcode_ext(5), target, false);
				emit_imm8((uint8_t)immediate);
			} else {
				emit_instruction(0x81, opcode_ext(5), target, false);
				emit_imm32(immediate);
			}
		}
	
		inline void Asm::subq(uint32_t immediate, Operand target) {
			if (immediate <= UINT8_MAX) {
				emit_instruction(0x83, opcode_ext(5), target, true);
				emit_imm8(immediate);
			} else {
				emit_instruction(0x81, opcode_ext(5), target, true);
				emit_imm32(immediate);
			}
		}
	
		inline void Asm::subl(Operand source, Operand target) {
			choose_and_emit_instruction(0x29, 0x2b, source, target, false);
		}
	
		inline void Asm::subq(Operand source, Operand target) {
			choose_and_emit_instruction(0x29, 0x2b, source, target, true);
		}
		
		inline CodeBuffer::Fixup& Asm::subq(Operand target, bool single_byte) {
			if (single_byte) {
				emit_instruction(0x83, opcode_ext(5), target, true);
				return emit_imm8();
			} else {
				emit_instruction(0x81, opcode_ext(5), target, true);
				return emit_imm32();
			}
		}
	
		inline void Asm::andq(Operand source, Operand target) {
			choose_and_emit_instruction(0x21, 0x23, source, target, true);
		}
	
		inline void Asm::xorq(Operand source, Operand target) {
			choose_and_emit_instruction(0x31, 0x33, source, target, true);
		}
	
		inline void Asm::xorb(Operand source, Operand target) {
			choose_and_emit_instruction(0x30, 0x32, source, target, false);
		}
	
		inline void Asm::orq(uint32_t mask, Operand target) {
			if (mask <= UINT8_MAX) {
				emit_instruction(0x83, opcode_ext(1), target, true);
				emit_imm8(mask);
			} else {
				emit_instruction(0x81, opcode_ext(1), target, true);
				emit_imm32(mask);
			}
		}
	
		inline void Asm::shr(uint32_t immediate, Operand target) {
			emit_instruction(0xc1, opcode_ext(5), target);
			emit_imm32(immediate);
		}
	
		inline void Asm::movb(uint8_t immediate, Operand target) {
			if (!target.is_memory()) {
				emit_rex(target.reg.ext ? REX_EXTEND_RM : REX_NONE);
				emit(0xb0 + target.reg.reg);
			} else {
				emit_instruction(0xc6, opcode_ext(0), target, false);
			}
			emit_imm8(immediate);
		}
	
		inline void Asm::movl(uint32_t immediate, Operand target) {
			if (!target.is_memory()) {
				emit_rex(target.reg.ext ? REX_EXTEND_RM : REX_NONE);
				emit(0xb8 + target.reg.reg);
			} else {
				emit_instruction(0xc7, opcode_ext(0), target, false);
			}
			emit_imm32(immediate);
		}
	
		inline void Asm::movl(int32_t immediate, Operand target) {
			if (!target.is_memory()) {
				emit_rex(target.reg.ext ? REX_EXTEND_RM : REX_NONE);
				emit(0xb8 + target.reg.reg);
			} else {
				emit_instruction(0xc7, opcode_ext(0), target, false);
			}
			emit_imm32(immediate);
		}
	
		inline void Asm::movq(uint64_t immediate, Register target) {
			emit_rex((target.ext ? REX_EXTEND_RM : REX_NONE) | REX_WIDE_OPERAND);
			emit(0xb8 + target.reg);
			emit_imm64(immediate);
		}
		
		inline void Asm::movq(uintptr_t immediate, Register target) {
			movq((uint64_t)immediate, target);
		}
		
		inline CodeBuffer::Fixup& Asm::movq(Register target) {
			emit_rex((target.ext ? REX_EXTEND_RM : REX_NONE) | REX_WIDE_OPERAND);
			emit(0xb8 + target.reg);
			return emit_imm64();
		}
	
		inline void Asm::movl(Operand source, Operand target) {
			if (source == target) return;
			choose_and_emit_instruction(0x89, 0x8b, source, target, false);
		}
	
		inline void Asm::movq(Operand source, Operand target) {
			if (source == target) return;
			choose_and_emit_instruction(0x89, 0x8b, source, target, true);
		}
	
		inline void Asm::leaq(Operand address, Register target) {
			ASSERT(address.is_memory());
			if (address.is_address() && address.address.disp == 0) {
				movq(address.address.reg, target);
			} else {
				emit_instruction(0x8d, target, address, true);
			}
		}
	
		inline void Asm::setb(Condition cc, Operand target) {
			auto reg = opcode_ext(0);
			byte rex = rex_for_operands(reg, target);
			emit_rex(rex);
			emit(0x0f);
			emit(0x90 + cc);
			emit_operands(reg, target);
		}
	
		inline void Asm::pushq(uint64_t immediate) {
			emit(0x68);
			emit_imm64(immediate);
		}
	
		inline void Asm::pushq(Operand source) {
			if (source.is_memory()) {
				emit_instruction(0xff, opcode_ext(6), source, false);
			} else {
				emit_rex(source.reg.ext ? REX_EXTEND_RM : REX_NONE); // push doesn't need REX.W in 64-bit mode
				emit(0x50 + source.reg.reg);
			}
		}
	
		inline void Asm::popq(Operand target) {
			if (target.is_memory()) {
				emit_instruction(0x8f, opcode_ext(0), target, false);
			} else {
				emit_rex(target.reg.ext ? REX_EXTEND_RM : REX_NONE); // pop doesn't need REX.W in 64-bit mode
				emit(0x58 + target.reg.reg);
			}
		}
	
		inline void Asm::cmpb(uint8_t immediate, Operand source) {
			emit_instruction(0x80, opcode_ext(7), source, false);
			emit_imm8(immediate);
		}
	
		inline void Asm::cmpl(uint32_t immediate, Operand source) {
			emit_instruction(0x81, opcode_ext(7), source, false);
			emit_imm32(immediate);
		}
	
		inline void Asm::cmpq(uint32_t immediate, Operand source) {
			emit_instruction(0x81, opcode_ext(7), source, true);
			emit_imm32(immediate);
		}
	
		inline void Asm::cmpb(Operand left, Operand right) {
			choose_and_emit_instruction(0x38, 0x3a, left, right, false);
		}
	
		inline void Asm::cmpl(Operand left, Operand right) {
			choose_and_emit_instruction(0x39, 0x3b, left, right, false);
		}
	
		inline void Asm::cmpq(Operand left, Operand right) {
			choose_and_emit_instruction(0x39, 0x3b, left, right, true);
		}
		
		inline void Asm::call(void* function) {
			intptr_t f = (intptr_t)function;
			if (false /* PIC */) {
				movq((uintptr_t)f, R10);
				call(R10);
			} else {
				emit(0xe8);
				Fixup& fixup = emit_imm32();
				fixup.type = CodeBuffer::FixupOffsetToPointer;
				fixup.displacement = -4;
				fixup.value = f;
			}
		}
	
		inline void Asm::call(Operand target) {
			emit_instruction(0xff, opcode_ext(2), target, true);
		}
	
		inline void Asm::j(Condition cc, const Label& label) {
			emit(0x0f);
			emit(0x80 + cc);
			emit_label_reference(label);
		}
	
		inline void Asm::jmp(const Label& label) {
			emit(0xe9);
			emit_label_reference(label);
		}
	
		inline void Asm::jmp(Operand target) {
			emit_instruction(0xff, opcode_ext(4), target, true);
		}
	
		inline void Asm::ret() { emit(0xc3); }
		inline void Asm::leave() { emit(0xc9); }
		inline void Asm::int3() { emit(0xcc); }
		inline void Asm::nop() { emit(0x90); }
		
		inline byte Asm::rex_for_operands(Register reg, Operand rm) const {
			byte flags = REX_NONE;
			if (reg.ext) flags |= REX_EXTEND_REG;
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
	
		inline void Asm::emit_sib(SibScale scale, byte index, byte base) {
			byte scale_shifted = ((uint8_t)scale << 6) & 0xc0;
			byte index_shifted = (index << 3) & 0x38;
			byte base_masked = base & 0x07;
			emit(scale_shifted | index_shifted | base_masked);
		}
		
		inline ssize_t Asm::emit_operands(Register reg, Operand rm) {
			RmMode mod;
			switch (rm.type) {
				case OP_REGISTER: {
					mod = RM_REGISTER;
					emit_modrm(mod, reg.reg, rm.reg.reg);
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
					emit_modrm(mod, reg.reg, rm.address.reg.reg);
					ssize_t disp_offset = get_current_offset();
					if (rm.address.disp_size == 1 || force_disp8)
						emit_imm8(rm.address.disp);
					else if (rm.address.disp_size == 4)
						emit_imm32(rm.address.disp);
					else if (rm.address.disp_size == 0) {}
					else
						ASSERT(false); // invalid disp size
					return disp_offset;
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
				
					emit_modrm(mod, reg.reg, 4);
					emit_sib(rm.sib.scale, rm.sib.index.reg, rm.sib.base.reg);
					ssize_t disp_offset = get_current_offset();
					if (rm.sib.disp_size == 1 || force_disp8) {
						emit_imm8(rm.sib.disp);
					} else if (rm.sib.disp_size == 4) {
						emit_imm32(rm.sib.disp);
					} else if (rm.sib.disp_size == 0) {
					} else
						ASSERT(false); // invalid disp size
					return disp_offset;
				}
				default: {
					ASSERT(false && "Invalid operand type!");
				}
			}
			return -1;
		}
		
		inline void Asm::choose_and_emit_instruction(byte reg_to_rm, byte rm_to_reg, Operand source, Operand target, bool wide) {
			ASSERT(!(source.is_memory() && target.is_memory()));
			if (source.is_memory()) {
				emit_instruction(rm_to_reg, target.reg, source, wide);
			} else {
				emit_instruction(reg_to_rm, source.reg, target, wide);
			}
		}
		
		inline void Asm::emit_instruction(byte opcode, Register reg, Operand rm, bool wide) {
			byte rex = rex_for_operands(reg, rm);
			if (wide) rex |= REX_WIDE_OPERAND;
			emit_rex(rex);
			emit(opcode);
			emit_operands(reg, rm);
		}
		
		inline Label& Asm::declare_label(const char* name) {
			labels.emplace_back(name);
			return labels.back();
		}
		
		inline void Asm::label(Label& l) {
			l.offset = get_current_offset();
		}
		
		inline void Asm::bind_label_references() {
			for (auto it = label_references.begin(); it != label_references.end(); ++it) {
				it->bind();
			}
		}
		
		inline void Asm::emit_label_reference(const Label& label, int32_t adjust) {
			Fixup& fixup = emit_imm32();
			fixup.type = CodeBuffer::FixupRelative;
			fixup.displacement = -4 - adjust;
			label_references.emplace_back(fixup, label);
		}	
	}
}

#endif /* end of include guard: ASM_HPP_MY11KBZ */
