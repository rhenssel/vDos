#include "vDos.h"
#include "video.h"
#include "render.h"
#include "wchar.h"
#include "..\..\SDL-1.2.15\include\SDL_syswm.h"

Render_t render;
ScalerLineHandler_t RENDER_DrawLine;

Bit8u rendererCache[RENDER_MAXHEIGHT * RENDER_MAXWIDTH];

Bit16u curAttrChar[txtMaxLins*txtMaxCols];											// Current displayed textpage
Bit16u *newAttrChar;																// To be replaced by

void SimpleRenderer(const void *s)
	{
	render.cache.curr_y++;
	if (render.cache.invalid)
		{
		wmemcpy((wchar_t*)render.cache.pointer, (wchar_t*)s, render.cache.width>>1);
		render.cache.past_y = render.cache.curr_y;
		render.cache.start_x = 0;
		}
	else
		{
		Bit32u *cache = (Bit32u *)render.cache.pointer;
		Bit32u *source = (Bit32u *)s;
		for (Bitu index = render.cache.width/4; index; index--)
			{
			if (*source != *cache)
				{
				index *= 4;
				if ((Bitu)render.cache.start_x > render.cache.width - index)
					render.cache.start_x = render.cache.width - index;
				wmemcpy((wchar_t*)cache, (wchar_t*)source, index>>1);
				render.cache.past_y = render.cache.curr_y;
				break;
				}	
			source++;
			cache++;
			}
		}
	render.cache.pointer += render.cache.width;
	}

static void RENDER_EmptyLineHandler(const void * src)
	{
	}

static void RENDER_StartLineHandler(const void * s)
	{
	if (s)
		{
		if (wmemcmp((wchar_t*)render.cache.pointer, (wchar_t*)s, render.cache.width>>1))
			{
			if (!GFX_StartUpdate())
				{
				RENDER_DrawLine = RENDER_EmptyLineHandler;
				return;
				}
			render.cache.start_y = render.cache.past_y = render.cache.curr_y;
			RENDER_DrawLine = SimpleRenderer;
			RENDER_DrawLine(s);
			return;
			}
		}
	render.cache.pointer += render.cache.width;
	render.cache.curr_y++;
	}

static void RENDER_FinishLineHandler(const void * s)
	{
	if (s)
		wmemcpy((wchar_t*)render.cache.pointer, (wchar_t*)s, render.cache.width>>1);
	render.cache.pointer += render.cache.width;
	}

bool RENDER_StartUpdate(void)
	{
	if (render.updating || !render.active)
		return false;

	render.cache.pointer = (Bit8u*)&rendererCache;
	render.cache.start_x = render.cache.width;
	render.cache.past_x = render.cache.width;
	render.cache.start_y = 0;
	render.cache.past_y = 0;
	render.cache.curr_y = 0;

	if (render.cache.nextInvalid)													// Always do a full screen update
		{
		render.cache.nextInvalid = false;
		render.cache.invalid = true;
		if (!GFX_StartUpdate())
			return false;
		RENDER_DrawLine = SimpleRenderer;
		}
	else
		RENDER_DrawLine = RENDER_StartLineHandler;
	render.updating = true;
	return true;
	}

void RENDER_Halt(void)
	{
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	GFX_EndUpdate();
	render.updating = false;
	render.active = false;
	}

void RENDER_EndUpdate()
	{
	if (!render.updating)
		return;
	RENDER_DrawLine = RENDER_EmptyLineHandler;
	if (render.cache.start_y != render.cache.past_y)
		GFX_EndUpdate();
	render.cache.invalid = false;
	render.updating = false;
	}

void RENDER_Reset(void)
	{
	GFX_SetSize(render.cache.width, render.cache.height);							// Setup the scaler variables

	RENDER_DrawLine = RENDER_FinishLineHandler;										// Finish this frame using a copy only handler
	
	render.cache.nextInvalid = true;												// Signal the next frame to first reinit the cache
	render.active = true;
	}

void RENDER_SetSize(Bitu width, Bitu height)
	{
	RENDER_Halt();
	if (!width || !height || width > RENDER_MAXWIDTH || height > RENDER_MAXHEIGHT)
		return;
	render.cache.width	= width;
	render.cache.height	= height;
	RENDER_Reset();
	}

void RENDER_Init()
	{
	render.updating = true;
	}
