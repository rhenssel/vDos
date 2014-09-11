#ifndef VDOS_VIDEO_H
#define VDOS_VIDEO_H

void GFX_Events(void);
void GFX_SetSize(Bitu width, Bitu height);

bool GFX_StartUpdate(void);
void GFX_EndUpdate(void);

#endif
