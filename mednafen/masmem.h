#ifndef __MDFN_PSX_MASMEM_H
#define __MDFN_PSX_MASMEM_H

#include "mednafen-types.h"
#include "jit/Common/Opcode.h"

#ifdef JIT
#include "jit/JitCommon/JitState.h"
#endif

// TODO, WIP (big-endian stores and loads not fully supported yet)

#ifdef MSB_FIRST
#define MAS_NATIVE_IS_BIGENDIAN 1
#else
#define MAS_NATIVE_IS_BIGENDIAN 0
#endif

static INLINE uint16 LoadU16_RBO(const uint16 *a)
{
 #ifdef ARCH_POWERPC
  uint16 tmp;

  __asm__ ("lhbrx %0, %y1" : "=r"(tmp) : "Z"(*a));

  return(tmp);

 #else
  uint16 tmp = *a;
  return((tmp << 8) | (tmp >> 8));
 #endif
}

static INLINE uint32 LoadU32_RBO(const uint32 *a)
{
 #ifdef ARCH_POWERPC
  uint32 tmp;

  __asm__ ("lwbrx %0, %y1" : "=r"(tmp) : "Z"(*a));

  return(tmp);
 #else
  uint32 tmp = *a;
  return((tmp << 24) | ((tmp & 0xFF00) << 8) | ((tmp >> 8) & 0xFF00) | (tmp >> 24));
 #endif
}

static INLINE void StoreU16_RBO(uint16 *a, const uint16 v)
{
 #ifdef ARCH_POWERPC
  __asm__ ("sthbrx %0, %y1" : : "r"(v), "Z"(*a));
 #else
  uint16 tmp = (v << 8) | (v >> 8);
  *a = tmp;
 #endif
}

static INLINE void StoreU32_RBO(uint32 *a, const uint32 v)
{
 #ifdef ARCH_POWERPC
  __asm__ ("stwbrx %0, %y1" : : "r"(v), "Z"(*a));
 #else
  uint32 tmp = (v << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24);
  *a = tmp;
 #endif
}

static INLINE uint16 LoadU16_LE(const uint16 *a)
{
#ifdef MSB_FIRST
   return LoadU16_RBO(a);
#else
   return *a;
#endif
}

static INLINE uint32 LoadU32_LE(const uint32 *a)
{
#ifdef MSB_FIRST
   return LoadU32_RBO(a);
#else
   return *a;
#endif
}

static INLINE void StoreU16_LE(uint16 *a, const uint16 v)
{
#ifdef MSB_FIRST
 StoreU16_RBO(a, v);
#else
 *a = v;
#endif
}

static INLINE void StoreU32_LE(uint32 *a, const uint32 v)
{
#ifdef MSB_FIRST
 StoreU32_RBO(a, v);
#else
 *a = v;
#endif
}


// address must not be >= size specified by template parameter, and address must be a multiple of the byte-size of the
// unit(1,2,4) being read(except for Read/WriteU24, which only needs to be byte-aligned).
//
// max_unit_type should be uint16 or uint32
//
// pre_padding and post_padding are specified in units of sizeof(max_unit_type).
//
 //, unsigned pre_padding_count, unsigned post_padding_count>
 template<unsigned size, typename max_unit_type, bool big_endian> //, unsigned pre_padding_count, unsigned post_padding_count>
 struct MultiAccessSizeMem
 {
  //max_unit_type pre_padding[pre_padding_count ? pre_padding_count : 1];
 
  union
  {
   uint8 data8[size];
   uint16 data16[size / sizeof(uint16)];
   uint32 data32[size / sizeof(uint32)];
  };
 
  //max_unit_type post_padding[post_padding_count ? post_padding_count : 1];
 
  INLINE uint8 ReadU8(uint32 address)
  {
   return data8[address];
  }
 
  INLINE uint16 ReadU16(uint32 address)
  {
   if(MAS_NATIVE_IS_BIGENDIAN == big_endian)
    return *(uint16*)(((uint8*)data16) + address);
   else
    return LoadU16_RBO((uint16*)(((uint8*)data16) + address));
  }
 
  INLINE uint32 ReadU32(uint32 address)
  {
   if(MAS_NATIVE_IS_BIGENDIAN == big_endian)
    return *(uint32*)(((uint8*)data32) + address);
   else
    return LoadU32_RBO((uint32*)(((uint8*)data32) + address));
  }
 
  INLINE uint32 ReadU24(uint32 address)
  {
   uint32 ret;
 
   if(!big_endian)
   {
    ret = ReadU8(address) | (ReadU8(address + 1) << 8) | (ReadU8(address + 2) << 16);
   }
 
   return(ret);
  }
 
 
  INLINE void WriteU8(uint32 address, uint8 value)
  {
   data8[address] = value;
  }
 
  INLINE void WriteU16(uint32 address, uint16 value)
  {
   if(MAS_NATIVE_IS_BIGENDIAN == big_endian)
    *(uint16*)(((uint8*)data16) + address) = value;
   else
    StoreU16_RBO((uint16*)(((uint8*)data16) + address), value);
  }
 
  INLINE void WriteU32(uint32 address, uint32 value)
  {
   if(MAS_NATIVE_IS_BIGENDIAN == big_endian)
    *(uint32*)(((uint8*)data32) + address) = value;
   else
    StoreU32_RBO((uint32*)(((uint8*)data32) + address), value);
  }
 
  INLINE void WriteU24(uint32 address, uint32 value)
  {
   if(!big_endian)
   {
    WriteU8(address + 0, value >> 0);
    WriteU8(address + 1, value >> 8);
    WriteU8(address + 2, value >> 16);
   }
  }
 
  template<typename T>
  INLINE T Read(uint32 address)
  {
   if(sizeof(T) == 4)
    return(ReadU32(address));
   else if(sizeof(T) == 2)
    return(ReadU16(address));
   else
    return(ReadU8(address));
  }
 
  template<typename T>
  INLINE void Write(uint32 address, T value)
  {
   if(sizeof(T) == 4)
    WriteU32(address, value);
   else if(sizeof(T) == 2)
    WriteU16(address, value);
   else
    WriteU8(address, value);
  }
 };

//These externs allow direct access to the memory structs, but maybe it should be more compartmentalized?
//Like, just have functions to manipulate them.
extern MultiAccessSizeMem<1024, uint32, false> *ScratchRAM;
extern MultiAccessSizeMem<512 * 1024, uint32, false> *BIOSROM;
extern MultiAccessSizeMem<65536, uint32, false> *PIOMem;
extern MultiAccessSizeMem<2048 * 1024, uint32, false> *MainRAM;

namespace Memory {

    extern uint8 *base;
    void Init(bool WantPIOMem);
    void Clear();
#ifdef JIT
    Opcode Read_Instruction(uint32 address, bool resolve_replacements);
    INLINE Opcode Read_Instruction(uint32 address){return Read_Instruction(address, false);}
    Opcode Read_Opcode_JIT(MIPSComp::JitState* js);
    void Write_Opcode_JIT(const uint32 _Address, const Opcode& _Value);

    uint8 *GetPointer(const uint32 address);
    
    bool IsRAMAddress(const uint32 address);
    bool IsScratchpadAddress(const uint32 address);
    bool IsValidAddress(const uint32 address);
    //TODO either remove all references to this, or make it work the way
    //It should
    INLINE bool Lock(){return false;}

    uint8 Read_U8(const uint32 _Address);
    uint16 Read_U16(const uint32 _Address);
    uint32 Read_U32(const uint32 _Address);
    uint64 Read_U64(const uint32 _Address);
    uint32 Read_U8_ZX(const uint32 _Address);
    uint32 Read_U16_ZX(const uint32 _Address);
    void Write_U8(const uint8 _Data, const uint32 _Address);
    void Write_U16(const uint16 _Data, const uint32 _Address);
    void Write_U32(const uint32 _Data, const uint32 _Address);
    void Write_U64(const uint64 _Data, const uint32 _Address);

    uint8 ReadUnchecked_U8(const uint32 _Address);
    uint16 ReadUnchecked_U16(const uint32 _Address);
    uint32 ReadUnchecked_U32(const uint32 _Address);
    uint32 Read_U32_instr(const uint32 _Address);
    void WriteUnchecked_U8(const uint8 _iValue, const uint32 _Address);
    void WriteUnchecked_U16(const uint16 _iValue, const uint32 _Address);
    void WriteUnchecked_U32(const uint32 _iValue, const uint32 _Address);

    inline uint32 GetScratchpadMemoryBase() { return 0x1f800000;}
    inline uint32 GetScratchpadMemoryEnd() { return GetScratchpadMemoryBase() + sizeof(ScratchRAM->data8);}
    inline uint32 GetKernelMemoryBase() { return 0;}
    inline uint32 GetUserMemoryEnd() { return GetKernelMemoryBase() + sizeof(MainRAM->data8);}
    inline uint32 GetKernelMemoryEnd() { return GetKernelMemoryBase() + 0x0000ffff;}
    // "Volatile" RAM is between 0x08400000 and 0x08800000, can be requested by the
    // game through sceKernelVolatileMemTryLock.
    
    inline uint32 GetUserMemoryBase() { return GetKernelMemoryBase() + 0x00010000;}
    //inline uint32 GetDefaultLoadAddress() { return 0;}
#endif
}

#undef MAS_NATIVE_IS_BIGENDIAN

#endif
