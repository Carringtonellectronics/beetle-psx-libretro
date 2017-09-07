#pragma once

#include "jit/Debugger/DebugInterface.h"

namespace MIPSAsm {
	bool MipsAssembleOpcode(const char* line, DebugInterface* cpu, uint32 address);
	std::wstring GetAssembleError();
}
