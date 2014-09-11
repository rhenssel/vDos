#include <string.h>
#include <ctype.h>
#include <process.h>
#include "vDos.h"
#include "mem.h"
#include "dos_inc.h"
#include "regs.h"
#include "callback.h"
#include "cpu.h"
#include "shell.h"

#ifdef _MSC_VER
#pragma pack(1)
#endif
struct EXE_Header {
	Bit16u signature;					// EXE Signature MZ or ZM
	Bit16u extrabytes;					// Bytes on the last page
	Bit16u pages;						// Pages in file
	Bit16u relocations;					// Relocations in file
	Bit16u headersize;					// Paragraphs in header
	Bit16u minmemory;					// Minimum amount of memory
	Bit16u maxmemory;					// Maximum amount of memory
	Bit16u initSS;
	Bit16u initSP;
	Bit16u checksum;
	Bit16u initIP;
	Bit16u initCS;
	Bit16u reloctable;					// 40h or greater for new-format (NE, LE, LX, W3, PE, etc.) executable
	Bit16u overlay;
};
#ifdef _MSC_VER
#pragma pack()
#endif

#define MAGIC1 0x5a4d
#define MAGIC2 0x4d5a
#define MAXENV 512
#define LOADNGO 0
#define LOAD    1
#define OVERLAY 3

static void SaveRegisters(void)
	{
	reg_sp -= 18;
	PhysPt ss_sp = SegPhys(ss)+reg_sp;
	vPC_rStosw(ss_sp+ 0, reg_ax);
	vPC_rStosw(ss_sp+ 2, reg_cx);
	vPC_rStosw(ss_sp+ 4, reg_dx);
	vPC_rStosw(ss_sp+ 6, reg_bx);
	vPC_rStosw(ss_sp+ 8, reg_si);
	vPC_rStosw(ss_sp+10, reg_di);
	vPC_rStosw(ss_sp+12, reg_bp);
	vPC_rStosw(ss_sp+14, SegValue(ds));
	vPC_rStosw(ss_sp+16, SegValue(es));
	}

static void RestoreRegisters(void)
	{
	PhysPt ss_sp = SegPhys(ss)+reg_sp;
	reg_ax = vPC_rLodsw(ss_sp+ 0);
	reg_cx = vPC_rLodsw(ss_sp+ 2);
	reg_dx = vPC_rLodsw(ss_sp+ 4);
	reg_bx = vPC_rLodsw(ss_sp+ 6);
	reg_si = vPC_rLodsw(ss_sp+ 8);
	reg_di = vPC_rLodsw(ss_sp+10);
	reg_bp = vPC_rLodsw(ss_sp+12);
	SegSet16(ds, vPC_rLodsw(ss_sp+14));
	SegSet16(es, vPC_rLodsw(ss_sp+16));
	reg_sp += 18;
	}

void DOS_Terminate(Bit16u pspseg, bool tsr, Bit8u exitcode)
	{
	dos.return_code = exitcode;
	dos.return_mode = (tsr) ? (Bit8u)RETURN_TSR : (Bit8u)RETURN_EXIT;
	
	DOS_PSP curpsp(pspseg);
	if (pspseg == curpsp.GetParent())
		return;
	if (!tsr)																	// Free Files owned by process
		curpsp.CloseFiles();
	
	RealPt old22 = curpsp.GetInt22();											// Get the termination address
	curpsp.RestoreVectors();													// Restore vector 22, 23, 24
	dos.psp(curpsp.GetParent());												// Set the parent PSP
	DOS_PSP parentpsp(curpsp.GetParent());

	SegSet16(ss,RealSeg(parentpsp.GetStack()));									// Restore the SS:SP to the previous one
	reg_sp = RealOff(parentpsp.GetStack());

	RestoreRegisters();															// Restore the old CS:IP from int 22h

	vPC_rStosd(SegPhys(ss)+reg_sp, old22);										// Set the CS:IP stored in int 0x22 back on the stack
	vPC_rStosw(SegPhys(ss)+reg_sp+4, 0x7202);									// set IOPL=3 (Strike Commander), nested task set, interrupts enabled, test flags cleared

	if (!tsr)																	// Free memory owned by process
		DOS_FreeProcessMemory(pspseg);
	}

static bool MakeEnv(char *name, Bit16u *segment)
	{
	char namebuf[DOS_PATHLENGTH];
	if (!DOS_Canonicalize(name, namebuf))								// Shouldn't happen, but test now
		return false;													// To prevent memory allocation

	char * envread, * envwrite;											// If segment to copy environment is 0 copy the caller's environment
	if (*segment == 0)
		{
		DOS_PSP psp(dos.psp());
		envread = (char *)(MemBase + (psp.GetEnvironment()<<4));
		}
	else
		envread = (char *)(MemBase + (*segment<<4));

	Bit16u envsize = 1;
	if (envread)
		while (*(envread+envsize))
			envsize += strlen(envread+envsize)+1;
	envsize++;															// Account for trailing \0\0 (note we started at offset 1)
	if (envsize+strlen(namebuf)+3 > MAXENV)								// Room to append \1\0filespec\0?
		return false;
	Bit16u size = MAXENV/16;
	if (!DOS_AllocateMemory(segment, &size))
		return false;
	envwrite = (char *)(MemBase + (*segment<<4));
	if (envread)
		memcpy(envwrite, envread, envsize);
	else
		*((Bit16u *)envwrite) = 0;
	envwrite += envsize;
	*((Bit16u *)envwrite) = 1;
	envwrite += 2;
	envread = namebuf;
	while (*envread)													// Pathname should be in uppercase
		*envwrite++ =  toupper(*(envread++));
	*envwrite = 0;
	return true;
	}

bool DOS_NewPSP(Bit16u segment, Bit16u size)
	{
	DOS_PSP psp(segment);
	psp.MakeNew(size);
	Bit16u parent_psp_seg = psp.GetParent();
	DOS_PSP psp_parent(parent_psp_seg);
	psp.CopyFileTable(&psp_parent, false);
	psp.SetCommandTail(SegOff2dWord(parent_psp_seg, 0x80));						// Copy command line as well (Kings Quest AGI -cga switch)
	return true;
	}

bool DOS_ChildPSP(Bit16u segment, Bit16u size)
	{
	DOS_PSP psp(segment);
	psp.MakeNew(size);
	Bit16u parent_psp_seg = psp.GetParent();
	DOS_PSP psp_parent(parent_psp_seg);
	psp.CopyFileTable(&psp_parent, true);
	psp.SetCommandTail(SegOff2dWord(parent_psp_seg, 0x80));
	psp.SetFCB1(SegOff2dWord(parent_psp_seg, 0x5c));
	psp.SetFCB2(SegOff2dWord(parent_psp_seg, 0x6c));
	psp.SetEnvironment(psp_parent.GetEnvironment());
	psp.SetSize(size);
	return true;
	}

static void SetupPSP(Bit16u pspseg, Bit16u memsize, Bit16u envseg)
	{
	DOS_MCB mcb((Bit16u)(pspseg-1));											// Fix the PSP for psp and environment MCB's
	mcb.SetPSPSeg(pspseg);
	mcb.SetPt((Bit16u)(envseg-1));
	mcb.SetPSPSeg(pspseg);

	DOS_PSP psp(pspseg);
	psp.MakeNew(memsize);
	psp.SetEnvironment(envseg);

	DOS_PSP oldpsp(dos.psp());													// Copy file handles
	psp.CopyFileTable(&oldpsp, true);
	}

static void SetupCMDLine(Bit16u pspseg, DOS_ParamBlock & block)
	{
	DOS_PSP psp(pspseg);
	psp.SetCommandTail(block.exec.cmdtail);										// If cmdtail==0 it will inited as empty in SetCommandTail
	}

bool DOS_Execute(char * name, PhysPt block_pt, Bit8u flags)
	{
	EXE_Header head;
	Bit16u fhandle;
	Bit16u fLen;
	Bit32u fPos;
	Bit16u pspseg, envseg, loadseg, memsize, readsize;
	Bitu headersize = 0, imagesize = 0;
	DOS_ParamBlock block(block_pt);
	bool isCom = false;

	block.LoadData();

	if (flags != LOADNGO && flags != OVERLAY && flags != LOAD)
		{
		DOS_SetError(DOSERR_FORMAT_INVALID);
		return false;
		}
	if (!DOS_OpenFile(name, OPEN_READ, &fhandle))
		return false;
	fLen = sizeof(EXE_Header);													// Check for EXE or COM File
	if (!DOS_ReadFile(fhandle, (Bit8u *)&head, &fLen))
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		DOS_CloseFile(fhandle);
		return false;
		}
	if (fLen < sizeof(EXE_Header))
		{
		if (fLen == 0)															// Prevent executing zero byte files
			{
			DOS_SetError(DOSERR_ACCESS_DENIED);
			DOS_CloseFile(fhandle);
			return false;
			}
		isCom = true;															// If less than header size: .com file
		}
	else
		{
		if ((head.signature != MAGIC1) && (head.signature != MAGIC2))
			isCom = true;
		else
			{
//			if (head.reloctable == 0x40)										// Non-Dos program (docu mentions > 40h, but PKLITE/Dos has 54)
			if (head.reloctable >= 0x40 && head.initIP == 0 && head.initCS == 0)	// Non-Dos program
				{																// Init.. always 0 with Windows? Q&D
				DOS_CloseFile(fhandle);
				if (vPC_rLodsw(block_pt))										// First word of epb_block should be 0
					{
					DOS_SetError(DOSERR_FORMAT_INVALID);
					return false;
					}
				char comline[256];
				char winDirCur[512];											// Setting Windows directory to DOS drive+current directory
				char winDirNew[512];											// and calling the program
				char winName[256];
				Bit8u drive;

				DOS_MakeName(name, winDirNew, &drive);							// Mainly to get the drive and pathname w/o it
				if (drive != 25 && GetCurrentDirectory(512, winDirCur))			// Can't have DOS Z: as Windows current directory
					{
					strcpy(winName, Drives[drive]->GetInfo());
					strcat(winName, winDirNew);
					strcpy(winDirNew, Drives[DOS_GetDefaultDrive()]->GetInfo());// Windows directory of DOS drive
					strcat(winDirNew, Drives[DOS_GetDefaultDrive()]->curdir);	// Append DOS current directory
					if (SetCurrentDirectory(winDirNew))
						{
						PhysPt comPtr = SegOff2Ptr(vPC_rLodsw(block_pt+4), vPC_rLodsw(block_pt+2));
						memset(comline, 0, 256);
						vPC_rBlockRead(comPtr+1, comline, vPC_rLodsb(comPtr));	// Get commandline, directories are supposed Windows at this moment!
						if (_spawnl(P_NOWAIT, winName, winName, comline, NULL) != -1)
							{
							SetCurrentDirectory(winDirCur);
							return true;
							}
						SetCurrentDirectory(winDirCur);
						}
					}
				DOS_SetError(DOSERR_FILE_NOT_FOUND);							// jsut pick one
				return false;
				}
			head.pages &= 0x07ff;
			headersize = head.headersize*16;
			imagesize = head.pages*512-headersize; 
			if (imagesize+headersize < 512)
				imagesize = 512-headersize;
			}
		}

	if (flags != OVERLAY)
		{
		Bit16u minsize;
		Bit16u maxsize = 0xffff;
		if (isCom)
			{
			fPos = 0;															// Reduce minimum of needed memory size to filesize
			DOS_SeekFile(fhandle, &fPos, DOS_SEEK_END);
			if (fPos > 0xff00)													// Maximum file size is 64KB - 256 bytes
				{
				DOS_CloseFile(fhandle);
				DOS_SetError(DOSERR_FORMAT_INVALID);
				return false;
				}
			minsize = (Bit16u)(fPos>>4) + 256;
			}
		else
			{
			minsize = long2para(imagesize+(head.minmemory<<4)+256);				// Exe size calculated from header
			if (head.maxmemory != 0)
				maxsize = long2para(imagesize+(head.maxmemory<<4)+256);
			}
		Bit8u UMB_flag, old_memstrat;
		Bit16u UMB_total, UMB_largest, UMB_count;
		Bit16u maxfree = 0xffff;
		if (LoadHigh)
			{
			DOS_GetFreeUMB(&UMB_total, &UMB_largest, &UMB_count);
			if (UMB_largest > (minsize+MAXENV/16))								// For simplisity, LoadHigh if enough to store environment + program
				{
				UMB_flag = dos_infoblock.GetUMBChainState();
				old_memstrat = (Bit8u)(DOS_GetMemAllocStrategy()&0xff);
				if ((UMB_flag&1) == 0)
					DOS_LinkUMBsToMemChain(1);
				DOS_SetMemAllocStrategy(0x41);									// Search in UMBs best fit
				maxfree = UMB_largest - 512 - 16;
				}
			else
				LoadHigh = false;
			}
		envseg = block.exec.envseg;
		if (!MakeEnv(name, &envseg))											// Create an environment block
			{
			DOS_CloseFile(fhandle);
			return false;
			}
		if (!LoadHigh)
			{
			DOS_AllocateMemory(&pspseg, &maxfree);
			if (maxfree < minsize)
				{
				DOS_CloseFile(fhandle);
				DOS_FreeMemory(envseg);
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				return false;
				}
			}
		memsize = min(maxfree, maxsize);
		if (!DOS_AllocateMemory(&pspseg, &memsize))
			E_Exit("DOS: Exec error in memory");
		if (LoadHigh)
			{
			DOS_LinkUMBsToMemChain(UMB_flag);
			DOS_SetMemAllocStrategy(old_memstrat);								// Restore strategy
			LoadHigh = false;
			}
		loadseg = pspseg+16;
		if (!isCom)
			if ((head.minmemory == 0) && (head.maxmemory == 0))					// Load program into upper part of allocated memory?
				loadseg = (Bit16u)(((pspseg+memsize)*0x10-imagesize)/0x10);
		}
	else
		{
		if (isCom)																// Can't overlay a .COM
			{
			DOS_SetError(DOSERR_FORMAT_INVALID);
			DOS_CloseFile(fhandle);
			return false;
			}
		loadseg = block.overlay.loadseg;
		}
	Bit8u* loadaddress = MemBase+(loadseg<<4);									// Load the executable

	if (isCom)																	// COM Load 64k - 256 bytes max
		{
		fPos = 0;
		DOS_SeekFile(fhandle, &fPos, DOS_SEEK_SET);	
		readsize = 0xffff-256;
		DOS_ReadFile(fhandle, loadaddress, &readsize);
		}
	else																		// Load EXE in 32kb blocks and then relocates
		{
		fPos = headersize;
		DOS_SeekFile(fhandle, &fPos, DOS_SEEK_SET);
		while (readsize = min(0x8000, imagesize))
			{
			imagesize -= readsize;												// Sometimes imagsize != filesize
			DOS_ReadFile(fhandle, loadaddress, &readsize);
			loadaddress += readsize;
			}
		if (head.relocations)
			{																	// Relocate the exe image
			Bit16u relocate;
			if (flags == OVERLAY)
				relocate = block.overlay.relocation;
			else
				relocate = loadseg;
			fPos = head.reloctable;
			DOS_SeekFile(fhandle, &fPos, DOS_SEEK_SET);
			RealPt * relocpts = new Bit32u[head.relocations];
			readsize = 4*head.relocations;
			DOS_ReadFile(fhandle, (Bit8u *)relocpts, &readsize);
			for (int i = 0; i < head.relocations; i++)
				{
				PhysPt address = SegOff2Ptr(RealSeg(relocpts[i])+loadseg, RealOff(relocpts[i]));
				vPC_rStosw(address, vPC_rLodsw(address)+relocate);
				}
			delete[] relocpts;
			}
		}
	DOS_CloseFile(fhandle);

	if (flags == OVERLAY)
		return true;															// Everything done for overlays

	SetupPSP(pspseg, memsize, envseg);											// Create psp after closing exe, to avoid dead file handle of exe in copied psp
	SetupCMDLine(pspseg, block);
	RealPt csip, sssp;
	if (isCom)
		{
		csip = SegOff2dWord(pspseg, 0x100);
		sssp = SegOff2dWord(pspseg, 0xfffe);
		vPC_rStosw(SegOff2Ptr(pspseg, 0xfffe), 0);
		}
	else
		{
		if (head.initSP < 4)
			E_Exit("Initial SP value too low");
		csip = SegOff2dWord(loadseg+head.initCS, head.initIP);
		sssp = SegOff2dWord(loadseg+head.initSS, head.initSP);
		}

	if (flags == LOAD)
		{
		SaveRegisters();
		DOS_PSP callpsp(dos.psp());
		// Save the SS:SP on the PSP of calling program
		callpsp.SetStack(RealMakeSeg(ss, reg_sp));
		reg_sp += 18;
		// Switch the psp's
		dos.psp(pspseg);
		DOS_PSP newpsp(dos.psp());
		dos.dta(SegOff2dWord(newpsp.GetSegment(), 0x80));
		// First word on the stack is the value ax should contain on startup
		vPC_rStosw(RealSeg(sssp-2), RealOff(sssp-2), 0xffff);
		block.exec.initsssp = sssp-2;
		block.exec.initcsip = csip;
		block.SaveData();
		return true;
		}

	if (flags == LOADNGO)
		{
		if ((reg_sp > 0xfffe) || (reg_sp < 18))
			LOG(LOG_EXEC, LOG_ERROR)("stack underflow/wrap at EXEC");
		// Get Caller's program CS:IP of the stack and set termination address to that
		RealSetVec(0x22, SegOff2dWord(vPC_rLodsw(SegPhys(ss)+reg_sp+2), vPC_rLodsw(SegPhys(ss)+reg_sp)));
		SaveRegisters();
		DOS_PSP callpsp(dos.psp());
		// Save the SS:SP on the PSP of calling program
		callpsp.SetStack(RealMakeSeg(ss, reg_sp));
		// Switch the psp's and set new DTA
		dos.psp(pspseg);
		DOS_PSP newpsp(dos.psp());
		dos.dta(SegOff2dWord(newpsp.GetSegment(), 0x80));
		// save vectors
		newpsp.SaveVectors();
		// copy fcbs
		newpsp.SetFCB1(block.exec.fcb1);
		newpsp.SetFCB2(block.exec.fcb2);
		// Set the stack for new program
		SegSet16(ss, RealSeg(sssp));
		reg_sp = RealOff(sssp);
		// Add some flags and CS:IP on the stack for the IRET
		CPU_Push16(RealSeg(csip));
		CPU_Push16(RealOff(csip));
		/* DOS starts programs with a RETF, so critical flags
		 * should not be modified (IOPL in v86 mode);
		 * interrupt flag is set explicitly, test flags cleared */
		reg_flags = (reg_flags&(~FMASK_TEST))|FLAG_IF;
		// Jump to retf so that we only need to store cs:ip on the stack
		reg_ip++;
		// Setup the rest of the registers
		reg_ax = reg_bx = 0;
		reg_cx = 0xff;
		reg_dx = pspseg;
		reg_si = RealOff(csip);
		reg_di = RealOff(sssp);
		reg_bp = 0x91c;																// DOS internal stack begin relict
		SegSet16(ds, pspseg);
		SegSet16(es, pspseg);
		char bareName[8] = { 0 };													// Add the filename to PSP and environment's MCB
		int i = 0;
		for (char *ptr = name; *ptr; ptr++)
			{
			switch (*ptr)
				{
			case ':': case '\\':
				i = 0;
				memset(bareName, 0, 8);
				break;
			case '.':
				i = 8;
				break;
			default:
				if (i < 8)
					bareName[i++] = (char)toupper(*ptr);
				}
			}
		DOS_MCB pspmcb(dos.psp()-1);
		pspmcb.SetFileName(bareName);
		return true;
		}
	return false;
	}
