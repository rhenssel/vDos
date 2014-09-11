#include <string.h>
#include <math.h>

#include "vDos.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "cpu.h"
#include "mouse.h"
#include "pic.h"
#include "inout.h"
#include "int10.h"
#include "bios.h"
#include "dos_inc.h"

static Bitu call_int33, call_int74, int74_ret_callback, call_mouse_bd;

struct button_event {
	Bit8u type;
	Bit8u buttons;
};

#define QUEUE_SIZE 16
#define MOUSE_BUTTONS 2
#define MOUSE_IRQ 12
#define POS_X ((Bit16s)(mouse.x) & mouse.granMask)
#define POS_Y (Bit16s)(mouse.y)

static Bit16u defaultTextAndMask = 0x77FF;
static Bit16u defaultTextXorMask = 0x7700;

static struct {
	Bit8u	buttons;
	Bit16u	times_pressed[MOUSE_BUTTONS];
	Bit16u	times_released[MOUSE_BUTTONS];
	Bit16u	last_released_x[MOUSE_BUTTONS];
	Bit16u	last_released_y[MOUSE_BUTTONS];
	Bit16u	last_pressed_x[MOUSE_BUTTONS];
	Bit16u	last_pressed_y[MOUSE_BUTTONS];
	Bit16u	hidden;
	Bit16s	max_x, max_y;
	Bit16s	mickey_x, mickey_y;
	Bit16u	x, y;
	button_event event_queue[QUEUE_SIZE];
	Bit8u	events;
	Bit16u	sub_seg, sub_ofs;
	Bit16u	sub_mask;
	bool	wp6x;							// WP6.x with Mouse Driver (Absolute/Pen)

	bool	background;
	Bit16s	backposx, backposy;
	Bit8u	backChar;
	Bit8u	backAttr;
	Bit16u  textAndMask, textXorMask;

	Bit16u	oldhidden;
	Bit8u	page;
	bool	enabled;
	bool	timer_in_progress;
	bool	in_UIR;
	Bit16s	granMask;
} mouse;

#define MOUSE_HAS_MOVED			1
#define MOUSE_LEFT_PRESSED		2
#define MOUSE_LEFT_RELEASED		4
#define MOUSE_RIGHT_PRESSED		8
#define MOUSE_RIGHT_RELEASED	16
#define MOUSE_MIDDLE_PRESSED	32
#define MOUSE_MIDDLE_RELEASED	64
#define MOUSE_DELAY				5.0


void MOUSE_Limit_Events(Bitu /*val*/)
	{
	mouse.timer_in_progress = false;
	if (mouse.events)
		{
		mouse.timer_in_progress = true;
		PIC_AddEvent(MOUSE_Limit_Events, MOUSE_DELAY);
		PIC_ActivateIRQ(MOUSE_IRQ);
		}
	}

void Mouse_AddEvent(Bit8u type)
	{
	if (mouse.events < QUEUE_SIZE)
		{
		if (mouse.events > 0)
			{
			if (type == MOUSE_HAS_MOVED)			// Skip duplicate events
				return;
			/* Always put the newest element in the front as that the events are 
			 * handled backwards (prevents doubleclicks while moving)
			 */
			for (Bitu i = mouse.events; i; i--)
				mouse.event_queue[i] = mouse.event_queue[i-1];
			}
		mouse.event_queue[0].type = type;
		mouse.event_queue[0].buttons = mouse.buttons;
		mouse.events++;
		}
	if (!mouse.timer_in_progress)
		{
		mouse.timer_in_progress = true;
		PIC_AddEvent(MOUSE_Limit_Events, MOUSE_DELAY);
		PIC_ActivateIRQ(MOUSE_IRQ);
		}
	}

// ***************************************************************************
// Mouse cursor - text mode
// ***************************************************************************
// Write and read directly to the screen. Do no use int_setcursorpos (LOTUS123)
extern void WriteChar(Bit16u col, Bit16u row, Bit8u page, Bit8u chr, Bit8u attr, bool useattr);
extern void ReadCharAttr(Bit16u col, Bit16u row, Bit8u page, Bit16u * result);

void RestoreCursorBackgroundText()
	{
	if (!mouse.hidden && mouse.background)
		{
		WriteChar(mouse.backposx, mouse.backposy, vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE), mouse.backChar, mouse.backAttr, true);
		mouse.background = false;
		}
	}

void DrawCursor()									// In graphics mode, always use standard SDL cursor
	{
	if (!mouse.hidden && CurMode->type == M_TEXT)	// Not hidden and in Textmode
		{
		RestoreCursorBackgroundText();				// Restore Background
		// Save Background
		mouse.backposx = POS_X>>3;
		mouse.backposy = POS_Y>>3;
		// use current page (CV program)
		Bit8u page = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);

		Bit16u result;
		ReadCharAttr(mouse.backposx, mouse.backposy, page, &result);
		mouse.backChar = (Bit8u)(result & 0xFF);
		mouse.backAttr = (Bit8u)(result>>8);
		mouse.background = true;
		// Write Cursor
		result = (result & mouse.textAndMask) ^ mouse.textXorMask;
		WriteChar(mouse.backposx, mouse.backposy, page, (Bit8u)(result&0xFF), (Bit8u)(result>>8), true);
		}
	}

void Mouse_CursorMoved(Bit16u x, Bit16u y, Bit16u scale_x, Bit16u scale_y)
	{
	if (CurMode->type == M_TEXT)
		{
		mouse.mickey_x += x*8/scale_x - mouse.x;
		mouse.mickey_y += (y*8/scale_y - mouse.y)*2;
		mouse.x = x*8/scale_x;
		if (mouse.wp6x)								// WP6.x with Mouse Driver (Absolute/Pen) expects times 2
			mouse.y = y*16/scale_y;
		else
			mouse.y = y*8/scale_y;
		if (mouse.mickey_x != 0 || mouse.mickey_y != 0)
			Mouse_AddEvent(MOUSE_HAS_MOVED);
		if (mouse.hidden || (mouse.backposx == POS_X>>3 && mouse.backposy == POS_Y>>3))
			return;
		DrawCursor();
		}
	else
		{
		mouse.mickey_x += x/scale_x - mouse.x;
		mouse.mickey_y += y/scale_y - mouse.y;
		mouse.x = x/scale_x;
		mouse.y = y/scale_y;
		if (mouse.mickey_x != 0 || mouse.mickey_y != 0)
			Mouse_AddEvent(MOUSE_HAS_MOVED);
		}
	}

void Mouse_ButtonPressed(Bit8u button)
	{
	switch (button)
		{
	case 0:
		mouse.buttons |= 1;
		Mouse_AddEvent(MOUSE_LEFT_PRESSED);
		break;
	case 1:
		mouse.buttons |= 2;
		Mouse_AddEvent(MOUSE_RIGHT_PRESSED);
		break;
	default:
		return;
	}
	mouse.times_pressed[button]++;
	mouse.last_pressed_x[button] = POS_X;
	mouse.last_pressed_y[button] = POS_Y;
}

void Mouse_ButtonReleased(Bit8u button)
	{
	switch (button)
		{
	case 0:
		mouse.buttons &= ~1;
		Mouse_AddEvent(MOUSE_LEFT_RELEASED);
		break;
	case 1:
		mouse.buttons &= ~2;
		Mouse_AddEvent(MOUSE_RIGHT_RELEASED);
		break;
	default:
		return;
		}
	mouse.times_released[button]++;	
	mouse.last_released_x[button] = POS_X;
	mouse.last_released_y[button] = POS_Y;
	}

static void Mouse_ResetHardware(void)
	{
	PIC_SetIRQMask(MOUSE_IRQ, false);
	}

// Does way too much. Many things should be moved to mouse reset one day
void Mouse_NewVideoMode(void)
	{
	// Get the correct resolution from the current video mode
	Bit8u mode = vPC_rLodsb(BIOS_VIDEO_MODE);
	switch (mode)
		{
	case 0x00:	case 0x01:	case 0x02:	case 0x03:
		{
		Bitu rows = vPC_rLodsb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
		if ((rows == 0) || (rows > 250))
			rows = 25-1;
		mouse.max_y = 8*(rows+1)-1;
		break;
		}
	case 0x04:	case 0x05:	case 0x06:	case 0x07:
	case 0x08:	case 0x09:	case 0x0a:	case 0x0d:
	case 0x0e:	case 0x13:
		mouse.max_y = 199;
		break;
	case 0x0f:	case 0x10:
		mouse.max_y = 349;
		break;
	case 0x11:	case 0x12:
		mouse.max_y = 479;
		break;
	default:
		LOG(LOG_MOUSE, LOG_ERROR)("Unhandled videomode %X on reset", mode);
		return;
		}
	mouse.hidden = 1;
	mouse.max_x = 639;
	mouse.granMask = (mode == 0x0d || mode == 0x13) ? 0xfffe : 0xffff;

	mouse.events = 0;
	mouse.timer_in_progress = false;
	PIC_RemoveEvents(MOUSE_Limit_Events);

	mouse.background	= false;
	mouse.textAndMask	= defaultTextAndMask;
	mouse.textXorMask	= defaultTextXorMask;
	mouse.page			= 0;
	mouse.enabled		= true;
	mouse.oldhidden		= 1;
	}

// Much too empty, Mouse_NewVideoMode contains stuff that should be in here
static void Mouse_Reset(void)
	{
	// Remove drawn mouse Legends of Valor
	if (CurMode->type == M_TEXT)
		RestoreCursorBackgroundText();
	mouse.hidden = 1;
	Mouse_NewVideoMode();

	mouse.mickey_x = 0;
	mouse.mickey_y = 0;

	// Dont set max coordinates here. it is done by SetResolution!
	mouse.x = 0;
	mouse.y = mouse.max_y;
//	mouse.x = ((mouse.max_x + 1)/ 2);
//	mouse.y = ((mouse.max_y + 1)/ 2);
	mouse.sub_mask = 0;
	mouse.in_UIR = false;
	}

static Bitu INT33_Handler(void)
	{
	switch (reg_ax)
		{
	case 0x00:		// Reset Driver and Read Status
		Mouse_ResetHardware();		// fallthrough
	case 0x21:		// Software Reset
		reg_ax = 0xffff;
		reg_bx = MOUSE_BUTTONS;
		Mouse_Reset();
		break;
	case 0x01:		// Show Mouse
		if (mouse.hidden)
			mouse.hidden--;
		DrawCursor();
		break;
	case 0x02:		// Hide Mouse
		{
		if (CurMode->type == M_TEXT)
			RestoreCursorBackgroundText();
		mouse.hidden++;
		}
		break;
	case 0x03:		// Return position and Button Status
		reg_bx = mouse.buttons;
		reg_cx = POS_X;
		reg_dx = POS_Y;
		break;
	case 0x04:		// Position Mouse, we just don't
		break;
	case 0x05:		// Return Button Press Data
		{
		reg_ax = mouse.buttons;
		Bit16u but = reg_bx;
		if (but >= MOUSE_BUTTONS)
			but = MOUSE_BUTTONS - 1;
		reg_cx = mouse.last_pressed_x[but];
		reg_dx = mouse.last_pressed_y[but];
		reg_bx = mouse.times_pressed[but];
		mouse.times_pressed[but] = 0;
		break;
		}
	case 0x06:		// Return Button Release Data
		{
		reg_ax = mouse.buttons;
		Bit16u but = reg_bx;
		if (but >= MOUSE_BUTTONS)
			but = MOUSE_BUTTONS - 1;
		reg_cx = mouse.last_released_x[but];
		reg_dx = mouse.last_released_y[but];
		reg_bx = mouse.times_released[but];
		mouse.times_released[but] = 0;
		break;
		}
	case 0x07:		// Define horizontal cursor range
	case 0x08:		// Define vertical cursor range
	case 0x09:		// Define GFX Cursor (removed)
		break;
	case 0x0a:		// Define Text Cursor
		mouse.textAndMask	= reg_cx;
		mouse.textXorMask	= reg_dx;
		break;
	case 0x0b:		// Read Motion Data
		reg_cx = mouse.mickey_x;
		reg_dx = mouse.mickey_y;
		mouse.mickey_x = 0;
		mouse.mickey_y = 0;
		break;
	case 0x0c:		// Define interrupt subroutine parameters
		mouse.sub_mask	= reg_cx;
		mouse.sub_seg	= SegValue(es);
		mouse.sub_ofs	= reg_dx;
		break;
	case 0x0f:		// Define mickey/pixel rate
		break;
	case 0x10:      // Define screen region for updating
		break;
	case 0x13:      // Set double-speed threshold
 		break;
	case 0x14:		// Exchange event-handler
		{
		Bit16u oldSeg = mouse.sub_seg;
		Bit16u oldOfs = mouse.sub_ofs;
		Bit16u oldMask = mouse.sub_mask;
		// Set new values
		mouse.sub_mask	= reg_cx;
		mouse.sub_seg	= SegValue(es);
		mouse.sub_ofs	= reg_dx;
		mouse.wp6x = reg_dx == 0x1b6 ? true : false;			// signature for WP6.x with Mouse Driver (Absolute/Pen)
		// Return old values
		reg_cx = oldMask;
		reg_dx = oldOfs;
		SegSet16(es, oldSeg);
		}
		break;		
	case 0x15:		// Get Driver storage space requirements
		reg_bx = sizeof(mouse);
		break;
	case 0x16:		// Save driver state
		vPC_rBlockWrite(SegPhys(es)+reg_dx, &mouse, sizeof(mouse));
		break;
	case 0x17:		// load driver state
		vPC_rBlockRead(SegPhys(es)+reg_dx, &mouse, sizeof(mouse));
		break;
	case 0x1a:		// Set mouse sensitivity
		// ToDo : double mouse speed value
//		Mouse_SetSensitivity(reg_bx, reg_cx, reg_dx);
		break;
	case 0x1b:		// Get mouse sensitivity (fixed values)
		reg_bx = 50;
		reg_cx = 50;
		reg_dx = 50;
		break;
	case 0x1c:		// Set interrupt rate
		// Can't really set a rate this is host determined
		break;
	case 0x1d:		// Set display page number
		mouse.page = reg_bl;
		break;
	case 0x1e:		// Get display page number
		reg_bx = mouse.page;
		break;
	case 0x1f:		// Disable Mousedriver
		// ES:BX old mouse driver Zero at the moment TODO
		reg_bx = 0;
		SegSet16(es, 0);	   
		mouse.enabled = false;		// Just for reporting not doing a thing with it
		mouse.oldhidden = mouse.hidden;
		mouse.hidden = 1;
		break;
	case 0x20:		// Enable Mousedriver
		mouse.enabled = true;
		mouse.hidden = mouse.oldhidden;
		break;
	case 0x22:		// Set language for messages
 		break;
	case 0x23:		// Get language for messages
		reg_bx = 0;	// USA
		break;
	case 0x24:		// Get Software version and mouse type
		reg_bx = 0x626;		// Version 6.26
		reg_ch = 0x04;		// PS/2 type
		reg_cl = 0;			// PS/2 (unused)
		break;
	case 0x26:		// Get Maximum virtual coordinates
		reg_bx = mouse.enabled ? 0x0000 : 0xffff;
		reg_cx = (Bit16u)mouse.max_x;
		reg_dx = (Bit16u)mouse.max_y;
		break;
	default:
		LOG(LOG_MOUSE, LOG_ERROR)("Mouse Function %04X not implemented!", reg_ax);
		break;
		}
	return CBRET_NONE;
	}

static Bitu MOUSE_BD_Handler(void)
	{
	// the stack contains offsets to register values
	Bit16u raxpt = vPC_rLodsw(SegValue(ss), reg_sp+0x0a);
	Bit16u rbxpt = vPC_rLodsw(SegValue(ss), reg_sp+0x08);
	Bit16u rcxpt = vPC_rLodsw(SegValue(ss), reg_sp+0x06);
	Bit16u rdxpt = vPC_rLodsw(SegValue(ss), reg_sp+0x04);

	// read out the actual values, registers ARE overwritten
	Bit16u rax = vPC_rLodsw(SegValue(ds), raxpt);
	reg_ax = rax;
	reg_bx = vPC_rLodsw(SegValue(ds), rbxpt);
	reg_cx = vPC_rLodsw(SegValue(ds), rcxpt);
	reg_dx = vPC_rLodsw(SegValue(ds), rdxpt);
	
	// some functions are treated in a special way (additional registers)
	switch (rax)
		{
	case 0x09:		// Define GFX Cursor
	case 0x16:		// Save driver state
	case 0x17:		// load driver state
		SegSet16(es, SegValue(ds));
		break;
	case 0x0c:		// Define interrupt subroutine parameters
	case 0x14:		// Exchange event-handler
		if (reg_bx != 0)
			SegSet16(es, reg_bx);
		else
			SegSet16(es, SegValue(ds));
		break;
	case 0x10:		// Define screen region for updating
		reg_cx = vPC_rLodsw(SegValue(ds), rdxpt);
		reg_dx = vPC_rLodsw(SegValue(ds), rdxpt+2);
		reg_si = vPC_rLodsw(SegValue(ds), rdxpt+4);
		reg_di = vPC_rLodsw(SegValue(ds), rdxpt+6);
		break;
	default:
		break;
		}

	INT33_Handler();
	// save back the registers, too
	vPC_rStosw(SegValue(ds), raxpt, reg_ax);
	vPC_rStosw(SegValue(ds), rbxpt, reg_bx);
	vPC_rStosw(SegValue(ds), rcxpt, reg_cx);
	vPC_rStosw(SegValue(ds), rdxpt, reg_dx);
	switch (rax)
		{
	case 0x1f:		// Disable Mousedriver
		vPC_rStosw(SegValue(ds), rbxpt, SegValue(es));
		break;
	case 0x14:		// Exchange event-handler
		vPC_rStosw(SegValue(ds), rcxpt, SegValue(es));
		break;
	default:
		break;
		}
	reg_ax = rax;
	return CBRET_NONE;
	}

static Bitu INT74_Handler(void)
	{
	if (mouse.events > 0)
		{
		mouse.events--;
		// Check for an active Interrupt Handler that will get called
		if (mouse.sub_mask & mouse.event_queue[mouse.events].type)
			{
			reg_ax = mouse.event_queue[mouse.events].type;
			reg_bx = mouse.event_queue[mouse.events].buttons;
			reg_cx = POS_X;
			reg_dx = POS_Y;
			reg_si = (Bit16s)(mouse.mickey_x);
			reg_di = (Bit16s)(mouse.mickey_y);
			CPU_Push16(RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
			CPU_Push16(RealOff(CALLBACK_RealPointer(int74_ret_callback)));
			SegSet16(cs, mouse.sub_seg);
			reg_ip = mouse.sub_ofs;
			mouse.in_UIR = true;
			return CBRET_NONE;
			}
		}
	SegSet16(cs, RealSeg(CALLBACK_RealPointer(int74_ret_callback)));
	reg_ip = RealOff(CALLBACK_RealPointer(int74_ret_callback));
	return CBRET_NONE;
	}

Bitu MOUSE_UserInt_CB_Handler(void)
	{
	mouse.in_UIR = false;
	if (mouse.events && !mouse.timer_in_progress)
		{
		mouse.timer_in_progress = true;
		PIC_AddEvent(MOUSE_Limit_Events, MOUSE_DELAY);
		}
	return CBRET_NONE;
	}

void MOUSE_Init(Section* /*sec*/)
	{
	// Callback for mouse interrupt 0x33
	call_int33 = CALLBACK_Allocate();
//	RealPt i33loc = SegOff2dWord(CB_SEG+1, (call_int33*CB_SIZE)-0x10);
	RealPt i33loc = SegOff2dWord(DOS_GetMemory(0x1)-1, 0x10);
	CALLBACK_Setup(call_int33, &INT33_Handler, CB_MOUSE, dWord2Ptr(i33loc), "Mouse");
//	vPC_rStosd(0x33<<2, i33loc);

	call_mouse_bd = CALLBACK_Allocate();
	CALLBACK_Setup(call_mouse_bd, &MOUSE_BD_Handler, CB_RETF8, SegOff2Ptr(RealSeg(i33loc), RealOff(i33loc)+2), "MouseBD");
	// pseudocode for CB_MOUSE (including the special backdoor entry point):
	//	jump near i33hd
	//	callback MOUSE_BD_Handler
	//	retf 8
	//  label i33hd:
	//	callback INT33_Handler
	//	iret

	if (usesMouse)
		vPC_rStosd(0x33<<2, i33loc);
	else
		vPC_rStosd(0x33<<2, i33loc+13);				// HazaFata checks for 0 (or IRET at location)

	// Callback for ps2 irq
	call_int74 = CALLBACK_Allocate();
	CALLBACK_Setup(call_int74, &INT74_Handler, CB_IRQ12, "int 74");
	// pseudocode for CB_IRQ12:
	//	push ds
	//	push es
	//	pushad
	//	sti
	//	callback INT74_Handler
	//		doesn't return here, but rather to CB_IRQ12_RET
	//		(ps2 callback/user callback inbetween if requested)

	int74_ret_callback = CALLBACK_Allocate();
	CALLBACK_Setup(int74_ret_callback, &MOUSE_UserInt_CB_Handler, CB_IRQ12_RET, "int 74 ret");
	// pseudocode for CB_IRQ12_RET:
	//	callback MOUSE_UserInt_CB_Handler
	//	cli
	//	mov al, 0x20
	//	out 0xa0, al
	//	out 0x20, al
	//	popad
	//	pop es
	//	pop ds
	//	iret

	RealSetVec((0x70+MOUSE_IRQ-8), CALLBACK_RealPointer(call_int74));

	memset(&mouse, 0, sizeof(mouse));
	mouse.hidden = 1;			// Hide mouse on startup
	mouse.sub_seg = 0x6362;		// magic value

	Mouse_ResetHardware();
	Mouse_Reset();
	}
