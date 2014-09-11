#ifndef vDOS_PROGRAMS_H
#define vDOS_PROGRAMS_H

#ifndef VDOS_H
#include "vDos.h"
#endif
#ifndef vDOS_DOS_INC_H
#include "dos_inc.h"
#endif

#ifndef CH_LIST
#define CH_LIST
#include <list>
#endif

#ifndef CH_STRING
#define CH_STRING
#include <string>
#endif

class CommandLine
	{
public:
	CommandLine(char const * const name, char const * const cmdline);
	const char * GetFileName() {return file_name.c_str();}

	bool FindString(char const * const name, std::string & value, bool remove = false);
	const char * FindCommand(unsigned int which);
	bool FindStringBegin(char const * const begin, std::string & value, bool remove=false);
	bool FindStringRemain(char const * const name, std::string & value);
	unsigned int GetCount(void);
	void Shift(unsigned int amount = 1);
private:
	typedef std::list<std::string>::iterator cmd_it;
	std::list<std::string> cmds;
	std::string file_name;
	bool FindEntry(char const * const name, cmd_it & it, bool neednext = false);
	};

class Program {
public:
	Program();
	virtual ~Program(){
		delete cmd;
		delete psp;
	}
	CommandLine * cmd;
	DOS_PSP * psp;
	virtual void Run(void) = 0;
	char * GetEnvStr(const char * entry);
	char * GetEnvNum(Bitu num);
	bool SetEnv(const char * entry,const char * new_string);
	void WriteOut(const char * format,...);				// Write to standard output
	void WriteOut_NoParsing(const char * format);		// Write to standard output, no parsing
};

typedef void (PROGRAMS_Main)(Program * * make);
void PROGRAMS_MakeFile(char const * const name, PROGRAMS_Main * main);

#endif
