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
#include <cstdio>
#include "mednafen/mednafen-types.h"
#include "jit/Common/expression_parser.h"

struct MemMap;

enum {
	GPR_SIZE_32,
	GPR_SIZE_64,
};

class DebugInterface
{
public:
	virtual const char *disasm(unsigned int address, unsigned int align) {return "NODEBUGGER";}
	virtual int getInstructionSize(int instruction) {return 1;}

	virtual bool isAlive() {return true;}
	virtual bool isBreakpoint(unsigned int address) {return false;}
	virtual void setBreakpoint(unsigned int address){}
	virtual void clearBreakpoint(unsigned int address){}
	virtual void clearAllBreakpoints() {}
	virtual void toggleBreakpoint(unsigned int address){}
	virtual unsigned int readMemory(unsigned int address){return 0;}
	virtual unsigned int getPC() {return 0;}
	virtual void setPC(unsigned int address) {}
	virtual void step() {}
	virtual void runToBreakpoint() {}
	virtual int getColor(unsigned int address){return 0xFFFFFFFF;}
	virtual std::string getDescription(unsigned int address) {return "";}
	virtual bool initExpression(const char* exp, PostfixExpression& dest) { return false; };
	virtual bool parseExpression(PostfixExpression& exp, uint32& dest) { return false; };

	
	virtual uint32 GetHi() { return 0; };
	virtual uint32 GetLo() { return 0; };
	virtual void SetHi(uint32 val) { };
	virtual void SetLo(uint32 val) { };
	virtual const char *GetName() = 0;
	virtual int GetGPRSize() = 0; //32 or 64
	virtual uint32 GetGPR32Value(int reg) {return 0;}
	virtual void SetGPR32Value(int reg) {}
	virtual void SetGPR64Value(int reg) {}
	virtual uint32 GetPC() = 0;
	virtual void SetPC(uint32 _pc) = 0;
	virtual uint32 GetLR() {return GetPC();}
	virtual void DisAsm(uint32 op, uint32 pc, int align, char *out) {sprintf(out,"[%08x] UNKNOWN", op);}
	//More stuff for debugger
	virtual int GetNumCategories() {return 0;}
	virtual int GetNumRegsInCategory(int cat) {return 0;}
	virtual const char *GetCategoryName(int cat) {return 0;}
	virtual const char *GetRegName(int cat, int index) {return 0;}
	virtual void PrintRegValue(int cat, int index, char *out)
	{
		sprintf(out,"%08X",GetGPR32Value(index));
	}
	virtual uint32 GetRegValue(int cat, int index) {return 0;}
	virtual void SetRegValue(int cat, int index, uint32 value) {}

};
