#ifndef __XMS_H__
#define __XMS_H__

Bitu	XMS_QueryFreeMemory		(Bit16u& largestFree, Bit16u& totalFree);
Bitu	XMS_AllocateMemory		(Bitu size, Bit16u& handle);
Bitu	XMS_FreeMemory			(Bitu handle);
Bitu	XMS_MoveMemory			(PhysPt bpt);
Bitu	XMS_LockMemory			(Bitu handle, Bit32u& address);
Bitu	XMS_UnlockMemory		(Bitu handle);
Bitu	XMS_GetHandleInformation(Bitu handle, Bit8u& lockCount, Bit8u& numFree, Bit16u& size);
Bitu	XMS_ResizeMemory		(Bitu handle, Bitu newSize);

Bitu	XMS_EnableA20			(bool enable);
Bitu	XMS_GetEnabledA20		(void);

#endif
