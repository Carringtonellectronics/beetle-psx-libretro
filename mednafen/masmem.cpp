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

#ifdef JIT
#include "jit/JitCommon/JitCommon.h"
#include "jittimestamp.h"
#include "jit/Memory/jitICache.h"
#endif

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

static int tsDelta;

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
#ifdef JIT
    
    Opcode Read_Opcode_JIT(MIPSComp::JitState* js){
        tsDelta = 0;
        Opcode inst = Read_Instruction(js->compilerPC);

        js->downcountAmount += tsDelta;

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

static const uint32_t addr_mask[8] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x7FFFFFFF, 0x1FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
// =================================
// From Memmap.cpp
// ----------------

// Read and write shortcuts

// GetPointer must always return an address in the bottom 32 bits of address space, so that 64-bit
// programs don't have problems directly addressing any part of memory.
//TODO it so it makes sense on PSX
uint8 *GetPointer(uint32 address) {
    //This is essntially a read, so we'll treat it as if it is
    INFO_LOG(GETPOINTER, "Getting pointer at 0x%08x\n", address);
	JTTTS_increment_timestamp(DMACycleSteal);
    uint32 origAddress = address;
    address &= addr_mask[address >> 29];
    //if(address == 0xa0 && IsWrite)
    // DBG_Break();
    //INFO_LOG(READ, "Masked address: %p\n", address);

    if(origAddress >= 0x1F800000 && origAddress <= 0x1F8003FF)
    {
        return ScratchRAM->data8 + (origAddress & 0x3FF);
         
    }

    if(address < 0x00800000)
    {
        return MainRAM->data8 + (address & 0x1FFFFF);
    }

    if(address >= 0x1FC00000 && address <= 0x1FC7FFFF)
    {
        return BIOSROM->data8 + (address & 0x7FFFF);
    }

    DEBUG_LOG(MEM, "[MEM] Unknown GetPointer from %08x at time %d, PC = 0x%08x\n", origAddress, JITTS_get_timestamp(), currentMIPS->pc);
    return nullptr;
}

template <typename T, bool increment = true>
inline void ReadFromHardware(T &var, uint32 address) {
    //INFO_LOG(READ, "Reading at address %p\n", address);
    if(increment){
        JTTTS_increment_timestamp(DMACycleSteal);
    }
    //if(address == 0xa0 && IsWrite)
    // DBG_Break();
    //INFO_LOG(READ, "Masked address: %p\n", address);
    
    if(address < 0x00800000)
    {
        if(increment){
            JTTTS_increment_timestamp(3);
        }
        var = MainRAM->Read<T>(address & 0x1FFFFF);
        return;
    }

    if(address >= 0x1FC00000 && address <= 0x1FC7FFFF)
    {
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
                if(increment){
                    JTTTS_increment_timestamp(36);
                }
                if(JITTS_get_timestamp() >= events[PSX_EVENT__SYNFIRST].next->event_time)
                    PSX_EventHandler(JITTS_get_timestamp());

                var = SPU->Read(JITTS_get_timestamp(), address) | (SPU->Read(JITTS_get_timestamp(), address | 2) << 16);
            
            }
            else
            {
                if(increment){
                    JTTTS_increment_timestamp(16); // Just a guess, need to test.
                }
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
            if(increment){
                JTTTS_increment_timestamp( 6 * sizeof(T)); //24;
            }
            var = CDC->Read(JITTS_get_timestamp(), address & 0x3);
            return;
        }

        if(address >= 0x1F801810 && address <= 0x1F801817)
        {
            if(increment){
                JTTTS_increment_timestamp(1);
            }
            var = GPU_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801820 && address <= 0x1F801827)
        {
            if(increment){
                JTTTS_increment_timestamp(1);
            }
            var = MDEC_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801000 && address <= 0x1F801023)
        {
            unsigned index = (address & 0x1F) >> 2;
            if(increment){
                JTTTS_increment_timestamp(1);
            }
            var = SysControl.Regs[index] | SysControl_OR[index];
            var >>= (address & 3) * 8;
            return;
        }

        if(address >= 0x1F801040 && address <= 0x1F80104F)
        {
            if(increment){
                JTTTS_increment_timestamp(1);
            }
            var = FIO->Read(JITTS_get_timestamp(), address);
        }

        if(address >= 0x1F801050 && address <= 0x1F80105F)
        {
            if(increment){
                JTTTS_increment_timestamp(1);
            }
            var = SIO_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801070 && address <= 0x1F801077) // IRQ
        {
            if(increment){
                JTTTS_increment_timestamp(1);
            }
            var = ::IRQ_Read(address);
            return;
        }

        if(address >= 0x1F801080 && address <= 0x1F8010FF)    // DMA
        {
            if(increment){
                JTTTS_increment_timestamp(1);
            }
            var = DMA_Read(JITTS_get_timestamp(), address);
            return;
        }

        if(address >= 0x1F801100 && address <= 0x1F80113F) // Root counters
        {
            if(increment){
                JTTTS_increment_timestamp(1);
            }
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
            if((address & 0x007FFFFF) < 65536)
            {
                var = PIOMem->Read<T>(address & 0x007FFFFF);
            }
            else if((address & 0x007FFFFF) < (65536 + TextMem.size()))
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
        var = BIU;
        return;
    }
    var = (T)~0U;
    DEBUG_LOG(MEM, "[MEM] Unknown read %d from 0x%08x at time %d\n", (int)(sizeof(T) * 8), address, JITTS_get_timestamp());
}


template<typename T, bool increment = true>
INLINE void ReadMemory(T &var, uint32_t address)
{
   T ret;

   if(address >= 0x1F800000 && address <= 0x1F8003FF)
   {
      var = ScratchRAM->Read<T>(address & 0x3FF);
      return;
   }
/*
   if(address == 0x28){
       INFO_LOG(READ, "Reading from 0x28.\n");
   }
*/
   address &= addr_mask[address >> 29];
   //timestamp += (ReadFudge >> 4) & 2;

   //assert(!(CP0.SR & 0x10000));

   ReadFromHardware<T, increment>(var, address);
}


Opcode Read_Instruction(uint32 address, bool resolve_replacements){
    /*
    Opcode instr = Opcode(ICache.ICache[(address & 0xFFC) >> 2].Data);
    
    if(ICache.ICache[(address & 0xFFC) >> 2].TV != address)
    {
        // FIXME: Handle executing out of scratchpad.
        if(address >= 0xA0000000 || !(BIU & 0x800))
        {
            instr = Opcode(LoadU32_LE((uint32_t *)&FastMap[address >> FAST_MAP_SHIFT][address]));
            tsDelta += 4;
        }
        else
        {
            __ICache *ICI = &ICache.ICache[((address & 0xFF0) >> 2)];
            const uint32_t *FMP = (uint32_t *)&FastMap[(address &~ 0xF) >> FAST_MAP_SHIFT][address &~ 0xF];

            // | 0x2 to simulate (in)validity bits.
            ICI[0x00].TV = (address &~ 0xF) | 0x00 | 0x2;
            ICI[0x01].TV = (address &~ 0xF) | 0x04 | 0x2;
            ICI[0x02].TV = (address &~ 0xF) | 0x08 | 0x2;
            ICI[0x03].TV = (address &~ 0xF) | 0x0C | 0x2;

            // When overclock is enabled, remove code cache fetch latency
            tsDelta += 3;
            
            switch(address & 0xC)
            {
            case 0x0:
                tsDelta += 1;
                ICI[0x00].TV &= ~0x2;
                ICI[0x00].Data = LoadU32_LE(&FMP[0]);
            case 0x4:
                tsDelta += 1;
                ICI[0x01].TV &= ~0x2;
                ICI[0x01].Data = LoadU32_LE(&FMP[1]);
            case 0x8:
                tsDelta += 1;
                ICI[0x02].TV &= ~0x2;
                ICI[0x02].Data = LoadU32_LE(&FMP[2]);
            case 0xC:
                tsDelta += 1;
                ICI[0x03].TV &= ~0x2;
                ICI[0x03].Data = LoadU32_LE(&FMP[3]);
                break;
            }
            instr = Opcode(ICache.ICache[(address & 0xFFC) >> 2].Data);
        }
    }
    */
   /* if (MIPS_IS_RUNBLOCK(inst.encoding) && MIPSComp::jit) {
        inst = MIPSComp::jit->GetOriginalOp(inst);
    }*/
    //For now, let's get rid of the cache
    uint32_t encoding;
    ReadMemory<uint32_t>(encoding, address);
    return Opcode(encoding);
}
template <typename T>
inline void WriteToHardware(uint32 address, const T data) {
    
    //if(address == 0xa0 && IsWrite)
    // DBG_Break(); 
    //INFO_LOG(WRITE, "Writing at address %p\n", address);

    if(address < 0x00800000)
    {
        //JTTTS_increment_timestamp(1); // Best-case timing.
        MainRAM->Write<T>(address & 0x1FFFFF, data);
        return;
    }

    if(address >= 0x1FC00000 && address <= 0x1FC7FFFF)
    {
        BIOSROM->Write<T>(address & 0x7FFFF, data);
        return;
    }
    //This maybe isn't appropriate here.
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
        JitSetBIU(data);
        return;
    }

    DEBUG_LOG(MEM, "[MEM] Unknown write%d to %08x at time %d, =%08x(%d)\n", (int)(sizeof(T) * 8), address, JITTS_get_timestamp(), &data, data);
}

template<typename T>
inline void WriteMemory(uint32_t address, uint32_t value){
    if(MDFN_LIKELY(!(currentMIPS->CP0.SR & 0x10000)))
    {
        
       address &= addr_mask[address >> 29];
 
       if(address >= 0x1F800000 && address <= 0x1F8003FF)
       {
          ScratchRAM->Write<T>(address & 0x3FF, value);
          return;
       }
       WriteToHardware<T>(address, value);
    }
    else
    {
        if(address >= 0xB0 && address <= 0xB0 + 0x10){
            INFO_LOG(READ, "Writing to 0x%08x : 0x%08x\n", address, value);
        }

        if(BIU & BIU_ENABLE_ICACHE_S1)	// Instruction cache is enabled/active
        {
            if(BIU & (BIU_TAG_TEST_MODE | BIU_INVALIDATE_MODE | BIU_LOCK_MODE))
            {
                const uint8 valid_bits = (BIU & BIU_TAG_TEST_MODE) ? ((value << ((address & 0x3) * 8)) & 0x0F) : 0x00;
                __ICache* const ICI = &ICache.ICache[((address & 0xFF0) >> 2)];
 
                //
                // Set validity bits and tag.
                //
                for(unsigned i = 0; i < 4; i++)
                    ICI[i].TV = ((valid_bits & (1U << i)) ? 0x00 : 0x02) | (address & 0xFFFFFFF0) | (i << 2);
            }
            else
          {
            ICache.ICache[(address & 0xFFC) >> 2].Data = value << ((address & 0x3) * 8);
          }
       }
 
       if((BIU & 0x081) == 0x080)	// Writes to the scratchpad(TODO test)
       {
             ScratchRAM->Write<T>(address & 0x3FF, value);
       }
     }

     //INFO_LOG(JIT, "Write to 0x%08x with data %u (0x%08x)\n", address, value, value);


    if(address >= 0x1F800000 && address <= 0x1F8003FF)
    {
        ScratchRAM->Write<T>(address & 0x3FF, value);
        return;
    }
    
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
	}else if (address >= 0x1F800000 && address <= 0x1F8003FF) {
        //Scratch Pad
        return true;
    } else if ((address >= 0xBFC00000 && address <= 0xBFC7FFFF) ||
               (address >= 0x9FC00000 && address <= 0x9FC7FFFF) ||
               (address >= 0x1FC00000 && address <= 0x1FC7FFFF)) {
        //BIOS
        return true;
	} 
	return false;
}

uint8 Read_U8(const uint32 _Address)
{		
	uint8 _var = 0;
	ReadMemory<u8>(_var, _Address);
	return (u8)_var;
}

uint16 Read_U16(const uint32 _Address)
{
	uint16 _var = 0;
	ReadMemory<u16_le>(_var, _Address);
	return (u16)_var;
}

uint32 Read_U32(const uint32 _Address)
{
	uint32 _var = 0;
	ReadMemory<u32_le>(_var, _Address);
	return _var;
}

uint64 Read_U64(const uint32 _Address)
{
	u64_le _var = 0;
	ReadMemory<u64_le>(_var, _Address);
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
	WriteMemory<u8>(_Address, _Data);
}

void Write_U16(const uint16 _Data, const uint32 _Address)
{
	WriteMemory<u16_le>(_Address, _Data);
}

void Write_U32(const uint32 _Data, const uint32 _Address)
{	
	WriteMemory<u32_le>(_Address, _Data);
}

void Write_U64(const uint64 _Data, const uint32 _Address)
{
	WriteMemory<u64_le>(_Address, _Data);
}
uint8 ReadUnchecked_U8(const uint32 _Address)
{
	uint8 _var = 0;
	ReadMemory<u8>(_var, _Address);
	return _var;
}

uint16 ReadUnchecked_U16(const uint32 _Address)
{
	uint16 _var = 0;
	ReadMemory<u16_le>(_var, _Address);
	return _var;
}

uint32 ReadUnchecked_U32(const uint32 _Address)
{
	uint32 _var = 0;
	ReadMemory<u32_le>(_var, _Address);
	return _var;
}

uint32 Read_U32_instr(const uint32 _Address){
    uint32 _var = 0;
    ReadMemory<u32_le, false>(_var, _Address);
    return _var;
}

void WriteUnchecked_U8(const uint8 _iValue, const uint32 _Address)
{
	WriteToHardware<uint8_t>(_Address & addr_mask[_Address >> 29], _iValue);
}

void WriteUnchecked_U16(const uint16 _iValue, const uint32 _Address)
{
	WriteToHardware<u16_le>(_Address & addr_mask[_Address >> 29], _iValue);
}

void WriteUnchecked_U32(const uint32 _iValue, const uint32 _Address)

{
	WriteToHardware<u32_le>(_Address & addr_mask[_Address >> 29], _iValue);
}
#endif
} //Namespace Memory