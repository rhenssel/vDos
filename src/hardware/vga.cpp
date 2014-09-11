#include "vDos.h"
#include "setup.h"
#include "video.h"
#include "pic.h"
#include "vga.h"

#include <string.h>

VGA_Type vga;

Bit32u ExpandTable[256];
Bit32u Expand16Table[4][16];
Bit32u FillTable[16];

void VGA_SetMode(VGAModes mode)
	{
	if (vga.mode == mode)
		return;
	vga.mode = mode;
	VGA_SetupHandlers();
	VGA_StartResize();
	}

void VGA_DetermineMode(void)
	{
	// Test for VGA output active or direct color modes
	if (vga.attr.mode_control & 1)	 // graphics mode
		{
		if ((vga.gfx.mode & 0x40))
			VGA_SetMode(M_VGA);
		else if (vga.gfx.mode & 0x20)
			VGA_SetMode(M_CGA4);
		else if ((vga.gfx.miscellaneous & 0x0c) == 0x0c)
			VGA_SetMode(M_CGA2);
		else
			VGA_SetMode(M_EGA);
		}
	else
		VGA_SetMode(M_TEXT);
	}

void VGA_StartResize(Bitu delay /*=50*/)
	{
	if (!vga.draw.resizing)
		{
		vga.draw.resizing = true;
		if (vga.mode == M_ERROR)
			delay = 5;
		// Start a resize after delay (default 50 ms)
		if (delay == 0)
			VGA_SetupDrawing(0);
		else
			PIC_AddEvent(VGA_SetupDrawing, (float)delay);
		}
	}

void VGA_Init(Section* sec)
	{
	vga.draw.resizing = false;
	vga.mode = M_ERROR;			// For first init
	vga.vmemsize = 256*1024;	// 256kB VGA memory
	VGA_SetupMemory(sec);		// memory is allocated here
	VGA_SetupMisc();
	VGA_SetupDAC();
	VGA_SetupGFX();
	VGA_SetupSEQ();
	VGA_SetupAttr();
	// Generate tables
	for (Bitu i = 0; i < 256; i++)
		ExpandTable[i] = i | (i << 8) | (i <<16) | (i << 24);
	for (Bitu i = 0; i < 16; i++)
		FillTable[i] = ((i & 1) ? 0x000000ff : 0) | ((i & 2) ? 0x0000ff00 : 0) | ((i & 4) ? 0x00ff0000 : 0) | ((i & 8) ? 0xff000000 : 0);
	for (Bitu j = 0; j < 4; j++)
		for (Bitu i = 0; i < 16; i++)
			Expand16Table[j][i] = ((i & 1) ? 1 << (24 + j) : 0) | ((i & 2) ? 1 << (16 + j) : 0) | ((i & 4) ? 1 << (8 + j) : 0) | ((i & 8) ? 1 << j : 0);
	}
