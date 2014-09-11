#ifndef VDOS_SHELL_H
#define VDOS_SHELL_H

#include <ctype.h>
#ifndef VDOS_H
#include "vDos.h"
#endif
#ifndef vDOS_PROGRAMS_H
#include "programs.h"
#endif

#include <string>
#include <list>

#define CMD_MAXLINE 4096
extern Bitu call_shellstop;

class DOS_Shell;

class BatchFile
	{
public:
	BatchFile(DOS_Shell * host, char const* const resolved_name, char const* const entered_name, char const * const cmd_line);
	virtual ~BatchFile();
	virtual bool ReadLine(char * line);
	bool Goto(char * where);
	void Shift(void);
	Bit16u file_handle;
	Bit32u location;
	bool echo;
	DOS_Shell * shell;
	BatchFile * prev;
	CommandLine * cmd;
	std::string filename;
	};

class DOS_Shell : public Program
	{
private:
	std::list<std::string> l_history;
	char * Which(char * name);
	
public:
	DOS_Shell();

	void Run(void);
	void RunInternal(void); //for command /C
// A load of subfunctions
	void ParseLine(char * line);
	Bitu GetRedirection(char *s, char **ifn, char **ofn,bool * append);
	void InputCommand(char * line);
	void ShowPrompt();
	void DoCommand(char * cmd);
	bool Execute(char * name,char * args);
// Orginally external commands
	void CMD_MEM(char * args);
	void CMD_USE(char * args);
// Some supported commands
	void CMD_CALL(char * args);
	void CMD_CHDIR(char * args);
	void CMD_CLS(char * args);
	void CMD_COPY(char * args);
	void CMD_DATE(char * args);
	void CMD_DELETE(char * args);
	void CMD_DIR(char * args);
	void CMD_ECHO(char * args);
	void CMD_EXIT(char * args);
	void CMD_GOTO(char * args);
	void CMD_IF(char * args);
	void CMD_LOADHIGH(char* args);
	void CMD_MKDIR(char * args);
	void CMD_PATH(char * args);
	void CMD_PAUSE(char * args);
	void CMD_PROMPT(char * args);
	void CMD_REM(char * args);
	void CMD_RENAME(char * args);
	void CMD_RMDIR(char * args);
	void CMD_SET(char * args);
	void CMD_SHIFT(char * args);
	void CMD_TIME(char * args);
	void CMD_TYPE(char * args);
	void CMD_VER(char * args);
	/* The shell's variables */
	Bit16u input_handle;
	BatchFile * bf;
	bool echo;
	bool exit;
	bool call;
};

struct SHELL_Cmd {
	const char * name;								// Command name
	void (DOS_Shell::*handler)(char * args);		// Handler for this command
	const char * help;								// String with command help
};

#endif
