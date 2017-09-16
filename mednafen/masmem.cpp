#include "masmem.h"
#include "mednafen/mednafen-types.h"
#include "mednafen/psx/psx.h"
#include "mednafen/psx/mdec.h"
#include "mednafen/psx/frontio.h"
#include "mednafen/psx/timer.h"
#include "mednafen/psx/sio.h"
#include "mednafen/psx/cdc.h"
#include "mednafen/psx/spu.h"
#include "mednafen/mempatcher.h"
#include "jit/Common/Swap.h"
#include "mednafen-endian.h"
#include "jit/JitCommon/JitCommon.h"
#include "jittimestamp.h"

MultiAccessSizeMem<1024, uint32, false> *ScratchRAM = NULL;
MultiAccessSizeMem<512 * 1024, uint32, false> *BIOSROM = NULL;
MultiAccessSizeMem<65536, uint32, false> *PIOMem = NULL;
MultiAccessSizeMem<2048 * 1024, uint32, false> *MainRAM = new MultiAccessSizeMem<2048 * 1024, uint32, false>();


//externs needed for R/WFromHardware
extern PS_CPU *CPU;
extern PS_SPU *SPU;
extern PS_CDC *CDC;
extern FrontIO *FIO;

extern uint32_t TextMem_Start;
extern std::vector<uint8> TextMem;

//TODO Make this centralized somehow, shouldn't be in here and libretro.cpp
extern unsigned DMACycleSteal;

struct event_list_entry
{
   uint32_t which;
   int32_t event_time;
   event_list_entry *prev;
   event_list_entry *next;
};

extern event_list_entry events[PSX_EVENT__COUNT];

static const uint32_t SysControl_Mask[9] = { 0x00ffffff, 0x00ffffff, 0xffffffff, 0x2f1fffff,
                                             0xffffffff, 0x2f1fffff, 0x2f1fffff, 0xffffffff,
                                             0x0003ffff };

static const uint32_t SysControl_OR[9] = { 0x1f000000, 0x1f000000, 0x00000000, 0x00000000,
                                           0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                           0x00000000 };

static struct
{
    union
    {
        struct
        {
            uint32_t PIO_Base;   // 0x1f801000  // BIOS Init: 0x1f000000, Writeable bits: 0x00ffffff(assumed, verify), FixedOR = 0x1f000000
            uint32_t Unknown0;   // 0x1f801004  // BIOS Init: 0x1f802000, Writeable bits: 0x00ffffff, FixedOR = 0x1f000000
            uint32_t Unknown1;   // 0x1f801008  // BIOS Init: 0x0013243f, ????
            uint32_t Unknown2;   // 0x1f80100c  // BIOS Init: 0x00003022, Writeable bits: 0x2f1fffff, FixedOR = 0x00000000

            uint32_t BIOS_Mapping;  // 0x1f801010  // BIOS Init: 0x0013243f, ????
            uint32_t SPU_Delay;  // 0x1f801014  // BIOS Init: 0x200931e1, Writeable bits: 0x2f1fffff, FixedOR = 0x00000000 - Affects bus timing on access to SPU
            uint32_t CDC_Delay;  // 0x1f801018  // BIOS Init: 0x00020843, Writeable bits: 0x2f1fffff, FixedOR = 0x00000000
            uint32_t Unknown4;   // 0x1f80101c  // BIOS Init: 0x00070777, ????
            uint32_t Unknown5;   // 0x1f801020  // BIOS Init: 0x00031125(but rewritten with other values often), Writeable bits: 0x0003ffff, FixedOR = 0x00000000 -- Possibly CDC related
        };
        uint32_t Regs[9];
    };
} SysControl;

namespace Memory
{
    uint8 *base;

    void Init(bool WantPIOMem){
        if(WantPIOMem)
            PIOMem = new MultiAccessSizeMem<65536, uint32, false>();
        
        BIOSROM = new MultiAccessSizeMem<512 * 1024, uint32, false>();
        ScratchRAM = new MultiAccessSizeMem<1024, uint32, false>();
        base = MainRAM->data8;
    }
    
    void Clear(){
        if(PIOMem){
            delete PIOMem;
            PIOMem = new MultiAccessSizeMem<65536, uint32, false>();
        }
        if(BIOSROM){
            delete BIOSROM;
            BIOSROM = new MultiAccessSizeMem<512 * 1024, uint32, false>();
        }
        if(ScratchRAM){
            delete ScratchRAM;
            ScratchRAM = new MultiAccessSizeMem<1024, uint32, false>();
        }
        memset(MainRAM->data8, 0, sizeof(MainRAM->data8));
    }

    Opcode Read_Instruction(uint32 address, bool resolve_replacements){
        Opcode inst = Opcode(Read_U32(address));
        if (MIPS_IS_RUNBLOCK(inst.encoding) && MIPSComp::jit) {
            inst = MIPSComp::jit->GetOriginalOp(inst);
        }
        return inst;
    }
    
    Opcode Read_Opcode_JIT(uint32 address){
        Opcode inst = Opcode(Read_U32(address));
        if (MIPS_IS_RUNBLOCK(inst.encoding) && MIPSComp::jit) {
            return MIPSComp::jit->GetOriginalOp(inst);
        } else {
            return inst;
        }
    }
    
    void Write_Opcode_JIT(const uint32 _Address, const Opcode& _Value)
    {
        Memory::WriteUnchecked_U32(_Value.encoding, _Address);
    }
    
// =================================
// From Memmap.cpp
// ----------------

// Read and write shortcuts

// GetPointer must always return an address in the bottom 32 bits of address space, so that 64-bit
// programs don't have problems directly addressing any part of memory.
//TODO it so it makes sense on PSX
uint8 *GetPointer(const uint32 address) {
    //INFO_LOG(GETPOINTER, "Getting pointer at %p", address);
	if (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF)) {
		// RAM
		return MainRAM->data8 + (address & 0x1FFFFFFF);
	} else if(address >= 0x1F000000 && address <= 0x1F7FFFFF) {
        if(PIOMem)
        {
            if((address & 0x7FFFFF) < 65536)
            {
                return PIOMem->data8 + (address & 0x7FFFFF);
            }
            else if((address & 0x7FFFFF) < (65536 + TextMem.size()))
            {
                return &TextMem[(address & 0x7FFFFF) - 65536];
            }
        }
        ERROR_LOG(MEMMAP, "Unknown GetPointer %08x PC %08x LR %08x\n", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
        return nullptr;
    } else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
		// Scratchpad
		return ScratchRAM->data8 + address;
	} else if (address >= 0xBFC00000 && address <= 0xBFC7FFFF) {
		// BIOS
		return BIOSROM->data8 + address;
	} else {
		ERROR_LOG(MEMMAP, "Unknown GetPointer %08x PC %08x LR %08x\n", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
		return nullptr;
	}
}

static const uint32_t addr_mask[8] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x7FFFFFFF, 0x1FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

template <typename T>
inline void ReadFromHardware(T &var, uint32 address) {
    //INFO_LOG(READ, "Reading at address %p\n", address);
    JTTTS_increment_timestamp(DMACycleSteal);
    uint32 origAddress = address;
    address &= addr_mask[address >> 29];
    //if(address == 0xa0 && IsWrite)
    // DBG_Break();
    //INFO_LOG(READ, "Masked address: %p\n", address);

    if(address >= 0x1F800000 && address <= 0x1F8003FF)
    {
        var = ScratchRAM->Read<T>(address & 0x3FF);
        return; 
    }

    if(address < 0x00800000)
    {
         var = MainRAM->Read<T>(address & 0x1FFFFF);
        return;
    }
    //TODO might not need this anymore?
    if(address >= 0xbfc00000 && address <= 0xbfc7ffff)
    {
        var = BIOSROM->Read<T>(address & 0x7FFFF);
        return;
    }

    if(address >= 0x1FC00000 && address <= 0x1FC7FFFF)
    {
        //Bios mirror?
        var = BIOSROM->Read<T>(address & 0x7FFFF);
        return;
    }

    if(JITTS_get_timestamp() >= events[PSX_EVENT__SYNFIRST].next->event_time)
        PSX_EventHandler(JITTS_get_timestamp());

    if(address >= 0x1F801000 && address <= 0x1F802FFF)
    {

        //if(IsWrite)
        // printf("HW Write%d: %08x %08x\n", (unsigned int)(sizeof(T)*8), (unsigned int)address, (unsigned int)var);
        //else
        // printf("HW Read%d: %08x\n", (unsigned int)(sizeof(T)*8), (unsigned int)address);

        if(address >= 0x1F801C00 && address <= 0x1F801FFF) // SPU
        {
            if(sizeof(T) == 4)
            {
            
                JTTTS_increment_timestamp(36);

                if(JITTS_get_timestamp() >= events[PSX_EVENT__SYNFIRST].next->event_time)
                    PSX_EventHandler(JITTS_get_timestamp());

                var = SPU->Read(JITTS_get_timestamp(), address) | (SPU->Read(JITTS_get_timestamp(), address | 2) << 16);
            
            }
            else
            {
                JTTTS_increment_timestamp(16); // Just a guess, need to test.

                    if(JITTS_get_timestamp() >= events[PSX_EVENT__SYNFIRST].next->event_time)
                        PSX_EventHandler(JITTS_get_timestamp());

                    var = SPU->Read(JITTS_get_timestamp(), address & ~1);
            }
        return;
        }     // End SPU


        // CDC: TODO - 8-bit access.
        if(address >= 0x1f801800 && address <= 0x1f80180F)
        {
            INFO_LOG(READ, "Reading CD at address %p\n", address);
            JTTTS_increment_timestamp( 6 * sizeof(T)); //24;
            var = CDC->Read(JITTS_get_timestamp(), address & 0x3);
            return;
        }

        if(address >= 0x1F801810 && address <= 0x1F801817)
        {
            JTTTS_increment_timestamp(1);
            var = GPU_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801820 && address <= 0x1F801827)
        {
            JTTTS_increment_timestamp(1);
            var = MDEC_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801000 && address <= 0x1F801023)
        {
            unsigned index = (address & 0x1F) >> 2;
            JTTTS_increment_timestamp(1);
            var = SysControl.Regs[index] | SysControl_OR[index];
            var >>= (address & 3) * 8;
            return;
        }

        if(address >= 0x1F801040 && address <= 0x1F80104F)
        {
            JTTTS_increment_timestamp(1);
            var = FIO->Read(JITTS_get_timestamp(), address);
        }

        if(address >= 0x1F801050 && address <= 0x1F80105F)
        {
            JTTTS_increment_timestamp(1);
            var = SIO_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801070 && address <= 0x1F801077) // IRQ
        {
            JTTTS_increment_timestamp(1);
            var = ::IRQ_Read(address);
            return;
        }

        if(address >= 0x1F801080 && address <= 0x1F8010FF)    // DMA
        {
            JTTTS_increment_timestamp(1);
            var = DMA_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801100 && address <= 0x1F80113F) // Root counters
        {
            JTTTS_increment_timestamp(1);
            var = TIMER_Read(JITTS_get_timestamp(), address);
            return;
        }
    }


    if(address >= 0x1F000000 && address <= 0x1F7FFFFF)
    {
        //if((address & 0x7FFFFF) <= 0x84)
        //PSX_WARNING("[PIO] Read%d from 0x%08x at time %d", (int)(sizeof(T) * 8), address, timestamp);

        var = (T)(~0U); // A game this affects:  Tetris with Cardcaptor Sakura

        if(PIOMem)
        {
            if((address & 0x7FFFFF) < 65536)
            {
                var = PIOMem->Read<T>(address & 0x7FFFFF);
            }
            else if((address & 0x7FFFFF) < (65536 + TextMem.size()))
            {
                switch(sizeof(T))
                {
                    case 1: var = TextMem[(address & 0x7FFFFF) - 65536]; break;
                    case 2: var = MDFN_de16lsb(&TextMem[(address & 0x7FFFFF) - 65536]); break;
                    case 4: var = MDFN_de32lsb(&TextMem[(address & 0x7FFFFF) - 65536]); break;
                }
            }
        }
        
        return;
    }

    if(address == 0xFFFE0130) // Per tests on PS1, ignores the access(sort of, on reads the value is forced to 0 if not aligned) if not aligned to 4-bytes.
    {
        var = CPU->GetBIU();
        return;
    }
    var = 0;
    DEBUG_LOG(MEM, "[MEM] Unknown read %d from %08x at time %d\n", (int)(sizeof(T) * 8), origAddress, JITTS_get_timestamp());
}

template <typename T>
inline void WriteToHardware(uint32 address, const T data) {
    //if(address == 0xa0 && IsWrite)
    // DBG_Break();
    //INFO_LOG(WRITE, "Writing at address %p\n", address);
    uint32 origAddress = address;
    address &= addr_mask[address >> 29];

    if(address >= 0x1F800000 && address <= 0x1F8003FF)
    {
        ScratchRAM->Write<T>(address & 0x3FF, data);
        return;
    }

    if(address < 0x00800000)
    {
        //JTTTS_increment_timestamp(1); // Best-case timing.
        MainRAM->Write<T>(address & 0x1FFFFF, data);
        return;
    }
    
    if(address >= 0xbfc00000 && address <= 0xbfc7ffff)
    {
        BIOSROM->Write<T>(address & 0x7FFFF, data);
        return;
    }

    if(address >= 0x1FC00000 && address <= 0x1FC7FFFF)
    {
        //Can't write to BIOS
        //Except you totally can, and the JIT compiler relies on it.
        BIOSROM->Write<T>(address & 0x7FFFF, data);
        return;
    }
    //This probably isn't appropriate here!
    if(JITTS_get_timestamp() >= events[PSX_EVENT__SYNFIRST].next->event_time)
        PSX_EventHandler(JITTS_get_timestamp());

    if(address >= 0x1F801000 && address <= 0x1F802FFF)
    {

        //if(IsWrite)
        // printf("HW Write%d: %08x %08x\n", (unsigned int)(sizeof(T)*8), (unsigned int)address, (unsigned int)data);
        //else
        // printf("HW Read%d: %08x\n", (unsigned int)(sizeof(T)*8), (unsigned int)address);

        if(address >= 0x1F801C00 && address <= 0x1F801FFF) // SPU
        {
        if(sizeof(T) == 4)
        {
                SPU->Write(JITTS_get_timestamp(), address | 0, data);
                SPU->Write(JITTS_get_timestamp(), address | 2, data >> 16);
        }
        else
        {
            SPU->Write(JITTS_get_timestamp(), address & ~1, data);
        }
        return;
        }     // End SPU


        // CDC: TODO - 8-bit access.
        if(address >= 0x1f801800 && address <= 0x1f80180F)
        {
            INFO_LOG(READ, "Writing CD at address %p\n", address);
            CDC->Write(JITTS_get_timestamp(), address & 0x3, data);
            return;
        }

        if(address >= 0x1F801810 && address <= 0x1F801817)
        {
            GPU_Write(JITTS_get_timestamp(), address, data);
            return;
        }

        if(address >= 0x1F801820 && address <= 0x1F801827)
        {
            MDEC_Write(JITTS_get_timestamp(), address, data);
            return;
        }

        if(address >= 0x1F801000 && address <= 0x1F801023)
        {
            unsigned index = (address & 0x1F) >> 2;
            //if(address == 0x1F801014 && IsWrite)
            // fprintf(stderr, "%08x %08x\n",address,data);
            T dat = data;
            dat <<= (address & 3) * 8;
            SysControl.Regs[index] = dat & SysControl_Mask[index];
            return;
        }

        if(address >= 0x1F801040 && address <= 0x1F80104F)
        {
            FIO->Write(JITTS_get_timestamp(), address, data);
            return;
        }

        if(address >= 0x1F801050 && address <= 0x1F80105F)
        {
            SIO_Write(JITTS_get_timestamp(), address, data);
            return;
        }

        if(address >= 0x1F801070 && address <= 0x1F801077) // IRQ
        {
            ::IRQ_Write(address, data);
            return;
        }

        if(address >= 0x1F801080 && address <= 0x1F8010FF)    // DMA
        {
            DMA_Write(JITTS_get_timestamp(), address, data);
            return;
        }

        if(address >= 0x1F801100 && address <= 0x1F80113F) // Root counters
        {
            TIMER_Write(JITTS_get_timestamp(), address, data);
            return;
        }
    }


    if(address >= 0x1F000000 && address <= 0x1F7FFFFF)
    {
        return;
    }

    if(address == 0xFFFE0130) // Per tests on PS1, ignores the access(sort of, on reads the value is forced to 0 if not aligned) if not aligned to 4-bytes.
    {
        CPU->SetBIU(data);
        return;
    }

    DEBUG_LOG(MEM, "[MEM] Unknown write%d to %08x at time %d, =%08x(%d)\n", (int)(sizeof(T) * 8), origAddress, JITTS_get_timestamp(), &data, data);
}

bool IsRAMAddress(const uint32 address) {
	return (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF));
}

bool IsScratchpadAddress(const uint32 address) {
	return (address >= 0x1F800000 && address <= 0x1F8003FF);
}
//TODO make sure this is valid
bool IsValidAddress(const uint32 address){
    if (address <= 0x001FFFFF || 
		(address >= 0x80000000 && address <= 0x801FFFFF) ||
		(address >= 0xa0000000 && address <= 0xa01FFFFF)) {
        //RAM
		return true;
	} else if (address >= 0x1F000000 && address <= 0x1F00FFFF) {
        //Parallel port
        return true;
	} else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
        //Scratch Pad
        return true;
	} else if (address >= 0xBFC00000 && address <= 0xBFC7FFFF) {
        //BIOS
        return true;
	} else if (address >= 0x1f801000 && address <= 0x1f802fff) {
        //HArdware
        return true;
    } else{
		return false;
	}
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

} //Namespace Memory