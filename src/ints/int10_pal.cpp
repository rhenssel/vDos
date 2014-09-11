#include "vDos.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"

#define ACTL_MAX_REG   0x14

static INLINE void ResetACTL(void)
	{
	IO_Read(vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_CRTC_ADDRESS) + 6);
	}

void INT10_SetSinglePaletteRegister(Bit8u reg, Bit8u val)
	{
	if (reg <= ACTL_MAX_REG)
		{
		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS, reg);
		IO_Write(VGAREG_ACTL_WRITE_DATA, val);
		}
	IO_Write(VGAREG_ACTL_ADDRESS, 32);		// Enable output and protect palette
	}

void INT10_SetAllPaletteRegisters(PhysPt data)
	{
	ResetACTL();
	// First the colors
	for(Bit8u i = 0; i < 0x10; i++)
		{
		IO_Write(VGAREG_ACTL_ADDRESS, i);
		IO_Write(VGAREG_ACTL_WRITE_DATA, vPC_rLodsb(data));
		data++;
		}
	// Then the border
	IO_Write(VGAREG_ACTL_ADDRESS, 0x11);
	IO_Write(VGAREG_ACTL_WRITE_DATA, vPC_rLodsb(data));
	IO_Write(VGAREG_ACTL_ADDRESS, 32);		// Enable output and protect palette
	}

void INT10_ToggleBlinkingBit(Bit8u state)
	{
	Bit8u value;
	//	state&=0x01;
	ResetACTL();
		
	IO_Write(VGAREG_ACTL_ADDRESS, 0x10);
	value = IO_Read(VGAREG_ACTL_READ_DATA);
	if (state <= 1)
		{
		value &= 0xf7;
		value |= state<<3;
		}

	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS, 0x10);
	IO_Write(VGAREG_ACTL_WRITE_DATA, value);
	IO_Write(VGAREG_ACTL_ADDRESS, 32);		// Enable output and protect palette

	if (state <= 1)
		{
		Bit8u msrval = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR)&0xdf;
		if (state)
			msrval |= 0x20;
		vPC_rStosb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR, msrval);
		}
	}

void INT10_GetSinglePaletteRegister(Bit8u reg, Bit8u * val)
	{
	if (reg <= ACTL_MAX_REG)
		{
		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS, reg+32);
		*val = IO_Read(VGAREG_ACTL_READ_DATA);
		IO_Write(VGAREG_ACTL_WRITE_DATA, *val);
		}
	}

void INT10_GetAllPaletteRegisters(PhysPt data)
	{
	ResetACTL();
	// First the colors
	for (Bit8u i = 0; i < 0x10; i++)
		{
		IO_Write(VGAREG_ACTL_ADDRESS, i);
		vPC_rStosb(data, IO_Read(VGAREG_ACTL_READ_DATA));
		ResetACTL();
		data++;
		}
	// Then the border
	IO_Write(VGAREG_ACTL_ADDRESS, 0x11+32);
	vPC_rStosb(data, IO_Read(VGAREG_ACTL_READ_DATA));
	ResetACTL();
	}

void INT10_SetSingleDACRegister(Bit8u index, Bit8u red,Bit8u green, Bit8u blue)
	{
	IO_Write(VGAREG_DAC_WRITE_ADDRESS, (Bit8u)index);
	if ((vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_MODESET_CTL)&0x06) == 0)
		{
		IO_Write(VGAREG_DAC_DATA, red);
		IO_Write(VGAREG_DAC_DATA, green);
		IO_Write(VGAREG_DAC_DATA, blue);
		}
	else
		{
		// calculate clamped intensity, taken from VGABIOS
		Bit32u i = ((77*red + 151*green + 28*blue) + 0x80) >> 8;
		Bit8u ic = (i > 0x3f) ? 0x3f : ((Bit8u)(i & 0xff));
		IO_Write(VGAREG_DAC_DATA, ic);
		IO_Write(VGAREG_DAC_DATA, ic);
		IO_Write(VGAREG_DAC_DATA, ic);
		}
	}

void INT10_GetSingleDACRegister(Bit8u index, Bit8u * red, Bit8u * green, Bit8u * blue)
	{
	IO_Write(VGAREG_DAC_READ_ADDRESS, index);
	*red = IO_Read(VGAREG_DAC_DATA);
	*green = IO_Read(VGAREG_DAC_DATA);
	*blue = IO_Read(VGAREG_DAC_DATA);
	}

void INT10_SetDACBlock(Bit16u index, Bit16u count, PhysPt data)
	{
 	IO_Write(VGAREG_DAC_WRITE_ADDRESS, (Bit8u)index);
	if ((vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_MODESET_CTL)&0x06) == 0)
		{
		for (;count > 0; count--)
			{
			IO_Write(VGAREG_DAC_DATA, vPC_rLodsb(data++));
			IO_Write(VGAREG_DAC_DATA, vPC_rLodsb(data++));
			IO_Write(VGAREG_DAC_DATA, vPC_rLodsb(data++));
			}
		}
	else
		{
		for (;count > 0; count--)
			{
			Bit8u red = vPC_rLodsb(data++);
			Bit8u green = vPC_rLodsb(data++);
			Bit8u blue = vPC_rLodsb(data++);

			// calculate clamped intensity, taken from VGABIOS
			Bit32u i = ((77*red + 151*green + 28*blue) + 0x80) >> 8;
			Bit8u ic = (i > 0x3f) ? 0x3f : ((Bit8u)(i & 0xff));
			IO_Write(VGAREG_DAC_DATA, ic);
			IO_Write(VGAREG_DAC_DATA, ic);
			IO_Write(VGAREG_DAC_DATA, ic);
			}
		}
	}

void INT10_GetDACBlock(Bit16u index, Bit16u count, PhysPt data)
	{
 	IO_Write(VGAREG_DAC_READ_ADDRESS, (Bit8u)index);
	for (; count > 0; count--)
		{
		vPC_rStosb(data++, IO_Read(VGAREG_DAC_DATA));
		vPC_rStosb(data++, IO_Read(VGAREG_DAC_DATA));
		vPC_rStosb(data++, IO_Read(VGAREG_DAC_DATA));
		}
	}

void INT10_SelectDACPage(Bit8u function, Bit8u mode)
	{
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS, 0x10);
	Bit8u old10 = IO_Read(VGAREG_ACTL_READ_DATA);
	if (!function)
		{		// Select paging mode
		if (mode)
			old10 |= 0x80;
		else
			old10 &= 0x7f;
		//IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		IO_Write(VGAREG_ACTL_WRITE_DATA, old10);
		}
	else
		{		// Select page
		IO_Write(VGAREG_ACTL_WRITE_DATA, old10);
		if (!(old10 & 0x80))
			mode <<= 2;
		mode &= 0xf;
		IO_Write(VGAREG_ACTL_ADDRESS, 0x14);
		IO_Write(VGAREG_ACTL_WRITE_DATA, mode);
		}
	IO_Write(VGAREG_ACTL_ADDRESS, 32);		// Enable output and protect palette
	}

void INT10_GetDACPage(Bit8u* mode, Bit8u* page)
	{
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS, 0x10);
	Bit8u reg10 = IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA, reg10);
	*mode = (reg10&0x80) ? 0x01 : 0x00;
	IO_Write(VGAREG_ACTL_ADDRESS, 0x14);
	*page = IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA, *page);
	if (*mode)
		*page &= 0xf;
	else
		{
		*page &= 0xc;
		*page >>= 2;
		}
	}

void INT10_SetPelMask(Bit8u mask)
	{
	IO_Write(VGAREG_PEL_MASK, mask);
	}	

void INT10_GetPelMask(Bit8u & mask)
	{
	mask = IO_Read(VGAREG_PEL_MASK);
	}

void INT10_SetBackgroundBorder(Bit8u val)
	{
	Bit8u color_select = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAL);
	color_select = (color_select & 0xe0) | (val & 0x1f);
	vPC_rStosb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL, color_select);
	
	val = ((val << 1) & 0x10) | (val & 0x7);
	// Aways set the overscan color
	INT10_SetSinglePaletteRegister(0x11, val);
	// Don't set any extra colors when in text mode
	if (CurMode->mode <= 3)
		return;
	INT10_SetSinglePaletteRegister(0, val);
	val = (color_select & 0x10) | 2 | ((color_select & 0x20) >> 5);
	INT10_SetSinglePaletteRegister(1, val);
	val += 2;
	INT10_SetSinglePaletteRegister(2, val);
	val += 2;
	INT10_SetSinglePaletteRegister(3, val);
	}

void INT10_SetColorSelect(Bit8u val)
	{
	Bit8u temp = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xdf) | ((val & 1) ? 0x20 : 0x0);
	vPC_rStosb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL, temp);
	if (CurMode->mode <= 3)		// Maybe even skip the total function!
		return;
	val = (temp & 0x10) | 2 | val;
	INT10_SetSinglePaletteRegister(1, val);
	val += 2;
	INT10_SetSinglePaletteRegister(2, val);
	val += 2;
	INT10_SetSinglePaletteRegister(3, val);
	}
