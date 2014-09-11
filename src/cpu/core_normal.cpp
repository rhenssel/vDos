#include <stdio.h>

#include "vDos.h"
#include "mem.h"
#include "cpu.h"
#include "lazyflags.h"
#include "inout.h"
#include "callback.h"
#include "pic.h"
#include "paging.h"

#define SaveMb(off, val)	vPC_rStosb(off, val)
#define SaveMw(off, val)	vPC_rStosw(off, val)
#define SaveMd(off, val)	vPC_rStosd(off, val)

#define CPU_PIC_CHECK 1
#define CPU_TRAP_CHECK 1

#define OPCODE_NONE			0x000
#define OPCODE_0F			0x100
#define OPCODE_SIZE			0x200

#define PREFIX_ADDR			0x1
#define PREFIX_REP			0x2

#define TEST_PREFIX_ADDR	(core.prefixes & PREFIX_ADDR)
#define TEST_PREFIX_REP		(core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)					\
	BaseDS = SegBase(_SEG);					\
	BaseSS = SegBase(_SEG);					\
	core.base_val_ds = _SEG;				\
	goto restart_opcode;

#define DO_PREFIX_ADDR()								\
	core.prefixes = (core.prefixes & ~PREFIX_ADDR) |	\
	(cpu.code.big ^ PREFIX_ADDR);						\
	core.ea_table = &EATable[(core.prefixes&1) * 256];	\
	goto restart_opcode;

#define DO_PREFIX_REP(_ZERO)				\
	core.prefixes |= PREFIX_REP;			\
	core.rep_zero = _ZERO;					\
	goto restart_opcode;

typedef PhysPt (*GetEAHandler)(void);

static const Bit32u AddrMaskTable[2]={0x0000ffff,  0xffffffff};

static struct {
	Bitu opcode_index;
	PhysPt cseip;
	PhysPt base_ds, base_ss;
	SegNames base_val_ds;
	bool rep_zero;
	Bitu prefixes;
	GetEAHandler * ea_table;
} core;

#define GETIP		(core.cseip-SegBase(cs))
#define SAVEIP		reg_eip = GETIP;
#define LOADIP		core.cseip = (SegBase(cs)+reg_eip);

#define SegBase(c)	SegPhys(c)
#define BaseDS		core.base_ds
#define BaseSS		core.base_ss

static inline Bit8u Fetchb()
	{
	Bit8u temp = vPC_rLodsb(core.cseip);
	core.cseip += 1;
	return temp;
	}

static inline Bit16u Fetchw()
	{
	Bit16u temp = vPC_rLodsw(core.cseip);
	core.cseip += 2;
	return temp;
	}

static inline Bit32u Fetchd()
	{
	Bit32u temp = vPC_rLodsd(core.cseip);
	core.cseip += 4;
	return temp;
	}

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


#define EALookupTable (core.ea_table)

Bits CPU_Core_Normal_Run(void)
	{
	while (CPU_Cycles-- > 0)
		{
		LOADIP;
		core.opcode_index = cpu.code.big*0x200;
		core.prefixes = cpu.code.big;
		core.ea_table = &EATable[cpu.code.big*256];
		BaseDS = SegBase(ds);
		BaseSS = SegBase(ss);
		core.base_val_ds = ds;
restart_opcode:
		switch (core.opcode_index+Fetchb())
			{
		#include "core_normal/prefix_none.h"
		#include "core_normal/prefix_0f.h"
		#include "core_normal/prefix_66.h"
		#include "core_normal/prefix_66_0f.h"
		default:
		illegal_opcode:
			CPU_Exception(6, 0);
			continue;
			}
		SAVEIP;
		}
	FillFlags();
	return CBRET_NONE;
decode_end:
	SAVEIP;
	FillFlags();
	return CBRET_NONE;
	}

// Mainly for debuggers
Bits CPU_Core_Normal_Trap_Run(void)
	{
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret = CPU_Core_Normal_Run();
	if (!cpu.trap_skip)
		CPU_HW_Interrupt(1);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Normal_Run;

	return ret;
	}

Bits PageFaultCore(void)
	{
	CPU_CycleLeft += CPU_Cycles;
	CPU_Cycles = 1;
	Bits ret = CPU_Core_Normal_Run();
	CPU_CycleLeft += CPU_Cycles;
	if (ret<0)
		E_Exit("Got a vDos close machine in pagefault core?");
	return ret;
	}
