#ifndef vDOS_PROGRAMS_H
#define vDOS_PROGRAMS_H

#include "vDos.h"
#include "dos_inc.h"
#include <list>
#include <string>

class CommandLine
	{
public:
	CommandLine(char const * const name, char const * const cmdline);
	const char * GetFileName() {return file_name.c_str();}

	bool FirstStart();
	const char * GetVarNum(unsigned int which);
	bool GetCommand(std::string & value);
	void Shift(unsigned int amount = 1);
private:
	typedef std::list<std::string>::iterator cmd_it;
	std::list<std::string> cmds;
	std::string file_name;
	bool FindEntry(char const * const name, cmd_it & it);
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
