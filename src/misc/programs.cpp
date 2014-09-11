#include <stdlib.h>
#include "programs.h"
#include "callback.h"
#include "support.h"
#include "shell.h"

// This registers a file on the virtual drive and creates the correct structure for it

static Bit8u exe_block[]={
	0xbc, 0x00, 0x04,				// MOV SP, 0x400 decrease stack size
	0xbb, 0x40, 0x00,				// MOV BX, 0x040 for memory resize
	0xb4, 0x4a,						// MOV AH, 0x4A	Resize memory block
	0xcd, 0x21,						// INT 0x21
//pos 12 is callback number
	0xFE, 0x38, 0x00, 0x00,			// CALLBack number
	0xb8, 0x00, 0x4c,				// Mov ax, 4c00
	0xcd, 0x21,						// INT 0x21
};

#define CB_POS 12

static std::vector<PROGRAMS_Main*> internal_progs;

void PROGRAMS_MakeFile(char const * const name, PROGRAMS_Main * main)
	{
	Bit8u * comdata = (Bit8u *)malloc(32);						// MEM LEAK
	memcpy(comdata, &exe_block, sizeof(exe_block));

	// Copy save the pointer in the vector and save it's index
	if (internal_progs.size() > 254)
		E_Exit("PROGRAMS_MakeFile: Too many programs");
	comdata[sizeof(exe_block)] = (Bit8u)internal_progs.size();
	internal_progs.push_back(main);
	VFILE_Register(name, comdata, sizeof(exe_block)+1);
	}

static Bitu PROGRAMS_Handler(void)
	{
	// This sets up everything for a program start up call
	Bit8u index = vPC_rLodsb(SegOff2Ptr(dos.psp(), 256+sizeof(exe_block)));		// Read the index from program code in memory
	if (index > internal_progs.size())
		E_Exit("Something is messing with the memory");
	Program * new_program;
	PROGRAMS_Main * handler = internal_progs[index];
	(*handler)(&new_program);
	new_program->Run();
	delete new_program;
	return CBRET_NONE;
	}

// Main functions used in all programs

Program::Program()
	{
	psp = new DOS_PSP(dos.psp());												// Setup the PSP and find get command line
	CommandTail tail;
	vPC_rBlockRead(SegOff2Ptr(dos.psp(), 128), &tail, 128);
	tail.buffer[tail.count < 127 ? tail.count : 126] = 0;

	char * envPtr = (char *)(MemBase + (psp->GetEnvironment()<<4));				// Scan environment for filename
	while (*envPtr)
		envPtr += strlen(envPtr)+1;
	envPtr += 3;
	cmd = new CommandLine(envPtr, tail.buffer);
	}

void Program::WriteOut(const char * format,...)
	{
	char buf[1024];
	va_list msg;
	
	va_start(msg, format);
	vsnprintf(buf, 1023, format, msg);
	va_end(msg);

	Bit16u size = (Bit16u)strlen(buf);
	DOS_WriteFile(STDOUT, (Bit8u *)buf, &size);
	}

void Program::WriteOut_NoParsing(const char* format)
	{
	Bit16u size = (Bit16u)strlen(format);
	DOS_WriteFile(STDOUT, (Bit8u *)format, &size);
	}

char * Program::GetEnvStr(const char* entry)
	{
	if (int elen = strlen(entry))
		{
		char * envPtr = (char *)(MemBase + (psp->GetEnvironment()<<4));
		while (*envPtr)									// Walk through the internal environment and look for a match
			{
			if (envPtr[elen] == '=' && !strnicmp(entry, envPtr, elen))
				return envPtr+elen+1;
			envPtr += strlen(envPtr)+1;
			}
		}
	return NULL;
	}

char * Program::GetEnvNum(Bitu num)
	{
	char * envPtr = (char *)(MemBase + (psp->GetEnvironment()<<4));
	while (*envPtr)										// Walk through the internal environment to num element
		{
		if (!num--)
			return envPtr;
		envPtr += strlen(envPtr)+1;
		}
	return NULL;
	}

bool Program::SetEnv(const char* entry, const char* new_string)
	{
	PhysPt env_read = SegOff2Ptr(psp->GetEnvironment(), 0);
	PhysPt env_write = env_read;
	char env_string[257];
	do
		{
		vPC_rStrnCpy(env_string, env_read, 256);
		if (!env_string[0])
			break;
		env_read += (PhysPt)(strlen(env_string)+1);
		if (!strchr(env_string, '='))
			continue;								// Remove corrupt entry?
		if ((strnicmp(entry, env_string, strlen(entry)) == 0) && env_string[strlen(entry)] == '=')
			continue;								// Remove current entry/value
		vPC_rBlockWrite(env_write, env_string, (Bitu)(strlen(env_string)+1));
		env_write += (PhysPt)(strlen(env_string)+1);
		}
	while (1);
// TODO Maybe save the program name sometime. not really needed though
	// Save the new entry
	if (new_string[0])
		{
		std::string bigentry(entry);
		for (std::string::iterator it = bigentry.begin(); it != bigentry.end(); ++it)
			*it = toupper(*it);
		sprintf(env_string, "%s=%s", bigentry.c_str(), new_string); 
		vPC_rBlockWrite(env_write, env_string, (Bitu)(strlen(env_string)+1));
		env_write += (PhysPt)(strlen(env_string)+1);
		}
	vPC_rStosd(env_write, 0);						// Clear out the final piece of the environment
	return true;
	}

void PROGRAMS_Init()
	{
	// Setup a special callback to start virtual programs
	Bitu call_program = CALLBACK_Allocate();
	CALLBACK_Setup(call_program, &PROGRAMS_Handler, CB_RETF, "internal program");
	exe_block[CB_POS] = (Bit8u)(call_program&0xff);
	exe_block[CB_POS+1] = (Bit8u)((call_program>>8)&0xff);
	}
