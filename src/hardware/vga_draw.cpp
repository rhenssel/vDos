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

static Bit8u* VGA_TEXT_Draw_Line(Bitu vidstart, Bitu line)
	{
	return TempLine;
	}

static void VGA_DrawSingleLine(Bitu /*blah*/)
	{
	RENDER_DrawLine(VGA_DrawLine(vga.draw.address, vga.draw.address_line));

	vga.draw.address_line++;
	if (vga.draw.address_line >= vga.draw.address_line_total)
		{
		vga.draw.address_line = 0;
		vga.draw.address += vga.draw.address_add;
		}
	vga.draw.lines_done++;

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
	if ((vga.crtc.vertical_retrace_end&0x30) == 0x10)
		vga.draw.vret_triggered = true;
	}

static void VGA_VerticalTimer(Bitu /*val*/)
	{
	vga.draw.delay.framestart = PIC_FullIndex();
	PIC_AddEvent(VGA_VerticalTimer, (float)vga.draw.delay.vtotal);
	
	// EGA: 82c435 datasheet: interrupt happens at display end
	// VGA: checked with scope; however disabled by default by jumper on VGA boards
	// add a little amount of time to make sure the last drawpart has already fired
	PIC_AddEvent(VGA_VertInterrupt, (float)(vga.draw.delay.vdend + 0.005));

	vga.draw.cursor.count++;															// For same blinking frequency with higher frameskip ???

	if (!RENDER_StartUpdate())															// Check if we can actually render, else skip the rest
		return;

	if (ttf.inUse)
		{
		GFX_StartUpdate();
		vga.draw.blink = ((vga.draw.blinking & time(NULL)) || !vga.draw.blinking) ? true : false;	// Eventually blink once per second
		vga.draw.cursor.address = vga.config.cursor_start*2;
		Bitu vidstart = 0;
		vidstart *= 2;
		newAttrChar = (Bit16u *)&vga.draw.linear_base[vidstart];						// Pointer to chars+attribs
		render.cache.past_y = 1;
		RENDER_EndUpdate();
		return;
		}

	vga.draw.address_line = 0;
	vga.draw.address = 0;

	switch (vga.mode)
		{
	case M_TEXT:
		vga.draw.linear_mask = 0x7fff;													// 8 pages
		vga.draw.cursor.address = vga.config.cursor_start*2;
		// Check for blinking and blinking change delay
		// If blinking is enabled, 'blink' will toggle between true and false. Otherwise it's true
		vga.draw.blink = ((vga.draw.blinking & time(NULL)) || !vga.draw.blinking) ? true : false;	// Eventually blink once per second
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
		break;
	case M_EGA:
		if (!(vga.crtc.mode_control&0x1))
			vga.draw.linear_mask &= ~0x10000;
		else
			vga.draw.linear_mask |= 0x10000;
		break;
	default:
		break;
		}

	if (vga.draw.lines_done < vga.draw.lines_total)
		{
		PIC_RemoveEvents(VGA_DrawSingleLine);
		RENDER_EndUpdate();
		}
	vga.draw.lines_done = 0;
	PIC_AddEvent(VGA_DrawSingleLine, (float)(vga.draw.delay.htotal/4.0));				// Add the draw event
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
		return;
		}

	// Calculate the FPS for this screen
	Bitu htotal, hdend, hbstart;
	Bitu vtotal, vdend, vrstart, vrend;

	htotal = vga.crtc.horizontal_total;
	hdend = vga.crtc.horizontal_display_end;
	hbstart = vga.crtc.start_horizontal_blanking;

	vtotal= vga.crtc.vertical_total | ((vga.crtc.overflow & 1) << 8);
	vdend = vga.crtc.vertical_display_end | ((vga.crtc.overflow & 2)<<7);
	vrstart = vga.crtc.vertical_retrace_start + ((vga.crtc.overflow & 0x04) << 6);
		
	// Additional bits only present on vga cards
	htotal += 3;
		
	vtotal |= (vga.crtc.overflow & 0x20) << 4;
	vtotal += 2;
	vdend |= (vga.crtc.overflow & 0x40) << 3; 
	vrstart |= ((vga.crtc.overflow & 0x80) << 2);

	htotal += 2;
	if (!htotal || !vtotal)
		return;

	hdend += 1;
	vdend += 1;

	vrend = vga.crtc.vertical_retrace_end&0xf;											// Vertical retrace
	if (vrend <= (vrstart&0xf))
		vrend += 16;
	vrend += vrstart - (vrstart&0xf);

	Bitu clock = (vga.misc_output & 0x0c) == 0 ? 25175000 : 28322000;
	clock /= 9-(vga.seq.clocking_mode&1);												// Adjust for 8 or 9 character clock mode
	if (vga.seq.clocking_mode & 0x8)													// Check for pixel doubling, master clock/2
		clock /=2;
	
//	double fps = (double)clock/(vtotal*htotal);											// The screen refresh frequency
	double fps = 50.0;
	vga.draw.delay.htotal = htotal*1000.0/clock;										// Horizontal total (that's how long a line takes with whistles and bells) in milliseconds
	vga.draw.delay.vrstart = vrstart*vga.draw.delay.htotal;								// Start and end of vertical retrace pulse
	vga.draw.delay.vrend = vrend*vga.draw.delay.htotal;
	vga.draw.delay.vdend = vdend*vga.draw.delay.htotal;									// Display end

	vga.draw.resizing = false;
	vga.draw.vret_triggered = false;

	if (hbstart < hdend)																// Check to prevent useless black areas
		hdend = hbstart;

	Bitu width = hdend;
	Bitu height = vdend;
	bool doublescan_merging = false;

	vga.draw.address_line_total = (vga.crtc.maximum_scan_line&0x1f)+1;
	switch(vga.mode)
		{
	case M_TEXT:
		// These use line_total internal
		// Doublescanning needs to be emulated by renderer doubleheight
		// EGA has no doublescanning bit at 0x80
		if (vga.crtc.maximum_scan_line&0x80)
			{
			doublescan_merging = true;													// Vga_draw only needs to draw every second line
			height /= 2;
			}
		break;
	default:
		if (vga.crtc.maximum_scan_line & 0x80)
			{
			doublescan_merging = true;													// Double scan method 1
			height /= 2;
			}
		else if (vga.draw.address_line_total == 2)										// 4,8,16?
			{
			doublescan_merging = true;													// Double scan method 2
			height /= 2;
			vga.draw.address_line_total = 1;											// Don't repeat in this case
			}
		break;
		}
	vga.draw.doublescan_merging = doublescan_merging;

	vga.draw.linear_base = vga.mem.linear;
	vga.draw.linear_mask = vga.vmemwrap-1;
	Bitu pix_per_char = 8;
	switch (vga.mode)
		{
	case M_TEXT:
		vga.draw.blocks = width;
		pix_per_char = 9;																// 9-pixel wide
		VGA_DrawLine = VGA_TEXT_Draw_Line;												// 8bpp version
		break;
	case M_VGA:
		pix_per_char = 4;
		VGA_DrawLine = VGA_Draw_Linear_Line;
		break;
	case M_EGA:
		vga.draw.blocks = width;
		VGA_DrawLine = VGA_Draw_Linear_Line;
		vga.draw.linear_base = vga.fastmem;
		vga.draw.linear_mask = (vga.vmemwrap<<1)-1;
		break;
		}
	width *= pix_per_char;
	VGA_CheckScanLength();
	
	vga.draw.lines_total = height;
	Bitu bpp = 8;																		// Set the bpp (always 8)
	vga.draw.line_length = width * ((bpp + 1) / 8);

	bool fps_changed = false;
	if (fabs(vga.draw.delay.vtotal-1000.0/fps) > 0.0001)								// Need to change the vertical timing?
		{
		fps_changed = true;
		vga.draw.delay.vtotal = 1000.0/fps;
		VGA_KillDrawing();
		PIC_RemoveEvents(VGA_VerticalTimer);
		VGA_VerticalTimer(0);
		}

	if ((width != vga.draw.width) || (height != vga.draw.height) ||
		(fabs(vga.draw.delay.vtotal-1000.0/fps) > 0.0001) ||
		fps_changed)																	// Need to resize the output window?
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
	vga.draw.lines_done = ~0;
	RENDER_EndUpdate();
	}
