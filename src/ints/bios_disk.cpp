#include "vDos.h"
#include "callback.h"
#include "bios.h"
#include "regs.h"
#include "mem.h"

Bitu call_int13;
Bitu diskparm0, diskparm1;

void CMOS_SetRegister(Bitu regNr, Bit8u val);	// For setting equipment word

static Bitu INT13_DiskHandler(void)
	{
	// unconditionally enable the interrupt flag
	CALLBACK_SIF(true);			// can be removed?

	reg_ah = 0xff;				// No, no low level disk IO
	CALLBACK_SCF(true);
	return CBRET_NONE;
	}

void BIOS_SetupDisks(void)
	{
	// TODO Start the time correctly
	call_int13 = CALLBACK_Allocate();	
	CALLBACK_Setup(call_int13, &INT13_DiskHandler, CB_IRET_STI, "Int 13 Bios disk");
	RealSetVec(0x13, CALLBACK_RealPointer(call_int13));

	diskparm0 = CALLBACK_Allocate();
	diskparm1 = CALLBACK_Allocate();

	RealSetVec(0x41,CALLBACK_RealPointer(diskparm0));
	RealSetVec(0x46,CALLBACK_RealPointer(diskparm1));

	PhysPt dp0physaddr = CALLBACK_PhysPointer(diskparm0);
	PhysPt dp1physaddr = CALLBACK_PhysPointer(diskparm1);
	for (int i = 0; i < 16; i++)
		{
		vPC_aStosb(dp0physaddr+i, 0);
		vPC_aStosb(dp1physaddr+i, 0);
		}

	// Setup the Bios Area
	vPC_rStosb(BIOS_HARDDISK_COUNT, 2);
	}
