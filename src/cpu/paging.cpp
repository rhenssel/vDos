#include <stdlib.h>
#include "vDos.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"
#include "lazyflags.h"

PagingBlock paging;

Bit8u PageHandler::readb(PhysPt addr)
	{
	E_Exit("No read handler for address %x", addr);	
	return 0;
	}

Bit16u PageHandler::readw(PhysPt addr)
	{
	return readb(addr+0) | (readb(addr+1) << 8);
	}

Bit32u PageHandler::readd(PhysPt addr)
	{
	return readb(addr+0) | (readb(addr+1) << 8) | (readb(addr+2) << 16) | (readb(addr+3) << 24);
	}

void PageHandler::writeb(PhysPt addr, Bit8u /*val*/)
	{
	E_Exit("No write handler for address %x", addr);	
	}

void PageHandler::writew(PhysPt addr, Bit16u val)
	{
	writeb(addr+0, (Bit8u) (val >> 0));
	writeb(addr+1, (Bit8u) (val >> 8));
	}

void PageHandler::writed(PhysPt addr, Bit32u val)
	{
	writeb(addr+0, (Bit8u) (val >> 0));
	writeb(addr+1, (Bit8u) (val >> 8));
	writeb(addr+2, (Bit8u) (val >> 16));
	writeb(addr+3, (Bit8u) (val >> 24));
	}

HostPt PageHandler::GetHostPt(PhysPt addr)
	{
	E_Exit("Access to invalid memory location %x", addr);
	return 0;
	}

Bitu PAGING_GetDirBase(void)
	{
	return paging.cr3;
	}

void PAGING_SetDirBase(Bitu cr3)
	{
//	E_Exit("Memory paging in protected mode is not supported");
	if (cr3&0xfff)													// On hold, needs more work
		E_Exit("Pgae fault: CR3 not page aligned");
	paging.cr3 = cr3;
	}

void PAGING_Enable(bool enabled)
	{
//	if (enabled)
//		E_Exit("Memory paging in protected mode is not supported");
	paging.enabled = enabled;										// On hold, needs more work
	}

bool PAGING_Enabled(void)
	{
	return paging.enabled;
	}

void PAGING_Init()
	{
	}
