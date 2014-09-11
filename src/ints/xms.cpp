#include <stdlib.h>
#include "vDos.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "xms.h"

#define XMS_HANDLES								50						// 50 XMS Memory Blocks

#define	XMS_GET_VERSION							0x00
#define	XMS_ALLOCATE_HIGH_MEMORY				0x01
#define	XMS_FREE_HIGH_MEMORY					0x02
#define	XMS_GLOBAL_ENABLE_A20					0x03
#define	XMS_GLOBAL_DISABLE_A20					0x04
#define	XMS_LOCAL_ENABLE_A20					0x05
#define	XMS_LOCAL_DISABLE_A20					0x06
#define	XMS_QUERY_A20							0x07
#define	XMS_QUERY_FREE_EXTENDED_MEMORY			0x08
#define	XMS_ALLOCATE_EXTENDED_MEMORY			0x09
#define	XMS_FREE_EXTENDED_MEMORY				0x0a
#define	XMS_MOVE_EXTENDED_MEMORY_BLOCK			0x0b
#define	XMS_LOCK_EXTENDED_MEMORY_BLOCK			0x0c
#define	XMS_UNLOCK_EXTENDED_MEMORY_BLOCK		0x0d
#define	XMS_GET_EMB_HANDLE_INFORMATION			0x0e
#define	XMS_RESIZE_EXTENDED_MEMORY_BLOCK		0x0f
#define	XMS_ALLOCATE_UMB						0x10
#define	XMS_DEALLOCATE_UMB						0x11

#define	XMS_FUNCTION_NOT_IMPLEMENTED			0x80
#define	HIGH_MEMORY_NOT_EXIST					0x90
#define	HIGH_MEMORY_IN_USE						0x91
#define	HIGH_MEMORY_NOT_ALLOCATED				0x93
#define XMS_OUT_OF_SPACE						0xa0
#define XMS_OUT_OF_HANDLES						0xa1
#define XMS_INVALID_HANDLE						0xa2
#define XMS_INVALID_SOURCE_HANDLE				0xa3
#define XMS_INVALID_SOURCE_OFFSET				0xa4
#define XMS_INVALID_DEST_HANDLE					0xa5
#define XMS_INVALID_DEST_OFFSET					0xa6
#define XMS_INVALID_LENGTH						0xa7
#define XMS_BLOCK_NOT_LOCKED					0xaa
#define XMS_BLOCK_LOCKED						0xab
#define XMS_LOCK_COUNT_OVERFLOW					0xac
#define	UMB_ONLY_SMALLER_BLOCK					0xb0
#define	UMB_NO_BLOCKS_AVAILABLE					0xb1

struct XMS_Handle {
	Bit32u		addr;												// Physical memory address
	Bit16u		size;												// Size in KB's
	Bit8u		locked;
	bool		free;
};
static XMS_Handle xms_handles[XMS_HANDLES];

RealPt xms_callback;
static bool umb_available;


static bool InvalidXMSHandle(Bit16u handle)
	{
	return (!handle || (handle >= XMS_HANDLES) || xms_handles[handle].free);
	}

// XMS_Defrag totally neglects memory blocks being blocked (unmovable)
// And why shouldn't it, accessing these blocks directly isn't possible in real mode?
// What with protected mode???
// Don't move locked blocks and find make thing more complicated, largest free block, etc???
Bit16u XMS_Defrag(Bit16u maxHandle)									// Defrag XMS memory, maximize specific handle (0 = n/a) and return free space in KB
	{
	Bit16u sortedHandles[XMS_HANDLES];
	int numHandles = 0;
	for (int i = 1; i < XMS_HANDLES; i++)
		if (!xms_handles[i].free)
			sortedHandles[numHandles++] = i;
	if (!numHandles)												// If no handles in use, return all XMS memory as free
		return TOT_MEM_BYTES/1024-1024-64;							// First 64KB of extended memory is not used as XMS

	for (int i = 0; i < numHandles-1; i++)							// Sort the handles in order of memory address
		for (int j = 0; j < numHandles-1-i; j++)
			if (xms_handles[sortedHandles[j]].addr > xms_handles[sortedHandles[j+1]].addr)
				{
				Bit16u temp = sortedHandles[j+1];
				sortedHandles[j+1] = sortedHandles[j];
				sortedHandles[j] = temp;
				}

	Bit32u lowAddr = 1024*(1024+64);
	for (int i = 0; i < numHandles; i++)							// Defrag all or upto maxHandle compacting up from start
		{
		XMS_Handle * handlePtr = &xms_handles[sortedHandles[i]];
		if (handlePtr->addr > lowAddr)
			{
			wmemmove((wchar_t *)(MemBase+lowAddr), (wchar_t *)(MemBase+handlePtr->addr), handlePtr->size*1024/2);
			handlePtr->addr = lowAddr;
			}
		lowAddr += handlePtr->size*1024;
		if (sortedHandles[i] == maxHandle)
			break;
		}
	Bit32u highAddr = TOT_MEM_BYTES;
	if (maxHandle)
		for (int i = numHandles-1; i; i--)							// Defrag all beyond maxHandle compacting down from end
			{
			if (sortedHandles[i] == maxHandle)
				break;
			XMS_Handle * handlePtr = &xms_handles[sortedHandles[i]];
			highAddr -= handlePtr->size*1024;
			if (handlePtr->addr < highAddr)
				{
				wmemmove((wchar_t *)(MemBase+highAddr), (wchar_t *)(MemBase+handlePtr->addr), handlePtr->size*1024/2);
				handlePtr->addr = highAddr;
				}
			}
	return (Bit16u)((highAddr-lowAddr)/1024);
	}

Bit8u XMS_QueryFreeMemory(Bit16u& freeXMS)
	{
	freeXMS = XMS_Defrag(0);
	if (!freeXMS)
		return XMS_OUT_OF_SPACE;
	return 0;
	}

Bit8u XMS_AllocateMemory(Bit16u size, Bit16u& handle)
	{	
	Bit16u index = 1;												// Size in KB, find free handle
	while (!xms_handles[index].free)
		if (++index == XMS_HANDLES)
			return XMS_OUT_OF_HANDLES;
	Bit16u freeSpace = XMS_Defrag(0);
	if (freeSpace < size)
		return XMS_OUT_OF_SPACE;
	XMS_Handle * handlePtr = &xms_handles[index];
	handlePtr->addr = (TOT_MEM_KB-freeSpace)*1024;					// After XMS_Defrag(0) blocks are compacted down
	handlePtr->size = size;
	handlePtr->free = false;
	handlePtr->locked = 0;
	handle = index;
	return 0;
	}

static Bit8u XMS_FreeMemory(Bit16u handle)
	{
	if (InvalidXMSHandle(handle))
		return XMS_INVALID_HANDLE;
	if (xms_handles[handle].locked)
		return XMS_BLOCK_LOCKED;
	xms_handles[handle].free = true;
	return 0;
	}

static Bit8u XMS_MoveMemory(PhysPt bpt)
	{
	Bitu length = vPC_rLodsd(bpt);
	if (length&1)
		return XMS_INVALID_LENGTH;
	Bit16u src_handle = vPC_rLodsw(bpt+4);
	Bit32u src_offset = vPC_rLodsd(bpt+6);
	Bit16u dest_handle = vPC_rLodsw(bpt+10);
	Bit32u dest_offset = vPC_rLodsd(bpt+12);
	PhysPt srcpt, destpt;
	if (src_handle)
		{
		if (InvalidXMSHandle(src_handle))
			return XMS_INVALID_SOURCE_HANDLE;
		if (src_offset >= xms_handles[src_handle].size*1024U)
			return XMS_INVALID_SOURCE_OFFSET;
		if (src_offset+length > xms_handles[src_handle].size*1024U)
			return XMS_INVALID_LENGTH;
		srcpt = xms_handles[src_handle].addr+src_offset;
		}
	else
		srcpt = dWord2Ptr(src_offset);
	if (dest_handle)
		{
		if (InvalidXMSHandle(dest_handle))
			return XMS_INVALID_DEST_HANDLE;
		if (dest_offset >= xms_handles[dest_handle].size*1024U)
			return XMS_INVALID_DEST_OFFSET;
		if (dest_offset+length > xms_handles[dest_handle].size*1024U)
			return XMS_INVALID_LENGTH;
		destpt = xms_handles[dest_handle].addr+dest_offset;
		}
	else
		destpt = dWord2Ptr(dest_offset);
	if ((srcpt+length <= 0xa0000 || srcpt > 0x100000) && (destpt+length <= 0xa0000 || destpt > 0x100000))
		wmemmove((wchar_t *)(MemBase+destpt), (wchar_t *)(MemBase+srcpt), length/2);
	else
		vPC_rMovsw(destpt, srcpt, length/2);
	return 0;
	}

static Bit8u XMS_LockMemory(Bit16u handle, Bit32u& address)
	{
	if (InvalidXMSHandle(handle))
		return XMS_INVALID_HANDLE;
	if (xms_handles[handle].locked == 255)
		return XMS_LOCK_COUNT_OVERFLOW;
	xms_handles[handle].locked++;
	address = xms_handles[handle].addr;
	return 0;
	}

static Bit8u XMS_UnlockMemory(Bit16u handle)
	{
 	if (InvalidXMSHandle(handle))
		return XMS_INVALID_HANDLE;
	if (xms_handles[handle].locked == 0)
		return XMS_BLOCK_NOT_LOCKED;
	xms_handles[handle].locked--;
	return 0;
	}

static Bit8u XMS_GetHandleInformation(Bit16u handle, Bit8u& lockCount, Bit8u& numFree, Bit16u& size)
	{
	if (InvalidXMSHandle(handle))
		return XMS_INVALID_HANDLE;
	numFree = 0;														// Count available handles
	for (Bitu i = 1; i < XMS_HANDLES; i++)
		if (xms_handles[i].free)
			numFree++;
	lockCount = xms_handles[handle].locked;
	size = xms_handles[handle].size;
	return 0;
	}

static Bit8u XMS_ResizeMemory(Bit16u handle, Bit16u newSize)
	{
	if (InvalidXMSHandle(handle))
		return XMS_INVALID_HANDLE;	
	if (xms_handles[handle].locked)									// Handle has to be unlocked
		return XMS_BLOCK_LOCKED;
	if (newSize > xms_handles[handle].size)							// If expanding, check available memory
		if (newSize > xms_handles[handle].size+XMS_Defrag(handle))
			return XMS_OUT_OF_SPACE;
	xms_handles[handle].size = newSize;
	return 0;
	}

static void SET_RESULT(Bitu res, bool touch_bl_on_succes = true)
	{
	if (touch_bl_on_succes || res)
		reg_bl = (Bit8u)res;
	reg_ax = (res == 0);
	}

Bitu XMS_Handler(void)
	{
	switch (reg_ah)
		{
	case XMS_GET_VERSION:
		reg_ax = 0x200;													// XMS version 2.00
		reg_bx = 0x201;													// Driver internal revision 2.01
		reg_dx = 0;														// No we don't have HMA
		break;
	case XMS_ALLOCATE_HIGH_MEMORY:										// ,, ,,
	case XMS_FREE_HIGH_MEMORY:
		reg_ax = 0;
		reg_bl = HIGH_MEMORY_NOT_EXIST;
		break;
	case XMS_QUERY_A20:
		reg_ax = 0;
		reg_bl = 0;
		break;
	case XMS_QUERY_FREE_EXTENDED_MEMORY:
		reg_bl = XMS_QueryFreeMemory(reg_ax);
		reg_dx = reg_ax;												// Free is always total
		break;
	case XMS_ALLOCATE_EXTENDED_MEMORY:
		SET_RESULT(XMS_AllocateMemory(reg_dx, reg_dx));
		break;
	case XMS_FREE_EXTENDED_MEMORY:
		SET_RESULT(XMS_FreeMemory(reg_dx));
		break;
	case XMS_MOVE_EXTENDED_MEMORY_BLOCK:
		SET_RESULT(XMS_MoveMemory(SegPhys(ds)+reg_si));
		break;
	case XMS_LOCK_EXTENDED_MEMORY_BLOCK:
		{
		Bit32u address;
		SET_RESULT(XMS_LockMemory(reg_dx, address));
		if (reg_ax)														// Success
			{ 
			reg_bx = (Bit16u)(address & 0xffff);
			reg_dx = (Bit16u)(address >> 16);
			}
		}
		break;
	case XMS_UNLOCK_EXTENDED_MEMORY_BLOCK:
		SET_RESULT(XMS_UnlockMemory(reg_dx));
		break;
	case XMS_GET_EMB_HANDLE_INFORMATION:
		SET_RESULT(XMS_GetHandleInformation(reg_dx, reg_bh, reg_bl, reg_dx), false);
		break;
	case XMS_RESIZE_EXTENDED_MEMORY_BLOCK:
		SET_RESULT(XMS_ResizeMemory(reg_dx, reg_bx));
		break;
/*	case XMS_ALLOCATE_UMB:
		{
		if (!umb_available)
			{
			reg_ax = 0;
			reg_bl = XMS_FUNCTION_NOT_IMPLEMENTED;
			break;
			}
		Bit16u umb_start = dos_infoblock.GetStartOfUMBChain();
		if (umb_start == 0xffff)
			{
			reg_ax = 0;
			reg_bl = UMB_NO_BLOCKS_AVAILABLE;
			reg_dx = 0;													// no upper memory available
			break;
			}
		Bit8u umb_flag = dos_infoblock.GetUMBChainState();				// Save status and linkage of upper UMB chain and link upper memory to the regular MCB chain
		if ((umb_flag&1) == 0)
			DOS_LinkUMBsToMemChain(1);
		Bit8u old_memstrat = DOS_GetMemAllocStrategy()&0xff;
		DOS_SetMemAllocStrategy(0x40);									// search in UMBs only

		Bit16u size = reg_dx;
		Bit16u seg;
		if (DOS_AllocateMemory(&seg, &size))
			{
			reg_ax = 1;
			reg_bx = seg;
			}
		else
			{
			reg_ax = 0;
			if (size == 0)
				reg_bl = UMB_NO_BLOCKS_AVAILABLE;
			else
				reg_bl = UMB_ONLY_SMALLER_BLOCK;
			reg_dx = size;												// Size of largest available UMB
			}

		Bit8u current_umb_flag = dos_infoblock.GetUMBChainState();		// Restore status and linkage of upper UMB chain
		if ((current_umb_flag&1) != (umb_flag&1))
			DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);
		}
		break;
	case XMS_DEALLOCATE_UMB:
		if (!umb_available)
			{
			reg_ax = 0;
			reg_bl = XMS_FUNCTION_NOT_IMPLEMENTED;
			break;
			}
		if (dos_infoblock.GetStartOfUMBChain() != 0xffff)
			if (DOS_FreeMemory(reg_dx))
				{
				reg_ax = 1;
				break;
				}
		reg_ax = 0;
		reg_bl = UMB_NO_BLOCKS_AVAILABLE;
		break;
*/
//	case XMS_GLOBAL_ENABLE_A20:										// We don't do A20
//	case XMS_GLOBAL_DISABLE_A20:
//	case XMS_LOCAL_ENABLE_A20:
//	case XMS_LOCAL_DISABLE_A20:
	default:
		reg_ax = 0;
		reg_bl = XMS_FUNCTION_NOT_IMPLEMENTED;
		}
	return CBRET_NONE;
	}


static	CALLBACK_HandlerObject callbackhandler;

void XMS_Init()
	{
	xms_callback = SegOff2dWord(DOS_GetMemory(0x1)-1, 0x10);		// Place hookable callback in writable memory area
	callbackhandler.Install(&XMS_Handler, CB_HOOKABLE, dWord2Ptr(xms_callback), "XMS Handler");
	   
	xms_handles[0].free	= false;									// Disable the 0 handle
	for (Bitu i = 1; i < XMS_HANDLES; i++)							// The rest is free to use
		xms_handles[i].free = true;

	DOS_BuildUMBChain(ConfGetBool("ems"));		// Set up UMB chain
	}
