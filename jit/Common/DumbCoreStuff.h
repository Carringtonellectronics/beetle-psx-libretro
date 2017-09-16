#ifndef DUMB_CORE_STUFF_H
#define DUMB_CORE_STUFF_H

#include "mednafen/mednafen-types.h"

enum CoreState {
	CORE_RUNNING = 0,
	CORE_NEXTFRAME = 1,
	CORE_STEPPING,
	CORE_POWERUP,
	CORE_POWERDOWN,
	CORE_ERROR,
};

extern volatile CoreState coreState;

void Core_UpdateState(CoreState newState);

#endif