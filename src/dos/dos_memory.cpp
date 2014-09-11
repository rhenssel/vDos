#include "vDos.h"
#include "mem.h"
#include "dos_inc.h"
#include "callback.h"

#define UMB_START_SEG 0x9fff

static Bit16u memAllocStrategy = 0;

static void DOS_CompressMemory(void)
	{
	Bit16u mcb_segment = dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	DOS_MCB mcb_next(0);
	Bitu counter = 0;

	while (mcb.GetType() != 'Z')
		{
		if (counter++ > 10000)
			E_Exit("DOS MCB list corrupted.");
		mcb_next.SetPt((Bit16u)(mcb_segment+mcb.GetSize()+1));
		if ((mcb.GetPSPSeg() == 0) && (mcb_next.GetPSPSeg() == 0))
			{
			mcb.SetSize(mcb.GetSize()+mcb_next.GetSize()+1);
			mcb.SetType(mcb_next.GetType());
			}
		else
			{
			mcb_segment += mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
			}
		}
	}

void DOS_FreeProcessMemory(Bit16u pspseg)
	{
	Bit16u mcb_segment = dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	for (Bitu counter = 0; ;)
		{
		if (counter++ > 10000)
			E_Exit("DOS MCB list corrupted.");
		if (mcb.GetPSPSeg() == pspseg)
			mcb.SetPSPSeg(MCB_FREE);
		if (mcb.GetType() == 0x5a)
			break;
		mcb_segment += mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
		}

	Bit16u umb_start = dos_infoblock.GetStartOfUMBChain();
	if (umb_start == UMB_START_SEG)
		{
		DOS_MCB umb_mcb(umb_start);
		for (;;)
			{
			if (umb_mcb.GetPSPSeg() == pspseg)
				umb_mcb.SetPSPSeg(MCB_FREE);
			if (umb_mcb.GetType() != 0x4d)
				break;
			umb_start += umb_mcb.GetSize()+1;
			umb_mcb.SetPt(umb_start);
			}
		}
	else if (umb_start != 0xffff)
		LOG(LOG_DOSMISC, LOG_ERROR)("Corrupt UMB chain: %x", umb_start);
	DOS_CompressMemory();
	}

Bit16u DOS_GetMemAllocStrategy()
	{
	return memAllocStrategy;
	}

bool DOS_SetMemAllocStrategy(Bit16u strat)
	{
	if ((strat&0x3f) < 3)
		{
		memAllocStrategy = strat;
		return true;
		}
	return false;			// otherwise an invalid allocation strategy was specified
	}

bool DOS_GetFreeUMB(Bit16u * total, Bit16u * largest, Bit16u * count)
	{
	*total = *largest = *count = 0;

//	DOS_CompressMemory();
	Bit16u mcb_segment;
	Bit16u umb_start = dos_infoblock.GetStartOfUMBChain();
	if (umb_start == UMB_START_SEG)
		mcb_segment = umb_start;
	else
		{
		if (umb_start != 0xffff)
			LOG(LOG_DOSMISC, LOG_ERROR)("Corrupt UMB chain: %x", umb_start);
		return false;
		}
	DOS_MCB mcb(0);
	DOS_MCB mcb_next(0);
	DOS_MCB psp_mcb(dos.psp()-1);

	while (mcb.GetType() != 0x5a)
		{
		mcb.SetPt(mcb_segment);
		if (mcb.GetPSPSeg() == 0)
			{
			*count += 1;
			Bit16u block_size = mcb.GetSize();
			*total += block_size;
			if (*largest < block_size)
				*largest = block_size;
			}
		mcb_segment += mcb.GetSize()+1;
		}
	return true;
	}

bool DOS_AllocateMemory(Bit16u * segment, Bit16u * blocks)
	{
	DOS_CompressMemory();
	Bit16u bigsize = 0;
	Bit16u mem_strat = memAllocStrategy;
	Bit16u mcb_segment = dos.firstMCB;

	Bit16u umb_start = dos_infoblock.GetStartOfUMBChain();
	if (umb_start == UMB_START_SEG)
		{
		if (mem_strat&0xc0)					// start with UMBs if requested (bits 7 or 6 set)
			mcb_segment = umb_start;
		}
	else if (umb_start != 0xffff)
		LOG(LOG_DOSMISC, LOG_ERROR)("Corrupt UMB chain: %x", umb_start);

	DOS_MCB mcb(0);
	DOS_MCB mcb_next(0);
	DOS_MCB psp_mcb(dos.psp()-1);
	char psp_name[9];
	psp_mcb.GetFileName(psp_name);
	Bit16u found_seg = 0, found_seg_size = 0;
	for (;;)
		{
		mcb.SetPt(mcb_segment);
		if (mcb.GetPSPSeg() == 0)
			{
			// Check for enough free memory in current block
			Bit16u block_size = mcb.GetSize();			
			if (block_size < (*blocks))
				{
				if (bigsize < block_size)										// current block is largest block that was found, but still not as big as requested
					bigsize = block_size;
				}
			else if ((block_size == *blocks) && ((mem_strat & 0x3f) < 2))		// MCB fits precisely, use it if search strategy is firstfit or bestfit
				{
				mcb.SetPSPSeg(dos.psp());
				mcb.SetFileName(psp_name);
				*segment = mcb_segment+1;
				return true;
				}
			else
				{
				switch (mem_strat & 0x3f)
					{
				case 0:		// firstfit
					mcb_next.SetPt((Bit16u)(mcb_segment+*blocks+1));
					mcb_next.SetPSPSeg(MCB_FREE);
					mcb_next.SetType(mcb.GetType());
					mcb_next.SetSize(block_size-*blocks-1);
					mcb.SetSize(*blocks);
					mcb.SetType(0x4d);		
					mcb.SetPSPSeg(dos.psp());
					mcb.SetFileName(psp_name);
					// TODO Filename
					*segment = mcb_segment+1;
					return true;
				case 1:		// bestfit
					if ((found_seg_size == 0) || (block_size < found_seg_size))
						{
						// first fitting MCB, or smaller than the last that was found
						found_seg = mcb_segment;
						found_seg_size = block_size;
						}
					break;
				default:	// everything else is handled as lastfit by dos
					// MCB is large enough, note it down
					found_seg = mcb_segment;
					found_seg_size = block_size;
					break;
					}
				}
			}
		// Onward to the next MCB if there is one
		if (mcb.GetType() == 0x5a)
			{
			if ((mem_strat&0x80) && (umb_start == UMB_START_SEG))
				{
				// bit 7 set: try high memory first, then low
				mcb_segment = dos.firstMCB;
				mem_strat &= (~0xc0);
				}
			else
				{
				// finished searching all requested MCB chains
				if (found_seg)
					{
					// a matching MCB was found (cannot occur for firstfit)
					if ((mem_strat & 0x3f) == 1)
						{
						// bestfit, allocate block at the beginning of the MCB
						mcb.SetPt(found_seg);
						mcb_next.SetPt((Bit16u)(found_seg+*blocks+1));
						mcb_next.SetPSPSeg(MCB_FREE);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetSize(found_seg_size-*blocks-1);
						mcb.SetSize(*blocks);
						mcb.SetType(0x4d);		
						mcb.SetPSPSeg(dos.psp());
						mcb.SetFileName(psp_name);
						// TODO Filename
						*segment = found_seg+1;
						}
					else
						{
						// lastfit, allocate block at the end of the MCB
						mcb.SetPt(found_seg);
						if (found_seg_size == *blocks)
							{
							// use the whole block
							mcb.SetPSPSeg(dos.psp());
							// Not consistent with line 124. But how many application will use this information ?
							mcb.SetFileName(psp_name);
							*segment = found_seg+1;
							return true;
							}
						*segment = found_seg+1+found_seg_size - *blocks;
						mcb_next.SetPt((Bit16u)(*segment-1));
						mcb_next.SetSize(*blocks);
						mcb_next.SetType(mcb.GetType());
						mcb_next.SetPSPSeg(dos.psp());
						mcb_next.SetFileName(psp_name);
						// Old Block
						mcb.SetSize(found_seg_size-*blocks-1);
						mcb.SetPSPSeg(MCB_FREE);
						mcb.SetType(0x4D);
						}
					return true;
					}
				// no fitting MCB found, return size of largest block
				*blocks = bigsize;
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				return false;
				}
			}
		else
			mcb_segment += mcb.GetSize()+1;
		}
	return false;
	}

bool DOS_ResizeMemory(Bit16u segment, Bit16u * blocks)
	{
	if (segment < DOS_MEM_START+1)
		LOG(LOG_DOSMISC, LOG_ERROR)("Program resizes %X, take care", segment);
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType() != 0x4d) && (mcb.GetType() != 0x5a))
		{
		DOS_SetError(DOSERR_MCB_DESTROYED);
		return false;
		}

	DOS_CompressMemory();
	Bit16u total = mcb.GetSize();
	if (*blocks == total)								// Same size, nothing to do
		return true;
	if (*blocks < total)								// Shrinking MCB
		{
		DOS_MCB	mcb_new_next(segment+(*blocks));
		mcb.SetSize(*blocks);
		mcb_new_next.SetType(mcb.GetType());
		if (mcb.GetType() == 0x5a)
			mcb.SetType(0x4d);							// Further blocks follow

		mcb_new_next.SetSize(total-*blocks-1);
		mcb_new_next.SetPSPSeg(MCB_FREE);
		mcb.SetPSPSeg(dos.psp());
		return true;
		}

	// MCB will grow, try to join with following MCB
	DOS_MCB	mcb_next(segment+total);
	if (mcb.GetType() != 0x5a)
		if (mcb_next.GetPSPSeg() == MCB_FREE)
			total += mcb_next.GetSize()+1;
	if (*blocks < total)
		{
		if (mcb.GetType() != 0x5a)
			mcb.SetType(mcb_next.GetType());			// Save type of following MCB
		mcb.SetSize(*blocks);
		mcb_next.SetPt((Bit16u)(segment+*blocks));
		mcb_next.SetSize(total-*blocks-1);
		mcb_next.SetType(mcb.GetType());
		mcb_next.SetPSPSeg(MCB_FREE);
		mcb.SetType(0x4d);
		mcb.SetPSPSeg(dos.psp());
		return true;
		}

	/* at this point: *blocks==total (fits) or *blocks>total,
	   in the second case resize block to maximum */

	if ((mcb_next.GetPSPSeg() == MCB_FREE) && (mcb.GetType() != 0x5a))
		mcb.SetType(mcb_next.GetType());		// adjust type of joined MCB
	mcb.SetSize(total);
	mcb.SetPSPSeg(dos.psp());
	if (*blocks == total)
		return true;	// block fit exactly

	*blocks = total;	// return maximum
	DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
	return false;
	}

bool DOS_FreeMemory(Bit16u segment)
	{
// TODO Check if allowed to free this segment
	if (segment < DOS_MEM_START+1)
		{
		LOG(LOG_DOSMISC, LOG_ERROR)("Program tried to free %X ---ERROR", segment);
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
		}
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType() != 0x4d) && (mcb.GetType() != 0x5a))
		{
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
		}
	mcb.SetPSPSeg(MCB_FREE);
//	DOS_CompressMemory();
	return true;
	}

void DOS_BuildUMBChain(bool umb_active)
	{
	if (umb_active)
		{
		Bit16u first_umb_seg = 0xd000;
		Bit16u first_umb_size = 0x2000;

		dos_infoblock.SetStartOfUMBChain(UMB_START_SEG);
		dos_infoblock.SetUMBChainState(0);		// UMBs not linked yet

		DOS_MCB umb_mcb(first_umb_seg);
		umb_mcb.SetPSPSeg(0);					// currently free
		umb_mcb.SetSize(first_umb_size-1);
		umb_mcb.SetType(0x5a);

		// Scan MCB-chain for last block
		Bit16u mcb_segment = dos.firstMCB;
		DOS_MCB mcb(mcb_segment);
		while (mcb.GetType() != 0x5a)
			{
			mcb_segment += mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
			}

		// A system MCB has to cover the space between the regular MCB-chain and the UMBs
		Bit16u cover_mcb = (Bit16u)(mcb_segment+mcb.GetSize()+1);
		mcb.SetPt(cover_mcb);
		mcb.SetType(0x4d);
		mcb.SetPSPSeg(0x0008);
		mcb.SetSize(first_umb_seg-cover_mcb-1);
		mcb.SetFileName("SC      ");
		}
	else
		{
		dos_infoblock.SetStartOfUMBChain(0xffff);
		dos_infoblock.SetUMBChainState(0);
		}
	}

bool DOS_LinkUMBsToMemChain(Bit16u linkstate)
	{
	// Get start of UMB-chain
	Bit16u umb_start = dos_infoblock.GetStartOfUMBChain();
	if (umb_start != UMB_START_SEG)
		{
		if (umb_start != 0xffff)
			LOG(LOG_DOSMISC, LOG_ERROR)("Corrupt UMB chain: %x", umb_start);
		return false;
		}

	if ((linkstate&1) == (dos_infoblock.GetUMBChainState()&1))
		return true;
	
	// Scan MCB-chain for last block before UMB-chain
	Bit16u mcb_segment = dos.firstMCB;
	Bit16u prev_mcb_segment = dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	while ((mcb_segment != umb_start) && (mcb.GetType() != 0x5a))
		{
		prev_mcb_segment = mcb_segment;
		mcb_segment += mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
		}
	DOS_MCB prev_mcb(prev_mcb_segment);

	switch (linkstate)
		{
	case 0:		// unlink
		if ((prev_mcb.GetType() == 0x4d) && (mcb_segment == umb_start))
			prev_mcb.SetType(0x5a);
		dos_infoblock.SetUMBChainState(0);
		break;
	case 1:		// link
		if (mcb.GetType() == 0x5a)
			{
			mcb.SetType(0x4d);
			dos_infoblock.SetUMBChainState(1);
			}
		break;
	default:
		LOG_MSG("Invalid link state %x when reconfiguring MCB chain", linkstate);
		return false;
		}
	return true;
	}

static Bitu DOS_default_handler(void)
	{
	LOG(LOG_CPU, LOG_ERROR)("DOS rerouted Interrupt Called %X", lastint);
	return CBRET_NONE;
	}

static	CALLBACK_HandlerObject callbackhandler;

void DOS_SetupMemory(bool low)
	{
	/* Let DOS claim a few bios interrupts. Makes vDos more compatible with 
	 * buggy games, which compare against the interrupt table. (probably a 
	 * broken linked list implementation) */
	callbackhandler.Allocate(&DOS_default_handler, "DOS default int");
	Bit16u ihseg = 0x70;
	Bit16u ihofs = 0x08;
	vPC_rStosb(ihseg, ihofs+0x00, (Bit8u)0xFE);						// GRP 4
	vPC_rStosb(ihseg, ihofs+0x01, (Bit8u)0x38);						// Extra Callback instruction
	vPC_rStosw(ihseg, ihofs+0x02, callbackhandler.Get_callback());	// The immediate word
	vPC_rStosb(ihseg, ihofs+0x04, (Bit8u)0xCF);						// An IRET Instruction
	RealSetVec(0x01, SegOff2dWord(ihseg, ihofs));					// BioMenace (offset!=4)
	RealSetVec(0x02, SegOff2dWord(ihseg, ihofs));					// BioMenace (segment<0x8000)
	RealSetVec(0x03, SegOff2dWord(ihseg, ihofs));					// Alien Incident (offset!=0)
	RealSetVec(0x04, SegOff2dWord(ihseg, ihofs));					// Shadow President (lower byte of segment!=0)
//	RealSetVec(0x0f, SegOff2dWord(ihseg, ihofs));					// Always a tricky one (soundblaster irq)

	// Create a dummy device MCB with PSPSeg=0x0008
	DOS_MCB mcb_devicedummy((Bit16u)DOS_MEM_START);
	mcb_devicedummy.SetPSPSeg(MCB_DOS);				// Devices
	mcb_devicedummy.SetSize(1);
	mcb_devicedummy.SetType(0x4d);					// More blocks will follow
//	mcb_devicedummy.SetFileName("SD      ");

	Bit16u mcb_sizes = 2;
	// Create a small empty MCB (result from a growing environment block)
//	DOS_MCB tempmcb((Bit16u)DOS_MEM_START+mcb_sizes);
//	tempmcb.SetPSPSeg(MCB_FREE);
//	tempmcb.SetSize(4);
//	mcb_sizes += 5;
//	tempmcb.SetType(0x4d);

	int extra_size = low ? 0 : 4096 - DOS_MEM_START - mcb_sizes - 17;	// disable first 64Kb if low not set in config
	// Lock the previous empty MCB
	DOS_MCB tempmcb2((Bit16u)DOS_MEM_START+mcb_sizes);
	tempmcb2.SetPSPSeg(0x40);
	tempmcb2.SetSize(16+extra_size);
	mcb_sizes += 17+extra_size;
	tempmcb2.SetType(0x4d);

	DOS_MCB mcb((Bit16u)DOS_MEM_START+mcb_sizes);
	mcb.SetPSPSeg(MCB_FREE);						// Free
	mcb.SetType(0x5a);								// Last Block
	mcb.SetSize(0x9FFE - DOS_MEM_START - mcb_sizes);

	dos.firstMCB = DOS_MEM_START;
	dos_infoblock.SetFirstMCB(DOS_MEM_START);
	}
