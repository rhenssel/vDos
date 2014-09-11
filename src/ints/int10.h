#include "vga.h"
#include "mem.h"

#define S3_LFB_BASE		0xC0000000

#define BIOSMEM_SEG		0x40

#define BIOSMEM_CURRENT_MODE  0x49
#define BIOSMEM_NB_COLS       0x4A
#define BIOSMEM_PAGE_SIZE     0x4C
#define BIOSMEM_CURRENT_START 0x4E
#define BIOSMEM_CURSOR_POS    0x50
#define BIOSMEM_CURSOR_TYPE   0x60
#define BIOSMEM_CURRENT_PAGE  0x62
#define BIOSMEM_CRTC_ADDRESS  0x63
#define BIOSMEM_CURRENT_MSR   0x65
#define BIOSMEM_CURRENT_PAL   0x66
#define BIOSMEM_NB_ROWS       0x84
#define BIOSMEM_CHAR_HEIGHT   0x85
#define BIOSMEM_VIDEO_CTL     0x87
#define BIOSMEM_SWITCHES      0x88
#define BIOSMEM_MODESET_CTL   0x89
#define BIOSMEM_DCC_INDEX     0x8A
#define BIOSMEM_CRTCPU_PAGE   0x8A
#define BIOSMEM_VS_POINTER    0xA8

// VGA registers
#define VGAREG_ACTL_ADDRESS            0x3c0
#define VGAREG_ACTL_WRITE_DATA         0x3c0
#define VGAREG_ACTL_READ_DATA          0x3c1

#define VGAREG_INPUT_STATUS            0x3c2
#define VGAREG_WRITE_MISC_OUTPUT       0x3c2
#define VGAREG_VIDEO_ENABLE            0x3c3
#define VGAREG_SEQU_ADDRESS            0x3c4
#define VGAREG_SEQU_DATA               0x3c5

#define VGAREG_PEL_MASK                0x3c6
#define VGAREG_DAC_STATE               0x3c7
#define VGAREG_DAC_READ_ADDRESS        0x3c7
#define VGAREG_DAC_WRITE_ADDRESS       0x3c8
#define VGAREG_DAC_DATA                0x3c9

#define VGAREG_READ_FEATURE_CTL        0x3ca
#define VGAREG_READ_MISC_OUTPUT        0x3cc

#define VGAREG_GRDC_ADDRESS            0x3ce
#define VGAREG_GRDC_DATA               0x3cf

#define VGAREG_MDA_CRTC_ADDRESS        0x3b4
#define VGAREG_MDA_CRTC_DATA           0x3b5
#define VGAREG_VGA_CRTC_ADDRESS        0x3d4
#define VGAREG_VGA_CRTC_DATA           0x3d5

#define VGAREG_MDA_WRITE_FEATURE_CTL   0x3ba
#define VGAREG_VGA_WRITE_FEATURE_CTL   0x3da
#define VGAREG_ACTL_RESET              0x3da
#define VGAREG_TDY_RESET               0x3da
#define VGAREG_TDY_ADDRESS             0x3da
#define VGAREG_TDY_DATA                0x3de
#define VGAREG_PCJR_DATA               0x3da

#define VGAREG_MDA_MODECTL             0x3b8
#define VGAREG_CGA_MODECTL             0x3d8
#define VGAREG_CGA_PALETTE             0x3d9

#define BIOS_NCOLS Bit16u ncols = vPC_rLodsw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
#define BIOS_NROWS Bit16u nrows = (Bit16u)vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_NB_ROWS)+1;

extern Bit8u int10_font_08[256 * 8];
extern Bit8u int10_font_14[256 * 14];
extern Bit8u int10_font_16[256 * 16];

struct VideoModeBlock {
	Bit16u	mode;
	VGAModes	type;
	Bitu	swidth, sheight;
	Bitu	twidth, theight;
	Bitu	cwidth, cheight;
	Bitu	ptotal, pstart, plength;

	Bitu	htotal, vtotal;
	Bitu	hdispend, vdispend;
};

extern VideoModeBlock ModeList_VGA[];
extern VideoModeBlock * CurMode;

typedef struct {
	struct {
		RealPt font_8_first;
		RealPt font_8_second;
		RealPt font_14;
		RealPt font_16;
		RealPt font_14_alternate;
		RealPt font_16_alternate;
		RealPt static_state;
		RealPt video_save_pointers;
		RealPt video_parameter_table;
		RealPt video_save_pointer_table;
		RealPt video_dcc_table;
		RealPt oemstring;
		RealPt vesa_modes;
		Bit16u used;
	} rom;
} Int10Data;

extern Int10Data int10;

static Bit8u CURSOR_POS_COL(Bit8u page)
	{
	return vPC_rLodsb(BIOSMEM_SEG, BIOSMEM_CURSOR_POS+page*2);
	}

static Bit8u CURSOR_POS_ROW(Bit8u page)
	{
	return vPC_rLodsb(BIOSMEM_SEG,BIOSMEM_CURSOR_POS+page*2+1);
	}

bool INT10_SetVideoMode(Bit16u mode);

void INT10_ScrollWindow(Bit8u rul,Bit8u cul,Bit8u rlr,Bit8u clr,Bit8s nlines,Bit8u attr,Bit8u page);

void INT10_SetActivePage(Bit8u page);

void INT10_SetCursorShape(Bit8u first, Bit8u last);
void INT10_SetCursorPos(Bit8u row, Bit8u col, Bit8u page);
void INT10_TeletypeOutput(Bit8u chr, Bit8u attr);
void INT10_TeletypeOutputAttr(Bit8u chr, Bit8u attr, bool useattr);
void INT10_ReadCharAttr(Bit16u * result, Bit8u page);
void INT10_WriteChar(Bit8u chr, Bit8u attr, Bit8u page, Bit16u count, bool showattr);
void INT10_WriteString(Bit8u row, Bit8u col, Bit8u flag, Bit8u attr, PhysPt string, Bit16u count, Bit8u page);

// Graphics Stuff
void INT10_PutPixel(Bit16u x, Bit16u y, Bit8u page, Bit8u color);
void INT10_GetPixel(Bit16u x, Bit16u y, Bit8u page, Bit8u * color);

/* Palette Group */
void INT10_SetSinglePaletteRegister(Bit8u reg, Bit8u val);
void INT10_SetAllPaletteRegisters(PhysPt data);
void INT10_GetSinglePaletteRegister(Bit8u reg,Bit8u * val);
void INT10_GetAllPaletteRegisters(PhysPt data);
void INT10_SetSingleDACRegister(Bit8u index, Bit8u red, Bit8u green, Bit8u blue);
void INT10_GetSingleDACRegister(Bit8u index, Bit8u * red, Bit8u * green, Bit8u * blue);
void INT10_SetDACBlock(Bit16u index, Bit16u count, PhysPt data);
void INT10_GetDACBlock(Bit16u index, Bit16u count, PhysPt data);
void INT10_SelectDACPage(Bit8u function, Bit8u mode);
void INT10_GetDACPage(Bit8u* mode, Bit8u* page);
void INT10_SetPelMask(Bit8u mask);
void INT10_GetPelMask(Bit8u & mask);

// Sub Groups
void INT10_SetupRomMemory(void);

// Video Parameter Tables
Bit16u INT10_SetupVideoParameterTable(PhysPt basepos);
void INT10_SetupBasicVideoParameterTable(void);

void FinishSetMode(bool clearmem);