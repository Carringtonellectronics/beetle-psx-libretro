#ifndef JIT_I_CACHE_H
#define JIT_I_CACHE_H

#include "mednafen/mednafen-types.h"

#ifdef FAST_MAP_SHIFT
#undef FAST_MAP_SHIFT
#endif

#ifdef FAST_MAP_PSIZE
#undef FAST_MAP_PSIZE
#endif

#define FAST_MAP_SHIFT 16
#define FAST_MAP_PSIZE (1 << FAST_MAP_SHIFT)

#define BIU_ENABLE_ICACHE_S1	0x00000800	// Enable I-cache, set 1
#define BIU_ICACHE_FSIZE_MASK	0x00000300  // I-cache fill size mask; 0x000 = 2 words, 0x100 = 4 words, 0x200 = 8 words, 0x300 = 16 words
#define BIU_ENABLE_DCACHE	   0x00000080	// Enable D-cache
#define BIU_DCACHE_SCRATCHPAD	0x00000008	// Enable scratchpad RAM mode of D-cache
#define BIU_TAG_TEST_MODE	   0x00000004	// Enable TAG test mode(IsC must be set to 1 as well presumably?)
#define BIU_INVALIDATE_MODE	0x00000002	// Enable Invalidate mode(IsC must be set to 1 as well presumably?)
#define BIU_LOCK_MODE		   0x00000001	// Enable Lock mode(IsC must be set to 1 as well presumably?)

struct __ICache
{
   uint32_t TV;
   uint32_t Data;
};

union __ICacheUnion
{
   __ICache ICache[1024];
   uint32_t ICache_Bulk[2048];
};

void JitInitICache();

void JitSetBIU(uint32_t val);

void JitSetFastmap(void* region_mem, uint32 region_address, uint32 region_size);

extern uint8 *FastMap[1 << (32 - FAST_MAP_SHIFT)];

extern union __ICacheUnion ICache;

extern uint32_t BIU;

#endif