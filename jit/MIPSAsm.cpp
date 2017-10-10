#include <cstdarg>
#include <cstring>
#include <vector>

#include "mednafen/mednafen-types.h"

#if defined(OS_WINDOWS) || defined(__ANDROID__)
// Temporarily turned off on Android
//TODO have this actually work the way it should
//#define USE_ARMIPS
#endif


#ifdef USE_ARMIPS
// This has to be before basictypes to avoid a define conflict.
#include "ext/armips/Core/Assembler.h"
#endif

#include "jit/Memory/MemMapHelpers.h"
#include "jit/JitCommon/JitCommon.h"
#include "jit/MIPSAsm.h"

namespace MIPSAsm
{	
	static std::wstring errorText;

std::wstring GetAssembleError()
{
	return errorText;
}

#ifdef USE_ARMIPS
class PspAssemblerFile: public AssemblerFile
{
public:
	PspAssemblerFile() {
		address = 0;
	}

	bool open(bool onlyCheck) override{ return true; };
	void close() override { };
	bool isOpen() override { return true; };
	bool write(void* data, size_t length) override {
		if (!Memory::IsValidAddress((u32)(address+length-1)))
			return false;

		Memory::Memcpy((u32)address,data,(u32)length);
		
		// In case this is a delay slot or combined instruction, clear cache above it too.
		if (MIPSComp::jit)
			MIPSComp::jit->InvalidateCacheAt((u32)(address - 4),(int)length+4);

		address += length;
		return true;
	}
	int64_t getVirtualAddress() override { return address; };
	int64_t getPhysicalAddress() override { return getVirtualAddress(); };
	int64_t getHeaderSize() override { return 0; }
	bool seekVirtual(int64_t virtualAddress) override {
		if (!Memory::IsValidAddress(virtualAddress))
			return false;
		address = virtualAddress;
		return true;
	}
	bool seekPhysical(int64_t physicalAddress) override { return seekVirtual(physicalAddress); }
	const std::wstring &getFileName() override { return dummyWFilename_; }
private:
	u64 address;
	std::wstring dummyWFilename_;
};
#endif

bool MipsAssembleOpcode(const char* line, DebugInterface* cpu, uint32 address)
{
#ifdef USE_ARMIPS
	PspAssemblerFile file;
	StringList errors;

	wchar_t str[64];
	swprintf(str,64,L".psp\n.org 0x%08X\n",address);

	ArmipsArguments args;
	args.mode = ArmipsMode::MEMORY;
	args.content = str + ConvertUTF8ToWString(line);
	args.silent = true;
	args.memoryFile = &file;
	args.errorsResult = &errors;

	if (g_symbolMap) {
		g_symbolMap->GetLabels(args.labels);
	}

	errorText = L"";
	if (!runArmips(args))
	{
		for (size_t i = 0; i < errors.size(); i++)
		{
			errorText += errors[i];
			if (i != errors.size()-1)
				errorText += L"\n";
		}

		return false;
	}

	return true;
#else
	errorText = L"Unsupported platform";
	return false;
#endif
}

}  // namespace
