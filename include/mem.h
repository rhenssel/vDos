#ifndef VDOS_MEM_H
#define VDOS_MEM_H

#ifndef VDOS_H
#include "vDos.h"
#endif

typedef Bit32u PhysPt;
typedef Bit8u * HostPt;
typedef Bit32u RealPt;

#define TOT_MEM_MB 64
#define TOT_MEM_KB (TOT_MEM_MB*1024)
#define TOT_MEM_BYTES (TOT_MEM_KB*1024)
//#define MEM_PAGESIZE 4096
#define MEM_PAGESIZE (16*1024)									// Set to 16KB page size of EMS
#define MEM_PAGES (TOT_MEM_BYTES/MEM_PAGESIZE)

extern HostPt MemBase;


// The Folowing seven functions recognize the paged memory system
Bit8u  vPC_rLodsb(PhysPt pt);
Bit16u vPC_rLodsw(PhysPt pt);
Bit32u vPC_rLodsd(PhysPt pt);
void vPC_rStosb(PhysPt pt, Bit8u val);
void vPC_rStosw(PhysPt pt, Bit16u val);
void vPC_rStosd(PhysPt pt, Bit32u val);
void vPC_rStoswb(PhysPt address, Bit16u val, Bitu count);

static inline void vPC_aStosb(PhysPt addr, Bit8u val)
	{
	*(Bit8u *)(MemBase+addr) = val;
	}

static inline void vPC_aStosw(PhysPt addr, Bit16u val)
	{
	*(Bit16u *)(MemBase+addr) = val;
	}

static inline void vPC_aStosd(PhysPt addr, Bit32u val)
	{
	*(Bit32u *)(MemBase+addr) = val;
	}

static inline Bit8u vPC_aLodsb(PhysPt addr)
	{
	return *(Bit8u *)(MemBase+addr);
	}

static inline void phys_writes(PhysPt addr, const char* string, Bitu length)
	{
	memcpy(MemBase+addr, string, length);
	}

void vPC_rMovsb(PhysPt dest, PhysPt src, Bitu bSize);
void vPC_rMovsbDn(PhysPt dest, PhysPt src, Bitu bSize);
void vPC_rMovsw(PhysPt dest, PhysPt src, Bitu wSize);
void vPC_rMovswDn(PhysPt dest, PhysPt src, Bitu wCount);
void vPC_rBlockWrite(PhysPt pt, void const * const data, Bitu size);
void vPC_rBlockRead(PhysPt pt, void * data, Bitu size);
void vPC_rStrnCpy(char * data, PhysPt pt, Bitu size);

Bitu vPC_rStrLen(PhysPt pt);

// The folowing functions are all shortcuts to the above functions using physical addressing
static Bit8u vPC_rLodsb(Bit16u seg, Bit16u off)
	{
	return vPC_rLodsb((seg<<4)+off);
	}

static Bit16u vPC_rLodsw(Bit16u seg, Bit16u off)
	{
	return vPC_rLodsw((seg<<4)+off);
	}

static Bit32u vPC_rLodsd(Bit16u seg, Bit16u off)
	{
	return vPC_rLodsd((seg<<4)+off);
	}

static void vPC_rStosb(Bit16u seg, Bit16u off, Bit8u val)
	{
	vPC_rStosb(((seg<<4)+off), val);
	}

static void vPC_rStosw(Bit16u seg, Bit16u off, Bit16u val)
	{
	vPC_rStosw(((seg<<4)+off), val);
	}

static void vPC_rStosd(Bit16u seg, Bit16u off, Bit32u val)
	{
	vPC_rStosd(((seg<<4)+off), val);
	}


static inline Bit16u RealSeg(RealPt pt)
	{
	return (Bit16u)(pt>>16);
	}

static inline Bit16u RealOff(RealPt pt)
	{
	return (Bit16u)(pt&0xffff);
	}

static inline PhysPt dWord2Ptr(RealPt pt)
	{
	return (RealSeg(pt)<<4)+RealOff(pt);
	}

static inline PhysPt SegOff2Ptr(Bit16u seg, Bit16u off)
	{
	return (seg<<4)+off;
	}

static inline RealPt SegOff2dWord(Bit16u seg, Bit16u off)
	{
	return (seg<<16)+off;
	}

static void RealSetVec(Bit8u vec, RealPt pt)
	{
	vPC_rStosd(vec<<2, pt);
	}

static void RealSetVec(Bit8u vec, RealPt pt, RealPt &old)
	{
	old = vPC_rLodsd(vec<<2);
	vPC_rStosd(vec<<2, pt);
	}

static RealPt RealGetVec(Bit8u vec)
	{
	return vPC_rLodsd(vec<<2);
	}	

#endif

