#include <stdlib.h>
#include "vDos.h"
#include "video.h"
#include "pic.h"
#include "cpu.h"
#include "callback.h"
#include "timer.h"
#include "setup.h"
#include "control.h"
#include "parport.h"

Config * control;
char vDosVersion[11];


// The whole load of startups for all the subfunctions
void GUI_StartUp(Section*);
void MEM_Init(Section *);
void PAGING_Init(Section *);
void IO_Init(Section * );
void CALLBACK_Init(Section*);
void PROGRAMS_Init(Section*);
void RENDER_Init(Section*);
void VGA_Init(Section*);
void DOS_Init(Section*);
void CPU_Init(Section*);
void KEYBOARD_Init(Section*);			//TODO This should setup INT 16 too but ok ;)
void MOUSE_Init(Section*);
void SERIAL_Init(Section*); 

void PIC_Init(Section*);
void TIMER_Init(Section*);
void BIOS_Init(Section*);
void CMOS_Init(Section*);

// Dos Internal mostly
void XMS_Init(Section*);
void AUTOEXEC_Init(Section*);
void SHELL_Init(void);
void INT10_Init(Section*);

static Bit32u mSecsLast = 0;
bool usesMouse;
int wpVersion;							// 1 - 99 (mostly 51..62, negative value will exclude some WP additions)

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
			if (mSecsNew <= mSecsLast+18)							// To be real save???
				{
				LPT_CheckTimeOuts(mSecsNew);
				Sleep(idleCount >= idleTrigger ? 1: 0);				// If idleTrigger or more repeated idle keyboard requests or int 28h called, sleep fixed (CPU usage drops down)
				if (idleCount >= idleTrigger)
					idleCount = 0;
				}
			mSecsLast = mSecsNew;
//			idleCount = 0;
			}
		}
	}


void vDOS_Init(void)
	{
    char sYear[5], sMonth[4];
	int nDay =10;
    static const char names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    sscanf(__DATE__, "%s %d %s", sMonth, &nDay, sYear);
	sprintf(vDosVersion, "%s.%02d.%02d", sYear, (strstr(names, sMonth)-names)/3+1, nDay);
	LOG_MSG("vDos version: %s", vDosVersion);

	Section_prop *secprop = control->SetSection_prop(&GUI_StartUp);
	secprop->Add_int("scale", 0);
	secprop->Add_string("window", "");
	secprop->Add_bool("low");
	secprop->Add_bool("umb");
	secprop->Add_string("colors", "");
	secprop->Add_bool("mouse");
//	secprop->Add_int("files", 80);
	secprop->Add_int("lins", 25);
	secprop->Add_int("cols", 80);
	secprop->Add_bool("frame");
	secprop->Add_string("font", "");
	secprop->Add_int("hide", 5);
	secprop->Add_int("wp");
	secprop->AddInitFunction(&IO_Init);
	secprop->AddInitFunction(&PAGING_Init);
	secprop->AddInitFunction(&MEM_Init);
	secprop->AddInitFunction(&CALLBACK_Init);
	secprop->AddInitFunction(&PIC_Init);
	secprop->AddInitFunction(&PROGRAMS_Init);
	secprop->AddInitFunction(&TIMER_Init);
	secprop->AddInitFunction(&CMOS_Init);
	secprop->AddInitFunction(&VGA_Init);
	secprop->AddInitFunction(&RENDER_Init);
	secprop->AddInitFunction(&CPU_Init);
	secprop->AddInitFunction(&KEYBOARD_Init);
	secprop->AddInitFunction(&BIOS_Init);
	secprop->AddInitFunction(&INT10_Init);
	secprop->AddInitFunction(&MOUSE_Init);			// Must be after int10 as it uses CurMode

	secprop->AddInitFunction(&SERIAL_Init);
	secprop->AddInitFunction(&PARALLEL_Init);

	// All the DOS Related stuff, which will eventually start up in the shell
	secprop->AddInitFunction(&DOS_Init);
	secprop->AddInitFunction(&XMS_Init);
	// TODO ?
	secprop->AddInitFunction(&AUTOEXEC_Init);
	control->SetStartUp(&SHELL_Init);
	}
