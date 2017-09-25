#include "jitICache.h"
#include <stdint.h>

uint32_t BIU = 0;
union __ICacheUnion ICache;
uint8 *FastMap[1 << (32 - FAST_MAP_SHIFT)];
uint8_t DummyPage[FAST_MAP_PSIZE];

void JitInitICache(){
    uint64_t a;
    unsigned i;
    
    memset(FastMap, 0, sizeof(FastMap));
    memset(DummyPage, 0xFF, sizeof(DummyPage));
    
    for(i = 0; i < 1024; i++)
    {
       ICache.ICache[i].TV = 0x3;
       ICache.ICache[i].Data = 0;
    }
 
    for(a = 0x00000000; a < (UINT64_C(1) << 32); a += FAST_MAP_PSIZE)
        JitSetFastmap(DummyPage, a, FAST_MAP_PSIZE);
}

void JitSetFastmap(void* region_mem, uint32 region_address, uint32 region_size){
    uint64_t A;
    // FAST_MAP_SHIFT
    // FAST_MAP_PSIZE
  
    for(A = region_address; A < (uint64)region_address + region_size; A += FAST_MAP_PSIZE)
       FastMap[A >> FAST_MAP_SHIFT] = ((uint8_t *)region_mem - region_address);
}

void JitSetBIU(uint32_t val){
    const uint32_t old_BIU = BIU;
    
       BIU = val & ~(0x440);
    
       if((BIU ^ old_BIU) & 0x800)
       {
          unsigned i;
    
          if(BIU & 0x800)	// ICache enabled
          {
             for(i = 0; i < 1024; i++)
                ICache.ICache[i].TV &= ~0x1;
          }
          else			// ICache disabled
          {
             for(i = 0; i < 1024; i++)
                ICache.ICache[i].TV |= 0x1;
          }
       }
}