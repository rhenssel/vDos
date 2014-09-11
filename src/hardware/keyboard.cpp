#include "vDos.h"
#include "inout.h"
#include "pic.h"
#include "timer.h"
#include <math.h>


enum KeyCommands {
	CMD_NONE,
	CMD_SETLEDS,
	CMD_SETTYPERATE,
	CMD_SETOUTPORT
};

KeyCommands command;

static Bitu read_p60(Bitu port, Bitu iolen)
	{
	return 0;
	}	

static void write_p60(Bitu port, Bitu val, Bitu iolen)
	{
	command = CMD_NONE;
	}

static Bit8u port_61_data = 0;

static Bitu read_p61(Bitu, Bitu)
	{
	return	(port_61_data & 0xF) |
			(TIMER_GetOutput2()? 0x20:0) |
			((fmod(PIC_FullIndex(),0.030) > 0.015) ? 0x10 : 0);
	}

static void write_p61(Bitu, Bitu val, Bitu)
	{
//	Bit8u val1 = val & 3;
//	if (val1 == 1 || val1 == 3)
//	if ((port_61_data^(Bit8u)val)&3)
//		Beep(1750, 300);
	port_61_data = val;
	}

static void write_p64(Bitu port, Bitu val, Bitu iolen)
	{
	if (val == 0xd1)
		command = CMD_SETOUTPORT;
	}

static Bitu read_p64(Bitu port, Bitu iolen)
	{
	return 0x1c;
	}

void KEYBOARD_Init()
	{
	IO_RegisterWriteHandler(0x60, write_p60, IO_MB);
	IO_RegisterReadHandler(0x60, read_p60, IO_MB);
	IO_RegisterWriteHandler(0x61, write_p61, IO_MB);
	IO_RegisterReadHandler(0x61, read_p61, IO_MB);
	IO_RegisterWriteHandler(0x64, write_p64, IO_MB);
	IO_RegisterReadHandler(0x64, read_p64, IO_MB);
	command = CMD_NONE;
	}
