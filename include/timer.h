#ifndef vDOS_TIMER_H
#define vDOS_TIMER_H

/* underlying clock rate in HZ */
#include <SDL.h>

#define PIT_TICK_RATE 1193182

typedef void (*TIMER_TickHandler)(void);

/* This will add 1 milliscond to all timers */
void TIMER_AddTick(void);

/* Functions for the system control port 61h */
bool TIMER_GetOutput2();

#endif
