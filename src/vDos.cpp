#include <stdlib.h>
#include "vDos.h"
#include "video.h"
#include "pic.h"
#include "cpu.h"
#include "callback.h"
#include "timer.h"
#include "parport.h"
#include "support.h"

char vDosVersion[11];

// The whole load of startups for all the subfunctions
void GUI_StartUp();
void MEM_Init();
void PAGING_Init();
void IO_Init();
void CALLBACK_Init();
void PROGRAMS_Init();
void RENDER_Init();
void VGA_Init();
void DOS_Init();
void CPU_Init();
void KEYBOARD_Init();																// TODO This should setup INT 16 too but ok ;)
void MOUSE_Init();
void SERIAL_Init(); 
void PIC_Init();
void TIMER_Init();
void BIOS_Init();
// void CMOS_Init();
void PARALLEL_Init();
// Dos Internal mostly
void XMS_Init();
void EMS_Init();
void AUTOEXEC_Init();
void SHELL_Init();
void INT10_Init();

static Bit32u mSecsLast = 0;
bool usesMouse;
int wpVersion;																		// 1 - 99 (mostly 51..62, negative value will exclude some WP additions)

Bit8u tempBuff1K [1024];	
Bit8u tempBuff2K [2*1024];	
Bit8u tempBuff4K [4*1024];
Bit8u tempBuff32K [32*1024];

Bit32s CPU_CycleMax = CPU_CycleHigh;

void RunPC(void)
	{
	while (1)
		{
		if (PIC_RunQueue())
			{
			Bits ret = (*cpudecoder)();
			if (ret < 0)
				return;
			if (ret > 0 && (*CallBack_Handlers[ret])())
				return;
			}
		else
			{
			GFX_Events();
			TIMER_AddTick();
			Bit32u mSecsNew = GetTickCount();
			if (mSecsNew <= mSecsLast+18)											// To be real save???
				{
				LPT_CheckTimeOuts(mSecsNew);
				Sleep(idleCount >= idleTrigger ? 2: 0);								// If idleTrigger or more repeated idle keyboard requests or int 28h called, sleep fixed (CPU usage drops down)
				if (idleCount > idleTrigger && CPU_CycleMax > CPU_CycleLow)
					CPU_CycleMax -= 100;											// Decrease cycles
				else if (idleCount <= idleTrigger  && CPU_CycleMax < CPU_CycleHigh)
					CPU_CycleMax += 1000;											// Fire up again
				if (idleCount >= idleTrigger)
					idleCount = 0;
				}
			mSecsLast = mSecsNew;
			idleCount = 0;
			}
		}
	}


#define LENNAME 10																	// Max length of name
#define MAXNAMES 50																	// Max number of names
#define MAXSTRLEN 8192																// Max storage of all config strings

enum Vtype { V_BOOL, V_INT, V_STRING};

static int confEntries = 0;															// Entries so far
static char confStrings[MAXSTRLEN];
static unsigned int confStrOffset = 0;												// Offset to store new strings
static char errorMess[600];
static bool addMess = true;

static struct
{
char name[LENNAME+1];
Vtype type;
union {bool _bool; int _int; char * _string;}value;
} ConfSetting[MAXNAMES];


// No checking on add/get functions, we call them ourselves...
void ConfAddBool(const char *name, bool value)
	{
	strcpy(ConfSetting[confEntries].name, name);
	ConfSetting[confEntries].type = V_BOOL;
	ConfSetting[confEntries].value._bool = value;
	confEntries++;
	}

void ConfAddInt(const char *name, int value)
	{
	strcpy(ConfSetting[confEntries].name, name);
	ConfSetting[confEntries].type = V_INT;
	ConfSetting[confEntries].value._int = value;
	confEntries++;
	}
	
void ConfAddString(const char *name, char* value)
	{
	strcpy(ConfSetting[confEntries].name, name);
	ConfSetting[confEntries].type = V_STRING;
	ConfSetting[confEntries].value._string = value;
	confEntries++;
	}

static int findEntry(const char *name)
	{
	for (int found = 0; found < confEntries; found++)
		if (!stricmp(name, ConfSetting[found].name))
			return found;
	return -1;
	}
	
bool ConfGetBool(const char *name)
	{
	int entry = findEntry(name);
	if (entry >= 0)
		return ConfSetting[entry].value._bool;
	return false;																	// To satisfy compiler and default
	}
int ConfGetInt(const char *name)
	{
	int entry = findEntry(name);
	if (entry >= 0)
		return ConfSetting[entry].value._int;
	return 0;																		// To satisfy compiler and default
	}
char * ConfGetString(const char *name)
	{
	int entry = findEntry(name);
	if (entry >= 0)
		return ConfSetting[entry].value._string;
	return "";																		// To satisfy compiler and default
	}
	
static char* ConfSetValue(const char* name, char* value)
	{
	int entry = findEntry(name);
	if (entry == -1)
		return "No valid config option\n";
	switch (ConfSetting[entry].type)
		{
	case V_BOOL:
		if (!stricmp(value, "on"))
			{
			ConfSetting[entry].value._bool = true;
			return NULL;
			}
		else if (!stricmp(value, "off"))
			{
			ConfSetting[entry].value._bool = false;
			return NULL;
			}
		break;
	case V_INT:
		{
		int testVal = atoi(value);
		char testStr[32];
		sprintf(testStr, "%d", testVal);
		if (!strcmp(value, testStr))
			{
			ConfSetting[entry].value._int = testVal;
			return NULL;
			}
		break;
		}
	case V_STRING:
		if (strlen(value) >= MAXSTRLEN - confStrOffset)
			return "vDos ran out of space to store settings\n";
		strcpy(confStrings+confStrOffset, value);
		ConfSetting[entry].value._string = confStrings+confStrOffset;
		confStrOffset += strlen(value)+1;
		return NULL;
		}
	return "Invalid value for this option\n";
	}

static char * ParseConfigLine(char *line)
	{
	char *val = strchr(line, '=');
	if (!val)
		return "= missing\n";
	*val = 0;
	val++;
	char *name = lrTrim(line);
	if (!strlen(name))
		return "Option name missing\n";
	val = lrTrim(val);
	if (!strlen(val))
		return "Value of option missing\n";
	if (strlen(name) == 4 && (!strnicmp(name, "LPT", 3) || !strnicmp(name, "COM", 3)) && (name[3] > '0' && name[3] <= '9'))
		{
		ConfAddString(name, val);
		ConfSetValue(name, val);													// Have to use this, ConfAddString() uses static ref to val!
		return NULL;
		}
	return ConfSetValue(name, val);
	}

void ConfAddError(char* desc, char* errLine)
	{
	if (addMess)
		{
		if (strlen(errLine) > 40)
			strcpy(errLine+37, "...");
		strcat(strcat(strcat(errorMess, "\n"), desc), errLine);
		if (strlen(errorMess) > 500)												// Don't flood the MesageBox with error lines
			{
			strcat(errorMess, "\n...");
			addMess = false;
			}
		}
	}

void ParseConfigFile()
	{
	char * parseRes;
	char *lineIn = (char *)tempBuff4K;
	FILE * cFile;
	errorMess[0] = 0;
	if (!(cFile = fopen("config.txt", "r")))
		return;
	while (fgets(lineIn, 4096, cFile))
		{
		char *line = lrTrim(lineIn);
		if (strlen(line) && !(!strnicmp(line, "rem", 3) && (line[3] == 0 || line[3] == 32 || line[3] == 9)))	// Filter out rem ...
			if (parseRes = ParseConfigLine(line))
				ConfAddError(parseRes, line);
		}
	fclose(cFile);
	}

static void ConfShowErrors()
	{
	if (errorMess[0])
		MessageBox(NULL, errorMess+1, "vDos: CONFIG.TXT has unresolved items", MB_OK|MB_ICONWARNING);
	}

void vDOS_Init(void)
	{
    char sYear[5], sMonth[4];
	int nDay =10;
    static const char names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(__DATE__, "%s %d %s", sMonth, &nDay, sYear);
	sprintf(vDosVersion, "%s.%02d.%02d", sYear, (strstr(names, sMonth)-names)/3+1, nDay);
	LOG_MSG("vDos version: %s", vDosVersion);

	ConfAddInt("scale", 0);
	ConfAddString("window", "");
	ConfAddBool("low", false);
	ConfAddBool("ems", false);
	ConfAddString("colors", "");
	ConfAddBool("mouse", false);
	ConfAddInt("lins", 25);
	ConfAddInt("cols", 80);
	ConfAddBool("frame", false);
	ConfAddString("font", "");
	ConfAddInt("hide", 5);
	ConfAddInt("wp", 0);
	ParseConfigFile();

	GUI_StartUp();
	IO_Init();
	PAGING_Init();
	MEM_Init();
	CALLBACK_Init();
	PIC_Init();
	PROGRAMS_Init();
	TIMER_Init();
//	CMOS_Init();
	VGA_Init();
	RENDER_Init();
	CPU_Init();
	KEYBOARD_Init();
	BIOS_Init();
	INT10_Init();
	MOUSE_Init();																	// Must be after int10 as it uses CurMode
	SERIAL_Init();
	PARALLEL_Init();
	// All the DOS related stuff
	DOS_Init();
	XMS_Init();
	EMS_Init();
	ConfShowErrors();
	AUTOEXEC_Init();
	SHELL_Init();																	// Start up main machine
	}
