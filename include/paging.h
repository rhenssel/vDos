#ifndef VDOS_PAGING_H
#define VDOS_PAGING_H

#ifndef VDOS_H
#include "vDos.h"
#endif
#ifndef VDOS_MEM_H
#include "mem.h"
#endif

#define MEM_PAGE_SIZE	(4096)
#define XMS_START		(0x110)

#define PFLAG_READABLE		0x1
#define PFLAG_WRITEABLE		0x2
#define PFLAG_HASROM		0x4
#define PFLAG_HASCODE		0x8				// Page contains dynamic code
#define PFLAG_NOCODE		0x10			// No dynamic code can be generated here
#define PFLAG_INIT			0x20			// No dynamic code can be generated here

class PageHandler
	{
public:
	virtual ~PageHandler(void) { }
	virtual Bitu readb(PhysPt addr);
	virtual Bitu readw(PhysPt addr);
	virtual Bitu readd(PhysPt addr);
	virtual void writeb(PhysPt addr, Bitu val);
	virtual void writew(PhysPt addr, Bitu val);
	virtual void writed(PhysPt addr, Bitu val);
	virtual HostPt GetHostReadPt(Bitu phys_page);
	virtual HostPt GetHostWritePt(Bitu phys_page);
	Bitu flags;
	};

// Some other functions
void PAGING_Enable(bool enabled);

Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);

void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler);
void MEM_ResetPageHandler(Bitu phys_page, Bitu pages);

struct PagingBlock {
	Bitu	cr3;
	Bitu	cr2;
};

extern PagingBlock paging; 

// Some support functions
PageHandler * MEM_GetPageHandler(Bitu phys_page);
#endif
