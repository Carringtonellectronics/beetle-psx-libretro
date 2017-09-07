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


#include "jit/Memory/MemMap.h"

#include "jit/MIPS.h"

#include "jit/Common/Swap.h"
namespace Memory
{

// =================================
// From Memmap.cpp
// ----------------

// Read and write shortcuts

// GetPointer must always return an address in the bottom 32 bits of address space, so that 64-bit
// programs don't have problems directly addressing any part of memory.
//TODO it so it makes sense on PSX
uint8 *GetPointer(const uint32 address) {
	if (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF)) {
		// RAM
		return GetPointerUnchecked(address);
	} else if (address >= 0x1F000000 && address <= 0x1F00FFFF) {
		// Parallel port
		//TODO make this call parallel port stuff
		return GetPointerUnchecked(address);
	} else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
		// Scratchpad
		return GetPointerUnchecked(address);
	} else if (address >= 0xBFC00000 && address <= 0xBFC7FFFF) {
		// BIOS
		return GetPointerUnchecked(address);
	} else {
		ERROR_LOG(MEMMAP, "Unknown GetPointer %08x PC %08x LR %08x", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
		return nullptr;
	}
}

template <typename T>
inline void ReadFromHardware(T &var, const uint32 address) {
	// TODO: Figure out the fastest order of tests for both read and write (they are probably different).
	// TODO: Make sure this represents the mirrors in a correct way.

	// Could just do a base-relative read, too.... TODO
	//TODO maybe speed this up somehow?
	if (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF)) {
		// RAM
		var = *(T*)GetPointerUnchecked(address);
	} else if (address >= 0x1F000000 && address <= 0x1F00FFFF) {
		// Parallel port
		//TODO make this call parallel port stuff
		var = *(T*)GetPointerUnchecked(address);
	} else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
		// Scratchpad
		var = *(T*)GetPointerUnchecked(address);
	} else if (address >= 0xBFC00000 && address <= 0xBFC7FFFF) {
		// BIOS
		var = *(T*)GetPointerUnchecked(address);
	} else {
		// In jit, we only flush PC when bIgnoreBadMemAccess is off.
		if (currentMIPS){
			WARN_LOG(MEMMAP, "WriteToHardware: Invalid address %08x	PC %08x LR %08x", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
		}
	}
}

template <typename T>
inline void WriteToHardware(uint32 address, const T data) {
	// Could just do a base-relative write, too.... TODO
	//TODO maybe speed this up somehow?
	if (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF)) {
		// RAM
		*(T*)GetPointerUnchecked(address) = data;
	} else if (address >= 0x1F000000 && address <= 0x1F00FFFF) {
		// Parallel port
		//TODO make this call parallel port stuff
		*(T*)GetPointerUnchecked(address) = data;
	} else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
		// Scratchpad
		*(T*)GetPointerUnchecked(address) = data;
	} else if (address >= 0xBFC00000 && address <= 0xBFC7FFFF) {
		// BIOS
		*(T*)GetPointerUnchecked(address) = data;
	} else {
		// In jit, we only flush PC when bIgnoreBadMemAccess is off.
		if (currentMIPS){
			WARN_LOG(MEMMAP, "WriteToHardware: Invalid address %08x	PC %08x LR %08x", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
		}
	}
}

// =====================

bool IsRAMAddress(const uint32 address) {
	return (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF));
}
//TODO implement?
bool IsVRAMAddress(const uint32 address) {
	return false;
}

bool IsScratchpadAddress(const uint32 address) {
	return (address >= 0x1F800000 && address <= 0x1F8003FF);
}

uint8 Read_U8(const uint32 _Address)
{		
	uint8 _var = 0;
	ReadFromHardware<u8>(_var, _Address);
	return (u8)_var;
}

uint16 Read_U16(const uint32 _Address)
{
	uint16 _var = 0;
	ReadFromHardware<u16_le>(_var, _Address);
	return (u16)_var;
}

uint32 Read_U32(const uint32 _Address)
{
	uint32 _var = 0;
	ReadFromHardware<u32_le>(_var, _Address);
	return _var;
}

uint64 Read_U64(const uint32 _Address)
{
	u64_le _var = 0;
	ReadFromHardware<u64_le>(_var, _Address);
	return _var;
}

uint32 Read_U8_ZX(const uint32 _Address)
{
	return (u32)Read_U8(_Address);
}

uint32 Read_U16_ZX(const uint32 _Address)
{
	return (u32)Read_U16(_Address);
}

void Write_U8(const uint8 _Data, const uint32 _Address)	
{
	WriteToHardware<u8>(_Address, _Data);
}

void Write_U16(const uint16 _Data, const uint32 _Address)
{
	WriteToHardware<u16_le>(_Address, _Data);
}

void Write_U32(const uint32 _Data, const uint32 _Address)
{	
	WriteToHardware<u32_le>(_Address, _Data);
}

void Write_U64(const uint64 _Data, const uint32 _Address)
{
	WriteToHardware<u64_le>(_Address, _Data);
}

#ifdef SAFE_MEMORY

uint8 ReadUnchecked_U8(const uint32 _Address)
{
	uint8 _var = 0;
	ReadFromHardware<u8>(_var, _Address);
	return _var;
}

uint16 ReadUnchecked_U16(const uint32 _Address)
{
	uint16 _var = 0;
	ReadFromHardware<u16_le>(_var, _Address);
	return _var;
}

uint32 ReadUnchecked_U32(const uint32 _Address)
{
	uint32 _var = 0;
	ReadFromHardware<u32_le>(_var, _Address);
	return _var;
}

void WriteUnchecked_U8(const uint8 _iValue, const uint32 _Address)
{
	WriteToHardware<u8>(_Address, _iValue);
}

void WriteUnchecked_U16(const uint16 _iValue, const uint32 _Address)
{
	WriteToHardware<u16_le>(_Address, _iValue);
}

void WriteUnchecked_U32(const uint32 _iValue, const uint32 _Address)
{
	WriteToHardware<u32_le>(_Address, _iValue);
}

#endif

}	// namespace Memory
