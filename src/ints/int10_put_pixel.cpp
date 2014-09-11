#include "vDos.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"

static Bit8u cga_masks[4]={0x3f,0xcf,0xf3,0xfc};
static Bit8u cga_masks2[8]={0x7f,0xbf,0xdf,0xef,0xf7,0xfb,0xfd,0xfe};

void INT10_PutPixel(Bit16u x, Bit16u y, Bit8u page, Bit8u color)
	{
	static bool putpixelwarned = false;

	switch (CurMode->type)
		{
	case M_CGA4:
		{
		if (vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) <= 5)
			{
			Bit16u off = (y>>1)*80+(x>>2);
			if (y&1)
				off += 8*1024;

			Bit8u old = vPC_rLodsb(0xb800, off);
			if (color & 0x80)
				{
				color &= 3;
				old ^= color << (2*(3-(x&3)));
				}
			else
				old = (old&cga_masks[x&3])|((color&3) << (2*(3-(x&3))));
			vPC_rStosb(0xb800, off, old);
			}
		else
			{
			Bit16u off = (y>>2)*160+((x>>2)&(~1));
			off += (8*1024) * (y & 3);

			Bit16u old = vPC_rLodsw(0xb800, off);
			if (color & 0x80)
				{
				old ^= (color&1) << (7-(x&7));
				old ^= ((color&2)>>1) << ((7-(x&7))+8);
				}
			else
				old = (old&(~(0x101<<(7-(x&7))))) | ((color&1) << (7-(x&7))) | (((color&2)>>1) << ((7-(x&7))+8));
			vPC_rStosw(0xb800, off, old);
			}
		}
		break;
	case M_CGA2:
		{
		Bit16u off = (y>>1)*80+(x>>3);
		if (y&1)
			off += 8*1024;
		Bit8u old = vPC_rLodsb(0xb800, off);
		if (color & 0x80)
			{
			color &= 1;
			old ^= color << ((7-(x&7)));
			}
		else
			old = (old&cga_masks2[x&7])|((color&1) << ((7-(x&7))));
		vPC_rStosb(0xb800,off,old);
		}
		break;
	case M_EGA:
		{
		// Set the correct bitmask for the pixel position
		IO_Write(0x3ce, 0x8);
		Bit8u mask = 128>>(x&7);
		IO_Write(0x3cf, mask);
		// Set the color to set/reset register
		IO_Write(0x3ce, 0);
		IO_Write(0x3cf, color);
		// Enable all the set/resets
		IO_Write(0x3ce, 1);
		IO_Write(0x3cf, 0xf);
		// test for xorring
		if (color & 0x80)
			{
			IO_Write(0x3ce, 3);
			IO_Write(0x3cf, 0x18);
			}
		// Perhaps also set mode 1 
		// Calculate where the pixel is in video memory
		if (CurMode->plength != (Bitu)vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE))
			LOG(LOG_INT10, LOG_ERROR)("PutPixel_EGA_p: %x!=%x", CurMode->plength, vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE));
		if (CurMode->swidth != (Bitu)vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS)*8)
			LOG(LOG_INT10, LOG_ERROR)("PutPixel_EGA_w: %x!=%x", CurMode->swidth, vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS)*8);
		PhysPt off = 0xa0000+vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE)*page+((y*vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS)*8+x)>>3);
		// Bitmask and set/reset should do the rest
		vPC_rLodsb(off);
		vPC_rStosb(off, 0xff);
		// Restore bitmask
		IO_Write(0x3ce, 8);
		IO_Write(0x3cf, 0xff);
		IO_Write(0x3ce, 1);
		IO_Write(0x3cf, 0);
		// Restore write operating if changed
		if (color & 0x80)
			{
			IO_Write(0x3ce, 3);
			IO_Write(0x3cf, 0);
			}
		break;
		}
	case M_VGA:
		vPC_rStosb(SegOff2Ptr(0xa000, y*320+x), color);
		break;
	default:
		if (!putpixelwarned)
			{
			putpixelwarned = true;		
			LOG(LOG_INT10, LOG_ERROR)("PutPixel unhandled mode type %d", CurMode->type);
			}
		break;
		}	
	}

void INT10_GetPixel(Bit16u x, Bit16u y, Bit8u page, Bit8u * color)
	{
	switch (CurMode->type)
		{
	case M_CGA4:
		{
		Bit16u off = (y>>1)*80+(x>>2);
		if (y&1)
			off += 8*1024;
		Bit8u val = vPC_rLodsb(0xb800, off);
		*color = (val>>(((3-(x&3)))*2)) & 3 ;
		}
		break;
	case M_CGA2:
		{
		Bit16u off = (y>>1)*80+(x>>3);
		if (y&1)
			off+=8*1024;
		Bit8u val = vPC_rLodsb(0xb800, off);
		*color = (val>>(((7-(x&7))))) & 1 ;
		}
		break;
	case M_EGA:
		{
		// Calculate where the pixel is in video memory
		if (CurMode->plength != (Bitu)vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE))
			LOG(LOG_INT10, LOG_ERROR)("GetPixel_EGA_p: %x!=%x", CurMode->plength, vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE));
		if (CurMode->swidth != (Bitu)vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS)*8)
			LOG(LOG_INT10, LOG_ERROR)("GetPixel_EGA_w: %x!=%x", CurMode->swidth, vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS)*8);
		PhysPt off = 0xa0000+vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE)*page+((y*vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS)*8+x)>>3);
		Bitu shift = 7-(x & 7);
		// Set the read map
		*color = 0;
		IO_Write(0x3ce, 4);
		IO_Write(0x3cf, 0);
		*color |= ((vPC_rLodsb(off)>>shift) & 1) << 0;
		IO_Write(0x3ce, 4);
		IO_Write(0x3cf, 1);
		*color |= ((vPC_rLodsb(off)>>shift) & 1) << 1;
		IO_Write(0x3ce, 4);
		IO_Write(0x3cf, 2);
		*color |= ((vPC_rLodsb(off)>>shift) & 1) << 2;
		IO_Write(0x3ce, 4);
		IO_Write(0x3cf, 3);
		*color |= ((vPC_rLodsb(off)>>shift) & 1) << 3;
		break;
		}
	case M_VGA:
		*color = vPC_rLodsb(SegOff2Ptr(0xa000, 320*y+x));
		break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("GetPixel unhandled mode type %d", CurMode->type);
		break;
		}	
	}

