// Copyright (C) 2003 Dolphin Project / 2012 PPSSPP Project

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


#include <algorithm>
#include <mutex>

#include "jit/Memory/MemoryUtil.h"
#include "jit/Memory/MemArena.h"

#include "mednafen/masmem.h"

#ifdef JIT
#include "jit/MIPS.h"

#include "jit/Debugger/SymbolMap.h"
#include "jit/Debugger/Breakpoints.h"
#include "jit/JitCommon/JitCommon.h"
#include "jit/JitCommon/JitBlockCache.h"

#endif
namespace Memory {

// The base pointer to the auto-mirrored arena.
u8* base = NULL;

// The MemArena class
MemArena g_arena;
// ==============

// 64-bit: Pointers to high-mem mirrors
// 32-bit: Same as above
uint8 *m_pPhysicalRAM;
uint8 *m_pUncachedRAM;
uint8 *m_pCachedRAM;
uint8 *m_pBIOS;
uint8 *m_pScratchPad;
uint8 *m_pParallelPort;
// Holds the ending address of the PSP's user space.
// Required for HD Remasters to work properly.
// This replaces RAM_NORMAL_SIZE at runtime.
uint32 g_MemorySize;
// Used to store the PSP model on game startup.
uint32 g_PSPModel;

std::recursive_mutex g_shutdownLock;

// We don't declare the IO region in here since its handled by other means.
static MemoryView views[] =
{
	{&m_pPhysicalRAM, 0x00000000, RAM_SIZE, 0},
	{&m_pCachedRAM, 0x80000000, RAM_SIZE, MV_MIRROR_PREVIOUS},
	{&m_pUncachedRAM, 0xa0000000, RAM_SIZE, MV_MIRROR_PREVIOUS},
	{&m_pParallelPort, 0x1f000000, 0x00020000, 0}, // is parrellel port needed/wanted here?
	{&m_pScratchPad, 0x1f80000, SCRATCHPAD_SIZE, 0},
	{&m_pBIOS, 0xbfc00000, 0x80000, 0}
};

static const int num_views = sizeof(views) / sizeof(MemoryView);

inline static bool CanIgnoreView(const MemoryView &view) {
#ifdef ARCH_32BIT
	// Basically, 32-bit platforms can ignore views that are masked out anyway.
	return (view.flags & MV_MIRROR_PREVIOUS) && (view.virtual_address & ~MEMVIEW32_MASK) != 0;
#else
	return false;
#endif
}

#if defined(IOS) && defined(ARCH_64BIT)
#define SKIP(a_flags, b_flags) \
	if ((b_flags) & MV_KERNEL) \
		continue;
#else
#define SKIP(a_flags, b_flags) \
	;
#endif

static bool Memory_TryBase(uint32 flags) {
	// OK, we know where to find free space. Now grab it!
	// We just mimic the popular BAT setup.

	size_t position = 0;
	size_t last_position = 0;

	// Zero all the pointers to be sure.
	for (int i = 0; i < num_views; i++) {
		if (views[i].out_ptr)
			*views[i].out_ptr = 0;
	}

	int i;
	for (i = 0; i < num_views; i++) {
		const MemoryView &view = views[i];
		if (view.size == 0)
			continue;
		SKIP(flags, view.flags);
		
		if (view.flags & MV_MIRROR_PREVIOUS) {
			position = last_position;
		}
#ifndef MASKED_PSP_MEMORY
		*view.out_ptr = (u8*)g_arena.CreateView(
			position, view.size, base + view.virtual_address);
		if (!*view.out_ptr) {
			log_cb(RETRO_LOG_INFO, "Failed at view %d\n", i);
			goto bail;
		}
#else
		if (CanIgnoreView(view)) {
			// This is handled by address masking in 32-bit, no view needs to be created.
			*view.out_ptr = *views[i - 1].out_ptr;
		} else {
			*view.out_ptr = (u8*)g_arena.CreateView(
				position, view.size, base + (view.virtual_address & MEMVIEW32_MASK));
			if (!*view.out_ptr) {
				log_cb(RETRO_LOG_INFO, "Failed at view %d\n", i);
				goto bail;
			}
		}
#endif
		last_position = position;
		position += g_arena.roundup(view.size);
	}

	return true;
bail:
	// Argh! ERROR! Free what we grabbed so far so we can try again.
	for (int j = 0; j <= i; j++) {
		if (views[i].size == 0)
			continue;
		SKIP(flags, views[i].flags);
		if (*views[j].out_ptr) {
			if (!CanIgnoreView(views[j])) {
				g_arena.ReleaseView(*views[j].out_ptr, views[j].size);
			}
			*views[j].out_ptr = NULL;
		}
	}
	return false;
}

bool MemoryMap_Setup(uint32 flags) {
#if defined(UWP)
	// We reserve the memory, then simply commit in TryBase.
	base = (u8*)VirtualAllocFromApp(0, 0x10000000, MEM_RESERVE, PAGE_READWRITE);
#else

	// Figure out how much memory we need to allocate in total.
	size_t total_mem = 0;
	for (int i = 0; i < num_views; i++) {
		if (views[i].size == 0)
			continue;
		SKIP(flags, views[i].flags);
		if (!CanIgnoreView(views[i]))
			total_mem += g_arena.roundup(views[i].size);
	}

	// Grab some pagefile backed memory out of the void ...
	g_arena.GrabLowMemSpace(total_mem);
#endif

#if !defined(OS_ANDROID)
	if (g_arena.NeedsProbing()) {
		int base_attempts = 0;
#if defined(OS_WINDOWS) && defined(ARCH_32BIT)
		// Try a whole range of possible bases. Return once we got a valid one.
		uintptr_t max_base_addr = 0x7FFF0000 - 0x10000000;
		uintptr_t min_base_addr = 0x01000000;
		uintptr_t stride = 0x400000;
#else
		// iOS
		uintptr_t max_base_addr = 0x1FFFF0000ULL - 0x80000000ULL;
		uintptr_t min_base_addr = 0x100000000ULL;
		uintptr_t stride = 0x800000;
#endif
		for (uintptr_t base_addr = min_base_addr; base_addr < max_base_addr; base_addr += stride) {
			base_attempts++;
			base = (uint8 *)base_addr;
			if (Memory_TryBase(flags)) {
				log_cb(RETRO_LOG_INFO, "Found valid memory base at %p after %i tries.\n", base, base_attempts);
				return true;
			}
		}
		log_cb(RETRO_LOG_ERROR, "MemoryMap_Setup: Failed finding a memory base.\n");
		//Panic somehow?
		//log_cb(RETRO_LOG_ERROR, "MemoryMap_Setup: Failed finding a memory base.");
		return false;
	}
	else
#endif
	{
#if !defined(UWP)
		base = g_arena.Find4GBBase();
#endif
	}

	// Should return true...
	return Memory_TryBase(flags);
}

void MemoryMap_Shutdown(uint32 flags) {
	for (int i = 0; i < num_views; i++) {
		if (views[i].size == 0)
			continue;
		SKIP(flags, views[i].flags);
		if (*views[i].out_ptr)
			g_arena.ReleaseView(*views[i].out_ptr, views[i].size);
		*views[i].out_ptr = nullptr;
	}
	g_arena.ReleaseSpace();

#if defined(UWP)
	VirtualFree(base, 0, MEM_RELEASE);
#endif
}

void Init() {
	// On some 32 bit platforms, you can only map < 32 megs at a time.
	// TODO: Wait, wtf? What platforms are those? This seems bad.
	g_MemorySize = RAM_SIZE;
	const static int MAX_MMAP_SIZE = 31 * 1024 * 1024;
	//TODO Add back in _dbg_assert_msg_
	//_dbg_assert_msg_(MEMMAP, g_MemorySize < MAX_MMAP_SIZE * 3, "ACK - too much memory for three mmap views.");
	for (size_t i = 0; i < ARRAY_SIZE(views); i++) {
		if (views[i].flags & MV_IS_PRIMARY_RAM)
			views[i].size = std::min((int)g_MemorySize, MAX_MMAP_SIZE);
		if (views[i].flags & MV_IS_EXTRA1_RAM)
			views[i].size = std::min(std::max((int)g_MemorySize - MAX_MMAP_SIZE, 0), MAX_MMAP_SIZE);
		if (views[i].flags & MV_IS_EXTRA2_RAM)
			views[i].size = std::min(std::max((int)g_MemorySize - MAX_MMAP_SIZE * 2, 0), MAX_MMAP_SIZE);
	}
	int flags = 0;
	if(!MemoryMap_Setup(flags)){
		log_cb(RETRO_LOG_ERROR, "MEMORY MAP SETUP FAILED!!!!!\n");
	}else{
		log_cb(RETRO_LOG_INFO, "Memory system initialized. Base at %p (RAM at @ %p, uncached @ %p)\n",
			base, m_pPhysicalRAM, m_pUncachedRAM);
	}
}
/*
void DoState(PointerWrap &p) {
	
	auto s = p.Section("Memory", 1, 3);
	if (!s)
		return;

	if (s < 2) {
		if (!g_RemasterMode)
			g_MemorySize = RAM_NORMAL_SIZE;
		g_PSPModel = PSP_MODEL_FAT;
	} else if (s == 2) {
		// In version 2, we determine memory size based on PSP model.
		uint32 oldMemorySize = g_MemorySize;
		p.Do(g_PSPModel);
		p.DoMarker("PSPModel");
		if (!g_RemasterMode) {
			g_MemorySize = g_PSPModel == PSP_MODEL_FAT ? RAM_NORMAL_SIZE : RAM_DOUBLE_SIZE;
			if (oldMemorySize < g_MemorySize) {
				Shutdown();
				Init();
			}
		}
	} else {
		// In version 3, we started just saving the memory size directly.
		// It's no longer based strictly on the PSP model.
		uint32 oldMemorySize = g_MemorySize;
		p.Do(g_PSPModel);
		p.DoMarker("PSPModel");
		p.Do(g_MemorySize);
		if (oldMemorySize != g_MemorySize) {
			Shutdown();
			Init();
		}
	}

	p.DoArray(GetPointer(PSP_GetKernelMemoryBase()), g_MemorySize);
	p.DoMarker("RAM");

	p.DoArray(m_pPhysicalVRAM1, VRAM_SIZE);
	p.DoMarker("VRAM");
	p.DoArray(m_pPhysicalScratchPad, SCRATCHPAD_SIZE);
	p.DoMarker("ScratchPad");
}
*/
void Shutdown() {
	std::lock_guard<std::recursive_mutex> guard(g_shutdownLock);
	uint32 flags = 0;
	MemoryMap_Shutdown(flags);
	base = nullptr;
	log_cb(RETRO_LOG_DEBUG, "Memory system shut down.");
}

void Clear() {

	if (m_pPhysicalRAM)
		memset(m_pPhysicalRAM, 0xFF, g_MemorySize);
	if (m_pParallelPort)
		memset(m_pParallelPort, 0, 0x00020000);
	if (m_pBIOS)
		memset(m_pBIOS, 0, 0x00080000);
	if (m_pScratchPad)
		memset(m_pScratchPad, 0, SCRATCHPAD_SIZE);
}	

bool IsActive() {
	return base != nullptr;
}

// Wanting to avoid include pollution, MemMap.h is included a lot.
MemoryInitedLock::MemoryInitedLock()
{
	g_shutdownLock.lock();
}
MemoryInitedLock::~MemoryInitedLock()
{
	g_shutdownLock.unlock();
}

MemoryInitedLock Lock()
{
	return MemoryInitedLock();
}
//TODO Again, fix replacements to fix this.
#ifdef JIT
__forceinline static Opcode Read_Instruction(uint32 address, bool resolveReplacements, Opcode inst)
{
	if (!MIPS_IS_EMUHACK(inst.encoding)) {
		return inst;
	}
	if (MIPS_IS_RUNBLOCK(inst.encoding) && MIPSComp::jit) {
		inst = MIPSComp::jit->GetOriginalOp(inst);
		/*if (resolveReplacements && MIPS_IS_REPLACEMENT(inst)) {
			uint32 op;
			if (GetReplacedOpAt(address, &op)) {
				if (MIPS_IS_EMUHACK(op)) {
					ERROR_LOG(MEMMAP, "WTF 1");
					return Opcode(op);
				} else {
					return Opcode(op);
				}
			} else {
				ERROR_LOG(MEMMAP, "Replacement, but no replacement op? %08x", inst.encoding);
			}
		}*/
		return inst;
	} else if (resolveReplacements && MIPS_IS_REPLACEMENT(inst.encoding)) {
		uint32 op;
		/*if (GetReplacedOpAt(address, &op)) {
			if (MIPS_IS_EMUHACK(op)) {
				ERROR_LOG(MEMMAP, "WTF 2");
				return Opcode(op);
			} else {
				return Opcode(op);
			}
		} else*/ {
			return inst;
		}
	} else {
		return inst;
	}
}

Opcode Read_Instruction(uint32 address, bool resolveReplacements)
{
	Opcode inst = Opcode(Read_U32(address));
	return Read_Instruction(address, resolveReplacements, inst);
}

Opcode ReadUnchecked_Instruction(uint32 address, bool resolveReplacements)
{
	Opcode inst = Opcode(ReadUnchecked_U32(address));
	return Read_Instruction(address, resolveReplacements, inst);
}

Opcode Read_Opcode_JIT(uint32 address)
{
	Opcode inst = Opcode(Read_U32(address));
	if (MIPS_IS_RUNBLOCK(inst.encoding) && MIPSComp::jit) {
		return MIPSComp::jit->GetOriginalOp(inst);
	} else {
		return inst;
	}
}

// WARNING! No checks!
// We assume that _Address is cached
void Write_Opcode_JIT(const uint32 _Address, const Opcode& _Value)
{
	Memory::WriteUnchecked_U32(_Value.encoding, _Address);
}

void Memset(const uint32 _Address, const uint8 _iValue, const uint32 _iLength)
{
	uint8 *ptr = GetPointer(_Address);
	if (ptr != NULL) {
		memset(ptr, _iValue, _iLength);
	}
	else
	{
		for (size_t i = 0; i < _iLength; i++)
			Write_U8(_iValue, (u32)(_Address + i));
	}
#ifndef MOBILE_DEVICE
	CBreakPoints::ExecMemCheck(_Address, true, _iLength, currentMIPS->pc);
#endif
}
#endif
} // namespace
