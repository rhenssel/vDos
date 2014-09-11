#include "vDos.h"
#include "callback.h"
#include "mem.h"
#include "bios.h"
#include "regs.h"
#include "inout.h"
#include "dos_inc.h"
#include "SDL.h"

static Bitu call_int16, call_irq1;

int idleCount = 0;

// Nice table from BOCHS i should feel bad for ripping this
#define none 0
static struct {
  Bit16u normal;
  Bit16u shift;
  Bit16u control;
  Bit16u alt;
  }
scan_to_scanascii[MAX_SCAN_CODE + 1] = {
      {   none,   none,   none,   none },
      { 0x011b, 0x011b, 0x011b, 0x01f0 },	// escape
      { 0x0231, 0x0221,   none, 0x7800 },	// 1 !
      { 0x0332, 0x0340, 0x0300, 0x7900 },	// 2 @
      { 0x0433, 0x0423,   none, 0x7a00 },	// 3 #
      { 0x0534, 0x0524,   none, 0x7b00 },	// 4 $
      { 0x0635, 0x0625, 0x0680, 0x0680 },	// 5 % + EURO
//    { 0x0635, 0x0625,   none, 0x7c00 },	// 5 % + EURO
      { 0x0736, 0x075e, 0x071e, 0x7d00 },	// 6 ^
      { 0x0837, 0x0826,   none, 0x7e00 },	// 7 &
      { 0x0938, 0x092a,   none, 0x7f00 },	// 8 *
      { 0x0a39, 0x0a28,   none, 0x8000 },	// 9 (
      { 0x0b30, 0x0b29,   none, 0x8100 },	// 0 )
      { 0x0c2d, 0x0c5f, 0x0c1f, 0x8200 },	// - _
      { 0x0d3d, 0x0d2b,   none, 0x8300 },	// = +
      { 0x0e08, 0x0e08, 0x0e7f, 0x0ef0 },	// backspace
      { 0x0f09, 0x0f00, 0x9400,   none },	// tab
      { 0x1071, 0x1051, 0x1011, 0x1000 },	// Q
      { 0x1177, 0x1157, 0x1117, 0x1100 },	// W
      { 0x1265, 0x1245, 0x1205, 0x1200 },	// E
      { 0x1372, 0x1352, 0x1312, 0x1300 },	// R
      { 0x1474, 0x1454, 0x1414, 0x1400 },	// T
      { 0x1579, 0x1559, 0x1519, 0x1500 },	// Y
      { 0x1675, 0x1655, 0x1615, 0x1600 },	// U
      { 0x1769, 0x1749, 0x1709, 0x1700 },	// I
      { 0x186f, 0x184f, 0x180f, 0x1800 },	// O
      { 0x1970, 0x1950, 0x1910, 0x1900 },	// P
      { 0x1a5b, 0x1a7b, 0x1a1b, 0x1af0 },	// [ {
      { 0x1b5d, 0x1b7d, 0x1b1d, 0x1bf0 },	// ] }
      { 0x1c0d, 0x1c0d, 0x1c0a,   none },	// Enter
      {   none,   none,   none,   none },	// L Ctrl
      { 0x1e61, 0x1e41, 0x1e01, 0x1e00 },	// A
      { 0x1f73, 0x1f53, 0x1f13, 0x1f00 },	// S
      { 0x2064, 0x2044, 0x2004, 0x2000 },	// D
      { 0x2166, 0x2146, 0x2106, 0x2100 },	// F
      { 0x2267, 0x2247, 0x2207, 0x2200 },	// G
      { 0x2368, 0x2348, 0x2308, 0x2300 },	// H
      { 0x246a, 0x244a, 0x240a, 0x2400 },	// J
      { 0x256b, 0x254b, 0x250b, 0x2500 },	// K
      { 0x266c, 0x264c, 0x260c, 0x2600 },	// L
      { 0x273b, 0x273a,   none, 0x27f0 },	// ; :
      { 0x2827, 0x2822,   none, 0x28f0 },	// ' "
      { 0x2960, 0x297e,   none, 0x29f0 },	// ` ~
      {   none,   none,   none,   none },	// L shift
      { 0x2b5c, 0x2b7c, 0x2b1c, 0x2bf0 },	// | \//
      { 0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 },	// Z
      { 0x2d78, 0x2d58, 0x2d18, 0x2d00 },	// X
      { 0x2e63, 0x2e43, 0x2e03, 0x2e00 },	// C
      { 0x2f76, 0x2f56, 0x2f16, 0x2f00 },	// V
      { 0x3062, 0x3042, 0x3002, 0x3000 },	// B
      { 0x316e, 0x314e, 0x310e, 0x3100 },	// N
      { 0x326d, 0x324d, 0x320d, 0x3200 },	// M
      { 0x332c, 0x333c,   none, 0x33f0 },	// , <
      { 0x342e, 0x343e,   none, 0x34f0 },	// . >
      { 0x352f, 0x353f,   none, 0x35f0 },	// / ?
      {   none,   none,   none,   none },	// R Shift
      { 0x372a, 0x372a, 0x9600, 0x37f0 },	// *
      {   none,   none,   none,   none },	// L Alt
      { 0x3920, 0x3920, 0x3920, 0x3920 },	// space
      {   none,   none,   none,   none },	// caps lock
      { 0x3b00, 0x5400, 0x5e00, 0x6800 },	// F1
      { 0x3c00, 0x5500, 0x5f00, 0x6900 },	// F2
      { 0x3d00, 0x5600, 0x6000, 0x6a00 },	// F3
      { 0x3e00, 0x5700, 0x6100, 0x6b00 },	// F4
      { 0x3f00, 0x5800, 0x6200, 0x6c00 },	// F5
      { 0x4000, 0x5900, 0x6300, 0x6d00 },	// F6
      { 0x4100, 0x5a00, 0x6400, 0x6e00 },	// F7
      { 0x4200, 0x5b00, 0x6500, 0x6f00 },	// F8
      { 0x4300, 0x5c00, 0x6600, 0x7000 },	// F9
      { 0x4400, 0x5d00, 0x6700, 0x7100 },	// F10
      {   none,   none,   none,   none },	// Num Lock
      {   none,   none,   none,   none },	// Scroll Lock
      { 0x4700, 0x4737, 0x7700, 0x0007 },	// 7 Home
      { 0x4800, 0x4838, 0x8d00, 0x0008 },	// 8 Up
      { 0x4900, 0x4939, 0x8400, 0x0009 },	// 9 PgUp
      { 0x4a2d, 0x4a2d, 0x8e00, 0x4af0 },	// -
      { 0x4b00, 0x4b34, 0x7300, 0x0004 },	// 4 Left
      { 0x4cf0, 0x4c35, 0x8f00, 0x0005 },	// 5
      { 0x4d00, 0x4d36, 0x7400, 0x0006 },	// 6 Right
      { 0x4e2b, 0x4e2b, 0x9000, 0x4ef0 },	// +
      { 0x4f00, 0x4f31, 0x7500, 0x0001 },	// 1 End
      { 0x5000, 0x5032, 0x9100, 0x0002 },	// 2 Down
      { 0x5100, 0x5133, 0x7600, 0x0003 },	// 3 PgDn
      { 0x5200, 0x5230, 0x9200, 0x0000 },	// 0 Ins
      { 0x5300, 0x532e, 0x9300,   none },	// Del
      {   none,   none,   none,   none },
      {   none,   none,   none,   none },
      { 0x565c, 0x567c,   none,   none },	// (102-key)
      { 0x8500, 0x8700, 0x8900, 0x8b00 },	// F11
      { 0x8600, 0x8800, 0x8a00, 0x8c00 }	// F12
      };

static Bit8u pastedData[txtMaxLins*txtMaxCols];					// about one screen full should be enough?
static int pastedHead = 0;										// next character te process
static int pastedTail = 0;										// last chracater to process

void pasteKey(void)
	{
	if (pastedHead != pastedTail)
		{
		Bit16u code = pastedData[pastedHead++];
//		if (code >= 129)
//			code = Ansi2Ascii[code-128];
//		else if (code == 13)									// correct Enter?
		if (code == 13)											// correct Enter?
			code = 0x1c0d;
		else if (code == 9)										// and Tab?
			code = 0x0f09;
		BIOS_AddKeyToBuffer(code);
		}
	}


void BIOS_PasteClipboard(Bit16u *data)
	{
	if (pastedHead != pastedTail)								// if still processing previous, just ignore
		return;
	pastedHead = 0;
	pastedTail = Unicode2Ascii(data, pastedData, sizeof(pastedData));
/*
	for (pastedTail = 0; pastedTail < sizeof(pastedData); pastedHead++)
		if (!data[pastedHead])
			break;
		else if (data[pastedHead] == 10)						// take care of linfeed/return combinations
			{
			pastedData[pastedTail++] = 13;
			if (data[pastedHead+1] == 13)
				pastedHead++;
			}
		else if (data[pastedHead] == 13)
			{
			pastedData[pastedTail++] = 13;
			if (data[pastedHead+1] == 10)
				pastedHead++;
			}
		else
			pastedData[pastedTail++] = data[pastedHead];
*/
	if (pastedTail)												// if anything to paste
		{
//		pastedHead = 0;;
		if (vPC_rLodsw(BIOS_KEYBOARD_BUFFER_HEAD) == vPC_rLodsw(BIOS_KEYBOARD_BUFFER_TAIL))	// if keyboard buffer empty, do first paste character
			pasteKey();
		}
	}


bool BIOS_AddKeyToBuffer(Bit16u code)
	{
	if (vPC_rLodsb(BIOS_KEYBOARD_FLAGS2)&8)						// Pause state active?
		return true;

	Bit16u tail = vPC_rLodsw(BIOS_KEYBOARD_BUFFER_TAIL);
	Bit16u ttail = tail+2;
	if (ttail >= vPC_rLodsw(BIOS_KEYBOARD_BUFFER_END))
		ttail = vPC_rLodsw(BIOS_KEYBOARD_BUFFER_START);

	if (ttail == vPC_rLodsw(BIOS_KEYBOARD_BUFFER_HEAD))			// Check for buffer Full
		return false;
	vPC_rStosw(0x40, tail, code);
	vPC_rStosw(BIOS_KEYBOARD_BUFFER_TAIL, ttail);
	return true;
	}

static void add_key(Bit16u code)
	{
	if (code != 0)
		BIOS_AddKeyToBuffer(code);
	}

static bool get_key(Bit16u &code)
	{
	Bit16u head	= vPC_rLodsw(BIOS_KEYBOARD_BUFFER_HEAD);
	if (head == vPC_rLodsw(BIOS_KEYBOARD_BUFFER_TAIL))
		{
		idleCount++;
		return false;
		}
	idleCount = 0;
	Bit16u thead = head+2;
	if (thead >= vPC_rLodsw(BIOS_KEYBOARD_BUFFER_END))
		thead = vPC_rLodsw(BIOS_KEYBOARD_BUFFER_START);
	vPC_rStosw(BIOS_KEYBOARD_BUFFER_HEAD, thead);
	code = vPC_rLodsw(0x40, head);
	pasteKey();
	return true;
	}

static bool check_key(Bit16u &code)
	{
	Bit16u head = vPC_rLodsw(BIOS_KEYBOARD_BUFFER_HEAD);
	if (head == vPC_rLodsw(BIOS_KEYBOARD_BUFFER_TAIL))
		{
		idleCount++;
		return false;
		}
	idleCount = 0;
	code = vPC_rLodsw(0x40, head);
	return true;
	}

void UpdateKBflags()								// at least L.Shift + R.Shift are not captured by BIOS_addkey, so seperate
	{
	Bit8u flags;

	Bit8u kbState[256];
	GetKeyboardState(kbState);

	flags = kbState[VK_RSHIFT]>>7;											// flags are mostly masked by shifting
	flags |= kbState[VK_LSHIFT]>>6;
	flags |= kbState[VK_CONTROL]>>5;
	flags |= kbState[VK_MENU]>>4;
	flags |= kbState[VK_SCROLL]<<4;
	flags |= kbState[VK_NUMLOCK]<<5;
	flags |= kbState[VK_CAPITAL]<<6;
	flags |= kbState[VK_INSERT]<<7;
	vPC_rStosb(BIOS_KEYBOARD_FLAGS1, flags);

	flags = kbState[VK_LCONTROL]>>7;
	flags |= kbState[VK_LMENU]>>6;
	flags |= kbState[VK_SCROLL]>>3;
	flags |= kbState[VK_NUMLOCK]>>2;
	flags |= kbState[VK_CAPITAL]>>1;
	flags |= (kbState[VK_INSERT] & 0x80);
	vPC_rStosb(BIOS_KEYBOARD_FLAGS2, flags);

	flags = 0x10;															// enhanced keyboard installed
	flags |= kbState[VK_RCONTROL]>>5;
	flags |= kbState[VK_RMENU]>>4;
	vPC_rStosb(BIOS_KEYBOARD_FLAGS3, flags);

	flags = (kbState[VK_SCROLL] & 0x01);
	flags |= kbState[VK_NUMLOCK]<<1;
	flags |= kbState[VK_CAPITAL]<<2;
	vPC_rStosb(BIOS_KEYBOARD_LEDS, flags);
	}

void BIOS_AddKey(Bit8u scancode, Bit16u unicode, bool pressed)
	{
	// we should call Int 0x15, just to inform programs that has redirected this
	// But what about processor running in protected mode, this will mess things up?
//	reg_ah = 0x4f;									// KEYBOARD INTERCEPT (translate scancode)
//	reg_al = scancode;
//	CALLBACK_SCF(true);
//	CALLBACK_RunRealInt(0x15);						// and this should be the redirected one

	Bit8u flags1, flags2, flags3;
	
	UpdateKBflags();
	flags1 = vPC_rLodsb(BIOS_KEYBOARD_FLAGS1);
	flags2 = vPC_rLodsb(BIOS_KEYBOARD_FLAGS2);
	flags3 = vPC_rLodsb(BIOS_KEYBOARD_FLAGS3);
	if (!pressed)
		{
		if (scancode == 0x38)												// Alt Released
			{
			add_key(vPC_rLodsb(BIOS_KEYBOARD_TOKEN));
			vPC_rStosb(BIOS_KEYBOARD_TOKEN, 0);
			}
		return;
		}

	if (scancode >= 0x47 && scancode <= 0x52 && (flags1&0x08))				// NumPad + Alt down
		{
		if (scan_to_scanascii[scancode].alt < 10)							// 0 - 9
			{
			Bit8u token = vPC_rLodsb(BIOS_KEYBOARD_TOKEN);
			token = token*10 + scan_to_scanascii[scancode].alt;
			vPC_rStosb(BIOS_KEYBOARD_TOKEN, token);
			return;
			}
		}
	vPC_rStosb(BIOS_KEYBOARD_TOKEN, 0);										// reset if another key

	if (scancode >= 0x3b && scancode <= 0x44)								// F1 - F10
		{
		if (flags1 & 0x08)													// Alt down
			scancode += 45;
		else if (flags1 & 0x04)												// Ctrl down
			scancode += 35;
		else if (flags1 & 0x03)												// Either shift down
			scancode += 25;
		add_key(scancode<<8);
		return;
		}

	if (scancode == 0x57 || scancode == 0x58)								// F11 + F12
		{
		scancode += 0x2e;
		if (flags1 & 0x08)													// Alt down
			scancode += 6;
		else if (flags1 & 0x04)												// Ctrl down
			scancode += 4;
		else if (flags1 & 0x03)												// Either shift down
			scancode += 2;
		add_key(scancode<<8);
		return;
		}

	if (scancode == 0x0f && !(flags1&0x08))									// Tab w/o Alt down
		{
		if (flags1 & 0x04)													// Ctrl down
			add_key(0x9400);
		else if (flags1 & 0x03)												// Either shift down
			add_key(0x0f00);
		else
			add_key(0x0f09);
		return;
		}

	if (scancode == 6 && (flags1&0x0c) == 0x0c)								// set unicode if Ctrl+Alt+5 (€)
		unicode = 0x20ac;
	else if (flags2&0x02)													// Left Alt down
		{
		if (scancode >= 2 && scancode <= 13)								// top row '1' - '='
			return add_key((scancode+0x76)<<8);
		if (unicode)														// letter
			return add_key(scancode<<8);
		}


	if (unicode)															// if unicode, look up corresponding ascii
		{
		for (int i = 1; i < 256; i++)										// could probably be ommited for unicode < 128 == ascii, wath the hack
			if (cpMap[i] == unicode)
				return add_key((scancode<<8) + i);
		if (!(flags1&0x08))													// if Alt not down
			add_key((scancode<<8) + unicode);
		return;
		}
	if (scancode >= 0x47 && scancode <= 0x53)								// Arrow keys/NumPad
		{
		if (flags1 & 0x08)													// Either Alt down
			add_key(scan_to_scanascii[scancode].normal+0x5000);
		else if (flags1 & 0x04)												// Either Control down
			add_key((scan_to_scanascii[scancode].control&0xff00)|0xe0);
		else if (((flags1 & 0x3) != 0) || ((flags1 & 0x20) != 0))			// Either Shift down or NumLock
			add_key((scan_to_scanascii[scancode].shift&0xff00)|0xe0);
		else
			add_key((scan_to_scanascii[scancode].normal&0xff00)|0xe0);
		return;
		}

	if (scancode <= MAX_SCAN_CODE)											// fallthru to report some combinations based upon the original IBM keyboard
		{																	// at least WP checks for instance for [Ctrl]6
		if (flags1 & 0x08)													// [Alt] down												
			add_key(scan_to_scanascii[scancode].alt);
		else if (flags1 & 0x04)												// [Ctrl] down
			add_key(scan_to_scanascii[scancode].control);
		else if (flags1 & 0x03)												// Either [Shift] down
			add_key(scan_to_scanascii[scancode].shift);
		else
			add_key(scan_to_scanascii[scancode].normal);					// last two shouldn't be needed, so what
		}
//	LOG_MSG("Scancode: %2x, Flags1: %2x, flags2: %2x, flags3: %2x, leds: %2x", scancode, vPC_rLodsb(BIOS_KEYBOARD_FLAGS1), vPC_rLodsb(BIOS_KEYBOARD_FLAGS2), vPC_rLodsb(BIOS_KEYBOARD_FLAGS3), vPC_rLodsb(BIOS_KEYBOARD_LEDS));
	return;
	}

static Bitu IRQ1_Handler(void)
	{
	return CBRET_NONE;
	}

// check whether key combination is enhanced or not, translate key if necessary
static bool IsEnhancedKey(Bit16u &key)
	{
	// test for special keys (return and slash on numblock)
	if ((key>>8) == 0xe0)
		{
		if (((key&0xff) == 0x0a) || ((key&0xff) == 0x0d))	// key is return on the numblock
			key = (key&0xff)|0x1c00;
		else												// key is slash on the numblock
			key = (key&0xff)|0x3500;
		return false;										// both keys are not considered enhanced keys
		}
	else if (((key>>8) > 0x84) || (((key&0xff) == 0xf0) && (key>>8)))	// key is enhanced key (either scancode part>0x84 or specially-marked keyboard combination, low part==0xf0) */
		return true;
	// convert key if necessary (extended keys)
	if ((key>>8) && ((key&0xff) == 0xe0))
		key &= 0xff00;
	return false;
	}

static Bitu INT16_Handler(void)
	{
	Bit16u temp = 0;
	UpdateKBflags();
	switch (reg_ah)
		{
	case 0x00:											// GET KEYSTROKE
		if ((get_key(temp)) && (!IsEnhancedKey(temp)))	// normal key found, return translated key in ax
			reg_ax = temp;
		else
			reg_ip += 1;								// enter small idle loop to allow for irqs to happen
		break;
	case 0x10:											// GET KEYSTROKE (enhanced keyboards only)
		if (get_key(temp))
			{
			if (((temp&0xff) == 0xf0) && (temp>>8))		// special enhanced key, clear low part before returning key
				temp &= 0xff00;
			reg_ax = temp;
			}
		else
			reg_ip += 1;								// enter small idle loop to allow for irqs to happen
		break;
	case 0x01:											// CHECK FOR KEYSTROKE
		vPC_rStosw(SegPhys(ss)+reg_sp+4, (vPC_rLodsw(SegPhys(ss)+reg_sp+4) | FLAG_IF));	// enable interrupt-flag after IRET of this int16
		for (;;)
			{
			if (check_key(temp))
				{
				if (!IsEnhancedKey(temp))				// normal key, return translated key in ax
					{
					CALLBACK_SZF(false);
					reg_ax = temp;
					break;
					}
				else									// remove enhanced key from buffer and ignore it
					get_key(temp);
				}
			else	// no key available
				{
				CALLBACK_Idle();						// to release time if a program is polling keyboard
				CALLBACK_SZF(true);
				break;
				}
			}
		break;
	case 0x11:											// CHECK FOR KEYSTROKE (enhanced keyboards only)
		if (!check_key(temp))
			CALLBACK_SZF(true);
		else
			{
			CALLBACK_SZF(false);
			if (((temp&0xff) == 0xf0) && (temp>>8))		// special enhanced key, clear low part before returning key
				temp &= 0xff00;
			reg_ax = temp;
			}
		break;
	case 0x02:											// GET SHIFT FlAGS
		reg_al = vPC_rLodsb(BIOS_KEYBOARD_FLAGS1);
		break;
	case 0x03:											// SET TYPEMATIC RATE AND DELAY
		break;
	case 0x05:											// STORE KEYSTROKE IN KEYBOARD BUFFER
		if (BIOS_AddKeyToBuffer(reg_cx))
			reg_al = 0;
		else
			reg_al = 1;
		break;
	case 0x12:											// GET EXTENDED SHIFT STATES
		reg_al = vPC_rLodsb(BIOS_KEYBOARD_FLAGS1);
		reg_ah = vPC_rLodsb(BIOS_KEYBOARD_FLAGS2);
		break;
		};
	return CBRET_NONE;
	}

void BIOS_SetupKeyboard(void)
	{
	// Setup the variables for keyboard in the bios data segment
	vPC_rStosw(BIOS_KEYBOARD_BUFFER_START, 0x1e);
	vPC_rStosw(BIOS_KEYBOARD_BUFFER_END, 0x3e);
	vPC_rStosw(BIOS_KEYBOARD_BUFFER_HEAD, 0x1e);
	vPC_rStosw(BIOS_KEYBOARD_BUFFER_TAIL, 0x1e);
	vPC_rStosb(BIOS_KEYBOARD_FLAGS1, 0);
	vPC_rStosb(BIOS_KEYBOARD_FLAGS2, 0);
	vPC_rStosb(BIOS_KEYBOARD_FLAGS3, 0x10);		// Enhanced keyboard installed
	vPC_rStosb(BIOS_KEYBOARD_TOKEN, 0);
	vPC_rStosb(BIOS_KEYBOARD_LEDS, 0);

	// Allocate/setup a callback for int 0x16 and for standard IRQ 1 handler
	call_int16 = CALLBACK_Allocate();	
	CALLBACK_Setup(call_int16, &INT16_Handler, CB_INT16, "Keyboard");
	RealSetVec(0x16, CALLBACK_RealPointer(call_int16));

	// IRQ1 has become a dummy, never called
	// int 15h - 4fh (transalet scancode) also will never be called, this is what we want?
	call_irq1 = CALLBACK_Allocate();
	CALLBACK_Setup(call_irq1, &IRQ1_Handler, CB_IRQ1, dWord2Ptr(BIOS_DEFAULT_IRQ1_LOCATION), "IRQ 1 Keyboard");
	RealSetVec(0x09, BIOS_DEFAULT_IRQ1_LOCATION);
	}

