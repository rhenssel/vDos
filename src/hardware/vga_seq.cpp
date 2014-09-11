#include "vDos.h"
#include "inout.h"
#include "vga.h"

Bitu read_p3c4(Bitu /*port*/, Bitu /*iolen*/)
	{
	return vga.seq.index;
	}

void write_p3c4(Bitu /*port*/, Bitu val, Bitu /*iolen*/)
	{
	vga.seq.index = val;
	}

void write_p3c5(Bitu /*port*/, Bitu val, Bitu iolen)
	{
	switch(vga.seq.index)
		{
	case 0:		// Reset
		vga.seq.reset = val;
		break;
	case 1:		// Clocking Mode
		if (val != vga.seq.clocking_mode)
			{
			// don't resize if only the screen off bit was changed
			if ((val&(~0x20)) != (vga.seq.clocking_mode&(~0x20)))
				{
				vga.seq.clocking_mode = val;
				VGA_StartResize();
				}
			else
				vga.seq.clocking_mode = val;
			if (val & 0x20)
				vga.attr.disabled |= 0x2;
			else
				vga.attr.disabled &= ~0x2;
			}
		/* TODO Figure this out :)
			0	If set character clocks are 8 dots wide, else 9.
			2	If set loads video serializers every other character
				clock cycle, else every one.
			3	If set the Dot Clock is Master Clock/2, else same as Master Clock
				(See 3C2h bit 2-3). (Doubles pixels). Note: on some SVGA chipsets
				this bit also affects the Sequencer mode.
			4	If set loads video serializers every fourth character clock cycle,
				else every one.
			5	if set turns off screen and gives all memory cycles to the CPU
				interface.
		*/
		break;
	case 2:		// Map Mask
		vga.seq.map_mask = val & 15;
		vga.config.full_map_mask = FillTable[val & 15];
		vga.config.full_not_map_mask = ~vga.config.full_map_mask;
		/*
			0  Enable writes to plane 0 if set
			1  Enable writes to plane 1 if set
			2  Enable writes to plane 2 if set
			3  Enable writes to plane 3 if set
		*/
		break;
	case 3:		// Character Map Select
		{
		vga.seq.character_map_select = val;
		Bit8u font1 = (val & 0x3) << 1;
		font1 |= (val & 0x10) >> 4;
		vga.draw.font_tables[0] = &vga.draw.font[font1*8*1024];
		Bit8u font2 = ((val & 0xc) >> 1);
		font2 |= (val & 0x20) >> 5;
		vga.draw.font_tables[1] = &vga.draw.font[font2*8*1024];
		}
		/*
			0,1,4  Selects VGA Character Map (0..7) if bit 3 of the character
					attribute is clear.
			2,3,5  Selects VGA Character Map (0..7) if bit 3 of the character
					attribute is set.
			Note: Character Maps are placed as follows:
			Map 0 at 0k, 1 at 16k, 2 at 32k, 3: 48k, 4: 8k, 5: 24k, 6: 40k, 7: 56k
		*/
		break;
	case 4:		// Memory Mode
		/* 
			0  Set if in an alphanumeric mode, clear in graphics modes.
			1  Set if more than 64kbytes on the adapter.
			2  Enables Odd/Even addressing mode if set. Odd/Even mode places all odd
				bytes in plane 1&3, and all even bytes in plane 0&2.
			3  If set address bit 0-1 selects video memory planes (256 color mode),
				rather than the Map Mask and Read Map Select Registers.
		*/
		vga.seq.memory_mode = val;
		// Changing this means changing the VGA Memory Read/Write Handler
		if (val&0x08)
			vga.config.chained = true;
		else
			vga.config.chained = false;
		VGA_SetupHandlers();
		break;
		}
	}

Bitu read_p3c5(Bitu /*port*/,Bitu iolen)
	{
	switch (vga.seq.index)
		{
	case 0:			// Reset
		return vga.seq.reset;
	case 1:			// Clocking Mode
		return vga.seq.clocking_mode;
	case 2:			// Map Mask
		return vga.seq.map_mask;
	case 3:			// Character Map Select
		return vga.seq.character_map_select;
	case 4:			// Memory Mode
		return vga.seq.memory_mode;
	default:
		return 0;
		}
	}

void VGA_SetupSEQ(void)
	{
	IO_RegisterWriteHandler(0x3c4, write_p3c4, IO_MB);
	IO_RegisterWriteHandler(0x3c5, write_p3c5, IO_MB);
	IO_RegisterReadHandler(0x3c4, read_p3c4, IO_MB);
	IO_RegisterReadHandler(0x3c5, read_p3c5, IO_MB);
	}

