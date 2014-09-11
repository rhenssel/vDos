#include "vDos.h"
#include "callback.h"
#include "bios.h"
#include "regs.h"

Bitu call_int13;

void CMOS_SetRegister(Bitu regNr, Bit8u val);		// For setting equipment word

static Bitu INT13_DiskHandler(void)
	{
	reg_ah = 1;										// No low level disk IO
	CALLBACK_SCF(true);
	return CBRET_NONE;
	}

void BIOS_SetupDisks(void)
	{
	call_int13 = CALLBACK_Allocate();	
	CALLBACK_Setup(call_int13, &INT13_DiskHandler, CB_IRET_STI, "Int 13 Bios disk");
	RealSetVec(0x13, CALLBACK_RealPointer(call_int13));

	vPC_rStosb(BIOS_HARDDISK_COUNT, 2);				// Setup the Bios Area
	}
