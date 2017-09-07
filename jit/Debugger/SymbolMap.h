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
#include <set>
#include <map>
#include <string>
#include <mutex>

#include "mednafen/mednafen-types.h"

enum SymbolType {
	ST_NONE     = 0,
	ST_FUNCTION = 1,
	ST_DATA     = 2,
	ST_ALL      = 3,
};

struct SymbolInfo {
	SymbolType type;
	uint32 address;
	uint32 size;
	uint32 moduleAddress;
};

struct SymbolEntry {
	std::string name;
	uint32 address;
	uint32 size;
};

struct LoadedModuleInfo {
	std::string name;
	uint32 address;
	uint32 size;
	bool active;
};

enum DataType {
	DATATYPE_NONE, DATATYPE_BYTE, DATATYPE_HALFWORD, DATATYPE_WORD, DATATYPE_ASCII
};

struct LabelDefinition{
#if defined(OS_WINDOWS) && !defined(UNICODE)
	std::string value;	
	std::string name;
#else
	std::wstring value;
	std::wstring name;
#endif
	
	
};

#ifdef _WIN32
struct HWND__;
typedef struct HWND__ *HWND;
#endif

class SymbolMap {
public:
	SymbolMap() : sawUnknownModule(false) {}
	void Clear();
	void SortSymbols();

	bool LoadSymbolMap(const char *filename);
	void SaveSymbolMap(const char *filename) const;
	bool LoadNocashSym(const char *ilename);
	void SaveNocashSym(const char *filename) const;

	SymbolType GetSymbolType(uint32 address) const;
	bool GetSymbolInfo(SymbolInfo *info, uint32 address, SymbolType symmask = ST_FUNCTION) const;
	uint32 GetNextSymbolAddress(uint32 address, SymbolType symmask);
	std::string GetDescription(unsigned int address) const;
	std::vector<SymbolEntry> GetAllSymbols(SymbolType symmask);

#ifdef _WIN32
	void FillSymbolListBox(HWND listbox, SymbolType symType) const;
#endif
	void GetLabels(std::vector<LabelDefinition> &dest) const;

	void AddModule(const char *name, uint32 address, uint32 size);
	void UnloadModule(uint32 address, uint32 size);
	uint32 GetModuleRelativeAddr(uint32 address, int moduleIndex = -1) const;
	uint32 GetModuleAbsoluteAddr(uint32 relative, int moduleIndex) const;
	int GetModuleIndex(uint32 address) const;
	bool IsModuleActive(int moduleIndex) const;
	std::vector<LoadedModuleInfo> getAllModules() const;

	void AddFunction(const char* name, uint32 address, uint32 size, int moduleIndex = -1);
	uint32 GetFunctionStart(uint32 address) const;
	int GetFunctionNum(uint32 address) const;
	uint32 GetFunctionSize(uint32 startAddress) const;
	uint32 GetFunctionModuleAddress(uint32 startAddress) const;
	bool SetFunctionSize(uint32 startAddress, uint32 newSize);
	bool RemoveFunction(uint32 startAddress, bool removeName);
	// Search for the first address their may be a function after address.
	// Only valid for currently loaded modules.  Not guaranteed there will be a function.
	uint32 FindPossibleFunctionAtAfter(uint32 address) const;

	void AddLabel(const char* name, uint32 address, int moduleIndex = -1);
	std::string GetLabelString(uint32 address) const;
	void SetLabelName(const char* name, uint32 address);
	bool GetLabelValue(const char* name, uint32& dest);

	void AddData(uint32 address, uint32 size, DataType type, int moduleIndex = -1);
	uint32 GetDataStart(uint32 address) const;
	uint32 GetDataSize(uint32 startAddress) const;
	uint32 GetDataModuleAddress(uint32 startAddress) const;
	DataType GetDataType(uint32 startAddress) const;

	static const uint32 INVALID_ADDRESS = (uint32)-1;

	void UpdateActiveSymbols();

private:
	void AssignFunctionIndices();
	const char *GetLabelName(uint32 address) const;
	const char *GetLabelNameRel(uint32 relAddress, int moduleIndex) const;

	struct FunctionEntry {
		uint32 start;
		uint32 size;
		int index;
		int module;
	};

	struct LabelEntry {
		uint32 addr;
		int module;
		char name[128];
	};

	struct DataEntry {
		DataType type;
		uint32 start;
		uint32 size;
		int module;
	};

	struct ModuleEntry {
		// Note: this index is +1, 0 matches any for backwards-compat.
		int index;
		uint32 start;
		uint32 size;
		char name[128];
	};

	// These are flattened, read-only copies of the actual data in active modules only.
	std::map<uint32, const FunctionEntry> activeFunctions;
	std::map<uint32, const LabelEntry> activeLabels;
	std::map<uint32, const DataEntry> activeData;

	// This is indexed by the end address of the module.
	std::map<uint32, const ModuleEntry> activeModuleEnds;

	typedef std::pair<int, uint32> SymbolKey;

	// These are indexed by the module id and relative address in the module.
	std::map<SymbolKey, FunctionEntry> functions;
	std::map<SymbolKey, LabelEntry> labels;
	std::map<SymbolKey, DataEntry> data;
	std::vector<ModuleEntry> modules;

	mutable std::recursive_mutex lock_;
	bool sawUnknownModule;
};

extern SymbolMap *g_symbolMap;

