#include "vDos.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include <list>

static Bitu call_int2f, call_int2a;

static std::list<MultiplexHandler*> Multiplex;
typedef std::list<MultiplexHandler*>::iterator Multiplex_it;

void DOS_AddMultiplexHandler(MultiplexHandler * handler)
	{
	Multiplex.push_front(handler);
	}

void DOS_DelMultiplexHandler(MultiplexHandler * handler)
	{
	for (Multiplex_it it = Multiplex.begin(); it != Multiplex.end(); it++)
		if(*it == handler)
			{
			Multiplex.erase(it);
			return;
			}
	}

static Bitu INT2F_Handler(void)
	{
	for (Multiplex_it it = Multiplex.begin(); it != Multiplex.end(); it++)
		if((*it)())
			return CBRET_NONE;
	LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Multiplex Unhandled call %4X", reg_ax);
	return CBRET_NONE;
	}

static Bitu INT2A_Handler(void)
	{
	return CBRET_NONE;
	}

static bool DOS_MultiplexFunctions(void)
	{
	switch (reg_ax)
		{
	case 0x1000:		// SHARE.EXE installation check
		reg_ax=0xffff;	// Pretend that share.exe is installed.
		break;
	case 0x1216:		// GET ADDRESS OF SYSTEM FILE TABLE ENTRY
		// reg_bx is a system file table entry, should coincide with
		// the file handle so just use that
		LOG(LOG_DOSMISC,LOG_ERROR)("Some BAD filetable call used bx=%X", reg_bx);
		if (reg_bx <= DOS_FILES)
			CALLBACK_SCF(false);
		else
			CALLBACK_SCF(true);
		if (reg_bx < 16)
			{
			RealPt sftrealpt = vPC_rLodsd(dWord2Ptr(dos_infoblock.GetPointer())+4);
			PhysPt sftptr = dWord2Ptr(sftrealpt);
			Bitu sftofs = 0x06+reg_bx*0x3b;

			if (Files[reg_bx])
				vPC_rStosb(sftptr+sftofs, Files[reg_bx]->refCtr);
			else
				vPC_rStosb(sftptr+sftofs, 0);

			if (!Files[reg_bx])
				return true;

			if (RealHandle(reg_bx) >= DOS_FILES)
				{
				vPC_rStosw(sftptr+sftofs+0x02, 0x02);	// file open mode
				vPC_rStosb(sftptr+sftofs+0x04, 0x00);	// file attribute
				vPC_rStosw(sftptr+sftofs+0x05, Files[reg_bx]->GetInformation());	// device info word
				vPC_rStosd(sftptr+sftofs+0x07, 0);		// device driver header
				vPC_rStosd(sftptr+sftofs+0x0d, 0);		// packed time + date
				vPC_rStosw(sftptr+sftofs+0x11, 0);		// size
				vPC_rStosw(sftptr+sftofs+0x15, 0);		// current position
				}
			else
				{
				Bit8u drive = Files[reg_bx]->GetDrive();
				vPC_rStosw(sftptr+sftofs+0x02, (Bit16u)(Files[reg_bx]->flags&3));	// file open mode
				vPC_rStosb(sftptr+sftofs+0x04, (Bit8u)(Files[reg_bx]->attr));		// file attribute
				vPC_rStosw(sftptr+sftofs+0x05, 0x40|drive);							// device info word
				vPC_rStosd(sftptr+sftofs+0x07, SegOff2dWord(dos.tables.dpb, drive));// dpb of the drive
				vPC_rStosw(sftptr+sftofs+0x0d, Files[reg_bx]->time);				// packed file time
				vPC_rStosw(sftptr+sftofs+0x0f, Files[reg_bx]->date);				// packed file date
				Bit32u curpos = 0;
				Files[reg_bx]->Seek(&curpos, DOS_SEEK_CUR);
				Bit32u endpos = 0;
				Files[reg_bx]->Seek(&endpos, DOS_SEEK_END);
				vPC_rStosd(sftptr+sftofs+0x11, endpos);		// size
				vPC_rStosd(sftptr+sftofs+0x15, curpos);		// current position
				Files[reg_bx]->Seek(&curpos, DOS_SEEK_SET);
				}

			// fill in filename in fcb style
			// (space-padded name (8 chars)+space-padded extension (3 chars))
			const char* filename=(const char*)Files[reg_bx]->GetName();
			if (strrchr(filename, '\\'))
				filename = strrchr(filename,'\\')+1;
			if (strrchr(filename, '/'))
				filename = strrchr(filename, '/')+1;
			if (!filename)
				return true;
			const char* dotpos = strrchr(filename, '.');
			if (dotpos)
				{
				dotpos++;
				size_t nlen = strlen(filename);
				size_t extlen = strlen(dotpos);
				Bits nmelen = (Bits)nlen-(Bits)extlen;
				if (nmelen < 1)
					return true;
				nlen -= (extlen+1);
				if (nlen > 8)
					nlen = 8;
				size_t i;
				for (i = 0; i < nlen; i++)
					vPC_rStosb((PhysPt)(sftptr+sftofs+0x20+i), filename[i]);
				for (i = nlen; i < 8; i++)
					vPC_rStosb((PhysPt)(sftptr+sftofs+0x20+i), ' ');
				
				if (extlen > 3)
					extlen = 3;
				for (i = 0; i < extlen; i++)
					vPC_rStosb((PhysPt)(sftptr+sftofs+0x28+i), dotpos[i]);
				for (i = extlen; i < 3; i++)
					vPC_rStosb((PhysPt)(sftptr+sftofs+0x28+i), ' ');
				}
			else
				{
				size_t i;
				size_t nlen = strlen(filename);
				if (nlen > 8)
					nlen = 8;
				for (i = 0; i < nlen; i++)
					vPC_rStosb((PhysPt)(sftptr+sftofs+0x20+i), filename[i]);
				for (i = nlen; i < 11; i++)
					vPC_rStosb((PhysPt)(sftptr+sftofs+0x20+i), ' ');
				}

			SegSet16(es, RealSeg(sftrealpt));
			reg_di = RealOff(sftrealpt+sftofs);
			reg_ax = 0xc000;
			}
		return true;
	case 0x1680:	//  RELEASE CURRENT VIRTUAL MACHINE TIME-SLICE
		// TODO Maybe do some idling but could screw up other systems :)
		return true;	// So no warning in the debugger anymore
	case 0x1689:	//  Kernel IDLE CALL
		return true;
	case 0x4a01:	// Query free hma space
	case 0x4a02:	// ALLOCATE HMA SPACE
		reg_bx = 0;	// number of bytes available in HMA or amount successfully allocated
		//ESDI=ffff:ffff Location of HMA/Allocated memory
		SegSet16(es, 0xffff);
		reg_di = 0xffff;
		return true;
		}
	return false;
	}

void DOS_SetupMisc(void)
	{
	// Setup the dos multiplex interrupt
	call_int2f = CALLBACK_Allocate();
	CALLBACK_Setup(call_int2f, &INT2F_Handler, CB_IRET, "DOS Int 2f");
	RealSetVec(0x2f, CALLBACK_RealPointer(call_int2f));
	DOS_AddMultiplexHandler(DOS_MultiplexFunctions);
	// Setup the dos network interrupt
	call_int2a = CALLBACK_Allocate();
	CALLBACK_Setup(call_int2a, &INT2A_Handler, CB_IRET, "DOS Int 2a");
	RealSetVec(0x2A, CALLBACK_RealPointer(call_int2a));
	}
