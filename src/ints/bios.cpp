#include "vDos.h"
#include "mem.h"
#include "bios.h"
#include "regs.h"
#include "cpu.h"
#include "callback.h"
#include "inout.h"
#include "serialport.h"
#include "parport.h"

void CMOS_SetRegister(Bitu regNr, Bit8u val); //For setting equipment word

static Bitu INT70_Handler(void)
	{
	// Acknowledge irq with cmos
	IO_Write(0x70, 0xc);
	IO_Read(0x71);
	if (vPC_rLodsb(BIOS_WAIT_FLAG_ACTIVE))
		{
		Bit32u count = vPC_rLodsd(BIOS_WAIT_FLAG_COUNT);
		if (count > 997)
			vPC_rStosd(BIOS_WAIT_FLAG_COUNT, count-997);
		else
			{
			vPC_rStosd(BIOS_WAIT_FLAG_COUNT, 0);
			PhysPt addr = dWord2Ptr(vPC_rLodsd(BIOS_WAIT_FLAG_POINTER));
			vPC_rStosb(addr, vPC_rLodsb(addr)|0x80);
			vPC_rStosb(BIOS_WAIT_FLAG_ACTIVE, 0);
			vPC_rStosd(BIOS_WAIT_FLAG_POINTER, BIOS_WAIT_FLAG_TEMP);
			IO_Write(0x70, 0xb);
			IO_Write(0x71, IO_Read(0x71)&~0x40);
			}
		} 
	// Signal EOI to both pics
	IO_Write(0xa0, 0x20);
	IO_Write(0x20, 0x20);
	return 0;
	}

static Bitu INT1A_Handler(void)
	{
	CALLBACK_SIF(true);
	switch (reg_ah)
		{
	case 0x00:															// Get System time
		{
		Bit32u ticks = vPC_rLodsd(BIOS_TIMER);
		reg_al = vPC_rLodsb(BIOS_24_HOURS_FLAG);
		vPC_rStosb(BIOS_24_HOURS_FLAG, 0);								// reset the midnight flag
		reg_cx = (Bit16u)(ticks >> 16);
		reg_dx = (Bit16u)(ticks & 0xffff);
		break;
		}
	case 0x01:															// Set System time
		vPC_rStosd(BIOS_TIMER, (reg_cx<<16)|reg_dx);
		break;
	case 0x02:															// GET REAL-TIME CLOCK TIME (AT, XT286, PS)
		IO_Write(0x70, 0x04);											// Hours
		reg_ch = IO_Read(0x71);
		IO_Write(0x70, 0x02);											// Minutes
		reg_cl = IO_Read(0x71);
		IO_Write(0x70, 0x00);											// Seconds
		reg_dh = IO_Read(0x71);
		reg_dl = 0;														// Daylight saving disabled
		CALLBACK_SCF(false);
		break;
	case 0x04:															// GET REAL-TIME ClOCK DATE  (AT, XT286, PS)
		IO_Write(0x70, 0x32);											// Centuries
		reg_ch = IO_Read(0x71);
		IO_Write(0x70, 0x09);											// Years
		reg_cl = IO_Read(0x71);
		IO_Write(0x70, 0x08);											// Months
		reg_dh = IO_Read(0x71);
		IO_Write(0x70, 0x07);											// Days
		reg_dl = IO_Read(0x71);
		CALLBACK_SCF(false);
		break;
	case 0x80:															// Pcjr Setup Sound Multiplexer
	case 0x81:															// Tandy sound system check
	case 0x82:															// Tandy sound system start recording
	case 0x83:															// Tandy sound system start playback
	case 0x84:															// Tandy sound system stop playing
	case 0x85:															// Tandy sound system reset
		break;
	case 0xb1:															// PCI Bios Calls
		CALLBACK_SCF(true);
		break;
	default:
		LOG(LOG_BIOS,LOG_ERROR)("INT1A:Undefined call %2X",reg_ah);
		}
	return CBRET_NONE;
	}	

static Bitu INT11_Handler(void)
	{
	reg_ax = vPC_rLodsw(BIOS_CONFIGURATION);
	return CBRET_NONE;
	}

static void BIOS_HostTimeSync()
	{
	_SYSTEMTIME systime;												// Windows localdate/time
	GetLocalTime(&systime);
	dos.date.day = (Bit8u)systime.wDay;
	dos.date.month = (Bit8u)systime.wMonth;
	dos.date.year = (Bit16u)systime.wYear;

	Bit32u ticks = (Bit32u)(((double)(
		systime.wHour*3600*1000+
		systime.wMinute*60*1000+
		systime.wSecond*1000+ 
		systime.wMilliseconds))*(((double)PIT_TICK_RATE/65536.0)/1000.0));
	vPC_rStosd(BIOS_TIMER, ticks);
	}

static Bitu INT8_Handler(void)
	{
	BIOS_HostTimeSync();
	return CBRET_NONE;
	}

static Bitu INT1C_Handler(void)
	{
	return CBRET_NONE;
	}

static Bitu INT12_Handler(void)
	{
	reg_ax = vPC_rLodsw(BIOS_MEMORY_SIZE);
	return CBRET_NONE;
	}

static Bitu INT17_Handler(void)
	{
	if (reg_ah > 2 || reg_dx > 2)										// 0-2 printer port functions, no more than 3 parallel ports
		return CBRET_NONE;

	switch(reg_ah)
		{
	case 0:																// PRINTER: Write Character
		parallelPorts[reg_dx]->Putchar(reg_al);
		break;
	case 1:																// PRINTER: Initialize port
		parallelPorts[reg_dx]->initialize();
		break;
		};
	reg_ah = parallelPorts[reg_dx]->getPrinterStatus();
	return CBRET_NONE;
	}

static Bitu INT14_Handler(void)
	{
	if (reg_ah > 3 || reg_dx > 3)										// 0-3 serial port functions, no more than 4 serial ports
		return CBRET_NONE;
	
	switch (reg_ah)
		{
	case 0:																// Initialize port
	case 3:																// Get status
		{
		reg_ah = 0;														// Line status
		reg_al = 0x10;													// Modem status, Clear to send
		break;
		}
	case 1:																// Transmit character
		serialPorts[reg_dx]->Putchar(reg_al);
		reg_ah = 0;														// Line status
		break;
	case 2:																// Read character
		reg_ah = 0x80;													// Nothing received
		break;
		}
	return CBRET_NONE;
	}

static Bitu INT15_Handler(void)
	{
	static Bit16u biosConfigSeg = 0;
	switch (reg_ah)
		{
	case 0xC0:															// Get Configuration
		{
		if (biosConfigSeg == 0)
			biosConfigSeg = DOS_GetMemory(1);							// We have 16 bytes
		PhysPt data	= SegOff2Ptr(biosConfigSeg, 0);
		vPC_rStosw(data, 8);											// 8 Bytes following
		vPC_rStosb(data+2, 0xFC);										// Model ID (PC)
		vPC_rStosb(data+3, 0x02);										// Submodel ID
		vPC_rStosb(data+4, 0x00);										// Bios Revision
		vPC_rStosb(data+5, 0x70);										// Feature Byte 1
		vPC_rStosw(data+6, 0);											// Feature Byte 2 + 3
		vPC_rStosw(data+8, 0);											// Feature Byte 4 + 5
		CPU_SetSegGeneral(es, biosConfigSeg);
		reg_bx = 0;
		reg_ah = 0;
		CALLBACK_SCF(false);
		};
		break;
	case 0x4f:															// BIOS - Keyboard intercept
		CALLBACK_SCF(true);												// Carry should be set but let's just set it just in case
		break;
	case 0x83:															// BIOS - SET EVENT WAIT INTERVAL
		reg_ah = 0x86;													// Not supported
	case 0x86:															// BIOS - WAIT (AT, PS)
		CALLBACK_SCF(true);
		break;
	case 0x87:															// Copy extended memory
		{
		Bitu   bytes	= reg_cx * 2;
		PhysPt data		= SegPhys(es)+reg_si;
		PhysPt source	= (vPC_rLodsd(data+0x12) & 0x00FFFFFF) + (vPC_rLodsb(data+0x16)<<24);
		PhysPt dest		= (vPC_rLodsd(data+0x1A) & 0x00FFFFFF) + (vPC_rLodsb(data+0x1E)<<24);
		vPC_rMovsb(dest, source, bytes);
		reg_ax = 0;
		CALLBACK_SCF(false);
		break;
		}
	case 0x88:															// SYSTEM - GET EXTENDED MEMORY SIZE (286+)
		reg_ax = 0;
		CALLBACK_SCF(false);
		break;
	case 0x89:															// SYSTEM - SWITCH TO PROTECTED MODE
		reg_ah = 0xff;
		CALLBACK_SCF(true);
		break;
	case 0x90:															// OS HOOK - DEVICE BUSY
	case 0x91:															// OS HOOK - DEVICE POST
		CALLBACK_SCF(false);
		reg_ah = 0;
		break;
	case 0xc2:															// BIOS PS2 Pointing Device Support
		CALLBACK_SCF(true);
		reg_ah = 1;
		break;
	case 0xc4:															// BIOS POS Programm option Select
		CALLBACK_SCF(true);
		break;
	case 0x06:
		break;
	default:
		LOG_MSG("INT15: Unsupported/unknown call %4X", reg_ax);
		reg_ah = 0x86;
		CALLBACK_SCF(true);
		CALLBACK_SZF(false);					
		}
	return CBRET_NONE;
	}

static Bitu Reboot_Handler(void)
	{
	return CBRET_NONE;
	}

void BIOS_SetupKeyboard(void);
void BIOS_SetupDisks(void);

// set com port data in bios data area
// parameter: array of 4 com port base addresses, all 4 are set!
void BIOS_SetComPorts(Bit16u baseaddr[])
	{
	for (Bitu i = 0; i < 4; i++)
		vPC_aStosw(BIOS_BASE_ADDRESS_COM1 + i*2, baseaddr[i]);
	// set equipment word
	Bit16u equipmentword = vPC_rLodsw(BIOS_CONFIGURATION);
	equipmentword &= (~0x0E00);
	equipmentword |= (4 << 9);
	vPC_aStosw(BIOS_CONFIGURATION, equipmentword);
	CMOS_SetRegister(0x14, (Bit8u)(equipmentword&0xff));	//Should be updated on changes
	}

void BIOS_SetLPTPort(Bitu port, Bit16u baseaddr)			// port is always < 3
	{
	vPC_aStosw(BIOS_ADDRESS_LPT1 + port*2, baseaddr);
	vPC_aStosb(BIOS_LPT1_TIMEOUT + port, 10);
	// set equipment word: count ports
	Bit16u portcount = 0;
	for (Bitu i = 0; i < 3; i++)
		if (vPC_rLodsw(BIOS_ADDRESS_LPT1 + i*2) != 0)
			portcount++;
	Bit16u equipmentword = vPC_rLodsw(BIOS_CONFIGURATION);
	equipmentword &= (~0xC000);
	equipmentword |= (portcount << 14);
	vPC_rStosw(BIOS_CONFIGURATION, equipmentword);
	}

static 	CALLBACK_HandlerObject callback[11];

void BIOS_Init()
	{
	// Clear the Bios Data Area (0x400-0x5ff, 0x600- is accounted to DOS)
	vPC_rStoswb(0x400, 0, 0x200);

	// Setup all the interrupt handlers the bios controls

	// INT 8 Clock IRQ Handler
	Bitu call_irq0 = CALLBACK_Allocate();	
	CALLBACK_Setup(call_irq0, INT8_Handler, CB_IRQ0, dWord2Ptr(BIOS_DEFAULT_IRQ0_LOCATION), "IRQ 0 Clock");
	RealSetVec(0x08, BIOS_DEFAULT_IRQ0_LOCATION);
	// pseudocode for CB_IRQ0:
	//	callback INT8_Handler
	//	push ax,dx,ds
	//	int 0x1c
	//	cli
	//	pop ds,dx
	//	mov al, 0x20
	//	out 0x20, al
	//	pop ax
	//	iret

	vPC_rStosd(BIOS_TIMER, 0);		// Calculate the correct time

	// INT 11 Get equipment list
	callback[1].Install(&INT11_Handler, CB_IRET, "Int 11 Equipment");
	callback[1].Set_RealVec(0x11);

	// INT 12 Memory Size default at 640 kb
	callback[2].Install(&INT12_Handler, CB_IRET, "Int 12 Memory");
	callback[2].Set_RealVec(0x12);
	vPC_rStosw(BIOS_MEMORY_SIZE, 640);
		
	// INT 13 Bios Disk Support
	BIOS_SetupDisks();

	// INT 14 Serial Ports
	callback[3].Install(&INT14_Handler, CB_IRET_STI, "Int 14 COM-port");
	callback[3].Set_RealVec(0x14);

	// INT 15 Misc Calls
	callback[4].Install(&INT15_Handler, CB_IRET, "Int 15 Bios");
	callback[4].Set_RealVec(0x15);

	// INT 16 Keyboard handled in another file
	BIOS_SetupKeyboard();

	// INT 17 Printer Routines
	callback[5].Install(&INT17_Handler, CB_IRET_STI, "Int 17 Printer");
	callback[5].Set_RealVec(0x17);

	// INT 1A TIME and some other functions
	callback[6].Install(&INT1A_Handler, CB_IRET_STI, "Int 1a Time");
	callback[6].Set_RealVec(0x1A);

	// INT 1C System Timer tick called from INT 8
	callback[7].Install(&INT1C_Handler, CB_IRET, "Int 1c Timer");
	callback[7].Set_RealVec(0x1C);
		
	// IRQ 8 RTC Handler
	callback[8].Install(&INT70_Handler, CB_IRET, "Int 70 RTC");
	callback[8].Set_RealVec(0x70);

	// Irq 9 rerouted to irq 2
	callback[9].Install(NULL, CB_IRQ9, "irq 9 bios");
	callback[9].Set_RealVec(0x71);

	// Reboot
	callback[10].Install(&Reboot_Handler, CB_IRET, "reboot");
	callback[10].Set_RealVec(0x18);
	RealPt rptr = callback[10].Get_RealPointer();
	RealSetVec(0x19, rptr);
	// set system BIOS entry point too
	vPC_aStosb(0xFFFF0, 0xEA);				// FARJMP
	vPC_aStosd(0xFFFF1, rptr);

	// Irq 2
	Bitu call_irq2 = CALLBACK_Allocate();	
	CALLBACK_Setup(call_irq2, NULL, CB_IRET_EOI_PIC1, dWord2Ptr(BIOS_DEFAULT_IRQ2_LOCATION), "irq 2 bios");
	RealSetVec(0x0a, BIOS_DEFAULT_IRQ2_LOCATION);

	// Some hardcoded vectors
	vPC_aStosb(dWord2Ptr(BIOS_DEFAULT_HANDLER_LOCATION), 0xcf);		// bios default interrupt vector location -> IRET
	vPC_aStosw(dWord2Ptr(RealGetVec(0x12))+0x12, 0x20);				// Hack for Jurresic

	vPC_aStosb(0xffffe, 0xfc);

	// System BIOS identification
	phys_writes(0xfe00e, "IBM COMPATIBLE 486 BIOS COPYRIGHT The DOSBox Team.",  50);
		
	// System BIOS version
	phys_writes(0xfe061, "vDOS FakeBIOS v1.0",  18);

	// write system BIOS date
	phys_writes(0xffff5, "01/01/92",  8);
	vPC_aStosb(0xfffff, 0x55);	// signature

	// program system timer
	// timer 2
	IO_Write(0x43, 0xb6);
	IO_Write(0x42, 1320&0xff);
	IO_Write(0x42, 1320>>8);

	// Setup some stuff in 0x40 bios segment
		
	// port timeouts
	for (Bitu i = BIOS_COM1_TIMEOUT; i <= BIOS_COM4_TIMEOUT; i++)
		vPC_rStosb(i, 1);

	// Setup equipment list
	// look http://www.bioscentral.com/misc/bda.htm
		
	Bit16u config = 0x24;	// Startup 80x25 color, PS2 mouse
	vPC_rStosw(BIOS_CONFIGURATION, config);
	CMOS_SetRegister(0x14, (Bit8u)(config&0xff));	//Should be updated on changes

	BIOS_HostTimeSync();
	}
