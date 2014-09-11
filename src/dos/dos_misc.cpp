#include "vDos.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "../src/ints/xms.h"


static Bitu INT2F_Handler(void)
	{
	switch (reg_ax)
		{
	case 0x1000:													// SHARE.EXE installation check
		reg_ax = 0xffff;											// Pretend SHARE.EXE is installed.
	case 0x1680:													// Release current virtual machine time-slice
	case 0x1689:													// Kernel IDLE CALL
		break;
	case 0x4300:													// XMS installed check
		reg_al = 0x80;
		break;
	case 0x4310:													// XMS handler seg:offset
		SegSet16(es, RealSeg(xms_callback));
		reg_bx = RealOff(xms_callback);
		break;			
	case 0x4a01:													// Query free hma space
	case 0x4a02:													// Allocate HMA space
		reg_bx = 0;													// Number of bytes available in HMA
		SegSet16(es, 0xffff);										// ES:DI = ffff:ffff HMA not used
		reg_di = 0xffff;
		break;
	default:
		LOG_MSG("DOS-Multiplex unhandled call %4X", reg_ax);
		}
	return CBRET_NONE;
	}

static Bitu INT2A_Handler(void)
	{
	return CBRET_NONE;
	}

void DOS_SetupMisc(void)
	{
	// Setup the dos multiplex interrupt
	Bitu call_int2f = CALLBACK_Allocate();
	CALLBACK_Setup(call_int2f, &INT2F_Handler, CB_IRET, "DOS Int 2f");
	RealSetVec(0x2f, CALLBACK_RealPointer(call_int2f));
	// Setup the dos network interrupt
	Bitu call_int2a = CALLBACK_Allocate();
	CALLBACK_Setup(call_int2a, &INT2A_Handler, CB_IRET, "DOS Int 2a");
	RealSetVec(0x2A, CALLBACK_RealPointer(call_int2a));
	}
