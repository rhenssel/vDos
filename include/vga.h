#ifndef VDOS_VGA_H
#define VDOS_VGA_H

#ifndef VDOS_H
#include "vDos.h"
#endif

class PageHandler;

enum VGAModes {
	M_EGA, M_VGA,
	M_TEXT,
	M_ERROR
};

typedef struct {
	bool attrindex;
} VGA_Internal;

typedef struct {
	// Video drawing
	Bitu display_start;
	bool retrace;																	// A retrace is active
	Bitu scan_len;
	Bitu cursor_start;

	// Some other screen related variables
//	Bitu line_compare;
	bool chained;																	// Enable or Disabled Chain 4 Mode

	// Specific stuff memory write/read handling
	Bit8u read_mode;
	Bit8u write_mode;
	Bit8u read_map_select;
	Bit8u color_dont_care;
	Bit8u color_compare;
	Bit8u data_rotate;
	Bit8u raster_op;

	Bit32u full_bit_mask;
	Bit32u full_map_mask;
	Bit32u full_not_map_mask;
	Bit32u full_set_reset;
	Bit32u full_not_enable_set_reset;
	Bit32u full_enable_set_reset;
	Bit32u full_enable_and_set_reset;
} VGA_Config;

typedef struct {
	bool resizing;
	Bitu width;
	Bitu height;
	Bitu blocks;
	Bitu address;
	Bit8u *linear_base;
	Bitu linear_mask;
	Bitu address_add;
	Bitu line_length;
	Bitu address_line_total;
	Bitu address_line;
	Bitu lines_total;
	Bitu lines_done;
	struct {
		double framestart;
		double vrstart, vrend;														// V-retrace
		double vdend, vtotal;
		double htotal;
		float singleline_delay;
	} delay;
	bool doublescan_merging;
	Bit8u font[64*1024];
	Bit8u * font_tables[2];
	Bitu blinking;
	bool blink;
	struct {
		Bitu address;
		Bit8u sline, eline;
		Bit8u count, delay;
		Bit8u enabled;
	} cursor;
	bool vret_triggered;
} VGA_Draw;

typedef struct {
	Bit8u index;
	Bit8u reset;
	Bit8u clocking_mode;
	Bit8u map_mask;
	Bit8u character_map_select;
	Bit8u memory_mode;
} VGA_Seq;

typedef struct {
	Bit8u palette[16];
	Bit8u mode_control;
	Bit8u horizontal_pel_panning;
	Bit8u color_plane_enable;
	Bit8u color_select;
	Bit8u index;
} VGA_Attr;

typedef struct {
	Bit8u horizontal_total;
	Bit8u horizontal_display_end;
	Bit8u start_horizontal_blanking;
	Bit8u end_horizontal_blanking;
	Bit8u start_horizontal_retrace;
	Bit8u end_horizontal_retrace;
	Bit8u vertical_total;
	Bit8u overflow;
	Bit8u preset_row_scan;
	Bit8u maximum_scan_line;
	Bit8u cursor_start;
	Bit8u cursor_end;
	Bit8u start_address_high;
	Bit8u start_address_low;
	Bit8u cursor_location_high;
	Bit8u cursor_location_low;
	Bit8u vertical_retrace_start;
	Bit8u vertical_retrace_end;
	Bit8u vertical_display_end;
	Bit8u offset;
	Bit8u underline_location;
	Bit8u start_vertical_blanking;
	Bit8u end_vertical_blanking;
	Bit8u mode_control;
	Bit8u line_compare;
	Bit8u index;
} VGA_Crtc;

typedef struct {
	Bit8u index;
	Bit8u set_reset;
	Bit8u enable_set_reset;
	Bit8u color_compare;
	Bit8u data_rotate;
	Bit8u read_map_select;
	Bit8u mode;
	Bit8u miscellaneous;
	Bit8u color_dont_care;
	Bit8u bit_mask;
} VGA_Gfx;

typedef struct  {
	Bit8u red;
	Bit8u green;
	Bit8u blue;
} RGBEntry;

typedef struct {
//	Bit8u bits;						/* DAC bits, usually 6 or 8 */
	Bit8u pel_mask;
	Bit8u pel_index;	
	Bit8u state;
	Bit8u write_index;
	Bit8u read_index;
//	Bitu first_changed;
	Bit8u combine[16];
	RGBEntry rgb[0x100];
	Bit16u xlat16[256];
	Bit8u hidac_counter;
	Bit8u reg02;
} VGA_Dac;

typedef union {
	Bit32u d;
	Bit8u b[4];
} VGA_Latch;

typedef struct {
	Bit8u* linear;
	Bit8u* linear_orgptr;
} VGA_Memory;

typedef struct {
	VGAModes mode;			// The mode the vga system is in
	Bit8u misc_output;
	VGA_Draw draw;
	VGA_Config config;
	VGA_Internal internal;
// Internal module groups
	VGA_Seq seq;
	VGA_Attr attr;
	VGA_Crtc crtc;
	VGA_Gfx gfx;
	VGA_Dac dac;
	VGA_Latch latch;
	VGA_Memory mem;
	Bit32u vmemwrap;		// this is assumed to be power of 2
	Bit8u* fastmem;			// memory for fast (usually 16-color) rendering, always twice as big as vmemsize
	Bit8u* fastmem_orgptr;
	Bit32u vmemsize;
} VGA_Type;

// Functions for different resolutions
void VGA_SetMode(VGAModes mode);
void VGA_DetermineMode(void);
void VGA_SetupHandlers(void);
void VGA_StartResize(Bitu delay=50);
void VGA_SetupDrawing(Bitu val);
void VGA_CheckScanLength(void);

// Some DAC/Attribute functions
void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal);
void VGA_ATTR_SetPalette(Bit8u index,Bit8u val);

// The VGA Subfunction startups
void VGA_SetupAttr(void);
void VGA_SetupMemory();
void VGA_SetupDAC(void);
void VGA_SetupCRTC(void);
void VGA_SetupMisc(void);
void VGA_SetupGFX(void);
void VGA_SetupSEQ(void);
void VGA_SetupOther(void);

// Some Support Functions
void VGA_SetBlinking(Bitu enabled);
void VGA_KillDrawing(void);

extern VGA_Type vga;

extern Bit32u ExpandTable[256];
extern Bit32u FillTable[16];
extern Bit32u Expand16Table[4][16];


#endif
