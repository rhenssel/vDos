#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "vDos.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"
#include "lazyflags.h"
#include "cpu.h"
#include "setup.h"

PagingBlock paging;

Bitu PageHandler::readb(PhysPt addr)
	{
	E_Exit("No read handler for address %x", addr);	
	return 0;
	}

Bitu PageHandler::readw(PhysPt addr)
	{
	return readb(addr+0) | (readb(addr+1) << 8);
	}

Bitu PageHandler::readd(PhysPt addr)
	{
	return readb(addr+0) | (readb(addr+1) << 8) | (readb(addr+2) << 16) | (readb(addr+3) << 24);
	}

void PageHandler::writeb(PhysPt addr, Bitu /*val*/)
	{
	E_Exit("No write handler for address %x", addr);	
	}

void PageHandler::writew(PhysPt addr, Bitu val)
	{
	writeb(addr+0, (Bit8u) (val >> 0));
	writeb(addr+1, (Bit8u) (val >> 8));
	}

void PageHandler::writed(PhysPt addr, Bitu val)
	{
	writeb(addr+0, (Bit8u) (val >> 0));
	writeb(addr+1, (Bit8u) (val >> 8));
	writeb(addr+2, (Bit8u) (val >> 16));
	writeb(addr+3, (Bit8u) (val >> 24));
	}

HostPt PageHandler::GetHostReadPt(Bitu /*phys_page*/)
	{
	return 0;
	}

HostPt PageHandler::GetHostWritePt(Bitu /*phys_page*/)
	{
	return 0;
	}

Bitu PAGING_GetDirBase(void)
	{
	return paging.cr3;
	}

void PAGING_SetDirBase(Bitu cr3)
	{
	E_Exit("Memory paging in protected mode is not supported");
//	paging.cr3 = cr3;
	}

void PAGING_Enable(bool enabled)
	{
	if (enabled)
		E_Exit("Memory paging in protected mode is not supported");
	}

class PAGING:public Module_base
	{
public:
	PAGING(Section* configuration):Module_base(configuration)
		{
		}
	~PAGING(){}
	};

static PAGING* test;

void PAGING_Init(Section * sec)
	{
	test = new PAGING(sec);
	}
