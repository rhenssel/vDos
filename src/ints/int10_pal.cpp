#include "vDos.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"

#define ACTL_MAX_REG   0x14

static inline void ResetACTL(void)
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
	IO_Write(VGAREG_ACTL_ADDRESS, 32);												// Enable output and protect palette
	}

void INT10_SetAllPaletteRegisters(PhysPt data)
	{
	ResetACTL();
	for (Bit8u i = 0; i < 0x10; i++)												// First the colors
		{
		IO_Write(VGAREG_ACTL_ADDRESS, i);
		IO_Write(VGAREG_ACTL_WRITE_DATA, vPC_rLodsb(data));
		data++;
		}
	IO_Write(VGAREG_ACTL_ADDRESS, 0x11);											// Then the border
	IO_Write(VGAREG_ACTL_WRITE_DATA, vPC_rLodsb(data));
	IO_Write(VGAREG_ACTL_ADDRESS, 32);												// Enable output and protect palette
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
	for (Bit8u i = 0; i < 0x10; i++)												// First the colors
		{
		IO_Write(VGAREG_ACTL_ADDRESS, i);
		vPC_rStosb(data, IO_Read(VGAREG_ACTL_READ_DATA));
		ResetACTL();
		data++;
		}
	IO_Write(VGAREG_ACTL_ADDRESS, 0x11+32);											// Then the border
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
		Bit32u i = ((77*red + 151*green + 28*blue) + 0x80) >> 8;					// Calculate clamped intensity, taken from VGABIOS
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

			Bit32u i = ((77*red + 151*green + 28*blue) + 0x80) >> 8;				// Calculate clamped intensity, taken from VGABIOS
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
	if (!function)																	// Select paging mode
		{
		if (mode)
			old10 |= 0x80;
		else
			old10 &= 0x7f;
		//IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		IO_Write(VGAREG_ACTL_WRITE_DATA, old10);
		}
	else																			// Select page
		{
		IO_Write(VGAREG_ACTL_WRITE_DATA, old10);
		if (!(old10 & 0x80))
			mode <<= 2;
		mode &= 0xf;
		IO_Write(VGAREG_ACTL_ADDRESS, 0x14);
		IO_Write(VGAREG_ACTL_WRITE_DATA, mode);
		}
	IO_Write(VGAREG_ACTL_ADDRESS, 32);												// Enable output and protect palette
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
