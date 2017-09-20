#include "jittimestamp.h"
#include "jit/Common/DumbCoreStuff.h"

uint32 internal_timestamp = 0;
int32 cur_slice_length = 0;
uint32 last_ts = 0;
uint32 next_event_ts = 0;
uint32 gte_next_ts = 0;
uint32 muldiv_next_ts = 0;

void JITTS_Init(){
    internal_timestamp = 0;
    gte_next_ts = 0;
    muldiv_next_ts = 0;
}

void JITTS_prepare(uint32 timestamp_in){
    if(gte_next_ts > 0)
        gte_next_ts -= internal_timestamp;
    if(muldiv_next_ts > 0)
        muldiv_next_ts -= internal_timestamp;

    last_ts = internal_timestamp = timestamp_in;
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

//Set when the GTE gets done, as timestamp + offset
void JITTS_increment_gte_done(uint32 offset){
    gte_next_ts = internal_timestamp + offset;
}
//Set when the Muldiv gets done, as timestamp + offset
void JITTS_increment_muldiv_done(uint32 offset){
    muldiv_next_ts = internal_timestamp + offset;
}
//update the gte timestamp if timestamp is less than gte timestamp. Return true if timestamp was < gte timestamp, otherwise false
bool JITTS_update_gte_done(){
    if(internal_timestamp < gte_next_ts){
        internal_timestamp = gte_next_ts;
        JTTTS_update_from_direct_access();
        return true;
    }
    return false;
}
//update the muldiv timestamp if timestamp is less than muldiv timestamp. Return true if timestamp was < muldiv timestamp, otherwise false
bool JITTS_update_muldiv_done(){
    if(internal_timestamp < muldiv_next_ts){
        if(muldiv_next_ts - 1 == internal_timestamp)
            muldiv_next_ts--;
        else
            internal_timestamp = muldiv_next_ts;
        
        JTTTS_update_from_direct_access();
        return true;
    }
    return false;
}