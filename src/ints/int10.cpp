#include "vDos.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "inout.h"
#include "int10.h"

Int10Data int10;

static Bitu INT10_Handler(void)
	{
	switch (reg_ah)
		{
	case 0x00:																				// Set videomode
		INT10_SetVideoMode(reg_al);
		break;
	case 0x01:																				// Set textmode cursor shape
		INT10_SetCursorShape(reg_ch, reg_cl);
		break;
	case 0x02:																				// Set cursor pos
		INT10_SetCursorPos(reg_dh, reg_dl, reg_bh);
		break;
	case 0x03:																				// Get cursor pos and shape
		idleCount++;
		reg_dl = CURSOR_POS_COL(reg_bh);
		reg_dh = CURSOR_POS_ROW(reg_bh);
		reg_cx = vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE);
		break;
	case 0x04:																				// Read light pen pos
		reg_ax = 0;																			// Light pen is not supported
		break;
	case 0x05:																				// Set active page
		INT10_SetActivePage(reg_al);
		break;	
	case 0x06:																				// Scroll up
		INT10_ScrollWindow(reg_ch, reg_cl, reg_dh, reg_dl, -reg_al, reg_bh, 0xFF);
		break;
	case 0x07:																				// Scroll down
		INT10_ScrollWindow(reg_ch, reg_cl, reg_dh, reg_dl, reg_al, reg_bh, 0xFF);
		break;
	case 0x08:																				// Read character & attribute at cursor
		INT10_ReadCharAttr(&reg_ax, reg_bh);
		break;						
	case 0x09:																				// Write character & attribute at cursor CX times
		INT10_WriteChar(reg_al, vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) == 0x11 ? (reg_bl&0x80)|0x3f : reg_bl, reg_bh, reg_cx, true);
		break;
	case 0x0A:																				// Write character at cursor CX times
		INT10_WriteChar(reg_al, reg_bl, reg_bh, reg_cx, false);
		break;
	case 0x0B:																				// Set background/border colour & palette
		break;
	case 0x0C:																				// Write graphics pixel
		INT10_PutPixel(reg_cx, reg_dx, reg_bh, reg_al);
		break;
	case 0x0D:																				// Read graphics pixel
		INT10_GetPixel(reg_cx, reg_dx, reg_bh, &reg_al);
		break;
	case 0x0E:																				// Teletype outPut
		INT10_TeletypeOutput(reg_al, reg_bl);
		break;
	case 0x0F:																				// Get videomode
		reg_bh = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
		reg_al = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE)|(vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_VIDEO_CTL)&0x80);
		reg_ah = (Bit8u)vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
		break;					
	case 0x10:																				// Palette functions
		switch (reg_al)
			{
		case 0x00:																			// Set single palette register
			INT10_SetSinglePaletteRegister(reg_bl, reg_bh);
			break;
		case 0x01:																			// Set border (overscan) color
			break;
		case 0x02:																			// Set all palette registers
			INT10_SetAllPaletteRegisters(SegPhys(es)+reg_dx);
			break;
		case 0x03:																			// Toggle intensity/blinking bit
			break;
		case 0x07:																			// Get single palette register
			INT10_GetSinglePaletteRegister(reg_bl, &reg_bh);
			break;
		case 0x08:																			// Read overscan (border color) register
			reg_bh = 0;
			break;
		case 0x09:																			// Read all palette registers and overscan register
			INT10_GetAllPaletteRegisters(SegPhys(es)+reg_dx);
			break;
		case 0x10:																			// Set individual dac register
			INT10_SetSingleDACRegister(reg_bl, reg_dh, reg_ch, reg_cl);
			break;
		case 0x12:																			// Set block of dac registers
			INT10_SetDACBlock(reg_bx, reg_cx, SegPhys(es)+reg_dx);
			break;
		case 0x13:																			// Select video dac color page
			INT10_SelectDACPage(reg_bl, reg_bh);
			break;
		case 0x15:																			// Get individual dac register
			INT10_GetSingleDACRegister(reg_bl, &reg_dh, &reg_ch, &reg_cl);
			break;
		case 0x17:																			// Get block of dac register
			INT10_GetDACBlock(reg_bx, reg_cx, SegPhys(es)+reg_dx);
			break;
		case 0x18:																			// Undocumented - Set pel mask
			INT10_SetPelMask(reg_bl);
			break;
		case 0x19:																			// Undocumented - Get pel mask
			INT10_GetPelMask(reg_bl);
			reg_bh = 0;	// bx for get mask
			break;
		case 0x1A:																			// Get video dac color page
			INT10_GetDACPage(&reg_bl, &reg_bh);
			break;
		default:
			break;
			}
		break;
	case 0x11:																				// Character generator functions
		switch (reg_al)
			{
		// Textmode calls
		case 0x00:																			// Load user font
		case 0x10:
		case 0x01:																			// Load 8x14 font
		case 0x11:
		case 0x02:																			// Load 8x8 font
		case 0x12:
		case 0x04:																			// Load 8x16 font
		case 0x14:
			break;
		case 0x03:																			// Set Block Specifier
			IO_Write(0x3c4, 0x3);
			IO_Write(0x3c5, reg_bl);
			break;
		// Graphics mode calls
		case 0x20:																			// Set User 8x8 Graphics characters
			RealSetVec(0x1f, SegOff2dWord(SegValue(es), reg_bp));
			break;
		case 0x21:																			// Set user graphics characters
			RealSetVec(0x43, SegOff2dWord(SegValue(es), reg_bp));
			vPC_rStosw(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT, reg_cx);
			goto graphics_chars;
		case 0x22:																			// Rom 8x14 set
			RealSetVec(0x43, int10.rom.font_14);
			vPC_rStosw(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT, 14);
			goto graphics_chars;
		case 0x23:																			// Rom 8x8 double dot set
			RealSetVec(0x43, int10.rom.font_8_first);
			vPC_rStosw(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT, 8);
			goto graphics_chars;
		case 0x24:																			// Rom 8x16 set
			RealSetVec(0x43, int10.rom.font_16);
			vPC_rStosw(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT, 16);
graphics_chars:
			switch (reg_bl)
				{
			case 0:
				vPC_rStosb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, reg_dl-1);
				break;
			case 1:
				vPC_rStosb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, 13);
				break;
			case 3:
				vPC_rStosb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, 42);
				break;
			case 2:
			default:
				vPC_rStosb(BIOSMEM_SEG,BIOSMEM_NB_ROWS, 24);
				break;
				}
			break;
		// General
		case 0x30:																			// Get Font Information
			switch (reg_bh)
				{
			case 0:																			// Interupt 0x1f vector
				{
				RealPt int_1f = RealGetVec(0x1f);
				SegSet16(es, RealSeg(int_1f));
				reg_bp = RealOff(int_1f);
				}
				break;
			case 1:																			// Interupt 0x43 vector
				{
				RealPt int_43 = RealGetVec(0x43);
				SegSet16(es, RealSeg(int_43));
				reg_bp = RealOff(int_43);
				}
				break;
			case 2:																			// Font 8x14
				SegSet16(es, RealSeg(int10.rom.font_14));
				reg_bp = RealOff(int10.rom.font_14);
				break;
			case 3:																			// Font 8x8 first 128
				SegSet16(es, RealSeg(int10.rom.font_8_first));
				reg_bp = RealOff(int10.rom.font_8_first);
				break;
			case 4:																			// Font 8x8 second 128
				SegSet16(es, RealSeg(int10.rom.font_8_second));
				reg_bp = RealOff(int10.rom.font_8_second);
				break;
			case 5:																			// Alpha alternate 9x14
				SegSet16(es, RealSeg(int10.rom.font_14_alternate));
				reg_bp = RealOff(int10.rom.font_14_alternate);
				break;
			case 6:																			// Font 8x16
				SegSet16(es, RealSeg(int10.rom.font_16));
				reg_bp = RealOff(int10.rom.font_16);
				break;
			case 7:																			// Alpha alternate 9x16
				SegSet16(es, RealSeg(int10.rom.font_16_alternate));
				reg_bp = RealOff(int10.rom.font_16_alternate);
				break;
			default:
				break;
				}
			if ((reg_bh <= 7))
				{
				reg_cx = vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT);
				reg_dl = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_NB_ROWS);
				}
			break;
		default:
			break;
			}
		break;
	case 0x12:																				// Alternate function select
		switch (reg_bl)
			{
		case 0x10:																			// Get EGA information
			reg_bh = (vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_CRTC_ADDRESS) == 0x3B4);	
			reg_bl = 3;																		// 256 kb
			reg_cl = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_SWITCHES) & 0x0F;
			reg_ch = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_SWITCHES) >> 4;
			break;
		case 0x20:																			// Set alternate printscreen
		case 0x30:																			// Select vertical resolution
		case 0x31:																			// Palette loading on modeset
		case 0x32:																			// Video adressing
		case 0x33:																			// Switch gray-scale summing
			break;	
		case 0x34:																			// Alternate function select (VGA) - cursor emulation
			{   
			if (reg_al > 1)																	// Bit 0: 0=enable, 1=disable
				{
				reg_al = 0;
				break;
				}
			Bit8u temp = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_VIDEO_CTL) & 0xfe;
			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_VIDEO_CTL, temp|reg_al);
			reg_al = 0x12;
			break;	
			}		
		case 0x35:
		case 0x36:																			// VGA refresh control
			break;
		default:
			break;
			}
		break;
	case 0x13:																				// Write string
		INT10_WriteString(reg_dh, reg_dl, reg_al, reg_bl, SegPhys(es)+reg_bp, reg_cx, reg_bh);
		break;
	case 0x1A:																				// Display Combination
		if (reg_al == 0)																	// Get DCC
			{
			RealPt vsavept = vPC_rLodsd(BIOSMEM_SEG, BIOSMEM_VS_POINTER);					// Walk the tables...
			RealPt svstable = vPC_rLodsd(RealSeg(vsavept), RealOff(vsavept)+0x10);
			if (svstable)
				{
				RealPt dcctable = vPC_rLodsd(RealSeg(svstable), RealOff(svstable)+0x02);
				Bit8u entries = vPC_rLodsb(RealSeg(dcctable), RealOff(dcctable)+0x00);
				Bit8u idx = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_DCC_INDEX);
				if (idx < entries)															// Check if index within range
					{
					Bit16u dccentry = vPC_rLodsw(RealSeg(dcctable), RealOff(dcctable)+0x04+idx*2);
					if ((dccentry&0xff) == 0)
						reg_bx = dccentry>>8;
					else
						reg_bx = dccentry;
					}
				else
					reg_bx = 0xffff;
				}
			else
				reg_bx = 0xffff;
			reg_ax = 0x1a;																	// High part destroyed or zeroed depending on BIOS
			}
		else if (reg_al == 1)																// Set dcc
			{
			Bit8u newidx = 0xff;
			RealPt vsavept=vPC_rLodsd(BIOSMEM_SEG, BIOSMEM_VS_POINTER);						// Walk the tables...
			RealPt svstable = vPC_rLodsd(RealSeg(vsavept), RealOff(vsavept)+0x10);
			if (svstable)
				{
				RealPt dcctable = vPC_rLodsd(RealSeg(svstable), RealOff(svstable)+0x02);
				Bit8u entries = vPC_rLodsb(RealSeg(dcctable), RealOff(dcctable)+0x00);
				if (entries)
					{
					Bitu ct;
					Bit16u swpidx = reg_bh|(reg_bl<<8);
					for (ct = 0; ct < entries; ct++)										// search the DDC index in the DCC table
						{
						Bit16u dccentry = vPC_rLodsw(RealSeg(dcctable), RealOff(dcctable)+0x04+ct*2);
						if ((dccentry == reg_bx) || (dccentry == swpidx))
							{
							newidx = (Bit8u)ct;
							break;
							}
						}
					}
				}
			vPC_rStosb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX, newidx);
			reg_ax = 0x1A;																	// High part destroyed or zeroed depending on BIOS
			}
		break;
	default:
		break;
		};
	return CBRET_NONE;
	}

static void INT10_Seg40Init(void)
	{
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT, 16);										// The default char height
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_VIDEO_CTL, 0x60);										// Clear the screen 
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_SWITCHES, 0xF9);										// Set the basic screen we have
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_MODESET_CTL, 0x51);										// Set the basic modeset options
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x09);										// Set the default MSR
	}

static void INT10_InitVGA(void)
	{
	IO_Write(0x3c2, 0xc3);																	// Switch to color mode and enable CPU access 480 lines
	IO_Write(0x3c4, 0x04);																	// More than 64k
	IO_Write(0x3c5, 0x02);
	}

void INT10_Init()
	{
	INT10_InitVGA();
	Bitu call_10 = CALLBACK_Allocate();														// Setup the INT 10 vector
	CALLBACK_Setup(call_10, &INT10_Handler, CB_IRET, "Int 10 video");
	RealSetVec(0x10, CALLBACK_RealPointer(call_10));
	INT10_SetupRomMemory();																	// Init the 0x40 segment and init the datastructures in the the video rom area
	INT10_Seg40Init();
	INT10_SetVideoMode(3);
	}
