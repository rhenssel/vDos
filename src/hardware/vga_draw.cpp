#include <string.h>
#include <math.h>
#include "vDos.h"
#include "video.h"
#include "render.h"
#include "vga.h"
#include "pic.h"
#include "timer.h"
#include "time.h"

#define VGA_PARTS 4

typedef Bit8u * (* VGA_Line_Handler)(Bitu vidstart, Bitu line);

static VGA_Line_Handler VGA_DrawLine;
static Bit8u TempLine[RENDER_MAXWIDTH * 4 + 256];

static Bit8u * VGA_Draw_Linear_Line(Bitu vidstart, Bitu /*line*/)
	{
	return &vga.draw.linear_base[vidstart & vga.draw.linear_mask];
	}

static const Bit8u* VGA_Text_Memwrap(Bitu vidstart)
	{
	vidstart &= vga.draw.linear_mask;
	Bitu line_end = 2 * vga.draw.blocks;
	if ((vidstart + line_end) > vga.draw.linear_mask)
		{
		// wrapping in this line
		Bitu break_pos = (vga.draw.linear_mask - vidstart) + 1;
		// need a temporary storage - TempLine/2 is ok for a bit more than 132 columns
		memcpy(&TempLine[sizeof(TempLine)/2], &vga.mem.linear[vidstart], break_pos);
		memcpy(&TempLine[sizeof(TempLine)/2 + break_pos], &vga.mem.linear[0], line_end - break_pos);
		return &TempLine[sizeof(TempLine)/2];
		}
	return &vga.mem.linear[vidstart];
	}

// combined 8/9-dot wide text mode 8bpp line drawing function
// line is de scanline (0-15) of the text row
static Bit8u* VGA_TEXT_Draw_Line89(Bitu vidstart, Bitu line)
	{
/*
	Bit8u* draw = ((Bit8u*)TempLine);
	const Bit8u* vidmem = VGA_Text_Memwrap(vidstart);	// pointer to chars+attribs
	Bitu blocks = vga.draw.blocks;

	while (blocks--)									// for each character in the scanline
		{
		Bitu chr = *vidmem++;
		Bitu attr = *vidmem++;
		Bitu font = vga.draw.font_tables[(attr >> 3)&1][(chr<<5)+line];		// the font pattern, bit 3 eventually selects alternatvive font
		
		Bitu background = attr >> 4;
		// if blinking is enabled bit7 is not mapped to attributes
		if (vga.draw.blinking)
			background &= 7;
		// choose foreground color if blinking not set for this cell or blink on
		Bitu foreground = (vga.draw.blink || (!(attr&0x80))) ? (attr&0xf) : background;
		// underline: all foreground [freevga: 0x77, previous 0x7]
		if (((attr&0x77) == 0x01) && (vga.crtc.underline_location&0x1f) == line)
			background = foreground;
		if (!font)					// shortcut if all pixels off
			{
			memset(draw, background, 9);
			draw += 9;
			}
		else
			{
			font <<= 1; // 9 pixels
			// extend to the 9th pixel if needed
			if ((font&0x2) && (vga.attr.mode_control&0x04) && (chr >= 0xc0) && (chr <= 0xdf))
				font |= 1;
			for (Bitu n = 0; n < 9; n++)
				{
				*draw++ = (font&0x100) ? foreground : background;
				font <<= 1;
				}
			}
		}

	// draw the text mode cursor if needed
	if ((vga.draw.cursor.count&0x8) && (line >= vga.draw.cursor.sline) && (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled)
// Removed blinking 
//	if ((line >= vga.draw.cursor.sline) && (line <= vga.draw.cursor.eline) && vga.draw.cursor.enabled)
		{
		// the adress of the attribute that makes up the cell the cursor is in
		Bits attr_addr = (vga.draw.cursor.address-vidstart) >> 1;
		if (attr_addr >= 0 && attr_addr < (Bits)vga.draw.blocks)
			memset(&TempLine[attr_addr * 9], vga.mem.linear[vga.draw.cursor.address+1] & 0xf, 8);
		}
*/
	return TempLine;
	}

static void VGA_ProcessSplit()
	{
	if (vga.attr.mode_control&0x20)
		{
		vga.draw.address = 0;
		// reset panning to 0 here so we don't have to check for 
		// it in the character draw functions. It will be set back
		// to its proper value in v-retrace
		}
	else
		{
		// In text mode only the characters are shifted by panning, not the address;
		// this is done in the text line draw function.
		vga.draw.address = vga.draw.byte_panning_shift*vga.draw.bytes_skip;
		}
	vga.draw.address_line = 0;
	}


static void VGA_DrawSingleLine(Bitu /*blah*/)
	{
	if (vga.attr.disabled)
		{
		memset(TempLine, 0, sizeof(TempLine));
		RENDER_DrawLine(TempLine);
		}
	else
		RENDER_DrawLine(VGA_DrawLine(vga.draw.address, vga.draw.address_line));

	vga.draw.address_line++;
	if (vga.draw.address_line >= vga.draw.address_line_total)
		{
		vga.draw.address_line = 0;
		vga.draw.address += vga.draw.address_add;
		}
	vga.draw.lines_done++;

	if (vga.draw.split_line == vga.draw.lines_done)
		VGA_ProcessSplit();
	if (vga.draw.lines_done < vga.draw.lines_total)
		PIC_AddEvent(VGA_DrawSingleLine, (float)vga.draw.delay.singleline_delay);
	else
		RENDER_EndUpdate();
	}

void VGA_SetBlinking(Bitu enabled)
	{
	if (enabled)
		{
		vga.draw.blinking = 1;
		vga.attr.mode_control |= 0x08;
		}
	else
		{
		vga.draw.blinking = 0;
		vga.attr.mode_control &= ~0x08;
		}
	}

static void VGA_VertInterrupt(Bitu /*val*/)
	{
	if ((!vga.draw.vret_triggered) && ((vga.crtc.vertical_retrace_end&0x30) == 0x10))
		vga.draw.vret_triggered = true;
	}

static void VGA_DisplayStartLatch(Bitu /*val*/)
	{
	vga.config.real_start = vga.config.display_start & (vga.vmemwrap-1);
	vga.draw.bytes_skip = vga.config.bytes_skip;
	}

static void VGA_VerticalTimer(Bitu /*val*/)
	{
	vga.draw.delay.framestart = PIC_FullIndex();
	PIC_AddEvent(VGA_VerticalTimer, (float)vga.draw.delay.vtotal);
	
	PIC_AddEvent(VGA_DisplayStartLatch, (float)vga.draw.delay.vrstart);
	// EGA: 82c435 datasheet: interrupt happens at display end
	// VGA: checked with scope; however disabled by default by jumper on VGA boards
	// add a little amount of time to make sure the last drawpart has already fired
	PIC_AddEvent(VGA_VertInterrupt, (float)(vga.draw.delay.vdend + 0.005));

	// for same blinking frequency with higher frameskip ???
	vga.draw.cursor.count++;

	// Check if we can actually render, else skip the rest
	if (!RENDER_StartUpdate())
		return;

	if (ttf.inUse)
		{
		GFX_StartUpdate();
		vga.draw.blink = ((vga.draw.blinking & time(NULL)) || !vga.draw.blinking) ? true : false;	// eventually blink once per second
		vga.draw.cursor.address = vga.config.cursor_start*2;
		Bitu vidstart = vga.config.real_start + vga.draw.bytes_skip;
		vidstart *= 2;
		const Bit8u* vidmem = &vga.draw.linear_base[vidstart];		// pointer to chars+attribs
		Bit8u* draw = ((Bit8u*)newAttrChar);
		for (Bitu blocks = ttf.cols * ttf.lins; blocks; blocks--)
			{
			*draw++ = *vidmem++;
			Bitu attr = *vidmem++;
		
			Bitu background = attr >> 4;
			if (vga.draw.blinking)									// if blinking is enabled bit7 is not mapped to attributes
				background &= 7;
			// choose foreground color if blinking not set for this cell or blink on
			Bitu foreground = (vga.draw.blink || (!(attr&0x80))) ? (attr&0xf) : background;
			// How about underline?
			*draw++ = (background<<4) + foreground;
			}
		render.cache.past_y = 1;
		RENDER_EndUpdate();
		return;
		}

	vga.draw.address_line = vga.config.hlines_skip;
	vga.draw.split_line = vga.config.line_compare+1;
	if (vga.draw.doublescan_merging)
		vga.draw.split_line /= 2;
	vga.draw.address = vga.config.real_start;
	vga.draw.byte_panning_shift = 0;

	switch (vga.mode)
		{
	case M_TEXT:
		vga.draw.byte_panning_shift = 2;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.linear_mask = 0x7fff;						// 8 pages
		vga.draw.cursor.address = vga.config.cursor_start*2;
		vga.draw.address *= 2;

		// check for blinking and blinking change delay
		// if blinking is enabled, 'blink' will toggle between true and false. Otherwise it's true
		vga.draw.blink = ((vga.draw.blinking & time(NULL)) || !vga.draw.blinking) ? true : false;	// eventually blink once per second
		break;
	case M_VGA:
		if (vga.crtc.underline_location & 0x40)
			{
			vga.draw.linear_base = vga.fastmem;
			vga.draw.linear_mask = 0xffff;
			}
		else
			{
			vga.draw.linear_base = vga.mem.linear;
			vga.draw.linear_mask = vga.vmemwrap - 1;
			}
		vga.draw.byte_panning_shift = 4;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		break;
	case M_EGA:
		if (!(vga.crtc.mode_control&0x1))
			vga.draw.linear_mask &= ~0x10000;
		else
			vga.draw.linear_mask |= 0x10000;
		vga.draw.byte_panning_shift = 8;
		vga.draw.address += vga.draw.bytes_skip;
		vga.draw.address *= vga.draw.byte_panning_shift;
		break;
	case M_CGA4:
	case M_CGA2:
		vga.draw.address = (vga.draw.address*2)&0x1fff;
		break;
	default:
		break;
		}
	if (vga.draw.split_line == 0)
		VGA_ProcessSplit();

	// add the draw event
	if (vga.draw.lines_done < vga.draw.lines_total)
		{
		PIC_RemoveEvents(VGA_DrawSingleLine);
		RENDER_EndUpdate();
		}
	vga.draw.lines_done = 0;
	PIC_AddEvent(VGA_DrawSingleLine, (float)(vga.draw.delay.htotal/4.0));
	}

void VGA_CheckScanLength(void)
	{
	switch (vga.mode)
		{
	case M_TEXT:
		vga.draw.address_add = vga.config.scan_len*4;
		return;
	case M_VGA:
		vga.draw.address_add = vga.config.scan_len*8;
		return;
	case M_EGA:
		vga.draw.address_add = vga.config.scan_len*16;
		return;
	case M_CGA2:
	case M_CGA4:
		vga.draw.address_add = 80;
		return;
	default:
		vga.draw.address_add = vga.draw.blocks*8;
		return;
		}
	}

void VGA_SetupDrawing(Bitu /*val*/)
	{
	if (vga.mode == M_ERROR)
		{
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		return;
		}

	// Calculate the FPS for this screen
	Bitu htotal, hdend, hbstart, hbend, hrstart, hrend;
	Bitu vtotal, vdend, vbstart, vbend, vrstart, vrend;
	Bitu hbend_mask, vbend_mask;

	htotal = vga.crtc.horizontal_total;
	hdend = vga.crtc.horizontal_display_end;
	hbend = vga.crtc.end_horizontal_blanking&0x1F;
	hbstart = vga.crtc.start_horizontal_blanking;
	hrstart = vga.crtc.start_horizontal_retrace;

	vtotal= vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8);
	vdend = vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2)<<7);
	vbstart = vga.crtc.start_vertical_blanking | ((vga.crtc.overflow & 0x08) << 5);
	vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6);
		
	// additional bits only present on vga cards
	htotal += 3;
	hbend |= (vga.crtc.end_horizontal_retrace&0x80) >> 2;
	hbend_mask = 0x3f;
		
	vtotal |= (vga.crtc.overflow & 0x20) << 4;
	vtotal += 2;
	vdend |= (vga.crtc.overflow & 0x40) << 3; 
	vbstart |= (vga.crtc.maximum_scan_line & 0x20) << 4;
	vrstart |= ((vga.crtc.overflow & 0x80) << 2);
	vbend_mask = 0xff;

	htotal += 2;
	if (!htotal || !vtotal)
		return;

	hdend += 1;
	vdend += 1;

	// horitzontal blanking
	if (hbend <= (hbstart & hbend_mask))
		hbend += hbend_mask + 1;
	hbend += hbstart - (hbstart & hbend_mask);

	// horizontal retrace
	hrend = vga.crtc.end_horizontal_retrace & 0x1f;
	if (hrend <= (hrstart&0x1f))
		hrend += 32;
	hrend += hrstart - (hrstart&0x1f);
	if (hrend > hbend)
		hrend = hbend; // S3 BIOS (???)
		
	// vertical retrace
	vrend = vga.crtc.vertical_retrace_end & 0xf;
	if (vrend <= (vrstart&0xf))
		vrend += 16;
	vrend += vrstart - (vrstart&0xf);

	// vertical blanking
	vbend = vga.crtc.end_vertical_blanking & vbend_mask;
	if (vbstart != 0)
		{
		// Special case vbstart==0:
		// Most graphics cards agree that lines zero to vbend are
		// blanked. ET4000 doesn't blank at all if vbstart==vbend.
		// ET3000 blanks lines 1 to vbend (255/6 lines).
		vbstart += 1;
		if (vbend <= (vbstart & vbend_mask))
			vbend += vbend_mask + 1;
		vbend += vbstart - (vbstart & vbend_mask);
		}
	vbend++;

	Bitu clock = (vga.misc_output & 0x0c) == 0 ? 25175000 : 28322000;
	clock /= 9-(vga.seq.clocking_mode&1);						// Adjust for 8 or 9 character clock mode
	// Check for pixel doubling, master clock/2
	if (vga.seq.clocking_mode & 0x8)
		clock /=2;
	
	// The screen refresh frequency
	double fps = (double)clock/(vtotal*htotal);

	// Horizontal total (that's how long a line takes with whistles and bells)
	vga.draw.delay.htotal = htotal*1000.0/clock; //in milliseconds
	// Start and End of horizontal blanking
	vga.draw.delay.hblkstart = hbstart*1000.0/clock; //in milliseconds
	vga.draw.delay.hblkend = hbend*1000.0/clock; 
	// Start and End of horizontal retrace
	vga.draw.delay.hrstart = hrstart*1000.0/clock;
	vga.draw.delay.hrend = hrend*1000.0/clock;
	// Start and End of vertical blanking
	vga.draw.delay.vblkstart = vbstart * vga.draw.delay.htotal;
	vga.draw.delay.vblkend = vbend * vga.draw.delay.htotal;
	// Start and End of vertical retrace pulse
	vga.draw.delay.vrstart = vrstart * vga.draw.delay.htotal;
	vga.draw.delay.vrend = vrend * vga.draw.delay.htotal;

	// Display end
	vga.draw.delay.vdend = vdend * vga.draw.delay.htotal;

	vga.draw.parts_total = VGA_PARTS;
	vga.draw.delay.parts = vga.draw.delay.vdend/vga.draw.parts_total;
	vga.draw.resizing = false;
	vga.draw.vret_triggered = false;

	// Check to prevent useless black areas
	if (hbstart < hdend)
		hdend = hbstart;

	Bitu width = hdend;
	Bitu height = vdend;
	bool doublescan_merging = false;

	vga.draw.address_line_total = (vga.crtc.maximum_scan_line&0x1f)+1;
	switch(vga.mode)
		{
	case M_CGA2:
	case M_CGA4:
	case M_TEXT:
		// these use line_total internal
		// doublescanning needs to be emulated by renderer doubleheight
		// EGA has no doublescanning bit at 0x80
		if (vga.crtc.maximum_scan_line&0x80)
			{
			// vga_draw only needs to draw every second line
			doublescan_merging = true;
			height /= 2;
			}
		break;
	default:
		if (vga.crtc.maximum_scan_line & 0x80)
			{
			doublescan_merging = true;			// double scan method 1
			height /= 2;
			}
		else if (vga.draw.address_line_total == 2)
			{ // 4,8,16?
			// double scan method 2
			doublescan_merging = true;
			height /= 2;
			vga.draw.address_line_total = 1;	// don't repeat in this case
			}
		break;
		}
	vga.draw.doublescan_merging = doublescan_merging;

	vga.draw.linear_base = vga.mem.linear;
	vga.draw.linear_mask = vga.vmemwrap - 1;
	Bitu pix_per_char = 8;
	switch (vga.mode)
		{
	case M_TEXT:
		vga.draw.blocks = width;
		pix_per_char = 9;						// 9-pixel wide
		VGA_DrawLine = VGA_TEXT_Draw_Line89;	// 8bpp version
		break;
	case M_VGA:
		pix_per_char = 4;
		VGA_DrawLine = VGA_Draw_Linear_Line;
		break;
	case M_EGA:
		vga.draw.blocks = width;
		VGA_DrawLine = VGA_Draw_Linear_Line;
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap<<1) - 1;
		break;
	default:
		LOG(LOG_VGA,LOG_ERROR)("Unhandled VGA mode %d while checking for resolution", vga.mode);
		break;
		}
	width *= pix_per_char;
	VGA_CheckScanLength();
	
	vga.draw.lines_total = height;
	vga.draw.parts_lines = vga.draw.lines_total/vga.draw.parts_total;
	Bitu bpp = 8;								// Set the bpp (always 8)
	vga.draw.line_length = width * ((bpp + 1) / 8);

	bool fps_changed = false;
	// need to change the vertical timing?
	if (fabs(vga.draw.delay.vtotal - 1000.0 / fps) > 0.0001)
		{
		fps_changed = true;
		vga.draw.delay.vtotal = 1000.0 / fps;
		VGA_KillDrawing();
		PIC_RemoveEvents(VGA_VerticalTimer);
		PIC_RemoveEvents(VGA_DisplayStartLatch);
		VGA_VerticalTimer(0);
		}

	// need to resize the output window?
	if ((width != vga.draw.width) || (height != vga.draw.height) ||
		(fabs(vga.draw.delay.vtotal - 1000.0 / fps) > 0.0001) ||
		fps_changed)
		{
		VGA_KillDrawing();
		vga.draw.width = width;
		vga.draw.height = height;
		RENDER_SetSize(width, height);
		}
	if (doublescan_merging)
		vga.draw.delay.singleline_delay = (float)(vga.draw.delay.htotal*2.0);
	else
		vga.draw.delay.singleline_delay = (float)vga.draw.delay.htotal;
	}

void VGA_KillDrawing(void)
	{
	PIC_RemoveEvents(VGA_DrawSingleLine);
	vga.draw.parts_left = 0;
	vga.draw.lines_done = ~0;
	RENDER_EndUpdate();
	}
