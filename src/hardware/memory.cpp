#include "control.h"
#include "vDos.h"
#include "mem.h"
#include "inout.h"
#include "setup.h"
#include "paging.h"
#include "regs.h"

#include <string.h>

#define EXT_MEMORY 16

static struct MemoryBlock {
	Bitu pages;
	PageHandler * * phandlers;
	MemHandle * mhandles;
	struct {
		bool enabled;
		Bit8u controlport;
	} a20;
} memory;

HostPt MemBase;

class IllegalPageHandler : public PageHandler
	{
public:
	IllegalPageHandler()
		{
		flags = PFLAG_INIT|PFLAG_NOCODE;
		}
	};

class RAMPageHandler : public PageHandler
	{
public:
	RAMPageHandler()
		{
		flags = PFLAG_READABLE|PFLAG_WRITEABLE;
		}
	HostPt GetHostReadPt(Bitu phys_page)
		{
		return MemBase+phys_page*MEM_PAGESIZE;
		}
	HostPt GetHostWritePt(Bitu phys_page)
		{
		return MemBase+phys_page*MEM_PAGESIZE;
		}
	};

class ROMPageHandler : public RAMPageHandler
	{
public:
	ROMPageHandler()
		{
		flags = PFLAG_READABLE|PFLAG_HASROM;
		}
	void writeb(PhysPt addr, Bitu val)
		{
		LOG(LOG_CPU, LOG_ERROR)("Write %x to rom at %x", val, addr);
		}
	void writew(PhysPt addr, Bitu val)
		{
		LOG(LOG_CPU, LOG_ERROR)("Write %x to rom at %x", val, addr);
		}
	void writed(PhysPt addr,Bitu val)
		{
		LOG(LOG_CPU, LOG_ERROR)("Write %x to rom at %x", val, addr);
		}
	};

static IllegalPageHandler illegal_page_handler;
static RAMPageHandler ram_page_handler;
static ROMPageHandler rom_page_handler;

PageHandler * MEM_GetPageHandler(Bitu phys_page)
	{
	if (phys_page < memory.pages)
		return memory.phandlers[phys_page];
	return &illegal_page_handler;
	}

void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler)
	{
	for (; pages > 0; pages--)
		memory.phandlers[phys_page++] = handler;
	}

void MEM_ResetPageHandler(Bitu phys_page, Bitu pages)
	{
	for (; pages > 0; pages--)
		memory.phandlers[phys_page++] = &ram_page_handler;
	}

Bitu vPC_rStrLen(PhysPt pt)
	{
	for (Bitu x = 0; x < 1024; x++)
		if (!vPC_rLodsb(pt+x))
			return x;
	return 0;		// Hope this doesn't happen
	}

void vPC_rBlockRead(PhysPt addr, void * data, Bitu size)
	{
	if (addr+ size <= 0xa0000)								// if in lower memory, read direct
		{
		memcpy(data, MemBase+addr, size);
		return;
		}
	Bit8u * write = reinterpret_cast<Bit8u *>(data);
	while (size--)
		*write++ = vPC_rLodsb(addr++);
	}

void vPC_rBlockWrite(PhysPt addr, void const * const data, Bitu size)
	{
	if (addr+size <= 0xa0000)										// if in lower memory, write direct
		{
		HostPt hw_addr = MemBase+addr;
		if (size > 10 && ((Bit32u)hw_addr&1) == ((Bit32u)data&1))	// optimize with words (worth the trouble?)
			{
			HostPt src = (HostPt)data;
			if ((Bit32u)(hw_addr) & 1)								// Odd (start) address
				{
				*((Bit8u *)hw_addr++) = *((Bit8u *)src++);
				size--;
				}
			wmemcpy((wchar_t *)hw_addr, (wchar_t *)src, size>>1);	// remaining number of bytes as words
			if (size&1)												// one more to do?
				*((Bit8u *)hw_addr+size-1) = *((Bit8u *)src+size-1);
			return;
			}
		memcpy(hw_addr, data, size);
		return;
		}
	Bit8u const * read = reinterpret_cast<Bit8u const * const>(data);
	// Move in chunks of 4K for paging to take effect
	while (size)
		{
		Bitu todo = 0x1000 - (addr & 0xfff);
		if (todo > size)
			todo = size;
		PageHandler * ph = MEM_GetPageHandler(addr>>12);
		if (ph->flags & PFLAG_WRITEABLE)
			{
			HostPt hw_addr = (ph->GetHostWritePt(addr>>12)) + (addr&0xfff);
			memcpy(hw_addr, read, todo);
			addr += todo;
			read += todo;
			}
		else
			for (Bitu i = 0; i < todo; i++)
				ph->writeb(addr++, *read++);
		size -= todo;
		}
	}

void vPC_rMovsb(PhysPt dest, PhysPt src, Bitu size)
	{
	if (dest+size <= 0xa0000 && src+size <= 0xa0000)		// if in lower memory, move direct
		memcpy(MemBase+dest, MemBase+src, size);
	else
		while (size--)
			vPC_rStosb(dest++, vPC_rLodsb(src++));
	}

void vPC_rStrnCpy(char * data, PhysPt pt, Bitu size)
	{
	if (pt+size <= 0xa0000)									// if in lower memory, move direct
		{
		strncpy(data, (char *)(MemBase+pt), size);
		return;
		}
	while (size--)
		{
		Bit8u r = vPC_rLodsb(pt++);
		if (!r)
			break;
		*data++ = r;
		}
	*data = 0;
	}

Bitu MEM_TotalPages(void)
	{
	return memory.pages;
	}

Bitu MEM_FreeLargest(void)
	{
	Bitu size = 0;
	Bitu largest = 0;
	for (Bitu index = XMS_START; index < memory.pages; index++)
		if (!memory.mhandles[index])
			size++;
		else
			{
			if (size > largest)
				largest = size;
			size = 0;
			}
	if (size > largest)
		return size;
	return largest;
	}

Bitu MEM_FreeTotal(void)
	{
	Bitu free = 0;
	for (Bitu index = XMS_START; index < memory.pages; index++)
		if (!memory.mhandles[index])
			free++;
	return free;
	}

//TODO Maybe some protection for this whole allocation scheme
Bitu BestMatch(Bitu size)
	{
	Bitu index = XMS_START;	
	Bitu first = 0;
	Bitu best = -1;
	Bitu best_first = 0;
	while (index < memory.pages)
		{
		if (!first)								// Check if we are searching for first free page
			{
			if (!memory.mhandles[index])		// Check if this is a free page
				first = index;	
			}
		else
			{
			if (memory.mhandles[index])			// Check if this still is used page
				{
				Bitu pages = index-first;
				if (pages == size)
					return first;
				if (pages > size && pages < best)
					{
					best = pages;
					best_first = first;
					}
				first = 0;						// Reset for new search
				}
			}
		index++;
		}
	// Check for the final block if we can
	if (first && (index-first >= size) && (index-first < best))
		return first;
	return best_first;
	}

MemHandle MEM_AllocatePages(Bitu pages)
	{
	MemHandle ret;
	if (!pages)
		return 0;
	Bitu index = BestMatch(pages);
	if (!index)
		return 0;
	MemHandle * next = &ret;
	while (pages)
		{
		*next = index;
		next = &memory.mhandles[index];
		index++;
		pages--;
		}
	*next = -1;
	return ret;
	}

MemHandle MEM_GetNextFreePage(void)
	{
	return (MemHandle)BestMatch(1);
	}

void MEM_ReleasePages(MemHandle handle)
	{
	while (handle > 0)
		{
		MemHandle next = memory.mhandles[handle];
		memory.mhandles[handle] = 0;
		handle = next;
		}
	}

bool MEM_ReAllocatePages(MemHandle & handle, Bitu pages)
	{
	if (handle <= 0)
		{
		if (!pages)
			return true;
		handle = MEM_AllocatePages(pages);
		return (handle > 0);
		}
	if (!pages)
		{
		MEM_ReleasePages(handle);
		handle = -1;
		return true;
		}
	MemHandle index = handle;
	MemHandle last;
	Bitu old_pages = 0;
	while (index > 0)
		{
		old_pages++;
		last = index;
		index = memory.mhandles[index];
		}
	if (old_pages == pages)
		return true;
	if (old_pages > pages)
		{
		// Decrease size
		pages--;
		index = handle;
		old_pages--;
		while (pages)
			{
			index = memory.mhandles[index];
			pages--;
			old_pages--;
			}
		MemHandle next = memory.mhandles[index];
		memory.mhandles[index] = -1;
		index = next;
		while (old_pages)
			{
			next = memory.mhandles[index];
			memory.mhandles[index] = 0;
			index = next;
			old_pages--;
			}
		return true;
		}
	else
		{
		// Increase size, check for enough free space
		Bitu need = pages-old_pages;
		index = last+1;
		Bitu free = 0;
		while ((index < (MemHandle)memory.pages) && !memory.mhandles[index])
			{
			index++;
			free++;
			}
		if (free >= need)
			{
			// Enough space allocate more pages
			index = last;
			while (need)
				{
				memory.mhandles[index] = index+1;
				need--;
				index++;
				}
			memory.mhandles[index] = -1;
			return true;
			}
		else
			{
			// Not Enough space allocate new block and copy
			MemHandle newhandle = MEM_AllocatePages(pages);
			if (!newhandle)
				return false;
			vPC_rMovsb(newhandle*4096, handle*4096, old_pages*4096);
			MEM_ReleasePages(handle);
			handle = newhandle;
			return true;
			}
		}
	return 0;
	}

 
//	A20 line handling, 
//	Basically maps the 4 pages at the 1mb to 0mb in the default page directory
bool MEM_A20_Enabled(void)
	{
	return memory.a20.enabled;
	}

void MEM_A20_Enable(bool enabled)
	{
//	Bitu phys_base = enabled ? (1024/4) : 0;				// Jos: should this in any way be built in again ???
//	for (Bitu i = 0; i < 16; i++)
//		PAGING_MapPage((1024/4)+i, phys_base+i);
	memory.a20.enabled = enabled;
	}

// Memory access functions
Bit8u vPC_rLodsb(PhysPt addr)
	{
	if (addr < 0xa0000)										// if in lower mem, read direct (always readable)
		return *(Bit8u *)(MemBase+addr);
	PageHandler * ph = MEM_GetPageHandler(addr>>12);
	if (ph->flags & PFLAG_READABLE && ph->flags ^ PFLAG_NOCODE)
		return *(Bit8u *)((ph->GetHostReadPt(addr>>12)) + (addr&0xfff));
	else
		return ph->readb(addr);
	}

Bit16u vPC_rLodsw(PhysPt addr)
	{
	if (addr < 0x9ffff)										// if in lower mem, read direct (always readable)
		return *(Bit16u *)(MemBase+addr);
	if ((addr & 0xfff) < 0xfff)
		{
		PageHandler * ph = MEM_GetPageHandler(addr>>12);
		if (ph->flags & PFLAG_READABLE && ph->flags ^ PFLAG_NOCODE)
			return *(Bit16u *)((ph->GetHostReadPt(addr>>12)) + (addr&0xfff));
		else
			return ph->readw(addr);
		}
	else
		return vPC_rLodsb(addr) | (vPC_rLodsb(addr+1) << 8);
	}

Bit32u vPC_rLodsd(PhysPt addr)
	{
	if (addr < 0x9fffd)										// if in lower mem, read direct (always readable)
		return *(Bit32u *)(MemBase+addr);
	if ((addr & 0xfff) < 0xffd)
		{
		PageHandler * ph = MEM_GetPageHandler(addr>>12);
		if (ph->flags & PFLAG_READABLE && ph->flags ^ PFLAG_NOCODE)
			return *(Bit32u *)((ph->GetHostReadPt(addr>>12)) + (addr&0xfff));
		else
			return ph->readd(addr);
		}
	else
		return vPC_rLodsw(addr) | (vPC_rLodsw(addr+2) << 16);
	}

// Nb val is a word, count is in bytes so it can set multiple words (count = *2) and bytes (val low/high set the same)
// and count *2 takes care of odd start addresses
void vPC_rStoswb(PhysPt addr, Bit16u val, Bitu count)
	{
	Bit8u low = val&0xff;
	Bit8u high = val>>8;
	if (addr+count <= 0xa0000)						// if in lower memeory, write direct
		{
		HostPt hw_addr = MemBase+addr;
		if ((Bit32u)(hw_addr) & 1)					// Odd (start) address
			{
			*((Bit8u *)hw_addr++) = low;
			val = (low << 8) + high;				// Exchange low/high for the rest in words
			count--;
			}
		wmemset((wchar_t *)hw_addr, val, count>>1);	// remaining number of bytes as words
		if (count&1)								// one to do
			*(Bit8u *)(hw_addr+count-1) = high;
		return;
		}
	// set in chunks of 4K for paging to take effect
	while (count)
		{
		Bitu todo = 0x1000 - (addr & 0xfff);
		if (todo > count)
			todo = count;
		PageHandler * ph = MEM_GetPageHandler(addr>>12);
		if (ph->flags & PFLAG_WRITEABLE && ph->flags ^ PFLAG_NOCODE)
			{
			HostPt hw_addr = (ph->GetHostWritePt(addr>>12)) + (addr&0xfff);
			addr += todo;
			if ((Bit32u)(hw_addr) & 1)					// Odd (start) address
				{
				*(hw_addr++) = low;
				val = (low << 8) + high;				// Exchange low/high for the rest in words
				todo--;
				count--;
				}
			todo >>= 1;									// Switch remaining number of bytes to words
			wmemset((wchar_t *)hw_addr, val, todo);
			hw_addr += todo*2;
			count -= todo*2;
			if (count == 1)								// This will be the last byte if an odd start address
				{
				*hw_addr = high;
				return;
				}
			}
		else											// not writeable, or use (VGA)handler
			{
			if ((Bit32u)(addr) & 1)						// Odd (start) address
				{
				ph->writeb(addr++, low);
				val = (low << 8) + high;				// Exchange low/high for the rest in words
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
			if (count == 1)								// This will be the last byte if an odd start address
				{
				ph->writeb(addr, high);
				return;
				}
			}
		}
	}

void vPC_rStosb(PhysPt addr, Bit8u val)
	{
	if (addr < 0xa0000)									// if in lower mem, write direct (always writeable)
		{
		*(Bit8u *)(MemBase+addr) = val;
		return;
		}
	PageHandler * ph = MEM_GetPageHandler(addr>>12);
	if (ph->flags & PFLAG_WRITEABLE && ph->flags ^ PFLAG_NOCODE)
		*(Bit8u *)((ph->GetHostWritePt(addr>>12)) + (addr&0xfff)) = val;
	else
		ph->writeb(addr, val);
	}

void vPC_rStosw(PhysPt addr, Bit16u val)
	{
	if (addr < 0x9ffff)									// if in lower mem, write direct (always writeable)
		{
		*(Bit16u *)(MemBase+addr) = val;
		return;
		}
	if ((addr & 0xfff) != 0xfff)
		{
		PageHandler * ph = MEM_GetPageHandler(addr>>12);
		if (ph->flags & PFLAG_WRITEABLE && ph->flags ^ PFLAG_NOCODE)
			*(Bit16u *)((ph->GetHostWritePt(addr>>12)) + (addr&0xfff)) = val;
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
	if (addr < 0x9fffd)									// if in lower mem, write direct (always writeable)
		{
		*(Bit32u *)(MemBase+addr) = val;
		return;
		}
	if ((addr & 0xfff) < 0xffd)
		{
		PageHandler * ph = MEM_GetPageHandler(addr>>12);
		if (ph->flags & PFLAG_WRITEABLE && ph->flags ^ PFLAG_NOCODE)
			{
			HostPt hw_addr = (ph->GetHostWritePt(addr>>12)) + (addr&0xfff);
			*(Bit32u *)(hw_addr) = val;
			}
		else
			ph->writed(addr, val);
		}
	else
		{
		for (int i = 0; i < 4; i++)
			{
			vPC_rStosb(addr++, val&0xff);
			val >>= 8;
			}
		}
	}

void phys_writes(PhysPt addr, const char* string, Bitu length)
	{
	memcpy(MemBase+addr, string, length);
	}

static void write_p92(Bitu port, Bitu val, Bitu iolen)
	{	
	// Bit 0 = system reset (switch back to real mode)
	if (val&1)
		E_Exit("XMS: CPU reset via port 0x92 not supported.");
	memory.a20.controlport = val & ~2;
	MEM_A20_Enable((val & 2)>0);
	}

static Bitu read_p92(Bitu port, Bitu iolen)
	{
	return memory.a20.controlport | (memory.a20.enabled ? 2 : 0);
	}

class MEMORY:public Module_base
	{
private:
	IO_ReadHandleObject ReadHandler;
	IO_WriteHandleObject WriteHandler;
public:	
	MEMORY(Section* configuration):Module_base(configuration)
		{
		// Setup the Physical Page Links
		Bitu memsize = 1 + EXT_MEMORY;				// 1Mb low + extended meory
		MemBase = new Bit8u[memsize*1024*1024];
		if (!MemBase)
			E_Exit("Can't allocate main memory of %d MB", memsize);
		// Clear the memory, as new doesn't always give zeroed memory
		// (Visual C debug mode). We want zeroed memory though.
		memset((void*)MemBase, 0, memsize*1024*1024);
		memory.pages = (memsize*1024*1024)/4096;
		// Allocate the data for the different page information blocks
		memory.phandlers = new PageHandler * [memory.pages];
		memory.mhandles = new MemHandle[memory.pages];
		for (Bitu i = 0; i < memory.pages; i++)
			{
			memory.phandlers[i] = &ram_page_handler;
			memory.mhandles[i] = 0;					// Set to 0 for memory allocation
			}
		// Setup rom at 0xc0000-0xc7fff
		for (Bitu i = 0xc0; i < 0xc8; i++)
			memory.phandlers[i] = &rom_page_handler;
		// Setup rom at 0xf0000-0xfffff
		for (Bitu i = 0xf0; i < 0x100; i++)
			memory.phandlers[i] = &rom_page_handler;
		// A20 Line - PS/2 system control port A
		WriteHandler.Install(0x92, write_p92, IO_MB);
		ReadHandler.Install(0x92, read_p92, IO_MB);
		MEM_A20_Enable(false);
		}
	~MEMORY()
		{
		delete [] MemBase;
		delete [] memory.phandlers;
		delete [] memory.mhandles;
		}
	};	

	
static MEMORY* test;	
	
static void MEM_ShutDown(Section * sec)
	{
	delete test;
	}

void MEM_Init(Section * sec)
	{
	test = new MEMORY(sec);
	sec->AddDestroyFunction(&MEM_ShutDown);		// shutdown function
	}
