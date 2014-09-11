#ifndef VDOS_PAGING_H
#define VDOS_PAGING_H

#ifndef VDOS_H
#include "vDos.h"
#endif
#ifndef VDOS_MEM_H
#include "mem.h"
#endif

#define MEM_PAGE_SIZE	(4096)

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
	virtual Bit8u readb(PhysPt addr);
	virtual Bit16u readw(PhysPt addr);
	virtual Bit32u readd(PhysPt addr);
	virtual void writeb(PhysPt addr, Bit8u val);
	virtual void writew(PhysPt addr, Bit16u val);
	virtual void writed(PhysPt addr, Bit32u val);
	virtual HostPt GetHostPt(PhysPt addr);
	Bit8u flags;
	};

// Some other functions
void PAGING_Enable(bool enabled);
bool PAGING_Enabled(void);

Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);

void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler);
void MEM_ResetPageHandler(Bitu phys_page, Bitu pages);

struct PagingBlock {
	Bitu	cr3;
	Bitu	cr2;
	bool	enabled;
};

extern PagingBlock paging; 

#endif
