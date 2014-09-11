#include <string.h>

#include "vDos.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "mouse.h"
#include "vga.h"
#include "bios.h"

#define _EGA_HALF_CLOCK		0x0001
#define _DOUBLESCAN			0x0002
#define _VGA_PIXEL_DOUBLE	0x0004
#define _S3_PIXEL_DOUBLE	0x0008
#define _REPEAT1			0x0010
#define _CGA_SYNCDOUBLE		0x0020

#define SEQ_REGS 0x05
#define GFX_REGS 0x09
#define ATT_REGS 0x15

VideoModeBlock ModeList_VGA[]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
//{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,8 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
//{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,8 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,8 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,8 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x005  ,M_CGA4   ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x006  ,M_CGA2   ,640 ,200 ,80 ,25 ,8 ,8  ,1 ,0xB8000 ,0x4000 ,100 ,449 ,80 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN | _REPEAT1},
{ 0x007  ,M_TEXT   ,720 ,400 ,80 ,25 ,8 ,16 ,8 ,0xB0000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},

{ 0x00D  ,M_EGA    ,320 ,200 ,40 ,25 ,8 ,8  ,8 ,0xA0000 ,0x2000 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	| _DOUBLESCAN	},
{ 0x00E  ,M_EGA    ,640 ,200 ,80 ,25 ,8 ,8  ,4 ,0xA0000 ,0x4000 ,100 ,449 ,80 ,400 ,_DOUBLESCAN },
{ 0x00F  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},/*was EGA_2*/
{ 0x010  ,M_EGA    ,640 ,350 ,80 ,25 ,8 ,14 ,2 ,0xA0000 ,0x8000 ,100 ,449 ,80 ,350 ,0	},
{ 0x011  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},/*was EGA_2 */
{ 0x012  ,M_EGA    ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 ,0	},
{ 0x013  ,M_VGA    ,320 ,200 ,40 ,25 ,8 ,8  ,1 ,0xA0000 ,0x2000 ,100 ,449 ,80 ,400 ,_REPEAT1   },

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	},
};

static Bit8u text_palette[64*3]=
{
	0x00,0x00,0x00,	0x00,0x00,0x2a,	0x00,0x2a,0x00,	0x00,0x2a,0x2a,	0x2a,0x00,0x00,	0x2a,0x00,0x2a,	0x2a,0x2a,0x00,	0x2a,0x2a,0x2a,
	0x00,0x00,0x15,	0x00,0x00,0x3f,	0x00,0x2a,0x15,	0x00,0x2a,0x3f,	0x2a,0x00,0x15,	0x2a,0x00,0x3f,	0x2a,0x2a,0x15,	0x2a,0x2a,0x3f,
	0x00,0x15,0x00,	0x00,0x15,0x2a,	0x00,0x3f,0x00,	0x00,0x3f,0x2a,	0x2a,0x15,0x00,	0x2a,0x15,0x2a,	0x2a,0x3f,0x00,	0x2a,0x3f,0x2a,
	0x00,0x15,0x15,	0x00,0x15,0x3f,	0x00,0x3f,0x15,	0x00,0x3f,0x3f,	0x2a,0x15,0x15,	0x2a,0x15,0x3f,	0x2a,0x3f,0x15,	0x2a,0x3f,0x3f,
	0x15,0x00,0x00,	0x15,0x00,0x2a,	0x15,0x2a,0x00,	0x15,0x2a,0x2a,	0x3f,0x00,0x00,	0x3f,0x00,0x2a,	0x3f,0x2a,0x00,	0x3f,0x2a,0x2a,
	0x15,0x00,0x15,	0x15,0x00,0x3f,	0x15,0x2a,0x15,	0x15,0x2a,0x3f,	0x3f,0x00,0x15,	0x3f,0x00,0x3f,	0x3f,0x2a,0x15,	0x3f,0x2a,0x3f,
	0x15,0x15,0x00,	0x15,0x15,0x2a,	0x15,0x3f,0x00,	0x15,0x3f,0x2a,	0x3f,0x15,0x00,	0x3f,0x15,0x2a,	0x3f,0x3f,0x00,	0x3f,0x3f,0x2a,
	0x15,0x15,0x15,	0x15,0x15,0x3f,	0x15,0x3f,0x15,	0x15,0x3f,0x3f,	0x3f,0x15,0x15,	0x3f,0x15,0x3f,	0x3f,0x3f,0x15,	0x3f,0x3f,0x3f
};

static Bit8u mtext_palette[64*3]=
{
	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,
	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,
	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,
	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,
	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,
	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,
	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,
	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f 
};

static Bit8u mtext_s3_palette[64*3]=
{
	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,
	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,
	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,
	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,
	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,	0x00,0x00,0x00,
	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,
	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,	0x2a,0x2a,0x2a,
	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f,	0x3f,0x3f,0x3f 
};

static Bit8u ega_palette[64*3]=
{
	0x00,0x00,0x00, 0x00,0x00,0x2a, 0x00,0x2a,0x00, 0x00,0x2a,0x2a, 0x2a,0x00,0x00, 0x2a,0x00,0x2a, 0x2a,0x15,0x00, 0x2a,0x2a,0x2a,
	0x00,0x00,0x00, 0x00,0x00,0x2a, 0x00,0x2a,0x00, 0x00,0x2a,0x2a, 0x2a,0x00,0x00, 0x2a,0x00,0x2a, 0x2a,0x15,0x00, 0x2a,0x2a,0x2a,
	0x15,0x15,0x15, 0x15,0x15,0x3f, 0x15,0x3f,0x15, 0x15,0x3f,0x3f, 0x3f,0x15,0x15, 0x3f,0x15,0x3f, 0x3f,0x3f,0x15, 0x3f,0x3f,0x3f,
	0x15,0x15,0x15, 0x15,0x15,0x3f, 0x15,0x3f,0x15, 0x15,0x3f,0x3f, 0x3f,0x15,0x15, 0x3f,0x15,0x3f, 0x3f,0x3f,0x15, 0x3f,0x3f,0x3f,
	0x00,0x00,0x00, 0x00,0x00,0x2a, 0x00,0x2a,0x00, 0x00,0x2a,0x2a, 0x2a,0x00,0x00, 0x2a,0x00,0x2a, 0x2a,0x15,0x00, 0x2a,0x2a,0x2a,
	0x00,0x00,0x00, 0x00,0x00,0x2a, 0x00,0x2a,0x00, 0x00,0x2a,0x2a, 0x2a,0x00,0x00, 0x2a,0x00,0x2a, 0x2a,0x15,0x00, 0x2a,0x2a,0x2a,
	0x15,0x15,0x15, 0x15,0x15,0x3f, 0x15,0x3f,0x15, 0x15,0x3f,0x3f, 0x3f,0x15,0x15, 0x3f,0x15,0x3f, 0x3f,0x3f,0x15, 0x3f,0x3f,0x3f,
	0x15,0x15,0x15, 0x15,0x15,0x3f, 0x15,0x3f,0x15, 0x15,0x3f,0x3f, 0x3f,0x15,0x15, 0x3f,0x15,0x3f, 0x3f,0x3f,0x15, 0x3f,0x3f,0x3f
};

static Bit8u vga_palette[256*3]=
{
	0x00,0x00,0x00,0x00,0x00,0x2a,0x00,0x2a,0x00,0x00,0x2a,0x2a,0x2a,0x00,0x00,0x2a,0x00,0x2a,0x2a,0x15,0x00,0x2a,0x2a,0x2a,
	0x15,0x15,0x15,0x15,0x15,0x3f,0x15,0x3f,0x15,0x15,0x3f,0x3f,0x3f,0x15,0x15,0x3f,0x15,0x3f,0x3f,0x3f,0x15,0x3f,0x3f,0x3f,
	0x00,0x00,0x00,0x05,0x05,0x05,0x08,0x08,0x08,0x0b,0x0b,0x0b,0x0e,0x0e,0x0e,0x11,0x11,0x11,0x14,0x14,0x14,0x18,0x18,0x18,
	0x1c,0x1c,0x1c,0x20,0x20,0x20,0x24,0x24,0x24,0x28,0x28,0x28,0x2d,0x2d,0x2d,0x32,0x32,0x32,0x38,0x38,0x38,0x3f,0x3f,0x3f,
	0x00,0x00,0x3f,0x10,0x00,0x3f,0x1f,0x00,0x3f,0x2f,0x00,0x3f,0x3f,0x00,0x3f,0x3f,0x00,0x2f,0x3f,0x00,0x1f,0x3f,0x00,0x10,
	0x3f,0x00,0x00,0x3f,0x10,0x00,0x3f,0x1f,0x00,0x3f,0x2f,0x00,0x3f,0x3f,0x00,0x2f,0x3f,0x00,0x1f,0x3f,0x00,0x10,0x3f,0x00,
	0x00,0x3f,0x00,0x00,0x3f,0x10,0x00,0x3f,0x1f,0x00,0x3f,0x2f,0x00,0x3f,0x3f,0x00,0x2f,0x3f,0x00,0x1f,0x3f,0x00,0x10,0x3f,
	0x1f,0x1f,0x3f,0x27,0x1f,0x3f,0x2f,0x1f,0x3f,0x37,0x1f,0x3f,0x3f,0x1f,0x3f,0x3f,0x1f,0x37,0x3f,0x1f,0x2f,0x3f,0x1f,0x27,

	0x3f,0x1f,0x1f,0x3f,0x27,0x1f,0x3f,0x2f,0x1f,0x3f,0x37,0x1f,0x3f,0x3f,0x1f,0x37,0x3f,0x1f,0x2f,0x3f,0x1f,0x27,0x3f,0x1f,
	0x1f,0x3f,0x1f,0x1f,0x3f,0x27,0x1f,0x3f,0x2f,0x1f,0x3f,0x37,0x1f,0x3f,0x3f,0x1f,0x37,0x3f,0x1f,0x2f,0x3f,0x1f,0x27,0x3f,
	0x2d,0x2d,0x3f,0x31,0x2d,0x3f,0x36,0x2d,0x3f,0x3a,0x2d,0x3f,0x3f,0x2d,0x3f,0x3f,0x2d,0x3a,0x3f,0x2d,0x36,0x3f,0x2d,0x31,
	0x3f,0x2d,0x2d,0x3f,0x31,0x2d,0x3f,0x36,0x2d,0x3f,0x3a,0x2d,0x3f,0x3f,0x2d,0x3a,0x3f,0x2d,0x36,0x3f,0x2d,0x31,0x3f,0x2d,
	0x2d,0x3f,0x2d,0x2d,0x3f,0x31,0x2d,0x3f,0x36,0x2d,0x3f,0x3a,0x2d,0x3f,0x3f,0x2d,0x3a,0x3f,0x2d,0x36,0x3f,0x2d,0x31,0x3f,
	0x00,0x00,0x1c,0x07,0x00,0x1c,0x0e,0x00,0x1c,0x15,0x00,0x1c,0x1c,0x00,0x1c,0x1c,0x00,0x15,0x1c,0x00,0x0e,0x1c,0x00,0x07,
	0x1c,0x00,0x00,0x1c,0x07,0x00,0x1c,0x0e,0x00,0x1c,0x15,0x00,0x1c,0x1c,0x00,0x15,0x1c,0x00,0x0e,0x1c,0x00,0x07,0x1c,0x00,
	0x00,0x1c,0x00,0x00,0x1c,0x07,0x00,0x1c,0x0e,0x00,0x1c,0x15,0x00,0x1c,0x1c,0x00,0x15,0x1c,0x00,0x0e,0x1c,0x00,0x07,0x1c,

	0x0e,0x0e,0x1c,0x11,0x0e,0x1c,0x15,0x0e,0x1c,0x18,0x0e,0x1c,0x1c,0x0e,0x1c,0x1c,0x0e,0x18,0x1c,0x0e,0x15,0x1c,0x0e,0x11,
	0x1c,0x0e,0x0e,0x1c,0x11,0x0e,0x1c,0x15,0x0e,0x1c,0x18,0x0e,0x1c,0x1c,0x0e,0x18,0x1c,0x0e,0x15,0x1c,0x0e,0x11,0x1c,0x0e,
	0x0e,0x1c,0x0e,0x0e,0x1c,0x11,0x0e,0x1c,0x15,0x0e,0x1c,0x18,0x0e,0x1c,0x1c,0x0e,0x18,0x1c,0x0e,0x15,0x1c,0x0e,0x11,0x1c,
	0x14,0x14,0x1c,0x16,0x14,0x1c,0x18,0x14,0x1c,0x1a,0x14,0x1c,0x1c,0x14,0x1c,0x1c,0x14,0x1a,0x1c,0x14,0x18,0x1c,0x14,0x16,
	0x1c,0x14,0x14,0x1c,0x16,0x14,0x1c,0x18,0x14,0x1c,0x1a,0x14,0x1c,0x1c,0x14,0x1a,0x1c,0x14,0x18,0x1c,0x14,0x16,0x1c,0x14,
	0x14,0x1c,0x14,0x14,0x1c,0x16,0x14,0x1c,0x18,0x14,0x1c,0x1a,0x14,0x1c,0x1c,0x14,0x1a,0x1c,0x14,0x18,0x1c,0x14,0x16,0x1c,
	0x00,0x00,0x10,0x04,0x00,0x10,0x08,0x00,0x10,0x0c,0x00,0x10,0x10,0x00,0x10,0x10,0x00,0x0c,0x10,0x00,0x08,0x10,0x00,0x04,
	0x10,0x00,0x00,0x10,0x04,0x00,0x10,0x08,0x00,0x10,0x0c,0x00,0x10,0x10,0x00,0x0c,0x10,0x00,0x08,0x10,0x00,0x04,0x10,0x00,

	0x00,0x10,0x00,0x00,0x10,0x04,0x00,0x10,0x08,0x00,0x10,0x0c,0x00,0x10,0x10,0x00,0x0c,0x10,0x00,0x08,0x10,0x00,0x04,0x10,
	0x08,0x08,0x10,0x0a,0x08,0x10,0x0c,0x08,0x10,0x0e,0x08,0x10,0x10,0x08,0x10,0x10,0x08,0x0e,0x10,0x08,0x0c,0x10,0x08,0x0a,
	0x10,0x08,0x08,0x10,0x0a,0x08,0x10,0x0c,0x08,0x10,0x0e,0x08,0x10,0x10,0x08,0x0e,0x10,0x08,0x0c,0x10,0x08,0x0a,0x10,0x08,
	0x08,0x10,0x08,0x08,0x10,0x0a,0x08,0x10,0x0c,0x08,0x10,0x0e,0x08,0x10,0x10,0x08,0x0e,0x10,0x08,0x0c,0x10,0x08,0x0a,0x10,
	0x0b,0x0b,0x10,0x0c,0x0b,0x10,0x0d,0x0b,0x10,0x0f,0x0b,0x10,0x10,0x0b,0x10,0x10,0x0b,0x0f,0x10,0x0b,0x0d,0x10,0x0b,0x0c,
	0x10,0x0b,0x0b,0x10,0x0c,0x0b,0x10,0x0d,0x0b,0x10,0x0f,0x0b,0x10,0x10,0x0b,0x0f,0x10,0x0b,0x0d,0x10,0x0b,0x0c,0x10,0x0b,
	0x0b,0x10,0x0b,0x0b,0x10,0x0c,0x0b,0x10,0x0d,0x0b,0x10,0x0f,0x0b,0x10,0x10,0x0b,0x0f,0x10,0x0b,0x0d,0x10,0x0b,0x0c,0x10,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
VideoModeBlock * CurMode;

static bool SetCurMode(Bit16u mode)
	{
	for (Bitu i = 0; ModeList_VGA[i].mode != 0xffff; i++)
		if (ModeList_VGA[i].mode == mode)
			{
			CurMode = &ModeList_VGA[i];
			return true;
			}
	return false;
	}

void FinishSetMode(bool clearmem)
	{
	// Clear video memory if needs be
	if (clearmem)
		{
		switch (CurMode->type)
			{
		case M_TEXT:
			{
			vPC_rStoswb(SegOff2Ptr((CurMode->mode == 7) ? 0xb000 : 0xb800, 0), 0x0720, 16*1024*2);
			break;
			}
		case M_EGA:	case M_VGA:
			// Hack we just acess the memory directly
			memset(vga.mem.linear, 0, vga.vmemsize);
			memset(vga.fastmem, 0, vga.vmemsize<<1);
		case M_CGA4:
		case M_CGA2:
			vPC_rStoswb(SegOff2Ptr(0xb800, 0), 0, 16*1024*2);
			break;
			}
		}
	// Setup the BIOS
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE, (Bit8u)CurMode->mode);
	vPC_rStosw(BIOSMEM_SEG, BIOSMEM_NB_COLS, (Bit16u)CurMode->twidth);
	vPC_rStosw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE, (Bit16u)CurMode->plength);
	vPC_rStosw(BIOSMEM_SEG, BIOSMEM_CRTC_ADDRESS, ((CurMode->mode==7 )|| (CurMode->mode==0x0f)) ? 0x3b4 : 0x3d4);
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_NB_ROWS, (Bit8u)(CurMode->theight-1));
	vPC_rStosw(BIOSMEM_SEG, BIOSMEM_CHAR_HEIGHT, (Bit16u)CurMode->cheight);
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_VIDEO_CTL, (0x60|(clearmem ? 0 : 0x80)));
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_SWITCHES, 0x09);

	// this is an index into the dcc table:
	vPC_rStosb(BIOSMEM_SEG, BIOSMEM_DCC_INDEX, 0x0b);
	vPC_rStosd(BIOSMEM_SEG, BIOSMEM_VS_POINTER, int10.rom.video_save_pointers);

	// Set cursor shape
	if (CurMode->type == M_TEXT)
		INT10_SetCursorShape(6, 7);
	// Set cursor pos for page 0..7
	for (Bit8u ct = 0; ct < 8; ct++)
		INT10_SetCursorPos(0, 0, ct);
	// Set active page 0
	INT10_SetActivePage(0);
	// Set some interrupt vectors
	switch (CurMode->cheight)
		{
	case 16:
		RealSetVec(0x43, int10.rom.font_16);
		break;
	case 14:
		RealSetVec(0x43, int10.rom.font_14);
		break;
	case 8:
		RealSetVec(0x43, int10.rom.font_8_first);
		break;
		}
	// Tell mouse resolution change
	Mouse_NewVideoMode();
	}

bool INT10_SetVideoMode(Bit16u mode)
	{
	bool clearmem = true;
	Bitu i;
	if (mode >= 0x100)
		{
		if (mode & 0x8000)
			clearmem = false;
		mode &= 0xfff;
		}
	if ((mode < 0x100) && (mode & 0x80))
		{
		clearmem = false;
		mode -= 0x80;
		}

	// First read mode setup settings from bios area
//	Bit8u video_ctl=vPC_rLodsb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL);
//	Bit8u vga_switches=vPC_rLodsb(BIOSMEM_SEG,BIOSMEM_SWITCHES);
	Bit8u modeset_ctl = vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_MODESET_CTL);
	if (!SetCurMode(mode))
		return false;

	// Setup the VGA to the correct mode
	// turn off video
	IO_Write(0x3c4, 0);
	IO_Write(0x3c5, 1);		// reset
	IO_Write(0x3c4, 1);
	IO_Write(0x3c5, 0x20);	// screen off

	Bit16u crtc_base;
	bool mono_mode = (mode == 7) || (mode == 0xf);  
	if (mono_mode)
		crtc_base = 0x3b4;
	else
		crtc_base = 0x3d4;

	// Setup MISC Output Register
	Bit8u misc_output = 2 | (mono_mode ? 0 : 1);

	switch (CurMode->vdispend)
		{
	case 400: 
		misc_output |= 0x60;
		break;
	case 480:
		misc_output |= 0xe0;
		break;
	case 350:
		misc_output |= 0xa0;
		break;
	case 200:
	default:
		misc_output |= 0x20;
		}
	IO_Write(0x3c2,misc_output);		//Setup for 3b4 or 3d4

	/* Program Sequencer */
	Bit8u seq_data[SEQ_REGS];
	memset(seq_data, 0, SEQ_REGS);
	
	seq_data[0] = 0x3;					// not reset
	seq_data[1] = 0x21;					// screen still disabled, will be enabled at end of setmode
	
	if (CurMode->special & _EGA_HALF_CLOCK)
		seq_data[1] |= 0x08;			// Check for half clock
	seq_data[4] |= 2;					// More than 64kb
	switch (CurMode->type)
		{
	case M_TEXT:
		seq_data[2] |= 3;				// Enable plane 0 and 1
		seq_data[4] |= 1;				// Alpanumeric
		seq_data[4] |= 4;				// odd/even enabled
		break;
	case M_VGA:
		seq_data[2] |= 0xf;				// Enable all planes for writing
		seq_data[4] |= 0xc;				// Graphics - odd/even - Chained
		break;
	case M_CGA2:
		seq_data[2] |= 0xf;				// Enable plane 0
		break;
	case M_CGA4:
		break;
	case M_EGA:
		seq_data[2] |= 0xf;				// Enable all planes for writing
		break;
		}
	for (Bit8u ct = 0; ct < SEQ_REGS; ct++)
		{
		IO_Write(0x3c4, ct);
		IO_Write(0x3c5, seq_data[ct]);
		}

	// Program CRTC
	// First disable write protection
	IO_Write(crtc_base, 0x11);
	IO_Write(crtc_base+1, IO_Read(crtc_base+1)&0x7f);
	// Clear all the regs
	for (Bit8u ct = 0; ct <= 0x18; ct++)
		{
		IO_Write(crtc_base, ct);
		IO_Write(crtc_base+1, 0);
		}
	Bit8u overflow = 0;
	Bit8u max_scanline = 0;
	Bit8u ver_overflow = 0;
	Bit8u hor_overflow = 0;
	// Horizontal Total
	IO_Write(crtc_base, 0);
	IO_Write(crtc_base+1, (Bit8u)(CurMode->htotal-5));
	hor_overflow |= ((CurMode->htotal-5) & 0x100) >> 8;
	// Horizontal Display End
	IO_Write(crtc_base, 1);
	IO_Write(crtc_base+1, (Bit8u)(CurMode->hdispend-1));
	hor_overflow |= ((CurMode->hdispend-1) & 0x100) >> 7;
	// Start horizontal Blanking
	IO_Write(crtc_base, 2);
	IO_Write(crtc_base+1, (Bit8u)CurMode->hdispend);
	hor_overflow |= ((CurMode->hdispend) & 0x100) >> 6;
	// End horizontal Blanking
	Bitu blank_end = (CurMode->htotal-2) & 0x7f;
	IO_Write(crtc_base, 3);
	IO_Write(crtc_base+1, 0x80|(blank_end & 0x1f));

	// Start Horizontal Retrace
	Bitu ret_start;
	if ((CurMode->special & _EGA_HALF_CLOCK) && (CurMode->type != M_CGA2))
		ret_start = (CurMode->hdispend+3);
	else if (CurMode->type == M_TEXT)
		ret_start = (CurMode->hdispend+5);
	else
		ret_start = (CurMode->hdispend+4);
	IO_Write(crtc_base, 4);
	IO_Write(crtc_base+1, (Bit8u)ret_start);
	hor_overflow |= (ret_start & 0x100) >> 4;

	// End Horizontal Retrace
	Bitu ret_end;
	if (CurMode->special & _EGA_HALF_CLOCK)
		{
		if (CurMode->type == M_CGA2)
			ret_end=0;			// mode 6
		else if (CurMode->special & _DOUBLESCAN)
			ret_end = (CurMode->htotal-18) & 0x1f;
		else
			ret_end = ((CurMode->htotal-18) & 0x1f) | 0x20;		// mode 0&1 have 1 char sync delay
		}
	else if (CurMode->type == M_TEXT)
		ret_end = (CurMode->htotal-3) & 0x1f;
	else
		ret_end = (CurMode->htotal-4) & 0x1f;
	
	IO_Write(crtc_base, 5);
	IO_Write(crtc_base+1, (Bit8u)(ret_end | (blank_end & 0x20) << 2));

	// Vertical Total
	IO_Write(crtc_base, 6);
	IO_Write(crtc_base+1, (Bit8u)(CurMode->vtotal-2));
	overflow |= ((CurMode->vtotal-2) & 0x100) >> 8;
	overflow |= ((CurMode->vtotal-2) & 0x200) >> 4;
	ver_overflow |= ((CurMode->vtotal-2) & 0x400) >> 10;

	Bitu vretrace;
	switch (CurMode->vdispend)
		{
	case 400:
		vretrace = CurMode->vdispend+12;
		break;
	case 480:
		vretrace = CurMode->vdispend+10;
		break;
	case 350:
		vretrace = CurMode->vdispend+37;
		break;
	default:
		vretrace = CurMode->vdispend+12;
		}

	// Vertical Retrace Start
	IO_Write(crtc_base, 0x10);
	IO_Write(crtc_base+1, (Bit8u)vretrace);
	overflow |= (vretrace & 0x100) >> 6;
	overflow |= (vretrace & 0x200) >> 2;
	ver_overflow |= (vretrace & 0x400) >> 6;

	// Vertical Retrace End
	IO_Write(crtc_base, 0x11);
	IO_Write(crtc_base+1, (vretrace+2) & 0xF);

	// Vertical Display End
	IO_Write(crtc_base, 0x12);
	IO_Write(crtc_base+1, (Bit8u)(CurMode->vdispend-1));
	overflow |= ((CurMode->vdispend-1) & 0x100) >> 7;
	overflow |= ((CurMode->vdispend-1) & 0x200) >> 3;
	ver_overflow |= ((CurMode->vdispend-1) & 0x400) >> 9;
	
	Bitu vblank_trim;
	switch (CurMode->vdispend)
		{
	case 400:
		vblank_trim = 6;
		break;
	case 480:
		vblank_trim = 7;
		break;
	case 350:
		vblank_trim = 5;
		break;
	default:
		vblank_trim = 8;
		}

	// Vertical Blank Start
	IO_Write(crtc_base, 0x15);
	IO_Write(crtc_base+1, (Bit8u)(CurMode->vdispend+vblank_trim));
	overflow |= ((CurMode->vdispend+vblank_trim) & 0x100) >> 5;
	max_scanline |= ((CurMode->vdispend+vblank_trim) & 0x200) >> 4;
	ver_overflow |= ((CurMode->vdispend+vblank_trim) & 0x400) >> 8;

	// Vertical Blank End
	IO_Write(crtc_base, 0x16);
	IO_Write(crtc_base+1, (Bit8u)(CurMode->vtotal-vblank_trim-2));

	// Line Compare
	Bitu line_compare = (CurMode->vtotal < 1024) ? 1023 : 2047;
	IO_Write(crtc_base, 0x18);
	IO_Write(crtc_base+1, line_compare&0xff);
	overflow |= (line_compare & 0x100) >> 4;
	max_scanline |= (line_compare & 0x200) >> 3;
	ver_overflow |= (line_compare & 0x400) >> 4;
	Bit8u underline = 0;
	// Maximum scanline / Underline Location
	if (CurMode->special & _DOUBLESCAN)
		max_scanline |= 0x80;
	if (CurMode->special & _REPEAT1)
		max_scanline |= 0x01;

	switch (CurMode->type)
		{
	case M_TEXT:
		switch (modeset_ctl & 0x90)
			{
		case 0:									// 350-lines mode: 8x14 font
			max_scanline |= (14-1);
			break;
		default:								// reserved
//		case 0x10:								// 400 lines 8x16 font
			max_scanline |= CurMode->cheight-1;
			break;
//		case 0x80:								// 200 lines: 8x8 font and doublescan
//			max_scanline |= (8-1);
//			max_scanline |= 0x80;
//			break;
			}
		underline = mono_mode ? 0x0f : 0x1f;	// mode 7 uses a diff underline position
		break;
	case M_VGA:
		underline = 0x40;
		break;
		}
	if (CurMode->vdispend == 350)
		underline = 0x0f;

	IO_Write(crtc_base, 0x09);
	IO_Write(crtc_base+1, max_scanline);
	IO_Write(crtc_base, 0x14);
	IO_Write(crtc_base+1, underline);

	// OverFlow
	IO_Write(crtc_base, 0x07);
	IO_Write(crtc_base+1, overflow);

	// Offset Register
	IO_Write(crtc_base, 0x13);
	IO_Write(crtc_base+1, (CurMode->hdispend/2) & 0xff);

	// Mode Control
	Bit8u mode_control = 0;

	switch (CurMode->type)
		{
	case M_TEXT:
	case M_VGA:
		mode_control = 0xa3;
		if (CurMode->special & _VGA_PIXEL_DOUBLE)
			mode_control |= 0x08;
		break;
	case M_CGA2:
		mode_control = 0xc2;		// 0x06 sets address wrap.
		break;
	case M_CGA4:
		mode_control = 0xa2;
		break;
	case M_EGA:
		if (CurMode->mode == 0x11)	// 0x11 also sets address wrap.  thought maybe all 2 color modes did but 0x0f doesn't.
			mode_control = 0xc3;	// so.. 0x11 or 0x0f a one off?
		else
			mode_control = 0xe3;
		break;
		}

	IO_Write(crtc_base, 0x17);
	IO_Write(crtc_base+1, mode_control);
	// Renable write protection
	IO_Write(crtc_base, 0x11);
	IO_Write(crtc_base+1, IO_Read(crtc_base+1)|0x80);

	// Write Misc Output
	IO_Write(0x3c2, misc_output);
	// Program Graphics controller
	Bit8u gfx_data[GFX_REGS];
	memset(gfx_data, 0, GFX_REGS);
	gfx_data[0x7] = 0xf;				// Color don't care
	gfx_data[0x8] = 0xff;				// BitMask
	switch (CurMode->type)
		{
	case M_TEXT:
		gfx_data[0x5] |= 0x10;						// Odd-Even Mode
		gfx_data[0x6] |= mono_mode ? 0x0a : 0x0e;	// Either b800 or b000
		break;
	case M_VGA:
		gfx_data[0x5] |= 0x40;		// 256 color mode
		gfx_data[0x6] |= 0x05;		// graphics mode at 0xa000-affff
		break;
	case M_EGA:
		gfx_data[0x6] |= 0x05;		// graphics mode at 0xa000-affff
		break;
	case M_CGA4:
		gfx_data[0x5] |= 0x20;		// CGA mode
		gfx_data[0x6] |= 0x0f;		// graphics mode at at 0xb800=0xbfff
		break;
	case M_CGA2:
		gfx_data[0x6] |= 0x0f;		// graphics mode at at 0xb800=0xbfff
		break;
		}
	for (Bit8u ct = 0; ct < GFX_REGS; ct++)
		{
		IO_Write(0x3ce, ct);
		IO_Write(0x3cf, gfx_data[ct]);
		}
	Bit8u att_data[ATT_REGS];
	memset(att_data, 0, ATT_REGS);
	att_data[0x12] = 0xf;			// Always have all color planes enabled
	// Program Attribute Controller
	switch (CurMode->type)
		{
	case M_EGA:
		att_data[0x10] = 1;			// Color Graphics
		switch (CurMode->mode)
			{
		case 0x0f:
			att_data[0x10] |= 0x0a;	//Monochrome
			att_data[0x01] = 0x08;
			att_data[0x04] = 0x18;
			att_data[0x05] = 0x18;
			att_data[0x09] = 0x08;
			att_data[0x0d] = 0x18;
			break;
		case 0x11:
			for (i=1; i<16; i++)
				att_data[i] = 0x3f;
			break;
		case 0x10:
		case 0x12: 
			goto att_text16;
		default:
			for (Bit8u ct = 0; ct < 8; ct++)
				{
				att_data[ct] = ct;
				att_data[ct+8] = ct+0x10;
				}
			break;
			}
		break;
	case M_TEXT:
		att_data[0x13] = 0;
//		att_data[0x10] = 0x08;						// Color Text with blinking, 8 Bit characters
		att_data[0x10] = 0;							// Color Text no blinking, 8 Bit characters
		vPC_rStosb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL, 0x30);
att_text16:
		if (CurMode->mode == 7)
			{
			att_data[0] = 0x00;
			att_data[8] = 0x10;
			for (i = 1; i < 8; i++)
				{
				att_data[i] = 0x08;
				att_data[i+8] = 0x18;
				}
			}
		else
			{
			for (Bit8u ct = 0; ct < 8; ct++)
				{
				att_data[ct] = ct;
				att_data[ct+8] = ct+0x38;
				}
			att_data[0x06] = 0x14;		// Odd Color 6 yellow/brown.
			}
		break;
	case M_CGA2:
		att_data[0x10] = 1;				// Color Graphics
		att_data[0] = 0;
		for (i = 1; i < 0x10; i++)
			att_data[i] = 0x17;
		att_data[0x12] = 1	;			//Only enable 1 plane
		vPC_rStosb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL, 0x3f);
		break;
	case M_CGA4:
		att_data[0x10] = 1;				// Color Graphics
		att_data[0] = 0;
		att_data[1] = 0x13;
		att_data[2] = 0x15;
		att_data[3] = 0x17;
		att_data[4] = 0x02;
		att_data[5] = 0x04;
		att_data[6] = 0x06;
		att_data[7] = 0x07;
		for (Bit8u ct = 8; ct < 16; ct++) 
			att_data[ct] = ct + 8;
		vPC_rStosb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL, 0x30);
		break;
	case M_VGA:
		for (Bit8u ct = 0; ct < 16; ct++)
			att_data[ct] = ct;
		att_data[0x10] = 0x41;			// Color Graphics 8-bit
		break;
		}
	IO_Read(mono_mode ? 0x3ba : 0x3da);
	if ((modeset_ctl & 8) == 0)
		{
		for (Bit8u ct = 0; ct < ATT_REGS; ct++)
			{
			IO_Write(0x3c0, ct);
			IO_Write(0x3c0, att_data[ct]);
			}
		IO_Write(0x3c0, 0x20);			// Disable palette access
		IO_Write(0x3c0, 0x00);
		IO_Write(0x3c6, 0xff);			// Reset Pelmask
		// Setup the DAC
		IO_Write(0x3c8, 0);
		switch (CurMode->type)
			{
		case M_EGA:
			if (CurMode->mode > 0xf)
				goto dac_text16;
			else if (CurMode->mode == 0xf)
				for (i = 0; i < 64*3; i++)
					IO_Write(0x3c9, mtext_s3_palette[i]);
			else
				for (i = 0; i < 64*3; i++)
					IO_Write(0x3c9, ega_palette[i]);
			break;
		case M_CGA2:
		case M_CGA4:
			for (i = 0; i < 64*3; i++)
				IO_Write(0x3c9, ega_palette[i]);
			break;
		case M_TEXT:
			if (CurMode->mode == 7)
				{
				for (i = 0; i < 64*3; i++)
					IO_Write(0x3c9, mtext_palette[i]);
				break;
				}
dac_text16:
			for (i = 0; i < 64*3; i++)
				IO_Write(0x3c9, text_palette[i]);
			break;
		case M_VGA:
			for (i = 0; i < 256*3; i++)
				IO_Write(0x3c9, vga_palette[i]);
			break;
			}
		}
	else
		{
		for (Bit8u ct = 16; ct < ATT_REGS; ct++)
			{
			if (ct == 0x11)		// skip overscan register
				continue;		
			IO_Write(0x3c0, ct);
			IO_Write(0x3c0, att_data[ct]);
			}
		}
	// Setup some special stuff for different modes
	switch (CurMode->type)
		{
	case M_TEXT:
		switch (CurMode->mode)
			{
//		case 0:
//			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x2c);
//			break;
//		case 1:
//			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x28);
//			break;
		case 2:
			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x2d);
			break;
		case 3:
		case 7:
			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x29);
			break;
			}
		break;
	case M_CGA2:
		vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x1e);
		break;
	case M_CGA4:
		if (CurMode->mode == 4)
			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x2a);
		else if (CurMode->mode == 5)
			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x2e);
		else
			vPC_rStosb(BIOSMEM_SEG, BIOSMEM_CURRENT_MSR, 0x2);
		break;
		}

	FinishSetMode(clearmem);

	// Set vga attrib register into defined state
	IO_Read(mono_mode ? 0x3ba : 0x3da);
	IO_Write(0x3c0, 0x20);

	// Enable screen memory access
	IO_Write(0x3c4, 1);
	IO_Write(0x3c5, seq_data[1] & ~0x20);
	return true;
	}