#include "vDos.h"
#include "inout.h"
#include "render.h"
#include "vga.h"

/*
3C6h (R/W):  PEL Mask
bit 0-7  This register is anded with the palette index sent for each dot.
         Should be set to FFh.

3C7h (R):  DAC State Register
bit 0-1  0 indicates the DAC is in Write Mode and 3 indicates Read mode.

3C7h (W):  PEL Address Read Mode
bit 0-7  The PEL data register (0..255) to be read from 3C9h.
Note: After reading the 3 bytes at 3C9h this register will increment,
      pointing to the next data register.

3C8h (R/W):  PEL Address Write Mode
bit 0-7  The PEL data register (0..255) to be written to 3C9h.
Note: After writing the 3 bytes at 3C9h this register will increment, pointing
      to the next data register.

3C9h (R/W):  PEL Data Register
bit 0-5  Color value
Note:  Each read or write of this register will cycle through first the
       registers for Red, Blue and Green, then increment the appropriate
       address register, thus the entire palette can be loaded by writing 0 to
       the PEL Address Write Mode register 3C8h and then writing all 768 bytes
       of the palette to this register.
*/

enum {DAC_READ, DAC_WRITE};

static void VGA_DAC_SendColor(Bitu index, Bitu src)
	{
	const Bit8u red = vga.dac.rgb[src].red;
	const Bit8u green = vga.dac.rgb[src].green;
	const Bit8u blue = vga.dac.rgb[src].blue;
	// Set entry in 16bit output lookup table
	vga.dac.xlat16[index] = ((blue>>1)&0x1f) | (((green)&0x3f)<<5) | (((red>>1)&0x1f) << 11);
	RENDER_SetPal(index, (red << 2) | ( red >> 4 ), (green << 2) | ( green >> 4 ), (blue << 2) | ( blue >> 4 ) );
	}

static void VGA_DAC_UpdateColor(Bitu index)
	{
	Bitu maskIndex = index & vga.dac.pel_mask;
	VGA_DAC_SendColor(index, maskIndex);
	}

static void write_p3c6(Bitu port, Bitu val, Bitu iolen)
	{
	if (vga.dac.hidac_counter > 3)
		{
		vga.dac.reg02 = val;
		vga.dac.hidac_counter = 0;
		VGA_StartResize();
		return;
		}
	if (vga.dac.pel_mask != val )
		{
		vga.dac.pel_mask = val;
		for (Bitu i = 0; i < 256; i++) 
			VGA_DAC_UpdateColor(i);
		}
	}

static Bitu read_p3c6(Bitu port,Bitu iolen)
	{
	vga.dac.hidac_counter++;
	return vga.dac.pel_mask;
	}

static void write_p3c7(Bitu port, Bitu val, Bitu iolen)
	{
	vga.dac.hidac_counter = 0;
	vga.dac.read_index = val;
	vga.dac.pel_index = 0;
	vga.dac.state = DAC_READ;
	vga.dac.write_index = val + 1;
	}

static Bitu read_p3c7(Bitu port, Bitu iolen)
	{
	vga.dac.hidac_counter = 0;
	if (vga.dac.state == DAC_READ)
		return 0x3;
	else
		return 0x0;
}

static void write_p3c8(Bitu port,Bitu val,Bitu iolen)
	{
	vga.dac.hidac_counter = 0;
	vga.dac.write_index = val;
	vga.dac.pel_index = 0;
	vga.dac.state = DAC_WRITE;
	}

static Bitu read_p3c8(Bitu port, Bitu iolen)
	{
	vga.dac.hidac_counter = 0;
	return vga.dac.write_index;
	}

static void write_p3c9(Bitu port, Bitu val, Bitu iolen)
	{
	vga.dac.hidac_counter = 0;
	val &= 0x3f;
	switch (vga.dac.pel_index)
		{
	case 0:
		vga.dac.rgb[vga.dac.write_index].red = val;
		vga.dac.pel_index = 1;
		break;
	case 1:
		vga.dac.rgb[vga.dac.write_index].green = val;
		vga.dac.pel_index = 2;
		break;
	case 2:
		vga.dac.rgb[vga.dac.write_index].blue = val;
		switch (vga.mode)
			{
		case M_VGA:
			VGA_DAC_UpdateColor(vga.dac.write_index);
			if (vga.dac.pel_mask != 0xff)
				{
				Bitu index = vga.dac.write_index;
				if ((index & vga.dac.pel_mask) == index )
					{
					for (Bitu i = index+1; i < 256; i++) 
						if ((i & vga.dac.pel_mask) == index)
							VGA_DAC_UpdateColor(i);
					}
				} 
			break;
		default:
			// Check for attributes and DAC entry link
			for (Bitu i = 0; i < 16; i++)
				if (vga.dac.combine[i] == vga.dac.write_index)
					VGA_DAC_SendColor(i, vga.dac.write_index);
			}
		vga.dac.write_index++;
//		vga.dac.read_index = vga.dac.write_index - 1;//disabled as it breaks Wari
		vga.dac.pel_index = 0;
		break;
		}
	}

static Bitu read_p3c9(Bitu port, Bitu iolen)
	{
	vga.dac.hidac_counter = 0;
	Bit8u ret;
	switch (vga.dac.pel_index)
		{
	case 0:
		ret = vga.dac.rgb[vga.dac.read_index].red;
		vga.dac.pel_index = 1;
		break;
	case 1:
		ret = vga.dac.rgb[vga.dac.read_index].green;
		vga.dac.pel_index = 2;
		break;
	case 2:
		ret = vga.dac.rgb[vga.dac.read_index].blue;
		vga.dac.read_index++;
		vga.dac.pel_index = 0;
//		vga.dac.write_index=vga.dac.read_index+1;//disabled as it breaks wari
		break;
	default:
		ret = 0;
		break;
		}
	return ret;
	}

void VGA_DAC_CombineColor(Bit8u attr, Bit8u pal)
	{
	// Check if this is a new color
	vga.dac.combine[attr] = pal;
	if (vga.mode != M_VGA)
		VGA_DAC_SendColor(attr, pal);
	}

void VGA_SetupDAC(void)
	{
	vga.dac.pel_mask = 0xff;
	vga.dac.pel_index = 0;
	vga.dac.state = DAC_READ;
	vga.dac.read_index = 0;
	vga.dac.write_index = 0;
	vga.dac.hidac_counter = 0;
	vga.dac.reg02 = 0;

	// Setup the DAC IO port Handlers
	IO_RegisterWriteHandler(0x3c6, write_p3c6, IO_MB);
	IO_RegisterReadHandler(0x3c6, read_p3c6, IO_MB);
	IO_RegisterWriteHandler(0x3c7, write_p3c7, IO_MB);
	IO_RegisterReadHandler(0x3c7, read_p3c7, IO_MB);
	IO_RegisterWriteHandler(0x3c8, write_p3c8, IO_MB);
	IO_RegisterReadHandler(0x3c8, read_p3c8, IO_MB);
	IO_RegisterWriteHandler(0x3c9, write_p3c9, IO_MB);
	IO_RegisterReadHandler(0x3c9, read_p3c9, IO_MB);
	}
