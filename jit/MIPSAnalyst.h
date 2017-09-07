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

#include <string>
#include <vector>

#include "mednafen/mednafen-types.h"
#include "jit/MIPS.h"
#include "libretro.h"

class DebugInterface;

extern retro_log_printf_t log_cb;

namespace MIPSAnalyst
{
	const int MIPS_NUM_GPRS = 32;

	struct RegisterAnalysisResults {
		bool used;
		int firstRead;
		int lastRead;
		int firstWrite;
		int lastWrite;
		int firstReadAsAddr;
		int lastReadAsAddr;

		int readCount;
		int writeCount;
		int readAsAddrCount;

		int TotalReadCount() const { return readCount + readAsAddrCount; }
		int FirstRead() const { return firstReadAsAddr < firstRead ? firstReadAsAddr : firstRead; }
		int LastRead() const { return lastReadAsAddr > lastRead ? lastReadAsAddr : lastRead; }

		void MarkRead(uint32 addr) {
			if (firstRead == -1)
				firstRead = addr;
			lastRead = addr;
			readCount++;
			used = true;
		}

		void MarkReadAsAddr(uint32 addr) {
			if (firstReadAsAddr == -1)
				firstReadAsAddr = addr;
			lastReadAsAddr = addr;
			readAsAddrCount++;
			used = true;
		}

		void MarkWrite(uint32 addr) {
			if (firstWrite == -1)
				firstWrite = addr;
			lastWrite = addr;
			writeCount++;
			used = true;
		}
	};

	struct AnalysisResults {
		RegisterAnalysisResults r[MIPS_NUM_GPRS];
	};

	AnalysisResults Analyze(uint32 address);

	// This tells us if the reg is used within intrs of addr (also includes likely delay slots.)
	bool IsRegisterUsed(MIPSGPReg reg, uint32 addr, int instrs);
	// This tells us if the reg is clobbered within intrs of addr (e.g. it is surely not used.)
	bool IsRegisterClobbered(MIPSGPReg reg, uint32 addr, int instrs);

	struct AnalyzedFunction {
		uint32 start;
		uint32 end;
		u64 hash;
		uint32 size;
		bool isStraightLeaf;
		bool hasHash;
		bool usesVFPU;
		bool foundInSymbolMap;
		char name[64];
	};

	struct ReplacementTableEntry;

	void Reset();

	bool IsRegisterUsed(uint32 reg, uint32 addr);
	// This will not only create a database of "AnalyzedFunction" structs, it also
	// will insert all the functions it finds into the symbol map, if insertSymbols is true.

	// If we have loaded symbols from the elf, we'll register functions as they are touched
	// so that we don't just dump them all in the cache.
	void RegisterFunction(uint32 startAddr, uint32 size, const char *name);
	void ScanForFunctions(uint32 startAddr, uint32 endAddr, bool insertSymbols);
	void ForgetFunctions(uint32 startAddr, uint32 endAddr);
	void CompileLeafs();

	void SetHashMapFilename(const std::string& filename = "");
	void LoadBuiltinHashMap();
	void LoadHashMap(const std::string& filename);
	void StoreHashMap(std::string filename = "");

	const char *LookupHash(u64 hash, uint32 funcSize);
	void ReplaceFunctions();

	void UpdateHashMap();
	void ApplyHashMap();

	std::vector<MIPSGPReg> GetInputRegs(MIPSOpcode op);
	std::vector<MIPSGPReg> GetOutputRegs(MIPSOpcode op);

	MIPSGPReg GetOutGPReg(MIPSOpcode op);
	bool ReadsFromGPReg(MIPSOpcode op, MIPSGPReg reg);
	bool IsDelaySlotNiceReg(MIPSOpcode branchOp, MIPSOpcode op, MIPSGPReg reg1, MIPSGPReg reg2 = MIPS_REG_ZERO);
	bool IsDelaySlotNiceVFPU(MIPSOpcode branchOp, MIPSOpcode op);
	bool IsDelaySlotNiceFPU(MIPSOpcode branchOp, MIPSOpcode op);
	bool IsSyscall(MIPSOpcode op);

	bool OpWouldChangeMemory(uint32 pc, uint32 addr, uint32 size);
	int OpMemoryAccessSize(uint32 pc);
	bool IsOpMemoryWrite(uint32 pc);
	bool OpHasDelaySlot(uint32 pc);

	typedef struct {
		DebugInterface* cpu;
		uint32 opcodeAddress;
		MIPSOpcode encodedOpcode;

		// shared between branches and conditional moves
		bool isConditional;
		bool conditionMet;

		// branches
		uint32 branchTarget;
		bool isBranch;
		bool isLinkedBranch;
		bool isLikelyBranch;
		bool isBranchToRegister;
		int branchRegisterNum;

		// data access
		bool isDataAccess;
		int dataSize;
		uint32 dataAddress;

		bool hasRelevantAddress;
		uint32 relevantAddress;
	} MipsOpcodeInfo;

	MipsOpcodeInfo GetOpcodeInfo(DebugInterface* cpu, uint32 address);

}	// namespace MIPSAnalyst
