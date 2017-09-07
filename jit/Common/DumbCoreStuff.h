#ifndef DUMB_CORE_STUFF_H
#define DUMB_CORE_STUFF_H

#include "mednafen/mednafen-types.h"

extern volatile uint32 coreState;

enum CoreState {
	CORE_RUNNING = 0,
	CORE_NEXTFRAME = 1,
	CORE_STEPPING,
	CORE_POWERUP,
	CORE_POWERDOWN,
	CORE_ERROR,
};

void Core_UpdateState(uint32 newState);

#endif