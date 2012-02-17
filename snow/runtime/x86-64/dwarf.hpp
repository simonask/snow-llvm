#pragma once
#ifndef DWARF_HPP_FZN5FJAH
#define DWARF_HPP_FZN5FJAH

#include "../internal.h"
#include "asm.hpp"

namespace snow {
	namespace dwarf {
		using namespace x86_64;
		
		static const byte DW_CIE_VERSION   = 0x1;
		static const int32_t DW_CIE_ID    = 0x0;

		enum DW_EH_PE {
			DW_EH_PE_absptr = 0x00,
			DW_EH_PE_omit = 0xff,
			DW_EH_PE_uleb128 = 0x01,
			DW_EH_PE_udata2 = 0x02,
			DW_EH_PE_udata4 = 0x03,
			DW_EH_PE_udata8 = 0x04,
			DW_EH_PE_sleb128 = 0x09,
			DW_EH_PE_sdata2 = 0x0A,
			DW_EH_PE_sdata4 = 0x0B,
			DW_EH_PE_sdata8 = 0x0C,
			DW_EH_PE_signed = 0x08,
			DW_EH_PE_pcrel = 0x10,
			DW_EH_PE_textrel = 0x20,
			DW_EH_PE_datarel = 0x30,
			DW_EH_PE_funcrel = 0x40,
			DW_EH_PE_aligned = 0x50,
			DW_EH_PE_indirect = 0x80,
		};
		
		enum DW_CFA {
			DW_CFA_extended    = 0x00,
			DW_CFA_nop         = 0x00,
			DW_CFA_advance_loc = 0x40,
			DW_CFA_offset      = 0x80,
			DW_CFA_restore     = 0xc0,
			DW_CFA_set_loc     = 0x01,
			DW_CFA_advance_loc1 = 0x02,
			DW_CFA_advance_loc2 = 0x03,
			DW_CFA_advance_loc4 = 0x04,
			DW_CFA_offset_extended = 0x05,
			DW_CFA_restore_extended = 0x06,
			DW_CFA_undefined   = 0x07,
			DW_CFA_same_value  = 0x08,
			DW_CFA_register    = 0x09,
			DW_CFA_remember_state = 0x0a,
			DW_CFA_restore_state = 0x0b,
			DW_CFA_def_cfa     = 0x0c,
			DW_CFA_def_cfa_register = 0x0d,
			DW_CFA_def_cfa_offset = 0x0e,
			DW_CFA_def_cfa_expression = 0x0f,
			DW_CFA_expression  = 0x10,
			DW_CFA_offset_extended_sf = 0x11,
			DW_CFA_def_cfa_sf  = 0x12,
			DW_CFA_def_cfa_offset_sf = 0x13,
			DW_CFA_val_offset  = 0x14,
			DW_CFA_val_offset_sf = 0x15,
			DW_CFA_val_expression = 0x16,
			DW_CFA_MIPS_advance_loc8 = 0x1d,
			DW_CFA_GNU_window_save = 0x2d,
			DW_CFA_GNU_args_size = 0x2e,
			DW_CFA_lo_user = 0x1c,
			DW_CFA_hi_user = 0x3f,
		};
		
		inline byte reg(Register r) {
			static Register regs_in_dwarf_order[] = { RAX, RDX, RCX, RBX, RSI, RDI, RBP, RSP, R8, R9, R10, R11, R12, R13, R14, R15, RIP };
			for (size_t i = 0; i < countof(regs_in_dwarf_order); ++i) {
				if (regs_in_dwarf_order[i] == r)
					return i;
			}
			ASSERT(false); // invalid register
			return 0xff;
		}
	}
}

#endif /* end of include guard: DWARF_HPP_FZN5FJAH */
