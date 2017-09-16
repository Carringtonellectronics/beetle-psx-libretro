#include "jittimestamp.h"
#include "jit/Common/DumbCoreStuff.h"

uint32 internal_timestamp = 0;
int32 cur_slice_length = 0;
uint32 last_ts = 0;
uint32 next_event_ts = 0;

void JITTS_Init(){
    internal_timestamp = 0;
}

void JITTS_set_next_event(uint32 ts){
    next_event_ts = ts;
    coreState = CORE_RUNNING;
    //INFO_LOG(JITTS, "Slice length: %d, Next event ts: %u, Internal timestamp: %u", cur_slice_length, next_event_ts, internal_timestamp);
    //update time slice, downcount, all that stuff
    JITTS_update_from_downcount();
}

uint32 JITTS_get_next_event(){return next_event_ts;}

//Updates internal timestamp from the change in currentMIPS->downcount,
//Then sets the downcount to what is calculated for the next event.
void JITTS_update_from_downcount(){
    //INFO_LOG(JITTS, "Updating Timestamp! Timestamp: %u, Downcount: %d\n", internal_timestamp, currentMIPS->downcount);
    int cyclesEx = cur_slice_length - currentMIPS->downcount;
    last_ts = (internal_timestamp += cyclesEx);
    cur_slice_length = next_event_ts - internal_timestamp;
    currentMIPS->downcount = cur_slice_length;

    if(currentMIPS->downcount < 0){
        coreState = CORE_NEXTFRAME;
        //INFO_LOG(CPU, "Changed core state! cur pc = %08x\n", currentMIPS->pc);
    }
    //INFO_LOG(JITTS, "Slice length: %d, Next event ts: %u, Internal timestamp: %u", cur_slice_length, next_event_ts, internal_timestamp);
    //INFO_LOG(JITTS, "Updated Timestamp! Timestamp: %u, Downcount: %d\n", internal_timestamp, currentMIPS->downcount);
}
//force the end of the current timeslice.
void JITTS_force_check(){
    int cyclesEx = cur_slice_length - currentMIPS->downcount;
    internal_timestamp += cyclesEx;
    cur_slice_length = -1;
    currentMIPS->downcount = -1;
}