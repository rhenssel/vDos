#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "vDos.h"
#include "regs.h"
#include "control.h"
#include "shell.h"
#include "callback.h"
#include "support.h"

#include "mouse.h"

Bitu call_shellstop;
/* Larger scope so shell_del autoexec can use it to
 * remove things from the environment */
Program * first_shell = 0; 

static Bitu shellstop_handler(void)
	{
	return CBRET_STOP;
	}

static void SHELL_ProgramStart(Program * * make)
	{
	*make = new DOS_Shell;
	}

DOS_Shell::DOS_Shell():Program()
	{
	input_handle = STDIN;
	echo = true;
	exit = false;
	bf = 0;
	call = false;

	DOS_PSP psp(dos. psp());							// Close eventually inherited file entries/handles
	for (Bit16u entry = 5; entry < 20; entry++)			// Perhaps not completly fool proof, but ok
		{
		Bit8u handle = psp.GetFileHandle(entry);
		if ((handle < DOS_FILES) && Files[handle])
			{
			Files[handle]->RemoveRef();
			psp.SetFileHandle(entry, 0xff);
			}
		}
	}

Bitu DOS_Shell::GetRedirection(char *s, char **ifn, char **ofn, bool * append)
	{
	char* lr = s;
	char* lw = s;
	char ch;
	Bitu num = 0;
	bool quote = false;
	char* t;

	while ((ch = *lr++))
		{
		if (quote && ch != '"')	 // don't parse redirection within quotes. Not perfect yet. Escaped quotes will mess the count up
			{
			*lw++ = ch;
			continue;
			}
		switch (ch)
			{
		case '"':
			quote = !quote;
			break;
		case '>':
			*append = ((*lr) == '>');
			if (*append)
				lr++;
			lr = ltrim(lr);
			if (*ofn)
				free(*ofn);
			*ofn = lr;
			while (*lr && *lr != ' ' && *lr != '<' && *lr != '|')
				lr++;
			// if it ends on a : => remove it.
			if ((*ofn != lr) && (lr[-1] == ':'))
				lr[-1] = 0;
//			if(*lr && *(lr+1)) 
//				*lr++=0; 
//			else 
//				*lr=0;
			t = (char*)malloc(lr-*ofn+1);
			safe_strncpy(t, *ofn, lr-*ofn+1);
			*ofn = t;
			continue;
		case '<':
			if (*ifn)
				free(*ifn);
			lr = ltrim(lr);
			*ifn = lr;
			while (*lr && *lr != ' ' && *lr != '>' && *lr != '|')
				lr++;
			if ((*ifn != lr) && (lr[-1] == ':'))
				lr[-1] = 0;
//			if(*lr && *(lr+1)) 
//				*lr++=0; 
//			else 
//				*lr=0;
			t = (char*)malloc(lr-*ifn+1);
			safe_strncpy(t, *ifn, lr-*ifn+1);
			*ifn = t;
			continue;
		case '|':
			ch = 0;
			num++;
			}
		*lw++ = ch;
		}
	*lw = 0;
	return num;
	}	

void DOS_Shell::ParseLine(char * line)
	{
	line = trim(line);

 	if (bf && line[0] == '@')						// Check for a leading '@' in batchfile
		line = ltrim(++line);
//	if (!strnicmp(line, "rem ", 4))
//		return;

	// Do redirection and pipe checks
	char * in  = 0;
	char * out = 0;

	Bit16u dummy, dummy2;
	Bit32u bigdummy = 0;
	Bitu num = 0;									// Number of commands in this line
	bool append;
	bool normalstdin  = false;						// wether stdin/out are open on start
	bool normalstdout = false;						// Bug: Assumed is they are "con"
	
	num = GetRedirection(line, &in, &out, &append);
	if (in || out)
		{
		normalstdin  = (psp->GetFileHandle(0) != 0xff); 
		normalstdout = (psp->GetFileHandle(1) != 0xff); 
		}
	if (in)
		if (DOS_OpenFile(in, OPEN_READ, &dummy))	// Test if file can be opened for reading
			{
			DOS_CloseFile(dummy);
			if (normalstdin)
				DOS_CloseFile(0);					// Close stdin
			DOS_OpenFile(in, OPEN_READ, &dummy);	// Open new stdin
			}
	if (out)
		{
		if (normalstdout)
			DOS_CloseFile(1);
		if (!normalstdin && !in)
			DOS_OpenFile("con", OPEN_READWRITE, &dummy);
		bool status = true;
		// Create if not exist. Open if exist. Both in read/write mode
		if (append)
			{
			if (status = DOS_OpenFile(out, OPEN_READWRITE, &dummy))
				DOS_SeekFile(1, &bigdummy, DOS_SEEK_END);
			else
				status = DOS_CreateFile(out, DOS_ATTR_ARCHIVE, &dummy);	//Create if not exists.
			}
		else
			status = DOS_OpenFileExtended(out, OPEN_READWRITE, DOS_ATTR_ARCHIVE, 0x12, &dummy, &dummy2);

		if (!status && normalstdout)
			DOS_OpenFile("con", OPEN_READWRITE, &dummy);	// Read only file, open con again
		if (!normalstdin && !in)
			DOS_CloseFile(0);
		}

	DoCommand(line);	// Run the actual command
	// Restore handles
	if (in)
		{
		DOS_CloseFile(0);
		if (normalstdin)
			DOS_OpenFile("con", OPEN_READWRITE, &dummy);
		free(in);
		}
	if (out)
		{
		DOS_CloseFile(1);
		if (!normalstdin)
			DOS_OpenFile("con", OPEN_READWRITE, &dummy);
		if (normalstdout)
			DOS_OpenFile("con", OPEN_READWRITE, &dummy);
		if (!normalstdin)
			DOS_CloseFile(0);
		free(out);
		}
	}

void DOS_Shell::RunInternal(void)
	{
	char input_line[CMD_MAXLINE] = {0};
	while (bf && bf->ReadLine(input_line)) 
		{
		if (echo && input_line[strspn(input_line, " \t")] != '@')
			{
			ShowPrompt();
			WriteOut_NoParsing(input_line);
			WriteOut("\n");
			}
		ParseLine(input_line);
		}
	return;
	}

void DOS_Shell::Run(void)
	{
	char input_line[CMD_MAXLINE] = {0};
	std::string line;

	if (cmd->FindStringRemain("/C", line) || cmd->FindStringBegin("/C", line, false) || cmd->FindStringBegin("/c", line, false))
		{
		strcpy(input_line, line.c_str());
		DOS_Shell temp;
		temp.echo = echo;
		temp.ParseLine(input_line);		// for *.exe *.com  |*.bat creates the bf needed by runinternal;
		temp.RunInternal();				// exits when no bf is found.
		return;
		}
	// Start a normal shell and check for a first command init
	if (cmd->FindString("/INIT", line, true))
		{
		strcpy(input_line, line.c_str());
		line.erase();
		ParseLine(input_line);
		}
	do
		{
		if (bf)
			{
			if (bf->ReadLine(input_line))
				{
				if (echo && input_line[strspn(input_line, " \t")] != '@')
					{
					ShowPrompt();
					WriteOut_NoParsing(input_line);
					WriteOut("\n");
					}
				ParseLine(input_line);
				if (echo)
					WriteOut("\n");
				}
			}
		else
			{
			if (echo)
				ShowPrompt();
			InputCommand(input_line);
			char * content = input_line;
			while (*content == ' ' || *content == '\t')			// strip leading spaces
				content++;
			if (*content)										// If not empty
				{
				ParseLine(content);
				if (echo && !bf)
					WriteOut("\n");
				}
			}
		}
	while (!exit);
	}

char * init_line = "";

void AUTOEXEC_Init(Section * sec)
	{
	FILE * aef = fopen("autoexec.txt", "rb");
	if (aef)
		{
		int fsize = _filelength(_fileno(aef));
		if (fsize > 0 && fsize < 4096)							// just ignore if greater than 4K (probably something else)
			{
			void * aedata = malloc(fsize);						// MEM LEAK, so what?
			fread(aedata, 1, fsize, aef);
			fclose(aef);
			VFILE_Register("AUTOEXEC.BAT", (Bit8u *)aedata, (Bit16u)fsize);
			init_line = "/INIT AUTOEXEC.BAT";
			return;
			}
		fclose(aef);
		}
	}

void SHELL_Init()
	{
	// Regular startup
	call_shellstop = CALLBACK_Allocate();
	// Setup the startup CS:IP to kill the last running machine when exitted
	RealPt newcsip = CALLBACK_RealPointer(call_shellstop);
	SegSet16(cs, RealSeg(newcsip));
	reg_ip = RealOff(newcsip);

	CALLBACK_Setup(call_shellstop, shellstop_handler, CB_IRET, "shell stop");
	PROGRAMS_MakeFile("COMMAND.COM", SHELL_ProgramStart);

	// Now call up the shell for the first time
	Bit16u psp_seg = DOS_FIRST_SHELL;
	Bit16u env_seg = DOS_FIRST_SHELL+19;
	//DOS_GetMemory(1+(4096/16))+1;
	Bit16u stack_seg = DOS_GetMemory(2048/16);
	SegSet16(ss, stack_seg);
	reg_sp = 2046;

	// Set up int 24 and psp (Telarium games)
	vPC_rStosb(psp_seg+16+1, 0, 0xea);		// far jmp
	vPC_rStosd(psp_seg+16+1, 1, vPC_rLodsd(0, 0x24*4));
	vPC_rStosd(0x24*4, ((Bit32u)psp_seg<<16) | ((16+1)<<4));

	// Set up int 23 to "int 20" in the psp. Fixes what.exe
	vPC_rStosd(0x23*4, ((Bit32u)psp_seg<<16));

	// Setup MCBs
	DOS_MCB pspmcb((Bit16u)(psp_seg-1));
	pspmcb.SetPSPSeg(psp_seg);	// MCB of the command shell psp
	pspmcb.SetSize(0x10+2);
	pspmcb.SetType(0x4d);
	DOS_MCB envmcb((Bit16u)(env_seg-1));
	envmcb.SetPSPSeg(psp_seg);	// MCB of the command shell environment
	envmcb.SetSize(DOS_MEM_START-env_seg);
	envmcb.SetType(0x4d);
	
	// Setup environment
	char env[] = "PROMPT=$P$G\0PATH=Z:\\\0COMSPEC=Z:\\COMMAND.COM\0\0\1\0Z:\\COMMAND.COM\0";
	vPC_rBlockWrite(SegOff2Ptr(env_seg, 0), env, sizeof(env));

	DOS_PSP psp(psp_seg);
	psp.MakeNew(0);
	dos.psp(psp_seg);

	/* The start of the file handle array in the psp must look like this:
	 * 01 01 01 00 02
	 * In order to achieve this: First open 2 files. Close the first and
	 * duplicate the second (so the entries get 01) */
	Bit16u dummy = 0;
	DOS_OpenFile("CON", OPEN_READWRITE, &dummy);	// STDIN
	DOS_OpenFile("CON", OPEN_READWRITE, &dummy);	// STDOUT
	DOS_CloseFile(0);								// Close STDIN
	DOS_ForceDuplicateEntry(1, 0);					// "new" STDIN
	DOS_ForceDuplicateEntry(1, 2);					// STDERR
	DOS_OpenFile("CON", OPEN_READWRITE, &dummy);	// STDAUX
	DOS_OpenFile("PRN", OPEN_READWRITE, &dummy);	// STDPRN		troubles me with FiAd, accessing it in subprograms and getting closed

	psp.SetParent(psp_seg);
	// Set the environment
	psp.SetEnvironment(env_seg);
	// Set the command line for the shell start up
	CommandTail tail;
	tail.count = (Bit8u)strlen(init_line);
	strcpy(tail.buffer, init_line);
	vPC_rBlockWrite(SegOff2Ptr(psp_seg, 128), &tail, 128);
	
	// Setup internal DOS Variables
	dos.dta(SegOff2dWord(psp_seg, 0x80));
	dos.psp(psp_seg);
	
	SHELL_ProgramStart(&first_shell);
	first_shell->Run();
	delete first_shell;
	first_shell = 0;		// Make clear that it shouldn't be used anymore
	}
