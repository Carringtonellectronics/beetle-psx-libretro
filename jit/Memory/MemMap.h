// Copyright (C) 2003 Dolphin Project / 2012 PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

#include <cstring>
#ifndef offsetof
#include <stddef.h>
#endif

// Includes
#include "mednafen/mednafen-types.h"
#include "jit/Common/Opcode.h"
#include "mednafen/mednafen.h"
#include "libretro.h"
//defines

// PPSSPP is very aggressive about trying to do memory accesses directly, for speed.
// This can be a problem when debugging though, as stray memory reads and writes will
// crash the whole emulator.
// If safe memory is enabled and JIT is disabled, all memory access will go through the proper
// memory access functions, and thus won't crash the emu when they go out of bounds.
#if defined(_DEBUG)
//#define SAFE_MEMORY
#endif

typedef void (*writeFn8 )(const u8, const u32);
typedef void (*writeFn16)(const u16,const u32);
typedef void (*writeFn32)(const u32,const u32);
typedef void (*writeFn64)(const u64,const u32);

typedef void (*readFn8 )(u8&,  const u32);
typedef void (*readFn16)(u16&, const u32);
typedef void (*readFn32)(u32&, const u32);
typedef void (*readFn64)(u64&, const u32);

// Logging 

extern retro_log_printf_t log_cb;

namespace Memory
{
// Base is a pointer to the base of the memory map. Yes, some MMU tricks
// are used to set up a full GC or Wii memory map in process memory.	on
// 32-bit, you have to mask your offsets with 0x3FFFFFFF. This means that
// some things are mirrored too many times, but eh... it works.

// In 64-bit, this might point to "high memory" (above the 32-bit limit),
// so be sure to load it into a 64-bit register.
extern uint8 *base; 

// These are guaranteed to point to "low memory" addresses (sub-32-bit).
// 64-bit: Pointers to low-mem (sub-0x10000000) mirror
// 32-bit: Same as the corresponding physical/virtual pointers.
extern uint8 *m_pPhysicalRAM;
extern uint8 *m_pUncachedRAM;
extern uint8 *m_pCachedRAM;
extern uint8 *m_pBIOS;
extern uint8 *m_pScratchPad;
extern uint8 *m_pParallelPort;


// This replaces RAM_NORMAL_SIZE at runtime.
extern uint32 g_MemorySize;
extern uint32 g_PSPModel;

// UWP has such limited memory management that we need to mask
// even in 64-bit mode.
#if defined(ARCH_32BIT) || defined(UWP)
#define MASKED_PSP_MEMORY
#endif

enum
{
	RAM_SIZE = 0x00200000,

	SCRATCHPAD_SIZE = 0x00000400,

};

enum {
	MV_MIRROR_PREVIOUS = 1,
	MV_IS_PRIMARY_RAM = 0x100,
	MV_IS_EXTRA1_RAM = 0x200,
	MV_IS_EXTRA2_RAM = 0x400,
	MV_KERNEL = 0x800  // Can be skipped on platforms where memory is tight.
};

struct MemoryView
{
	uint8 **out_ptr;
	uint32 virtual_address;
	uint32 size;
	uint32 flags;
};

// Uses a memory arena to set up an emulator-friendly memory map
bool MemoryMap_Setup(uint32 flags);
void MemoryMap_Shutdown(uint32 flags);

// Init and Shutdown
void Init();
void Shutdown();
/*
void DoState(PointerWrap &p);
*/
void Clear();
// False when shutdown has already been called.
bool IsActive();

class MemoryInitedLock {
public:
	MemoryInitedLock();
	~MemoryInitedLock();
};

// This doesn't lock memory access or anything, it just makes sure memory isn't freed.
// Use it when accessing PSP memory from external threads.
MemoryInitedLock Lock();
#ifdef JIT
// used by JIT to read instructions. Does not resolve replacements.
Opcode Read_Opcode_JIT(const uint32 _Address);
// used by JIT. Reads in the "Locked cache" mode
void Write_Opcode_JIT(const uint32 _Address, const Opcode& _Value);

// Should be used by analyzers, disassemblers etc. Does resolve replacements.
Opcode Read_Instruction(const uint32 _Address, bool resolveReplacements = false);
Opcode ReadUnchecked_Instruction(const uint32 _Address, bool resolveReplacements = false);
#endif
uint8  Read_U8(const uint32 _Address);
uint16 Read_U16(const uint32 _Address);
uint32 Read_U32(const uint32 _Address);
uint64 Read_U64(const uint32 _Address);

#if (defined(ARM) || defined(_ARM)) && !defined(_M_ARM)
#define _M_ARM
#endif

inline u8* GetPointerUnchecked(const uint32 address) {
#ifdef MASKED_PSP_MEMORY
	return (uint8 *)(base + (address & MEMVIEW32_MASK));
#else
	return (uint8 *)(base + address);
#endif
}

#ifdef SAFE_MEMORY
uint32 ReadUnchecked_U32(const uint32 _Address);
// ONLY for use by GUI and fast interpreter
uint8 ReadUnchecked_U8(const uint32 _Address);
uint16 ReadUnchecked_U16(const uint32 _Address);
void WriteUnchecked_U8(const uint8 _Data, const uint32 _Address);
void WriteUnchecked_U16(const uint16 _Data, const uint32 _Address);
void WriteUnchecked_U32(const uint32 _Data, const uint32 _Address);
#else

inline uint32 ReadUnchecked_U32(const uint32 address) {
#ifdef MASKED_PSP_MEMORY
	return *(uint32 *)(base + (address & MEMVIEW32_MASK));
#else
	return *(uint32 *)(base + address);
#endif
}

inline float ReadUnchecked_Float(const uint32 address) {
#ifdef MASKED_PSP_MEMORY
	return *(float *)(base + (address & MEMVIEW32_MASK));
#else
	return *(float *)(base + address);
#endif
}

inline uint16 ReadUnchecked_U16(const uint32 address) {
#ifdef MASKED_PSP_MEMORY
	return *(uint16 *)(base + (address & MEMVIEW32_MASK));
#else
	return *(uint16 *)(base + address);
#endif
}

inline uint8 ReadUnchecked_U8(const uint32 address) {
#ifdef MASKED_PSP_MEMORY
	return (*(uint8 *)(base + (address & MEMVIEW32_MASK)));
#else
	return (*(uint8 *)(base + address));
#endif
}

inline void WriteUnchecked_U32(uint32 data, uint32 address) {
#ifdef MASKED_PSP_MEMORY
	*(uint32 *)(base + (address & MEMVIEW32_MASK)) = data;
#else
	*(uint32 *)(base + address) = data;
#endif
}

inline void WriteUnchecked_Float(float data, uint32 address) {
#ifdef MASKED_PSP_MEMORY
	*(float *)(base + (address & MEMVIEW32_MASK)) = data;
#else
	*(float *)(base + address) = data;
#endif
}

inline void WriteUnchecked_U16(uint16 data, uint32 address) {
#ifdef MASKED_PSP_MEMORY
	*(uint16 *)(base + (address & MEMVIEW32_MASK)) = data;
#else
	*(uint16 *)(base + address) = data;
#endif
}

inline void WriteUnchecked_U8(uint8 data, uint32 address) {
#ifdef MASKED_PSP_MEMORY
	(*(uint8 *)(base + (address & MEMVIEW32_MASK))) = data;
#else
	(*(uint8 *)(base + address)) = data;
#endif
}

#endif

inline float Read_Float(uint32 address) 
{
	uint32 ifloat = Read_U32(address);
	float f;
	memcpy(&f, &ifloat, sizeof(float));
	return f;
}

// used by JIT. Return zero-extended 32bit values
uint32 Read_U8_ZX(const uint32 address);
uint32 Read_U16_ZX(const uint32 address);

void Write_U8(const uint8 data, const uint32 address);
void Write_U16(const uint16 data, const uint32 address);
void Write_U32(const uint32 data, const uint32 address);
void Write_U64(const uint64 data, const uint32 address);

inline void Write_Float(float f, uint32 address)
{
	uint32 u;
	memcpy(&u, &f, sizeof(float));
	Write_U32(u, address);
}

u8* GetPointer(const uint32 address);
bool IsRAMAddress(const uint32 address);
bool IsVRAMAddress(const uint32 address);
bool IsScratchpadAddress(const uint32 address);

inline const char* GetCharPointer(const uint32 address) {
	return (const char *)GetPointer(address);
}

inline void MemcpyUnchecked(void *to_data, const uint32 from_address, const uint32 len)
{
	memcpy(to_data, GetPointerUnchecked(from_address), len);
}

inline void MemcpyUnchecked(const uint32 to_address, const void *from_data, const uint32 len)
{
	memcpy(GetPointerUnchecked(to_address), from_data, len);
}

inline void MemcpyUnchecked(const uint32 to_address, const uint32 from_address, const uint32 len)
{
	MemcpyUnchecked(GetPointer(to_address), from_address, len);
}

inline bool IsValidAddress(const uint32 address) {
	if (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF)) {
		return true;
	} else if (address >= 0x1F000000 && address <= 0x1F00FFFF) {
		return true;
	} else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
		return true;
	} else if (address >= 0xBFC00000 && address <= 0xBFC7FFFF) {
		return true;
	} else {
		return false;
	}
}

inline uint32 ValidSize(const uint32 address, const uint32 requested_size) {
	uint32 max_size;
	if (address <= 0x001FFFFF) {
		max_size =  0x00200000 - address;
	}else if(address >= 0x80000000 && address <= 0x801FFFFF){
		max_size =  0x80200000 - address;
	}else if((address >= 0xa0000000 && address <= 0xa01FFFFF)){
		max_size =  0xa0200000 - address;
	}else if (address >= 0x1F000000 && address <= 0x1F00FFFF) {
		max_size = 0x1F010000 - address;
	} else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
		max_size = 0x1F800400 - address;
	} else if (address >= 0xBFC00000 && address <= 0xBFC7FFFF) {
		max_size = 0xBFC80000 - address;
	} else {
		return false;
	}

	if (requested_size > max_size) {
		return max_size;
	}
	return requested_size;
}

inline bool IsValidRange(const uint32 address, const uint32 size) {
	return IsValidAddress(address) && ValidSize(address, size) == size;
}

};

template <typename T>
struct PSPPointer
{
	uint32 ptr;

	inline T &operator*() const
	{
#ifdef MASKED_PSP_MEMORY
		return *(T *)(Memory::base + (ptr & Memory::MEMVIEW32_MASK));
#else
		return *(T *)(Memory::base + ptr);
#endif
	}

	inline T &operator[](int i) const
	{
#ifdef MASKED_PSP_MEMORY
		return *((T *)(Memory::base + (ptr & Memory::MEMVIEW32_MASK)) + i);
#else
		return *((T *)(Memory::base + ptr) + i);
#endif
	}

	inline T *operator->() const
	{
#ifdef MASKED_PSP_MEMORY
		return (T *)(Memory::base + (ptr & Memory::MEMVIEW32_MASK));
#else
		return (T *)(Memory::base + ptr);
#endif
	}

	inline PSPPointer<T> operator+(int i) const
	{
		PSPPointer other;
		other.ptr = ptr + i * sizeof(T);
		return other;
	}

	inline PSPPointer<T> &operator=(uint32 p)
	{
		ptr = p;
		return *this;
	}

	inline PSPPointer<T> &operator+=(int i)
	{
		ptr = ptr + i * sizeof(T);
		return *this;
	}

	inline PSPPointer<T> operator-(int i) const
	{
		PSPPointer other;
		other.ptr = ptr - i * sizeof(T);
		return other;
	}

	inline PSPPointer<T> &operator-=(int i)
	{
		ptr = ptr - i * sizeof(T);
		return *this;
	}

	inline PSPPointer<T> &operator++()
	{
		ptr += sizeof(T);
		return *this;
	}

	inline PSPPointer<T> operator++(int i)
	{
		PSPPointer<T> other;
		other.ptr = ptr;
		ptr += sizeof(T);
		return other;
	}

	inline PSPPointer<T> &operator--()
	{
		ptr -= sizeof(T);
		return *this;
	}

	inline PSPPointer<T> operator--(int i)
	{
		PSPPointer<T> other;
		other.ptr = ptr;
		ptr -= sizeof(T);
		return other;
	}

	inline operator T*()
	{
#ifdef MASKED_PSP_MEMORY
		return (T *)(Memory::base + (ptr & Memory::MEMVIEW32_MASK));
#else
		return (T *)(Memory::base + ptr);
#endif
	}

	inline operator const T*() const
	{
#ifdef MASKED_PSP_MEMORY
		return (const T *)(Memory::base + (ptr & Memory::MEMVIEW32_MASK));
#else
		return (const T *)(Memory::base + ptr);
#endif
	}

	bool IsValid() const
	{
		return Memory::IsValidAddress(ptr);
	}

	static PSPPointer<T> Create(uint32 ptr) {
		PSPPointer<T> p;
		p = ptr;
		return p;
	}
};


inline uint32 PSP_GetScratchpadMemoryBase() { return 0x1f80000;}
inline uint32 PSP_GetScratchpadMemoryEnd() { return 0x1f80000 + Memory::SCRATCHPAD_SIZE;}

inline uint32 PSP_GetKernelMemoryBase() { return 0;}
inline uint32 PSP_GetUserMemoryEnd() { return PSP_GetKernelMemoryBase() + Memory::g_MemorySize;}
inline uint32 PSP_GetKernelMemoryEnd() { return 0x0000ffff;}
// "Volatile" RAM is between 0x08400000 and 0x08800000, can be requested by the
// game through sceKernelVolatileMemTryLock.

inline uint32 PSP_GetUserMemoryBase() { return 0x00010000;}

inline uint32 PSP_GetDefaultLoadAddress() { return 0;}
//inline uint32 PSP_GetDefaultLoadAddress() { return 0x0898dab0;}
/*
inline uint32 PSP_GetVidMemBase() { return 0x04000000;}
inline uint32 PSP_GetVidMemEnd() { return 0x04800000;}
*/
template <typename T>
inline bool operator==(const PSPPointer<T> &lhs, const PSPPointer<T> &rhs)
{
	return lhs.ptr == rhs.ptr;
}

template <typename T>
inline bool operator!=(const PSPPointer<T> &lhs, const PSPPointer<T> &rhs)
{
	return lhs.ptr != rhs.ptr;
}

template <typename T>
inline bool operator<(const PSPPointer<T> &lhs, const PSPPointer<T> &rhs)
{
	return lhs.ptr < rhs.ptr;
}

template <typename T>
inline bool operator>(const PSPPointer<T> &lhs, const PSPPointer<T> &rhs)
{
	return lhs.ptr > rhs.ptr;
}

template <typename T>
inline bool operator<=(const PSPPointer<T> &lhs, const PSPPointer<T> &rhs)
{
	return lhs.ptr <= rhs.ptr;
}

template <typename T>
inline bool operator>=(const PSPPointer<T> &lhs, const PSPPointer<T> &rhs)
{
	return lhs.ptr >= rhs.ptr;
}
