#ifndef VDOS_H
#define VDOS_H

#include "config.h"
#include <io.h>
#include <windows.h>

void E_Exit(const char * message,...);

void MSG_Init();							// Set default (English) messages
void MSG_Add(const char*,const char*);		// Add messages to the internal langaugefile
const char* MSG_Get(char const *);			// Get messages from the internal langaugafile

extern char vDosVersion[];
class Section;

typedef void (LoopHandler)(void);

void RunPC();
void vDOS_Init(void);

class Config;
extern Config * control;
extern HWND	sdlHwnd;

#define MAX_PATH_LEN 512					// Maximum filename size

#define txtMaxCols	160
#define txtMaxLins	60

extern bool usesMouse;
extern int wpVersion;
extern bool wpExclude;
extern int idleCount;
#define idleTrigger 20						// When to sleep for 1 ms

#ifndef vDOS_LOGGING_H
#include "logging.h"
#endif

#endif
