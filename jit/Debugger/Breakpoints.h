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

#include <vector>

#include "jit/Debugger/DebugInterface.h"

enum BreakAction
{
	BREAK_ACTION_IGNORE = 0x00,
	BREAK_ACTION_LOG = 0x01,
	BREAK_ACTION_PAUSE = 0x02,
};

static inline BreakAction &operator |= (BreakAction &lhs, const BreakAction &rhs) {
	lhs = BreakAction(lhs | rhs);
	return lhs;
}

static inline BreakAction operator | (const BreakAction &lhs, const BreakAction &rhs) {
	return BreakAction((uint32)lhs | (uint32)rhs);
}

struct BreakPointCond
{
	DebugInterface *debug;
	PostfixExpression expression;
	std::string expressionString;

	BreakPointCond() : debug(nullptr)
	{
	}

	uint32 Evaluate()
	{
		uint32 result;
		if (debug->parseExpression(expression,result) == false) return 0;
		return result;
	}
};

struct BreakPoint
{
	BreakPoint() : hasCond(false) {}

	uint32	addr;
	bool temporary;

	BreakAction result;
	std::string logFormat;

	bool hasCond;
	BreakPointCond cond;

	bool IsEnabled() const {
		return (result & BREAK_ACTION_PAUSE) != 0;
	}

	bool operator == (const BreakPoint &other) const {
		return addr == other.addr;
	}
	bool operator < (const BreakPoint &other) const {
		return addr < other.addr;
	}
};

enum MemCheckCondition
{
	MEMCHECK_READ = 0x01,
	MEMCHECK_WRITE = 0x02,
	MEMCHECK_WRITE_ONCHANGE = 0x04,

	MEMCHECK_READWRITE = 0x03,
};

struct MemCheck
{
	MemCheck();
	uint32 start;
	uint32 end;

	MemCheckCondition cond;
	BreakAction result;
	std::string logFormat;

	uint32 numHits;

	uint32 lastPC;
	uint32 lastAddr;
	int lastSize;

	BreakAction Action(uint32 addr, bool write, int size, uint32 pc);
	void JitBefore(uint32 addr, bool write, int size, uint32 pc);
	void JitCleanup();

	void Log(uint32 addr, bool write, int size, uint32 pc);

	bool IsEnabled() const {
		return (result & BREAK_ACTION_PAUSE) != 0;
	}

	bool operator == (const MemCheck &other) const {
		return start == other.start && end == other.end;
	}
};

// BreakPoints cannot overlap, only one is allowed per address.
// MemChecks can overlap, as long as their ends are different.
// WARNING: MemChecks are not used in the interpreter or HLE currently.
class CBreakPoints
{
public:
	static const size_t INVALID_BREAKPOINT = -1;
	static const size_t INVALID_MEMCHECK = -1;

	static bool IsAddressBreakPoint(uint32 addr);
	static bool IsAddressBreakPoint(uint32 addr, bool* enabled);
	static bool IsTempBreakPoint(uint32 addr);
	static bool RangeContainsBreakPoint(uint32 addr, uint32 size);
	static void AddBreakPoint(uint32 addr, bool temp = false);
	static void RemoveBreakPoint(uint32 addr);
	static void ChangeBreakPoint(uint32 addr, bool enable);
	static void ChangeBreakPoint(uint32 addr, BreakAction result);
	static void ClearAllBreakPoints();
	static void ClearTemporaryBreakPoints();

	// Makes a copy.  Temporary breakpoints can't have conditions.
	static void ChangeBreakPointAddCond(uint32 addr, const BreakPointCond &cond);
	static void ChangeBreakPointRemoveCond(uint32 addr);
	static BreakPointCond *GetBreakPointCondition(uint32 addr);

	static void ChangeBreakPointLogFormat(uint32 addr, const std::string &fmt);

	static BreakAction ExecBreakPoint(uint32 addr);

	static void AddMemCheck(uint32 start, uint32 end, MemCheckCondition cond, BreakAction result);
	static void RemoveMemCheck(uint32 start, uint32 end);
	static void ChangeMemCheck(uint32 start, uint32 end, MemCheckCondition cond, BreakAction result);
	static void ClearAllMemChecks();

	static void ChangeMemCheckLogFormat(uint32 start, uint32 end, const std::string &fmt);

	static MemCheck *GetMemCheck(uint32 address, int size);
	static BreakAction ExecMemCheck(uint32 address, bool write, int size, uint32 pc);
	static BreakAction ExecOpMemCheck(uint32 address, uint32 pc);

	// Executes memchecks but used by the jit.  Cleanup finalizes after jit is done.
	static void ExecMemCheckJitBefore(uint32 address, bool write, int size, uint32 pc);
	static void ExecMemCheckJitCleanup();

	static void SetSkipFirst(uint32 pc);
	static uint32 CheckSkipFirst();

	// Includes uncached addresses.
	static const std::vector<MemCheck> GetMemCheckRanges();

	static const std::vector<MemCheck> GetMemChecks();
	static const std::vector<BreakPoint> GetBreakpoints();

	static bool HasMemChecks();

	static void Update(uint32 addr = 0);

	static bool ValidateLogFormat(DebugInterface *cpu, const std::string &fmt);
	static bool EvaluateLogFormat(DebugInterface *cpu, const std::string &fmt, std::string &result);

private:
	static size_t FindBreakpoint(uint32 addr, bool matchTemp = false, bool temp = false);
	// Finds exactly, not using a range check.
	static size_t FindMemCheck(uint32 start, uint32 end);

	static std::vector<BreakPoint> breakPoints_;
	static uint32 breakSkipFirstAt_;
	static uint64 breakSkipFirstTicks_;

	static std::vector<MemCheck> memChecks_;
	static std::vector<MemCheck *> cleanupMemChecks_;
};


