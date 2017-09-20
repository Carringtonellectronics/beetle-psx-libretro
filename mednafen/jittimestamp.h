#ifndef JIT_TIMESTAMP_H
#define JIT_TIMESTAMP_H

#include "mednafen-types.h"
#include "jit/MIPS.h"
//POSSIBLE ISSUES
//Downcount only updates in big chunks (on a function level). This could lead to granularity issues,
//Which will make timers and events called later than expected.

//internal timestamp. Should only be used by these functions
extern uint32 internal_timestamp;
extern uint32 last_ts;
extern int32 cur_slice_length;

void JITTS_Init();

void JITTS_prepare(uint32 timestamp_in);

//Note that this also decrements downcount
INLINE void JTTTS_increment_timestamp(uint32 inc){
    last_ts = internal_timestamp += inc;
    currentMIPS->downcount-=inc;
    cur_slice_length = currentMIPS->downcount;
}

//This function should be called after directly updating internal_timestamp.
//This is a function to make this file and functions compatible with functions that need a reference to the timestamp.
INLINE void JTTTS_update_from_direct_access(){
    uint32 delta = internal_timestamp - last_ts;
    currentMIPS->downcount -= delta;
    cur_slice_length -= delta;
    last_ts = internal_timestamp;
}
//TODO fix these finctions, they should make sure setting internal_timestamp updates
//the downcount
//Functions to equalize competing timestamps
INLINE uint32 JITTS_set_min_timestamp(uint32 competing_timestamp)
{
    return competing_timestamp < internal_timestamp ? 
        internal_timestamp = competing_timestamp : internal_timestamp;
}

INLINE uint32 JITTS_set_max_timestamp(uint32 competing_timestamp)
{
    return competing_timestamp > internal_timestamp ? 
        internal_timestamp = competing_timestamp : internal_timestamp;
}
//setter/getter
INLINE void JITTS_set_timestamp(uint32 timestamp){internal_timestamp = timestamp;}
INLINE uint32 JITTS_get_timestamp(){return internal_timestamp;}
void JITTS_set_next_event(uint32 ts);
uint32 JITTS_get_next_event();
//Updates from the change in currentMIPS->downcount. Used for JIT Comp
void JITTS_update_from_downcount();
//Does what it says. Should be called before cpu->Run for JIT to work correctly.
void JITTS_update_and_set_downcount();
//Forces currentMIPS->downcount to -1, basically stops the CPU so other stuff can run.
void JITTS_force_check();
//Set when the GTE gets done, as timestamp + offset
void JITTS_increment_gte_done(uint32 offset);
//Set when the Muldiv gets done, as timestamp + offset
void JITTS_increment_muldiv_done(uint32 offset);
//update the gte timestamp if timestamp is less than gte timestamp. Return true if timestamp was < gte timestamp, otherwise false
bool JITTS_update_gte_done();
//update the muldiv timestamp if timestamp is less than muldiv timestamp. Return true if timestamp was < muldiv timestamp, otherwise false
bool JITTS_update_muldiv_done();
#endif