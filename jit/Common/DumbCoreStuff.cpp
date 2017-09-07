#include "DumbCoreStuff.h"

volatile CoreState coreState = CORE_RUNNING;

void Core_UpdateState(CoreState newState) {
	coreState = newState;
}