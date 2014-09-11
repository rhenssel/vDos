#include <stdlib.h>
#include <string.h>
#include "vDos.h"
#include "mem.h"
#include "vga.h"
#include "paging.h"
#include "pic.h"
#include "inout.h"
#include "setup.h"
#include "cpu.h"

#define CHECKED(v) ((v)&(vga.vmemwrap-1))			// Checked linear offset
#define CHECKED2(v) ((v)&((vga.vmemwrap>>2)-1))		// Checked planar offset (latched access)
#define CHECKED3(v) ((v)&(vga.vmemwrap-1))

template <class Size>
static INLINE void hostWrite(HostPt off, Bitu val)
	{
	if (sizeof(Size) == 1)
		*(Bit8u *)(off) = (Bit8u)val;
	else if (sizeof(Size) == 2)
		*(Bit16u *)(off) = (Bit16u)val;
	else if (sizeof(Size) == 4)
		*(Bit32u *)(off) = (Bit32u)val;
	}

template <class Size>
static INLINE Bitu hostRead(HostPt off)
	{
	if (sizeof(Size) == 1)
		return *(Bit8u *)off;
	else if (sizeof(Size) == 2)
		return *(Bit16u *)off;
	else if (sizeof(Size) == 4)
		return *(Bit32u *)off;
	return 0;
	}

// Nice one from DosEmu
static Bit32u RasterOp(Bit32u input, Bit32u mask)
	{
	switch (vga.config.raster_op)
		{
	case 0:		// None
		return (input & mask) | (vga.latch.d & ~mask);
	case 1:		// AND
		return (input | ~mask) & vga.latch.d;
	case 2:		// OR
		return (input & mask) | vga.latch.d;
	case 3:		// XOR
		return (input & mask) ^ vga.latch.d;
		};
	return 0;
	}

static Bit32u ModeOperation(Bit8u val)
	{
	switch (vga.config.write_mode)
		{
	case 0:
		{
		// Write Mode 0: In this mode, the host data is first rotated as per the Rotate Count field, then the Enable Set/Reset mechanism selects data from this or the Set/Reset field. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory. 
		val = ((val >> vga.config.data_rotate) | (val << (8-vga.config.data_rotate)));
		Bit32u full = ExpandTable[val];
		full = (full & vga.config.full_not_enable_set_reset) | vga.config.full_enable_and_set_reset; 
		return RasterOp(full,vga.config.full_bit_mask);
		}
	case 1:
		// Write Mode 1: In this mode, data is transferred directly from the 32 bit latch register to display memory, affected only by the Memory Plane Write Enable field. The host data is not used in this mode. 
		return vga.latch.d;
	case 2:
		//Write Mode 2: In this mode, the bits 3-0 of the host data are replicated across all 8 bits of their respective planes. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory. 
		return RasterOp(FillTable[val&0xF], vga.config.full_bit_mask);
	case 3:
		// Write Mode 3: In this mode, the data in the Set/Reset field is used as if the Enable Set/Reset field were set to 1111b. Then the host data is first rotated as per the Rotate Count field, then logical ANDed with the value of the Bit Mask field. The resulting value is used on the data obtained from the Set/Reset field in the same way that the Bit Mask field would ordinarily be used. to select which bits come from the expansion of the Set/Reset field and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory.
		val = ((val >> vga.config.data_rotate) | (val << (8-vga.config.data_rotate)));
		return RasterOp(vga.config.full_set_reset, ExpandTable[val] & vga.config.full_bit_mask);
	default:
		return 0;
		}
	}

// Gonna assume that whoever maps vga memory, maps it on 32/64kb boundary
#define VGA_PAGE_A0		(0xA0000/4096)
#define VGA_PAGE_B0		(0xB0000/4096)
#define VGA_PAGE_B8		(0xB8000/4096)

static struct {
	Bitu base, mask;
} vgapages;
	
class VGA_UnchainedRead_Handler : public PageHandler
	{
public:
	Bitu readHandler(PhysPt start)
		{
		vga.latch.d = ((Bit32u*)vga.mem.linear)[start];
		switch (vga.config.read_mode)
			{
		case 0:
			return (vga.latch.b[vga.config.read_map_select]);
		case 1:
			VGA_Latch templatch;
			templatch.d = (vga.latch.d & FillTable[vga.config.color_dont_care]) ^ FillTable[vga.config.color_compare & vga.config.color_dont_care];
			return (Bit8u)~(templatch.b[0] | templatch.b[1] | templatch.b[2] | templatch.b[3]);
			}
		return 0;
		}
public:
	Bitu readb(PhysPt addr)
		{
		addr = CHECKED2(addr & vgapages.mask);
		return readHandler(addr);
		}
	Bitu readw(PhysPt addr)
		{
		addr = CHECKED2(addr & vgapages.mask);
		return (readHandler(addr+0) << 0) | (readHandler(addr+1) << 8);
		}
	Bitu readd(PhysPt addr)
		{
		addr = CHECKED2(addr & vgapages.mask);
		return (readHandler(addr+0) << 0) | (readHandler(addr+1) << 8) | (readHandler(addr+2) << 16) | (readHandler(addr+3) << 24);
		}
	};

class VGA_ChainedEGA_Handler : public PageHandler
	{
public:
	Bitu readHandler(PhysPt addr)
		{
		return vga.mem.linear[addr];
		}
	void writeHandler(PhysPt start, Bit8u val)
		{
		ModeOperation(val);
		// Update video memory and the pixel buffer
		VGA_Latch pixels;
		vga.mem.linear[start] = val;
		start >>= 2;
		pixels.d = ((Bit32u*)vga.mem.linear)[start];

		Bit8u * write_pixels = &vga.fastmem[start<<3];
		Bit32u colors0_3, colors4_7;
		VGA_Latch temp;
		temp.d = (pixels.d>>4) & 0x0f0f0f0f;
		colors0_3 = Expand16Table[0][temp.b[0]] | Expand16Table[1][temp.b[1]] | Expand16Table[2][temp.b[2]] | Expand16Table[3][temp.b[3]];
		*(Bit32u *)write_pixels = colors0_3;
		temp.d = pixels.d & 0x0f0f0f0f;
		colors4_7 = Expand16Table[0][temp.b[0]] | Expand16Table[1][temp.b[1]] | Expand16Table[2][temp.b[2]] | Expand16Table[3][temp.b[3]];
		*(Bit32u *)(write_pixels+4) = colors4_7;
	}
public:	
	VGA_ChainedEGA_Handler()
		{
		flags = PFLAG_NOCODE;
		}
	void writeb(PhysPt addr, Bitu val)
		{
		addr = CHECKED(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		}
	void writew(PhysPt addr, Bitu val)
		{
		addr = CHECKED(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		writeHandler(addr+1, (Bit8u)(val >> 8));
		}
	void writed(PhysPt addr, Bitu val)
		{
		addr = CHECKED(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		writeHandler(addr+1, (Bit8u)(val >> 8));
		writeHandler(addr+2, (Bit8u)(val >> 16));
		writeHandler(addr+3, (Bit8u)(val >> 24));
		}
	Bitu readb(PhysPt addr)
		{
		addr = CHECKED(addr & vgapages.mask);
		return readHandler(addr);
		}
	Bitu readw(PhysPt addr)
		{
		addr = CHECKED(addr & vgapages.mask);
		return (readHandler(addr+0) << 0) | (readHandler(addr+1) << 8);
		}
	Bitu readd(PhysPt addr)
		{
		addr = CHECKED(addr & vgapages.mask);
		return (readHandler(addr+0) << 0) | (readHandler(addr+1) << 8) | (readHandler(addr+2) << 16) | (readHandler(addr+3) << 24);
		}
	};

class VGA_UnchainedEGA_Handler : public VGA_UnchainedRead_Handler
	{
public:
	void writeHandler(PhysPt start, Bit8u val)
		{
		Bit32u data = ModeOperation(val);
		// Update video memory and the pixel buffer
		VGA_Latch pixels;
		pixels.d = ((Bit32u*)vga.mem.linear)[start];
		pixels.d &= vga.config.full_not_map_mask;
		pixels.d |= (data & vga.config.full_map_mask);
		((Bit32u*)vga.mem.linear)[start] = pixels.d;

		Bit8u * write_pixels = &vga.fastmem[start<<3];
		Bit32u colors0_3, colors4_7;
		VGA_Latch temp;
		temp.d = (pixels.d>>4) & 0x0f0f0f0f;
		colors0_3 = Expand16Table[0][temp.b[0]] | Expand16Table[1][temp.b[1]] | Expand16Table[2][temp.b[2]] | Expand16Table[3][temp.b[3]];
		*(Bit32u *)write_pixels = colors0_3;
		temp.d = pixels.d & 0x0f0f0f0f;
		colors4_7 = Expand16Table[0][temp.b[0]] | Expand16Table[1][temp.b[1]] | Expand16Table[2][temp.b[2]] | Expand16Table[3][temp.b[3]];
		*(Bit32u *)(write_pixels+4) = colors4_7;
		}
public:
	VGA_UnchainedEGA_Handler()
		{
		flags = PFLAG_NOCODE;
		}
	void writeb(PhysPt addr, Bitu val)
		{
		addr = CHECKED2(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		}
	void writew(PhysPt addr, Bitu val)
		{
		addr = CHECKED2(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		writeHandler(addr+1, (Bit8u)(val >> 8));
		}
	void writed(PhysPt addr, Bitu val)
		{
		addr = CHECKED2(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		writeHandler(addr+1, (Bit8u)(val >> 8));
		writeHandler(addr+2, (Bit8u)(val >> 16));
		writeHandler(addr+3, (Bit8u)(val >> 24));
		}
	};

// Slighly unusual version, will directly write 8,16,32 bits values
class VGA_ChainedVGA_Handler : public PageHandler
	{
public:
	VGA_ChainedVGA_Handler()
		{
		flags = PFLAG_NOCODE;
		}
	template <class Size>
	static INLINE Bitu readHandler(PhysPt addr)
		{
		return hostRead<Size>(&vga.mem.linear[((addr&~3)<<2)+(addr&3)]);
		}
	template <class Size>
	static INLINE void writeCache(PhysPt addr, Bitu val)
		{
		hostWrite<Size>(&vga.fastmem[addr], val);
		if (addr < 320)		// And replicate the first line
			hostWrite<Size>(&vga.fastmem[addr+64*1024], val);
		}
	template <class Size>
	static INLINE void writeHandler(PhysPt addr, Bitu val)
		{
		// No need to check for compatible chains here, this one is only enabled if that bit is set
		hostWrite<Size>(&vga.mem.linear[((addr&~3)<<2)+(addr&3)], val);
		}
	Bitu readb(PhysPt addr)
		{
		addr = CHECKED(addr & vgapages.mask);
		return readHandler<Bit8u>(addr);
		}
	Bitu readw(PhysPt addr)
		{
		addr = CHECKED(addr & vgapages.mask);
		if (addr & 1)
			return (readHandler<Bit8u>( addr+0 ) << 0) | (readHandler<Bit8u>( addr+1 ) << 8);
		return readHandler<Bit16u>(addr);
		}
	Bitu readd(PhysPt addr)
		{
		addr = CHECKED(addr & vgapages.mask);
		if (addr & 3)
			return (readHandler<Bit8u>(addr+0) << 0) | (readHandler<Bit8u>(addr+1) << 8) | (readHandler<Bit8u>(addr+2) << 16) | (readHandler<Bit8u>(addr+3) << 24);
		return readHandler<Bit32u>(addr);
		}
	void writeb(PhysPt addr, Bitu val)
		{
		addr = CHECKED(addr & vgapages.mask);
		writeHandler<Bit8u>(addr, val);
		writeCache<Bit8u>(addr, val);
		}
	void writew(PhysPt addr,Bitu val)
		{
		addr = CHECKED(addr & vgapages.mask);
		if (addr & 1)
			{
			writeHandler<Bit8u>(addr+0, val >> 0);
			writeHandler<Bit8u>(addr+1, val >> 8);
			}
		else
			writeHandler<Bit16u>(addr, val);
		writeCache<Bit16u>(addr, val);
		}
	void writed(PhysPt addr,Bitu val)
		{
		addr = CHECKED(addr & vgapages.mask);
		if (addr & 3)
			{
			writeHandler<Bit8u>(addr+0, val >> 0);
			writeHandler<Bit8u>(addr+1, val >> 8);
			writeHandler<Bit8u>(addr+2, val >> 16);
			writeHandler<Bit8u>(addr+3, val >> 24);
			}
		else
			writeHandler<Bit32u>( addr, val );
		writeCache<Bit32u>(addr, val);
		}
	};

class VGA_UnchainedVGA_Handler : public VGA_UnchainedRead_Handler
	{
public:
	void writeHandler(PhysPt addr, Bit8u val)
		{
		Bit32u data = ModeOperation(val);
		VGA_Latch pixels;
		pixels.d = ((Bit32u*)vga.mem.linear)[addr];
		pixels.d &= vga.config.full_not_map_mask;
		pixels.d |= (data & vga.config.full_map_mask);
		((Bit32u*)vga.mem.linear)[addr] = pixels.d;
//		if(vga.config.compatible_chain4)
//			((Bit32u*)vga.mem.linear)[CHECKED2(addr+64*1024)]=pixels.d; 
		}
public:
	VGA_UnchainedVGA_Handler()
		{
		flags = PFLAG_NOCODE;
		}
	void writeb(PhysPt addr, Bitu val)
		{
		addr = CHECKED2(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		}
	void writew(PhysPt addr, Bitu val)
		{
		addr = CHECKED2(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		writeHandler(addr+1, (Bit8u)(val >> 8));
		}
	void writed(PhysPt addr, Bitu val)
		{
		addr = CHECKED2(addr & vgapages.mask);
		writeHandler(addr+0, (Bit8u)(val >> 0));
		writeHandler(addr+1, (Bit8u)(val >> 8));
		writeHandler(addr+2, (Bit8u)(val >> 16));
		writeHandler(addr+3, (Bit8u)(val >> 24));
		}
	};


class VGA_TEXT_PageHandler : public PageHandler
	{
public:
	VGA_TEXT_PageHandler()
		{
		flags = PFLAG_NOCODE;
		}
	Bitu readb(PhysPt addr)
		{
		return vga.draw.font[addr & vgapages.mask];
		}
	void writeb(PhysPt addr,Bitu val)
		{
		if (vga.seq.map_mask & 0x4)
			vga.draw.font[addr & vgapages.mask] = (Bit8u)val;
		}
	};

class VGA_Map_Handler : public PageHandler
	{
public:
	VGA_Map_Handler()
		{
		flags = PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE;
		}
	HostPt GetHostReadPt(Bitu phys_page)
		{
 		phys_page -= vgapages.base;
		return &vga.mem.linear[CHECKED3(phys_page*4096)];
		}
	HostPt GetHostWritePt(Bitu phys_page)
		{
 		phys_page -= vgapages.base;
		return &vga.mem.linear[CHECKED3(phys_page*4096)];
		}
	};

static struct vg {
	VGA_Map_Handler				map;
	VGA_TEXT_PageHandler		text;
	VGA_ChainedEGA_Handler		cega;
	VGA_ChainedVGA_Handler		cvga;
	VGA_UnchainedEGA_Handler	uega;
	VGA_UnchainedVGA_Handler	uvga;
} vgaph;

void VGA_SetupHandlers(void)
	{
	PageHandler *newHandler;

	// This should be vga only
	switch (vga.mode)
		{
	case M_TEXT:
		// Check if we're not in odd/even mode
		if (vga.gfx.miscellaneous & 0x2)
			newHandler = &vgaph.map;
		else
			newHandler = &vgaph.text;
		break;
	case M_VGA:
		if (vga.config.chained)
			newHandler = &vgaph.cvga;
		else
			newHandler = &vgaph.uvga;
		break;
	case M_EGA:
		if (vga.config.chained) 
			newHandler = &vgaph.cega;
		else
			newHandler = &vgaph.uega;
		break;	
	case M_CGA4:
	case M_CGA2:
		newHandler = &vgaph.map;
		break;
	default:
		return;
		}
	switch ((vga.gfx.miscellaneous >> 2) & 3)
		{
	case 0:
		vgapages.base = VGA_PAGE_A0;
		vgapages.mask = 0x1ffff;
		MEM_SetPageHandler(VGA_PAGE_A0, 32, newHandler);
		break;
	case 1:
		vgapages.base = VGA_PAGE_A0;
		vgapages.mask = 0xffff;
		MEM_SetPageHandler(VGA_PAGE_A0, 16, newHandler);
		MEM_ResetPageHandler(VGA_PAGE_B0, 16);
		break;
	case 2:
		vgapages.base = VGA_PAGE_B0;
		vgapages.mask = 0x7fff;
		MEM_SetPageHandler(VGA_PAGE_B0, 8, newHandler);
		MEM_ResetPageHandler(VGA_PAGE_A0, 16);
		MEM_ResetPageHandler(VGA_PAGE_B8, 8);
		break;
	case 3:
		vgapages.base = VGA_PAGE_B8;
		vgapages.mask = 0x7fff;
		MEM_SetPageHandler(VGA_PAGE_B8, 8, newHandler);
		MEM_ResetPageHandler(VGA_PAGE_A0, 16);
		MEM_ResetPageHandler(VGA_PAGE_B0, 8);
		break;
		}
	}

static void VGA_Memory_ShutDown(Section * /*sec*/)
	{
	delete[] vga.mem.linear_orgptr;
	delete[] vga.fastmem_orgptr;
	}

void VGA_SetupMemory(Section* sec)
	{
	Bit32u vga_allocsize = 512*1024;			// Keep lower limit at 512k
/*		
	Bit32u vga_allocsize = vga.vmemsize;
	// Keep lower limit at 512k
	if (vga_allocsize < 512*1024)
		vga_allocsize = 512*1024;
*/	
	vga_allocsize += 4096 * 4;					// We reserve an extra scan line (max S3 scanline 4096)
	vga_allocsize += 16;						// for memory alignment	

	vga.mem.linear_orgptr = new Bit8u[vga_allocsize];
	vga.mem.linear = (Bit8u*)(((Bitu)vga.mem.linear_orgptr + 16-1) & ~(16-1));
	memset(vga.mem.linear, 0, vga_allocsize);

	vga.fastmem_orgptr = new Bit8u[(vga.vmemsize<<1)+4096+16];
	vga.fastmem = (Bit8u*)(((Bitu)vga.fastmem_orgptr + 16-1) & ~(16-1));

	// In most cases these values stay the same. Assumptions: vmemwrap is power of 2,
	// vmemwrap <= vmemsize, fastmem implicitly has mem wrap twice as big
	vga.vmemwrap = vga.vmemsize;

	sec->AddDestroyFunction(&VGA_Memory_ShutDown);
	}
