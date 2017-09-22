// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <cstddef>

//TODO factor out CommonTypes.h
#include "mednafen/mednafen-types.h"
#include "jit/Common/Opcode.h"
#include "mednafen/state.h"
#include "mednafen/mednafen.h"
#include "libretro.h"

extern retro_log_printf_t log_cb;

typedef Memory::Opcode MIPSOpcode;

// Unlike on the PPC, opcode 0 is not unused and thus we have to choose another fake
// opcode to represent JIT blocks and other emu hacks.
// I've chosen 0x68000000.
#define MIPS_EMUHACK_OPCODE 0x68000000
#define MIPS_EMUHACK_MASK 0xFC000000
#define MIPS_JITBLOCK_MASK 0xFF000000
#define MIPS_EMUHACK_VALUE_MASK 0x00FFFFFF

// There are 2 bits available for sub-opcodes, 0x03000000.
#define EMUOP_RUNBLOCK 0   // Runs a JIT block
#define EMUOP_RETKERNEL 1  // Returns to the simulated PSP kernel from a thread
#define EMUOP_CALL_REPLACEMENT 2

#define MIPS_IS_EMUHACK(op) (((op) & 0xFC000000) == MIPS_EMUHACK_OPCODE)  // masks away the subop
#define MIPS_IS_RUNBLOCK(op) (((op) & 0xFF000000) == MIPS_EMUHACK_OPCODE)  // masks away the subop
#define MIPS_IS_REPLACEMENT(op) (((op) & 0xFF000000) == (MIPS_EMUHACK_OPCODE | (EMUOP_CALL_REPLACEMENT << 24)))  // masks away the subop

#define MIPS_EMUHACK_CALL_REPLACEMENT (MIPS_EMUHACK_OPCODE | (EMUOP_CALL_REPLACEMENT << 24))
// These should probably be enums
#define CP0REG_BPC            3   /* PC breakpoint address */
#define CP0REG_BDA            5   /* Data load/store breakpoint address */
#define CP0REG_TAR            6   /* Target address */
#define CP0REG_DCIC           7   /* Cache control */
#define CP0REG_BADA           8
#define CP0REG_BDAM           9   /* Data load/store address mask */
#define CP0REG_BPCM           11  /* PC breakpoint address mask */
#define CP0REG_SR             12
#define CP0REG_CAUSE          13
#define CP0REG_EPC            14
#define CP0REG_PRID           15  /* Product ID */
#define CP0REG_ERREG          16

#define EXCEPTION_INT         0
#define EXCEPTION_MOD         1
#define EXCEPTION_TLBL        2
#define EXCEPTION_TLBS        3
#define EXCEPTION_ADEL        4 /* Address error on load */
#define EXCEPTION_ADES        5 /* Address error on store */
#define EXCEPTION_IBE         6 /* Instruction bus error */
#define EXCEPTION_DBE         7 /* Data bus error */
#define EXCEPTION_SYSCALL     8 /* System call */
#define EXCEPTION_BP          9 /* Breakpoint */
#define EXCEPTION_RI         10 /* Reserved instruction */
#define EXCEPTION_COPU       11 /* Coprocessor unusable */
#define EXCEPTION_OV         12 /* Arithmetic overflow */

enum MIPSGPReg {
	MIPS_REG_ZERO=0,
	MIPS_REG_COMPILER_SCRATCH=1,

	MIPS_REG_V0=2,
	MIPS_REG_V1=3,

	MIPS_REG_A0=4,
	MIPS_REG_A1=5,
	MIPS_REG_A2=6,
	MIPS_REG_A3=7,
	MIPS_REG_A4=8,
	MIPS_REG_A5=9,

	MIPS_REG_T0=8,  //alternate names for A4/A5
	MIPS_REG_T1=9,
	MIPS_REG_T2=10,
	MIPS_REG_T3=11,
	MIPS_REG_T4=12,
	MIPS_REG_T5=13,
	MIPS_REG_T6=14,
	MIPS_REG_T7=15,

	MIPS_REG_S0=16,
	MIPS_REG_S1=17,
	MIPS_REG_S2=18,
	MIPS_REG_S3=19,
	MIPS_REG_S4=20,
	MIPS_REG_S5=21,
	MIPS_REG_S6=22,
	MIPS_REG_S7=23,
	MIPS_REG_T8=24,
	MIPS_REG_T9=25,
	MIPS_REG_K0=26,
	MIPS_REG_K1=27,
	MIPS_REG_GP=28,
	MIPS_REG_SP=29,
	MIPS_REG_FP=30,
	MIPS_REG_RA=31,

	// Not real regs, just for convenience/jit mapping.
	// NOTE: These are not the same as the offsets the IR has to use!
	MIPS_REG_HI = 32,
	MIPS_REG_LO = 33,
	MIPS_REG_FPCOND = 34,
	MIPS_REG_VFPUCC = 35,

	MIPS_REG_INVALID=-1,
};

enum {
	VFPU_CTRL_SPREFIX,
	VFPU_CTRL_TPREFIX,
	VFPU_CTRL_DPREFIX,
	VFPU_CTRL_CC,
	VFPU_CTRL_INF4,
	VFPU_CTRL_RSV5,
	VFPU_CTRL_RSV6,
	VFPU_CTRL_REV,
	VFPU_CTRL_RCX0,
	VFPU_CTRL_RCX1,
	VFPU_CTRL_RCX2,
	VFPU_CTRL_RCX3,
	VFPU_CTRL_RCX4,
	VFPU_CTRL_RCX5,
	VFPU_CTRL_RCX6,
	VFPU_CTRL_RCX7,

	VFPU_CTRL_MAX,
	//unknown....
};

enum VCondition
{
	VC_FL,
	VC_EQ,
	VC_LT,
	VC_LE,
	VC_TR,
	VC_NE,
	VC_GE,
	VC_GT,
	VC_EZ,
	VC_EN,
	VC_EI,
	VC_ES,
	VC_NZ,
	VC_NN,
	VC_NI,
	VC_NS
};

// In memory, we order the VFPU registers differently. 
// Games use columns a whole lot more than rows, and it would thus be good if columns
// were contiguous in memory. Also, matrices aren't but should be.
extern uint8 voffset[128];
extern uint8 fromvoffset[128];

enum class CPUCore;

#if defined(ARCH_X86) || defined(ARCH_AMD64)

// Note that CTXREG is offset to point at the first floating point register, intentionally. This is so that a byte offset
// can reach both GPR and FPR regs.
#define MIPSSTATE_VAR(x) MDisp(X64JitConstants::CTXREG, \
	(int)(offsetof(MIPSState, x) - offsetof(MIPSState, f[0])))

// Workaround for compilers that don't like dynamic indexing in offsetof
#define MIPSSTATE_VAR_ELEM32(x, i) MDisp(X64JitConstants::CTXREG, \
	(int)(offsetof(MIPSState, x) - offsetof(MIPSState, f[0]) + (i) * 4))

// To get RIP/relative addressing (requires tight memory control so generated code isn't too far from the binary, and a reachable variable called mips):
// #define MIPSSTATE_VAR(x) M(&mips->x)

#endif

enum {
	NUM_X86_FPU_TEMPS = 16,
};

class MIPSState
{
public:
	MIPSState();
	~MIPSState();

	void Init();
	void Shutdown();
	void Reset();

	int StateAction(StateMem *sm, int load, int data_only);

	// MUST start with r and be followed by f, v, and t!
	uint32 r[32];
	union {
		float f[32];
		uint32 fi[32];
		int fs[32];
	};
	union {
		float v[128];
		uint32 vi[128];
	};

	// Register-allocated JIT Temps don't get flushed so we don't reserve space for them.
	// However, the IR interpreter needs some temps that can stick around between ops.
	// Can be indexed through r[] using indices 192+.
	uint32 t[16];     //192

	// If vfpuCtrl (prefixes) get mysterious values, check the VFPU regcache code.
	uint32 vfpuCtrl[16]; // 208

	float vt[16];  //224  TODO: VFPU temp

	// ARM64 wants lo/hi to be aligned to 64 bits from the base of this struct.
	uint32 padLoHi;    // 240

	union {
		struct {
			uint32 pc;   //241

			uint32 lo;   //242
			uint32 hi;   //243

			uint32 fcr31; //244 fpu control register
			uint32 fpcond;  //245 cache the cond flag of fcr31  (& 1 << 23)
		};
		uint32 other[6];
	};

	uint32 nextPC;
	int downcount;  // This really doesn't belong here, it belongs in timing.h. But you gotta do what you gotta do, this needs to be reachable in the ARM JIT.

	bool inDelaySlot;
	int llBit;  // ll/sc
	uint32 temp;  // can be used to save temporaries during calculations when we need more than R0 and R1
	uint32 mxcsrTemp;
	// Temporary used around delay slots and similar.
	u64 saved_flags;
	//Cop0
	struct
	{
	   union
	   {
		  uint32_t Regs[32];
		  struct
		  {
			 uint32_t Unused00;
			 uint32_t Unused01;
			 uint32_t Unused02;
			 uint32_t BPC;		// RW
			 uint32_t Unused04;
			 uint32_t BDA;		// RW
			 uint32_t TAR;
			 uint32_t DCIC;	   // RW
			 uint32_t BADA;	   // R
			 uint32_t BDAM;	   // R/W
			 uint32_t Unused0A;
			 uint32_t BPCM;	   // R/W
			 uint32_t SR;		// R/W
			 uint32_t CAUSE;	// R/W(partial)
			 uint32_t EPC;		// R
			 uint32_t PRID;	   // R
			 uint32_t ERREG;	// ?(may not exist, test)
		  };
	   };
	} CP0;

	// Debug stuff
	uint32 debugCount;	// can be used to count basic blocks before crashes, etc.

	// Temps needed for JitBranch.cpp experiments
	uint32 intBranchExit;
	uint32 jitBranchExit;

	uint32 savedPC;

	uint32 MEMORY_ALIGNED16(vcmpResult[4]);

	float sincostemp[2];

	static const uint32 FCR0_VALUE = 0x00003351;

#if defined(ARCH_X86) || defined(ARCH_AMD64)
	// FPU TEMP0, etc. are swapped in here if necessary (e.g. on x86.)
	float tempValues[NUM_X86_FPU_TEMPS];
#endif

	uint8 VfpuWriteMask() const {
		return (vfpuCtrl[VFPU_CTRL_DPREFIX] >> 8) & 0xF;
	}
	bool VfpuWriteMask(int i) const {
		return (vfpuCtrl[VFPU_CTRL_DPREFIX] >> (8 + i)) & 1;
	}

	bool HasDefaultPrefix() const;

	void SingleStep();
	int RunLoopUntil(u64 globalTicks);
	uint32 Run(uint32 timestamp_in);
	// To clear jit caches, etc.
	void InvalidateICache(uint32 address, int length = 4);

	void ClearJitCache();
};

void InitMips();

class MIPSDebugInterface;

//The one we are compiling or running currently
extern MIPSState *currentMIPS;
extern MIPSDebugInterface *currentDebugMIPS;
extern MIPSState *mipsr4k;

extern const float cst_constants[32];
