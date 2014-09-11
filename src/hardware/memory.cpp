#include "vDos.h"
#include "mem.h"
#include "inout.h"
#include "paging.h"
#include "cpu.h"
//#include "..\cpu\lazyflags.h"
#include <wchar.h>

static Bit8u a20_controlport;
static PageHandler * pageHandlers[MEM_PAGES];

HostPt MemBase;

class RAMPageHandler : public PageHandler
	{
public:
	RAMPageHandler()
		{
		flags = PFLAG_READABLE|PFLAG_WRITEABLE;
		}
	Bit8u readb(PhysPt addr)
		{
		return *(MemBase+addr);
		}
	Bit16u readw(PhysPt addr)
		{
		return *(Bit16u*)(MemBase+addr);
		}
	Bit32u readd(PhysPt addr)
		{
		return *(Bit32u*)(MemBase+addr);
		}
	void writeb(PhysPt addr, Bit8u val)
		{
		*(MemBase+addr) = val;
		}
	void writew(PhysPt addr, Bit16u val)
		{
		*(Bit16u*)(MemBase+addr) = val;
		}
	void writed(PhysPt addr, Bit32u val)
		{
		*(Bit32u*)(MemBase+addr) = val;
		}
	HostPt GetHostPt(PhysPt addr)
		{
		return MemBase+addr;
		}
	};

class ROMPageHandler : public RAMPageHandler
	{
public:
	ROMPageHandler()
		{
		flags = PFLAG_READABLE|PFLAG_HASROM;
		}
	void writeb(PhysPt addr, Bit8u val)
		{
		}
	void writew(PhysPt addr, Bit16u val)
		{
		}
	void writed(PhysPt addr, Bit32u val)
		{
		}
	};

static RAMPageHandler ram_page_handler;
static ROMPageHandler rom_page_handler;

static Bit32u last_pdptBase;
static Bit32u last_ptBase;

static inline PageHandler * MEM_GetPageHandler(PhysPt addr)
	{
	if (!PAGING_Enabled())
		{
		if (addr < TOT_MEM_BYTES)
			return pageHandlers[addr/MEM_PAGESIZE];
		E_Exit("Access to invalid memory location %x", addr);
		}

																		// Paging in protected mode on hold, dirty bit, PF execption...
	Bit32u pdptBase = (addr>>10)&~3;									// Far from correct
	if (pdptBase == last_pdptBase)
		return pageHandlers[last_ptBase];

	HostPt pdAddr = MemBase+PAGING_GetDirBase()+((addr>>20)&0xffc);
	Bit32u pdEntry = *(Bit32u *)pdAddr;
	if (!(pdEntry&1))
		E_Exit("Page fault: directory entry not present");
	Bit32u ptEntry;
	HostPt ptAddr = MemBase+(pdEntry&~0xfff)+((addr>>10)&0xffc);
	ptEntry = *(Bit32u *)ptAddr;
	if (!(ptEntry&1))
		E_Exit("Page fault: table entry not present");								// Should raise exception 0x0E
	if (ptEntry > TOT_MEM_BYTES)
		E_Exit("Page fault: table entry not valid");
	return pageHandlers[ptEntry>>12];
	}

void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler)
	{
	for (; pages > 0; pages--)
		pageHandlers[phys_page++] = handler;
	}

void MEM_ResetPageHandler(Bitu phys_page, Bitu pages)
	{
	for (; pages > 0; pages--)
		pageHandlers[phys_page++] = &ram_page_handler;
	}


// Memory access functions...

Bit8u vPC_rLodsb(PhysPt addr)
	{
	if (/*!PAGING_Enabled() && */(addr < 0xa0000 || addr > 0xfffff))	// If in lower or extended mem, read direct (always readable)
		return *(Bit8u *)(MemBase+addr);
	PageHandler * ph = MEM_GetPageHandler(addr);
	if (ph->flags & PFLAG_READABLE)
		return *(Bit8u *)(ph->GetHostPt(addr));
	else
		return ph->readb(addr);
	}

Bit16u vPC_rLodsw(PhysPt addr)
	{
	if (/*!PAGING_Enabled() && */(addr < 0x9ffff || addr > 0xfffff))	// If in lower or extended mem, read direct (always readable)
	{
		if (addr >= 0x41a && addr <= 0x41d)								// Start/end keyboard buffer pointer access
			idleCount++;
		return *(Bit16u *)(MemBase+addr);
	}
	if ((addr&(MEM_PAGESIZE-1)) != (MEM_PAGESIZE-1))
		{
		PageHandler * ph = MEM_GetPageHandler(addr);
		if (ph->flags & PFLAG_READABLE)
			return *(Bit16u *)(ph->GetHostPt(addr));
		else
			return ph->readw(addr);
		}
	else
		return vPC_rLodsb(addr) | (vPC_rLodsb(addr+1) << 8);
	}

Bit32u vPC_rLodsd(PhysPt addr)
	{
	if (/*!PAGING_Enabled() && */(addr < 0x9fffd || addr > 0xfffff))	// If in lower or exyended mem, read direct (always readable)
		return *(Bit32u *)(MemBase+addr);
	if ((addr&(MEM_PAGESIZE-1)) < (MEM_PAGESIZE-3))
		{
		PageHandler * ph = MEM_GetPageHandler(addr);
		if (ph->flags & PFLAG_READABLE)
			return *(Bit32u *)(ph->GetHostPt(addr));
		else
			return ph->readd(addr);
		}
	else
		return vPC_rLodsw(addr) | (vPC_rLodsw(addr+2) << 16);
	}

void vPC_rStosb(PhysPt addr, Bit8u val)
	{
	if (/*!PAGING_Enabled() && */(addr < 0xa0000 || addr > 0xfffff))	// If in lower or extended mem, write direct (always writeable)
		{
		*(MemBase+addr) = val;
		return;
		}
	PageHandler * ph = MEM_GetPageHandler(addr);
	if (ph->flags & PFLAG_WRITEABLE)
		*(Bit8u *)(ph->GetHostPt(addr)) = val;
	else
		ph->writeb(addr, val);
	}

void vPC_rStosw(PhysPt addr, Bit16u val)
	{
	if (/*!PAGING_Enabled() && */(addr < 0x9ffff || addr > 0xfffff))	// If in lower or extended mem, write direct (always writeable)
		{
		*(Bit16u *)(MemBase+addr) = val;
		return;
		}
	if ((addr&(MEM_PAGESIZE-1)) != (MEM_PAGESIZE-1))
		{
		PageHandler * ph = MEM_GetPageHandler(addr);
		if (ph->flags & PFLAG_WRITEABLE)
			*(Bit16u *)(ph->GetHostPt(addr)) = val;
		else
			ph->writew(addr, val);
		}
	else
		{
		vPC_rStosb(addr, val&0xff);
		vPC_rStosb(addr+1, val>>8);
		}
	}

void vPC_rStosd(PhysPt addr, Bit32u val)
	{
	if (/*!PAGING_Enabled() && */(addr < 0x9fffd || addr > 0xfffff))	// If in lower mem, write direct (always writeable)
		{
		*(Bit32u *)(MemBase+addr) = val;
		return;
		}
	if ((addr&(MEM_PAGESIZE-1)) < (MEM_PAGESIZE-3))
		{
		PageHandler * ph = MEM_GetPageHandler(addr);
		if (ph->flags & PFLAG_WRITEABLE)
			*(Bit32u *)(ph->GetHostPt(addr)) = val;
		else
			ph->writed(addr, val);
		}
	else
		{
		vPC_rStosw(addr, val&0xffff);
		vPC_rStosw(addr+2, val>>16);
		}
	}

void vPC_rMovsb(PhysPt dest, PhysPt src, Bitu bCount)
	{
	while (bCount)														// Move in chunks of MEM_PAGESIZE for paging to take effect
		{
		Bit16u srcOff = src&(MEM_PAGESIZE-1);
		Bit16u destOff = dest&(MEM_PAGESIZE-1);
		Bit16u bTodo = MEM_PAGESIZE - max(srcOff, destOff);
		if (bTodo > bCount)
			bTodo = bCount;
		bCount -= bTodo;
		PageHandler * phSrc = MEM_GetPageHandler(src);
		PageHandler * phDest = MEM_GetPageHandler(dest);

		if (phDest->flags & PFLAG_WRITEABLE && phSrc->flags & PFLAG_READABLE)
			{
			Bit8u *hDest = phDest->GetHostPt(dest);
			Bit8u *hSrc = phSrc->GetHostPt(src);
			src += bTodo;
			dest += bTodo;
			if ((hSrc <= hDest && hSrc+bTodo > hDest) || (hDest <= hSrc && hDest+bTodo > hSrc))
				while (bTodo--)											// If source and destination overlap, do it "by hand"
					*(hDest++) = *(hSrc++);								// memcpy() messes things up in another way than rep movsb does!
			else
				memcpy(hDest, hSrc, bTodo);
			}
		else															// Not writeable, or use (VGA)handler
			while (bTodo--)
				phDest->writeb(dest++, phSrc->readb(src++));
		}
	}

void vPC_rMovsbDn(PhysPt dest, PhysPt src, Bitu bCount)
	{
	while (bCount)														// Move in chunks of MEM_PAGESIZE for paging to take effect
		{
		Bit16u srcOff = src&(MEM_PAGESIZE-1);
		Bit16u destOff = dest&(MEM_PAGESIZE-1);
		Bit16u bTodo = min(srcOff, destOff)+1;
		if (bTodo > bCount)
			bTodo = bCount;
		bCount -= bTodo;
		PageHandler * phSrc = MEM_GetPageHandler(src);
		PageHandler * phDest = MEM_GetPageHandler(dest);

		if (phDest->flags & PFLAG_WRITEABLE && phSrc->flags & PFLAG_READABLE)
			{
			Bit8u *hDest = phDest->GetHostPt(dest);
			Bit8u *hSrc = phSrc->GetHostPt(src);
			src -= bTodo;
			dest -= bTodo;
			if ((hSrc <= hDest && hSrc+bTodo > hDest) || (hDest <= hSrc && hDest+bTodo > hSrc))
				while (bTodo--)											// If source and destination overlap, do it "by hand"
					*(hDest--) = *(hSrc--);								// memcpy() messes things up in another way than rep movsb does!
			else
				memcpy(hDest-bTodo+1, hSrc-bTodo+1, bTodo);
			}
		else															// Not writeable, or use (VGA)handler
			while (bTodo--)
				phDest->writeb(dest--, phSrc->readb(src--));
		}
	}

void vPC_rMovsw(PhysPt dest, PhysPt src, Bitu wCount)
	{
	Bitu bCount = wCount<<1;											// Have to use a byte count (words can be split over pages)
	while (bCount)														// Move in chunks of MEM_PAGESIZE for paging to take effect
		{
		Bit16u srcOff = src&(MEM_PAGESIZE-1);
		Bit16u destOff = dest&(MEM_PAGESIZE-1);
		Bit16u bTodo = MEM_PAGESIZE - max(srcOff, destOff);
		if (bTodo > bCount)
			bTodo = bCount;
		bCount -= bTodo;
		Bit16u wTodo = bTodo>>1;
		PageHandler * phSrc = MEM_GetPageHandler(src);
		PageHandler * phDest = MEM_GetPageHandler(dest);

		if (phDest->flags & PFLAG_WRITEABLE && phSrc->flags & PFLAG_READABLE)
			{
			Bit16u *hDest = (Bit16u*)phDest->GetHostPt(dest);
			Bit16u *hSrc = (Bit16u*)phSrc->GetHostPt(src);
			src += bTodo;
			dest += bTodo;
			if ((hSrc <= hDest && hSrc+wTodo > hDest) || (hDest <= hSrc && hDest+wTodo > hSrc))
				while (wTodo--)											// If source and destination overlap, do it "by hand"
				 	*(hDest++) = *(hSrc++);								// memcpy() messes things up in another way than rep movsb does!
			else if (wTodo)
				{
				wmemcpy((wchar_t*)hDest, (wchar_t*)hSrc, wTodo);
				hDest += wTodo;
				hSrc += wTodo;
				}
			if (bTodo&1)
				*((Bit8u*)hDest) = *((Bit8u*)hSrc);						// One byte left in these pages
			}
		else															// Not writeable, or use (VGA)handler
			{
			while (wTodo--)
				{
				phDest->writew(dest, phSrc->readw(src));
				dest += 2;
				src += 2;
				}
			if (bTodo&1)
				phDest->writeb(dest++, phSrc->readb(src++));
			}
		if (bCount&1)													// One byte to do of previous moves (page overrun)
			{
			vPC_rStosb(dest++, vPC_rLodsb(src++));
			bCount--;
			}
		}
	}

void vPC_rMovswDn(PhysPt dest, PhysPt src, Bitu wCount)
	{
	Bitu bCount = wCount<<1;											// Have to use a byte count (words can be split over pages)
	dest++;
	src++;
	while (bCount)														// Move in chunks of MEM_PAGESIZE for paging to take effect
		{
		Bit16u srcOff = src&(MEM_PAGESIZE-1);
		Bit16u destOff = dest&(MEM_PAGESIZE-1);
		Bit16u bTodo = min(srcOff, destOff)+1;
		if (bTodo > bCount)
			bTodo = bCount;
		bCount -= bTodo;
		Bit16u wTodo = bTodo>>1;
		PageHandler * phSrc = MEM_GetPageHandler(src+1);
		PageHandler * phDest = MEM_GetPageHandler(dest+1);

		if (phDest->flags & PFLAG_WRITEABLE && phSrc->flags & PFLAG_READABLE)
			{
			Bit8u *hDest = phDest->GetHostPt(dest);
			Bit8u *hSrc = phSrc->GetHostPt(src);
			src -= bTodo;
			dest -= bTodo;
			if ((hSrc <= hDest && hSrc+bTodo > hDest) || (hDest <= hSrc && hDest+bTodo > hSrc))
				while (wTodo--)											// If source and destination overlap, do it "by hand"
					{
				 	*(Bit16u*)(--hDest) = *(Bit16u*)(--hSrc);			// memcpy() messes things up in another way than rep movsb does!
					hDest--;
					hSrc--;
					}
			else if (wTodo)
				{
				hDest -= wTodo*2;
				hSrc -= wTodo*2;
				wmemcpy((wchar_t*)(hDest+1), (wchar_t*)(hSrc+1), wTodo);
				}
			if (bTodo&1)
				*hDest = *hSrc;											// One byte left in these pages
			}
		else															// Not writeable, or use (VGA)handler
			{
			while (wTodo--)
				{
				phDest->writew(dest-1, phSrc->readw(src-1));
				dest -= 2;
				src -= 2;
				}
			if (bTodo&1)
				phDest->writeb(dest--, phSrc->readb(src--));
			}
		if (bCount&1)													// One byte to do of previous moves (page overrun)
			{
			vPC_rStosb(dest--, vPC_rLodsb(src--));
			bCount--;
			}
		}
	}

Bitu vPC_rStrLen(PhysPt pt)
	{
	for (Bitu len = 0; len < 65536; len++)
		if (!vPC_rLodsb(pt+len))
			return len;
	return 0;															// Hope this doesn't happen
	}

void vPC_rBlockWrite(PhysPt writePhys, void const * const data, Bitu bCount)
	{
	Bit8u const * readAddr = (Bit8u const *)data;
	while (bCount)
		{
		Bitu doNow = MEM_PAGESIZE - (writePhys&(MEM_PAGESIZE-1));		// Move in chunks of MEM_PAGESIZE for paging to take effect
		if (doNow > bCount)
			doNow = bCount;
		bCount -= doNow;
		PageHandler * ph = MEM_GetPageHandler(writePhys);
		if (ph->flags & PFLAG_WRITEABLE)
			{
			memcpy(ph->GetHostPt(writePhys), readAddr, doNow);
			writePhys += doNow;
			readAddr += doNow;
			}
		else
			while (doNow--)
				ph->writeb(writePhys++, *readAddr++);
		}
	}

void vPC_rBlockRead(PhysPt readPhys, void * data, Bitu bCount)
	{
	Bit8u * writeAddr = (Bit8u *)data;
	while (bCount)
		{
		Bitu doNow = MEM_PAGESIZE - (readPhys & (MEM_PAGESIZE-1));		// Move in chunks of MEM_PAGESIZE for paging to take effect
		if (doNow > bCount)
			doNow = bCount;
		bCount -= doNow;
		PageHandler * ph = MEM_GetPageHandler(readPhys);
		if (ph->flags & PFLAG_READABLE)
			{
			memcpy(writeAddr, ph->GetHostPt(readPhys), doNow);
			writeAddr += doNow;
			readPhys += doNow;
			}
		else
			while (doNow--)
				*writeAddr++ = ph->readb(readPhys++);
		}
	}

void vPC_rStrnCpy(char * data, PhysPt pt, Bitu bCount)
	{
	while (bCount--)
		{
		Bit8u c = vPC_rLodsb(pt++);
		if (!c)
			break;
		*data++ = c;
		}
	*data = 0;
	}

// Nb val is a word, count is in bytes so it can set multiple words (count = *2) and bytes (val low/high set the same)
// and count *2 takes care of odd start addresses
void vPC_rStoswb(PhysPt addr, Bit16u val, Bitu count)
	{
	Bit8u low = val&0xff;
	Bit8u high = val>>8;
	while (count)														// Set in chunks of MEM_PAGESIZE for paging to take effect
		{
		Bitu todo = MEM_PAGESIZE - (addr&(MEM_PAGESIZE-1));
		if (todo > count)
			todo = count;
		PageHandler * ph = MEM_GetPageHandler(addr);
		if (ph->flags & PFLAG_WRITEABLE)
			{
			HostPt hw_addr = ph->GetHostPt(addr);
			addr += todo;
			if ((Bit32u)(hw_addr) & 1)									// Odd (start) address
				{
				*(hw_addr++) = low;
				val = (low << 8) + high;								// Exchange low/high for the rest in words
				todo--;
				count--;
				}
			todo >>= 1;													// Switch remaining number of bytes to words
			wmemset((wchar_t *)hw_addr, val, todo);
			hw_addr += todo*2;
			count -= todo*2;
			if (count == 1)												// Last byte if an odd start address
				{
				*hw_addr = high;
				return;
				}
			}
		else															// Not writeable, or use (VGA)handler
			{
			if ((Bit32u)(addr) & 1)										// Odd (start) address
				{
				ph->writeb(addr++, low);
				val = (low << 8) + high;								// Exchange low/high for the rest in words
				todo--;
				count--;
				}
			todo >>= 1;
			count -= todo*2;
			while (todo--)
				{
				ph->writew(addr, val);
				addr += 2;
				}
			if (count == 1)												// Last byte if an odd start address
				{
				ph->writeb(addr, high);
				return;
				}
			}
		}
	}

static void write_p92(Bitu port, Bitu val, Bitu iolen)
	{	
	if (val&1)															// Bit 0 = system reset (switch back to real mode)
		E_Exit("CPU reset via port 0x92 not supported.");
	a20_controlport = val & ~2;
	}

static Bitu read_p92(Bitu port, Bitu iolen)
	{
	return a20_controlport;
	}

static IO_ReadHandleObject ReadHandler;
static IO_WriteHandleObject WriteHandler;

void MEM_Init()
	{
	MemBase = (Bit8u *)_aligned_malloc(TOT_MEM_BYTES, 64);				// Setup the physical memory
	if (!MemBase)
		E_Exit("Can't allocate main memory of %d MB", TOT_MEM_MB);
	memset((void*)MemBase, 0, TOT_MEM_BYTES);							// Clear the memory
		
	for (Bitu i = 0; i < MEM_PAGES; i++)
		pageHandlers[i] = &ram_page_handler;
	for (Bitu i = 0xc0000/MEM_PAGESIZE; i < 0xc4000/MEM_PAGESIZE; i++)	// Setup rom at 0xc0000-0xc3fff
		pageHandlers[i] = &rom_page_handler;
	for (Bitu i = 0xf0000/MEM_PAGESIZE; i < 0x100000/MEM_PAGESIZE; i++)	// Setup rom at 0xf0000-0xfffff
		pageHandlers[i] = &rom_page_handler;
	WriteHandler.Install(0x92, write_p92, IO_MB);						// (Dummy) A20 Line - PS/2 system control port A
	ReadHandler.Install(0x92, read_p92, IO_MB);
	}
