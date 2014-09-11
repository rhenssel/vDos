#include <stdlib.h>
#include "vDos.h"
#include "callback.h"
#include "mem.h"
#include "cpu.h"

/* CallBack are located at 0xF000:0x1000  (see CB_SEG and CB_SOFFSET in callback.h)
   And they are 16 bytes each and you can define them to behave in certain ways like a
   far return or and IRET
*/

CallBack_Handler CallBack_Handlers[CB_MAX];
char* CallBack_Description[CB_MAX];

static Bitu call_stop, call_idle, call_default, call_default2;
Bitu call_priv_io;

static Bitu illegal_handler(void)
	{
	E_Exit("Illegal CallBack Called");
	return 1;
	}

Bitu CALLBACK_Allocate(void)
	{
	for (Bitu i = 1; i < CB_MAX; i++)
		if (CallBack_Handlers[i] == &illegal_handler)
			{
			CallBack_Handlers[i] = 0;
			return i;	
			}
	E_Exit("CALLBACK: Can't allocate handler");
	return 0;
	}

void CALLBACK_DeAllocate(Bitu in)
	{
	CallBack_Handlers[in] = &illegal_handler;
	}

void CALLBACK_Idle(void)
	{
	// this makes the cpu execute instructions to handle irq's and then come back
	Bitu oldIF = GETFLAG(IF);
	SETFLAGBIT(IF, true);
	Bit16u oldcs = SegValue(cs);
	Bit32u oldeip = reg_eip;
	SegSet16(cs, CB_SEG);
//	reg_eip = call_idle*CB_SIZE;				// This is wrong in DosBox 0.74
	reg_eip = CB_SOFFSET+call_idle*CB_SIZE;
	RunPC();
	reg_eip = oldeip;
	SegSet16(cs, oldcs);
	SETFLAGBIT(IF, oldIF);
	if (CPU_Cycles > 0) 
		CPU_Cycles = 0;
	}

static Bitu default_handler(void)
	{
	LOG_MSG("Unhandled interrupt called: %2X", lastint);
	Bit32u addr = (Bit32u)MemBase+4*0x0e;
	LOG_MSG("0:4xe: %x", *(Bit32u*)addr);
	addr = dWord2Ptr(*(Bit32u*)addr);
	LOG_MSG("addr: %x, val: %x", addr, *(Bit32u*)(MemBase+addr));

	return CBRET_NONE;
	}

static Bitu stop_handler(void)
	{
	return CBRET_STOP;
	}


// Shouldn't this be CPU_SW_Interrupt(Bitu num, Bitu oldeip)
// Do we want to call the original INT and not check pmode?

void CALLBACK_RunRealInt(Bit8u intnum)
	{
	Bit32u oldeip = reg_eip;
	Bit16u oldcs = SegValue(cs);
	reg_eip = CB_SOFFSET+(CB_MAX*CB_SIZE)+(intnum*6);
	SegSet16(cs, CB_SEG);
	RunPC();
	reg_eip = oldeip;
	SegSet16(cs, oldcs);
	}

void CALLBACK_SZF(bool val)
	{
	Bit16u tempf = vPC_rLodsw(SegPhys(ss)+reg_sp+4); 
	if (val)
		tempf |= FLAG_ZF; 
	else
		tempf &= ~FLAG_ZF; 
	vPC_rStosw(SegPhys(ss)+reg_sp+4, tempf); 
	}

void CALLBACK_SCF(bool val)
	{
	Bit16u tempf = vPC_rLodsw(SegPhys(ss)+reg_sp+4); 
	if (val)
		tempf |= FLAG_CF; 
	else
		tempf &= ~FLAG_CF; 
	vPC_rStosw(SegPhys(ss)+reg_sp+4, tempf); 
	}

void CALLBACK_SIF(bool val)
	{
	Bit16u tempf = vPC_rLodsw(SegPhys(ss)+reg_sp+4); 
	if (val)
		tempf |= FLAG_IF; 
	else
		tempf &= ~FLAG_IF; 
	vPC_rStosw(SegPhys(ss)+reg_sp+4, tempf); 
	}

void CALLBACK_SetDescription(Bitu nr, const char* descr)
	{
	if (descr)
		{
		CallBack_Description[nr] = new char[strlen(descr)+1];
		strcpy(CallBack_Description[nr], descr);
		}
	else
		CallBack_Description[nr] = 0;
	}

const char* CALLBACK_GetDescription(Bitu nr)
	{
	if (nr >= CB_MAX)
		return 0;
	return CallBack_Description[nr];
	}

Bitu CALLBACK_SetupExtra(Bitu callback, Bitu type, PhysPt physAddress, bool use_cb = true)
	{
	switch (type)
		{
	case CB_RETF:
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress, 0xCB);							// RETF
		return (use_cb ? 5 : 1);
	case CB_RETF8:
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+0, 0xCA);						// RETF 8
		vPC_aStosw(physAddress+1, 0x0008);
		return (use_cb ? 7 : 3);
	case CB_IRET:
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress, 0xCF);							// IRET
		return (use_cb ? 5 : 1);
	case CB_IRET_STI:
		vPC_aStosb(physAddress+0, 0xFB);						// STI
		if (use_cb)
			{
			vPC_aStosw(physAddress+1, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+3, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+1, 0xCF);						// IRET
		return (use_cb ? 6 : 2);
	case CB_IRET_EOI_PIC1:
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+0, 0x50);						// push ax
		vPC_aStosw(physAddress+1, 0x20b0);						// mov al, 0x20
		vPC_aStosw(physAddress+3, 0x20e6);						// out 0x20, al
		vPC_aStosw(physAddress+5, 0xcf58);						// pop ax + IRET
		return (use_cb ? 0x0b : 0x07);
	case CB_IRQ0:	// timer int8
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosw(physAddress+0, 0x5250);						// push ax + push dx
		vPC_aStosb(physAddress+2, 0x1e);						// push ds
		vPC_aStosw(physAddress+3, 0x1ccd);						// int 1c
		vPC_aStosb(physAddress+5, 0xfa);						// cli
		vPC_aStosw(physAddress+6, 0x5a1f);						// pop ds + pop dx
		vPC_aStosw(physAddress+8, 0x20b0);						// mov al, 0x20
		vPC_aStosw(physAddress+0x0a, 0x20e6);					// out 0x20, al
		vPC_aStosw(physAddress+0x0c, 0xcf58);					// pop ax + IRET
		return (use_cb ? 0x12 : 0x0e);
	case CB_IRQ1:	// keyboard int9
		vPC_aStosb(physAddress+0, 0x50);						// push ax
		vPC_aStosw(physAddress+1, 0x60e4);						// in al, 0x60
		vPC_aStosw(physAddress+3, 0x4fb4);						// mov ah, 0x4f
		vPC_aStosb(physAddress+5, 0xf9);						// stc
		vPC_aStosw(physAddress+6, 0x15cd);						// int 15
		if (use_cb)
			{
			vPC_aStosw(physAddress+0x08, 0x0473);				// jc skip
			vPC_aStosw(physAddress+0x0a, 0x38FE);				// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+0x0c, callback);			// The immediate word
			// jump here to (skip):
			physAddress += 6;
			}
		vPC_aStosb(physAddress+0x08, 0xfa);						// cli
		vPC_aStosw(physAddress+0x09, 0x20b0);					// mov al, 0x20
		vPC_aStosw(physAddress+0x0b, 0x20e6);					// out 0x20, al
		vPC_aStosw(physAddress+0x0d, 0xcf58);					// pop ax + IRET
		return (use_cb ? 0x15 : 0x0f);
	case CB_IRQ9:	// pic cascade interrupt
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+0, 0x50);						// push ax
		vPC_aStosw(physAddress+1, 0x61b0);						// mov al, 0x61
		vPC_aStosw(physAddress+3, 0xa0e6);						// out 0xa0, al
		vPC_aStosw(physAddress+5, 0x0acd);						// int a
		vPC_aStosw(physAddress+7, 0x58fa);						// cli + pop ax
		vPC_aStosb(physAddress+9, 0xcf);						// IRET
		return (use_cb ? 0x0e : 0x0a);
	case CB_IRQ12:	// ps2 mouse int74
		if (!use_cb)
			E_Exit("Int74 callback must implement a callback handler!");
		vPC_aStosw(physAddress+0, 0x061e);						// push ds + push ed
		vPC_aStosw(physAddress+2, 0x6066);						// pushad
		vPC_aStosw(physAddress+4, 0xfbfc);						// cld + sti
		vPC_aStosw(physAddress+6, 0x38FE);						// GRP 4 + Extra Callback instruction
		vPC_aStosw(physAddress+8, callback);					// The immediate word
		return 0x0a;
	case CB_IRQ12_RET:	// ps2 mouse int74 return
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+0x00, 0xfa);						// cli
		vPC_aStosw(physAddress+0x01, 0x20b0);					// mov al, 0x20
		vPC_aStosw(physAddress+0x03, 0xa0e6);					// out 0xa0, al
		vPC_aStosw(physAddress+0x05, 0x20e6);					// out 0x20, al
		vPC_aStosw(physAddress+0x07, 0x6166);					// popad
		vPC_aStosw(physAddress+0x09, 0x1f07);					// pop es + pop ds
		vPC_aStosb(physAddress+0x0b, 0xcf);						// IRET
		return (use_cb?0x10:0x0c);
	case CB_MOUSE:
		vPC_aStosw(physAddress+0, (Bit16u)0x07eb);				// jmp i33hd
		physAddress += 9;
		// jump here to (i33hd):
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+0, 0xCF);						// IRET
		return (use_cb ? 0x0e : 0x0a);
	case CB_INT16:
		vPC_aStosb(physAddress+0, 0xFB);						// STI
		if (use_cb)
			{
			vPC_aStosw(physAddress+1, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+3, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+1, 0xCF);						// IRET
		for (Bitu i = 0; i <= 0x0b; i++)
			vPC_aStosb(physAddress+2+i, 0x90);					// NOP
		vPC_aStosw(physAddress+0x0e, 0xedeb);					// jmp callback
		return (use_cb ? 0x10 : 0x0c);
	case CB_INT29:		// fast console output
		if (use_cb)
			{
			vPC_aStosw(physAddress+0, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+2,callback);					// The immediate word
			physAddress += 4;
			}
		vPC_aStosw(physAddress+0x00, 0x5350);					// push ax + push bx
		vPC_aStosw(physAddress+0x02, 0x0eb4);					// mov ah, 0x0e
		vPC_aStosb(physAddress+0x04, 0xbb);						// mov bx,
		vPC_aStosw(physAddress+0x05, 7);						// 0x0007
		vPC_aStosw(physAddress+0x07, 0x10cd);					// int 10
		vPC_aStosw(physAddress+0x09, 0x585b);					// pop bx + pop ax
		vPC_aStosb(physAddress+0x0b, 0xcf);						// IRET
		return (use_cb ? 0x10 : 0x0c);
	case CB_HOOKABLE:
		vPC_aStosw(physAddress+0, 0x03eb);						// jump near + offset
		vPC_aStosw(physAddress+2, 0x9090);						// NOP + NOP
		vPC_aStosb(physAddress+4, 0x90);						// NOP
		if (use_cb)
			{
			vPC_aStosw(physAddress+5, 0x38FE);					// GRP 4 + Extra Callback instruction
			vPC_aStosw(physAddress+7, callback);				// The immediate word
			physAddress += 4;
			}
		vPC_aStosb(physAddress+5, 0xCB);						// RETF
		return (use_cb ? 0x0a : 0x06);
	default:
		E_Exit("CALLBACK: Setup:Illegal type %d", type);
		}
	return 0;
	}

bool CALLBACK_Setup(Bitu callback, CallBack_Handler handler, Bitu type, const char* descr)
	{
	if (callback >= CB_MAX)
		return false;
	CALLBACK_SetupExtra(callback, type, CALLBACK_PhysPointer(callback)+0, (handler!=NULL));
	CallBack_Handlers[callback] = handler;
	CALLBACK_SetDescription(callback, descr);
	return true;
	}

Bitu CALLBACK_Setup(Bitu callback, CallBack_Handler handler, Bitu type, PhysPt addr, const char* descr)
	{
	if (callback >= CB_MAX)
		return 0;
	Bitu csize = CALLBACK_SetupExtra(callback, type, addr, (handler != NULL));
	if (csize > 0)
		{
		CallBack_Handlers[callback] = handler;
		CALLBACK_SetDescription(callback, descr);
		}
	return csize;
	}

void CALLBACK_RemoveSetup(Bitu callback)
	{
	for (Bitu i = 0; i < CB_SIZE; i++)
		vPC_aStosb(CALLBACK_PhysPointer(callback)+i, (Bit8u)0);
	}

CALLBACK_HandlerObject::~CALLBACK_HandlerObject()
	{
	if (!installed)
		return;
	if (m_type == CALLBACK_HandlerObject::SETUP)
		{
		if (vectorhandler.installed)
			{
			// See if we are the current handler. if so restore the old one
			if (RealGetVec(vectorhandler.interrupt) == Get_RealPointer())
				RealSetVec(vectorhandler.interrupt, vectorhandler.old_vector);
			else 
				LOG(LOG_MISC,LOG_WARN)("Interrupt vector changed on %X %s", vectorhandler.interrupt, CALLBACK_GetDescription(m_callback));
			}
		CALLBACK_RemoveSetup(m_callback);
		}
	else if (m_type == CALLBACK_HandlerObject::SETUPAT)
		E_Exit("Callback: SETUP at not handled yet.");
	else if (m_type == CALLBACK_HandlerObject::NONE)
		{
		// Do nothing. Merely DeAllocate the callback
		}
	else
		E_Exit("What kind of callback is this!");
	if (CallBack_Description[m_callback])
		delete [] CallBack_Description[m_callback];
	CallBack_Description[m_callback] = 0;
	CALLBACK_DeAllocate(m_callback);
	}

void CALLBACK_HandlerObject::Install(CallBack_Handler handler, Bitu type, const char* description)
	{
	if (!installed)
		{
		installed = true;
		m_type = SETUP;
		m_callback = CALLBACK_Allocate();
		CALLBACK_Setup(m_callback, handler, type, description);
		}
	else
		E_Exit("CALLBACK: Allready installed");
	}

void CALLBACK_HandlerObject::Install(CallBack_Handler handler, Bitu type, PhysPt addr, const char* description)
	{
	if (!installed)
		{
		installed = true;
		m_type = SETUP;
		m_callback = CALLBACK_Allocate();
		CALLBACK_Setup(m_callback, handler, type,addr, description);
		}
	else
		E_Exit("CALLBACK: Allready installed");
	}

void CALLBACK_HandlerObject::Allocate(CallBack_Handler handler, const char* description)
	{
	if (!installed)
		{
		installed = true;
		m_type = NONE;
		m_callback = CALLBACK_Allocate();
		CALLBACK_SetDescription(m_callback, description);
		CallBack_Handlers[m_callback] = handler;
		}
	else
		E_Exit("CALLBACK: Allready installed");
	}

void CALLBACK_HandlerObject::Set_RealVec(Bit8u vec)
	{
	if( !vectorhandler.installed)
		{
		vectorhandler.installed = true;
		vectorhandler.interrupt = vec;
		RealSetVec(vec, Get_RealPointer(), vectorhandler.old_vector);
		}
	else
		E_Exit ("CALLBACK: Double usage of vector handler");
	}

void CALLBACK_Init()
	{
	for (Bitu i = 0; i < CB_MAX; i++)
		CallBack_Handlers[i] = &illegal_handler;

	// Setup the Stop Handler
	call_stop = CALLBACK_Allocate();
	CallBack_Handlers[call_stop] = stop_handler;
	CALLBACK_SetDescription(call_stop, "stop");
	vPC_aStosw(CALLBACK_PhysPointer(call_stop)+0, 0x38FE);		// GRP 4 + Extra Callback instruction
	vPC_aStosw(CALLBACK_PhysPointer(call_stop)+2, (Bit16u)call_stop);

	// Setup the idle handler
	call_idle = CALLBACK_Allocate();
	CallBack_Handlers[call_idle] = stop_handler;
	CALLBACK_SetDescription(call_idle, "idle");
	for (Bitu i = 0; i <= 11; i++)
		vPC_aStosb(CALLBACK_PhysPointer(call_idle)+i, 0x90);	// NOP
	vPC_aStosw(CALLBACK_PhysPointer(call_idle)+12, 0x38FE);		// GRP 4 + Extra Callback instruction
	vPC_aStosw(CALLBACK_PhysPointer(call_idle)+14, (Bit16u)call_idle);

	// Default handlers for unhandled interrupts that have to be non-null
	call_default = CALLBACK_Allocate();
	CALLBACK_Setup(call_default, &default_handler, CB_IRET, "default");
	call_default2 = CALLBACK_Allocate();
	CALLBACK_Setup(call_default2, &default_handler, CB_IRET, "default");
   
	// Only setup default handler for first part of interrupt table
	for (Bit16u ct = 0; ct < 0x60; ct++)
		vPC_rStosd(ct*4, CALLBACK_RealPointer(call_default));
	for (Bit16u ct = 0x68; ct < 0x70; ct++)
		vPC_rStosd(ct*4, CALLBACK_RealPointer(call_default));

	// Setup block of 0xCD 0xxx instructions
	PhysPt rint_base = CALLBACK_GetBase()+CB_MAX*CB_SIZE;
	for (Bitu i = 0; i <= 0xff; i++)
		{
		vPC_aStosb(rint_base, 0xCD);
		vPC_aStosb(rint_base+1, (Bit8u)i);
		vPC_aStosw(rint_base+2, 0x38FE);								// GRP 4 + Extra Callback instruction
		vPC_aStosw(rint_base+4, (Bit16u)call_stop);
		rint_base += 6;
		}

	// setup a few interrupt handlers that point to bios IRETs by default
	vPC_rStosd(0x0e*4, CALLBACK_RealPointer(call_default2));			// design your own railroad
	vPC_rStosd(0x66*4, CALLBACK_RealPointer(call_default));				// war2d
	vPC_rStosd(0x67*4, CALLBACK_RealPointer(call_default));
	vPC_rStosd(0x68*4, CALLBACK_RealPointer(call_default));
	vPC_rStosd(0x5c*4, CALLBACK_RealPointer(call_default));				// Network stuff

	// virtualizable in-out opcodes
	call_priv_io = CALLBACK_Allocate();
	vPC_aStosw(CALLBACK_PhysPointer(call_priv_io)+0x00, 0xcbec);		// in al, dx + retf
	vPC_aStosw(CALLBACK_PhysPointer(call_priv_io)+0x02, 0xcbed);		// in ax, dx + retf
	vPC_aStosw(CALLBACK_PhysPointer(call_priv_io)+0x04, 0xed66);		// in eax, dx
	vPC_aStosb(CALLBACK_PhysPointer(call_priv_io)+0x06, (Bit8u)0xcb);	// retf

	vPC_aStosw(CALLBACK_PhysPointer(call_priv_io)+0x08, 0xcbee);		// out dx, al + retf
	vPC_aStosw(CALLBACK_PhysPointer(call_priv_io)+0x0a, 0xcbef);		// out dx, ax + retf
	vPC_aStosw(CALLBACK_PhysPointer(call_priv_io)+0x0c, 0xef66);		// out dx, eax
	vPC_aStosb(CALLBACK_PhysPointer(call_priv_io)+0x0e, (Bit8u)0xcb);	// retf
	}
