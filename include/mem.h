#ifndef VDOS_MEM_H
#define VDOS_MEM_H

#ifndef VDOS_H
#include "vDos.h"
#endif

typedef Bit32u PhysPt;
typedef Bit8u * HostPt;
typedef Bit32u RealPt;

typedef Bit32s MemHandle;

#define MEM_PAGESIZE 4096

extern HostPt MemBase;

bool MEM_A20_Enabled(void);
void MEM_A20_Enable(bool enable);

// Memory management
Bitu MEM_FreeTotal(void);			// Free 4 kb pages
Bitu MEM_FreeLargest(void);			// Largest free 4 kb pages block
Bitu MEM_TotalPages(void);			// Total amount of 4 kb pages
MemHandle MEM_AllocatePages(Bitu pages);
MemHandle MEM_GetNextFreePage(void);
PhysPt MEM_AllocatePage(void);
void MEM_ReleasePages(MemHandle handle);
bool MEM_ReAllocatePages(MemHandle & handle, Bitu pages);

// The Folowing six functions recognize the paged memory system
Bit8u  vPC_rLodsb(PhysPt pt);
Bit16u vPC_rLodsw(PhysPt pt);
Bit32u vPC_rLodsd(PhysPt pt);

void vPC_rStosb(PhysPt pt, Bit8u val);
void vPC_rStosw(PhysPt pt, Bit16u val);
void vPC_rStosd(PhysPt pt, Bit32u val);

void vPC_rStoswb(PhysPt address, Bit16u val, Bitu count);

void phys_writes(PhysPt addr, const char* string, Bitu length);

static INLINE void vPC_aStosb(PhysPt addr, Bit8u val)
	{
	*(Bit8u *)(MemBase+addr) = val;
	}

static INLINE void vPC_aStosw(PhysPt addr, Bit16u val)
	{
	*(Bit16u *)(MemBase+addr) = val;
	}

static INLINE void vPC_aStosd(PhysPt addr, Bit32u val)
	{
	*(Bit32u *)(MemBase+addr) = val;
	}

static INLINE Bit8u vPC_aLodsb(PhysPt addr)
	{
	return *(Bit8u *)(MemBase+addr);
	}

static INLINE Bit16u vPC_aLodsw(PhysPt addr)
	{
	return *(Bit16u *)(MemBase+addr);
	}

static INLINE Bit32u vPC_aLodsd(PhysPt addr)
	{
	return *(Bit32u *)(MemBase+addr);
	}

// These don't check for alignment, better be sure it's correct		Jos: they do !!!
void vPC_rMovsb(PhysPt dest, PhysPt src, Bitu size);
void vPC_rBlockWrite(PhysPt pt, void const * const data, Bitu size);
void vPC_rBlockRead(PhysPt pt, void * data, Bitu size);
void vPC_rStrnCpy(char * data, PhysPt pt, Bitu size);

Bitu vPC_rStrLen(PhysPt pt);

// The folowing functions are all shortcuts to the above functions using physical addressing
static INLINE Bit8u vPC_rLodsb(Bit16u seg, Bit16u off)
	{
	return vPC_rLodsb((seg<<4)+off);
	}

static INLINE Bit16u vPC_rLodsw(Bit16u seg, Bit16u off)
	{
	return vPC_rLodsw((seg<<4)+off);
	}

static INLINE Bit32u vPC_rLodsd(Bit16u seg, Bit16u off)
	{
	return vPC_rLodsd((seg<<4)+off);
	}

static INLINE void vPC_rStosb(Bit16u seg, Bit16u off, Bit8u val)
	{
	vPC_rStosb(((seg<<4)+off), val);
	}

static INLINE void vPC_rStosw(Bit16u seg, Bit16u off, Bit16u val)
	{
	vPC_rStosw(((seg<<4)+off), val);
	}

static INLINE void vPC_rStosd(Bit16u seg, Bit16u off, Bit32u val)
	{
	vPC_rStosd(((seg<<4)+off), val);
	}

static INLINE Bit16u RealSeg(RealPt pt)
	{
	return (Bit16u)(pt>>16);
	}

static INLINE Bit16u RealOff(RealPt pt)
	{
	return (Bit16u)(pt&0xffff);
	}

static INLINE PhysPt dWord2Ptr(RealPt pt)
	{
	return (RealSeg(pt)<<4)+RealOff(pt);
	}

static INLINE PhysPt SegOff2Ptr(Bit16u seg, Bit16u off)
	{
	return (seg<<4)+off;
	}

static INLINE RealPt SegOff2dWord(Bit16u seg, Bit16u off)
	{
	return (seg<<16)+off;
	}

static INLINE void RealSetVec(Bit8u vec, RealPt pt)
	{
	vPC_rStosd(vec<<2, pt);
	}

static INLINE void RealSetVec(Bit8u vec, RealPt pt, RealPt &old)
	{
	old = vPC_rLodsd(vec<<2);
	vPC_rStosd(vec<<2, pt);
	}

static INLINE RealPt RealGetVec(Bit8u vec)
	{
	return vPC_rLodsd(vec<<2);
	}	

#endif

