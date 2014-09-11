#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vDos.h"
#include "support.h"
#include "setup.h"
#include <list>
#include <string>
using namespace std;

#define LINE_IN_MAXLEN 2048

struct MessageBlock {
	string name;
	string val;
	MessageBlock(const char* _name, const char* _val):
	name(_name), val(_val){}
};

static list<MessageBlock> Lang;
typedef list<MessageBlock>::iterator itmb;

void inline MSG_Add(const char * _name, const char* _val)
	{
	Lang.push_back(MessageBlock(_name, _val));
	}

static void MSG_Replace(const char * _name, const char* _val)
	{
	for (itmb tel = Lang.begin(); tel != Lang.end(); tel++)				// Find the message
		if ((*tel).name == _name)
			{ 
			(*tel).val = _val;
			return;
			}
	}


static void LoadMessageFile(void)
	{
	FILE * mfile;
	char fName[] = "language.txt";
	char path[512];

	if (!(mfile  = fopen(fName, "r")))
		{
		strcpy(strrchr(strcpy(path, _pgmptr), '\\')+1, fName);			// Try to load it from where vDos was started
		if (!(mfile  = fopen(path, "r")))
			return;
		}

	char linein[LINE_IN_MAXLEN];
	char name[LINE_IN_MAXLEN];
	char string[LINE_IN_MAXLEN*10];
	// Start out with empty strings
	name[0] = 0;
	string[0] = 0;
	while (fgets(linein, LINE_IN_MAXLEN, mfile))
		{
		// Parse the read line
		// First remove characters 10 and 13 from the line
		char * parser = linein;
		char * writer = linein;
		while (*parser)
			{
			if (*parser != 10 && *parser != 13)
				*writer++ = *parser;
			*parser++;
			}
		*writer = 0;
		// New string name
		if (linein[0] == ':')
			{
			string[0] = 0;
			strcpy(name, linein+1);
			// End of string marker
			}
		else if (linein[0] == '.')
			{
			// Replace/Add the string to the internal langaugefile
			// Remove last newline (marker is \n.\n)
			size_t ll = strlen(string);
			if (ll && string[ll - 1] == '\n')
				string[ll - 1] = 0;		// Second if should not be needed, but better be safe.
			MSG_Replace(name, string);
			}
		else
			{
			// Normal string to be added
			strcat(string, linein);
			strcat(string, "\n");
			}
		}
	fclose(mfile);
	}

void MSG_Init()
	{
	MSG_Add("UNSAFETOCLOSE1",		"Unsafe to close vDos");
	MSG_Add("UNSAFETOCLOSE2",		"One or more files are open.\n\nExit from your DOS application.");
	MSG_Add("SYSMENU:COPY",			"Copy all text to and open file\tWin+Ctrl+C");
	MSG_Add("SYSMENU:PASTE",		"Paste from clipboard\tWin+Ctrl+V");
	MSG_Add("SYSMENU:DECREASE",		"Decrease font size\tWin+F11");
	MSG_Add("SYSMENU:INCREASE",		"Increase font size\tWin+F12");
	MSG_Add("SYSMENU:ABOUT",		"About...");

	MSG_Add("FILE_NOT_FOUND",		"File not found\n");
	MSG_Add("ILLEGAL_COMMAND",		"Bad command or file name\n");
	MSG_Add("ILLEGAL_PATH",			"Path not found\n");
	MSG_Add("INVALID_DIRECTORY",	"Invalid directory\n");
	MSG_Add("INVALID_DRIVE",		"Invalid drive specification\n");
	MSG_Add("INVALID_PARAMETER",	"Invalid parameter - %s\n");
	MSG_Add("INVALID_SWITCH",		"Invalid switch - %s\n");
	MSG_Add("MISSING_PARAMETER",	"Required parameter missing\n");
	MSG_Add("NO_WILD",				"This is a simple version of the command, no wildcards allowed!\n");
	MSG_Add("NO_PARAMS",			"This is a simple version of the command, no parameters allowed!\n");
	MSG_Add("SYNTAXERROR",			"Syntax error\n");
	MSG_Add("TOO_MANY_PARAMETERS",	"Too many parameters - %s\n");

	MSG_Add("MEM?",					"Displays the amount of free memory in your system.\n");
	MSG_Add("USE?",					"USE drive-letter: Windows-directory\n\n"
									"  USE c: c:\\dos program\n\n"
									"Assigns DOS drive C: to the Windows directory c:\\dos program\n");
	MSG_Add("CALL?",				"Call a batch program from another\n");
	MSG_Add("CHDIR?",				"Displays the name of or changes the current directory.\n\n"
									"CHDIR [drive:][path]\n"
									"CHDIR [..]\n"
									"CD [drive:][path]\n"
									"CD [..]\n\n"
									"  ..   Specifies that you want to change to the parent directory.\n\n"
									"Type CD drive: to display the current directory in the specified drive.\n"
									"Type CD without parameters to display the current drive and directory.\n");
	MSG_Add("CHOICE?",				"Waits for a keypress and sets ERRORLEVEL in batch programs.\n");
	MSG_Add("CLS?",					"Clear the screen\n");
	MSG_Add("COPY?",				"Copy file(s).\n");
	MSG_Add("DATE?",				"Displays the date.\n");
	MSG_Add("DELETE?",				"Deletes one or more files.\n");
	MSG_Add("DIR?",					"Displays a list of files and subdirectories in a directory.\n");
	MSG_Add("ECHO?",				"Display message or enable/disable command echoing\n");
	MSG_Add("EXIT?",				"Exit from the shell\n");
	MSG_Add("GOTO?",				"Directs DOS to a labelled line in a batch program.\n\n"
									"  label   Specifies a text string used in the batch program as a label.\n\n"
									"You type a label on a line by itself, beginning with a colon.\n");
	MSG_Add("IF?",					"Performs conditional processing in batch programs.\n");
	MSG_Add("LOADHIGH?",			"Load a program into upper memory\n");
	MSG_Add("MKDIR?",				"Creates a directory.\n\n"
									"MKDIR [drive:]path\n"
									"MD [drive:]path\n");
	MSG_Add("PATH?",				"Displays or sets a search path for executable files.\n");
	MSG_Add("PAUSE?",				"Suspends processing of a batch program.\n");
	MSG_Add("PROMPT?",				"Changes the DOS command prompt.\n\nPROMPT [text]\n\n  text    Specifies a new command prompt.\n");
	MSG_Add("REM?",					"Records comments (remarks) in a batch file or CONFIG.SYS.\n\n"
									"REM [comment]\n");
	MSG_Add("RMDIR?",				"Removes (deletes) a directory.\n\n"
									"RMDIR [drive:]path\n"
									"RD [drive:]path\n");
	MSG_Add("RENAME?",				"Rename file(s)\n\n"
									"RENAME [drive:][path]filename1 filename2\n"
									"REN [drive:][path]filename1 filename2.\n\n"
									"Note that you can not specify a new drive or path for your destination file.\n");
	MSG_Add("SET?",					"Displays, sets, or removes DOS environment variables.\n\n"
									"SET [variable=[string]]\n\n"
									"  variable  Specifies the environment-variable name.\n"
									"  string    Specifies a series of characters to assign to the variable.\n\n"
									"Type SET without parameters to display the current environment variables.\n");
	MSG_Add("SHIFT?",				"Leftshift commandline parameters in a batch script\n");
	MSG_Add("TIME?",				"Display the time\n");
	MSG_Add("TYPE?",				"Displays the contents of a text file.\n\n"
									"TYPE [drive:][path]filename\n");
	MSG_Add("VER?",					"Displays or sets the reported DOS version.\n\n"
									"VER [major minor]\n");

	MSG_Add("MEM:CONVEN",				"\n%5dK free conventional memory\n");
	MSG_Add("MEM:EXTEND",				"%5dK free extended memory\n");
	MSG_Add("MEM:UPPER1",				"%5dK free upper memory\n");
	MSG_Add("MEM:UPPER2",				"%5dK free upper memory in %d blocks, largest: %dK\n");
	MSG_Add("USE:MOUNTED",				"\n      Windows directory\n");
	MSG_Add("USE:NODIR",				"Directory %s doesn't exist\n");
	MSG_Add("USE:ALREADY_USED",			"%c: is already used\n");
	MSG_Add("USE:ERROR",				"Could not set %c: to %s\n");

	MSG_Add("COPY:FAILURE",				"Copy failure : %s\n");
	MSG_Add("COPY:SUCCESS",				"   %d file(s) copied\n");
	MSG_Add("DATE:DAYS",				"3SunMonTueWedThuFriSat");
	MSG_Add("DATE:FORMAT",				"M-D-Y");
	MSG_Add("DATE:NOW",					"Current date is %s\n");
	MSG_Add("DEL:ERROR",				"Unable to delete: %s\n");
	MSG_Add("DIR:BYTES_FREE",			"%32s bytes free\n");
	MSG_Add("DIR:BYTES_USED",			"    %5d file(s) %14s bytes\n");
	MSG_Add("DIR:CONTINUING",			"(continuing %s)\n");
	MSG_Add("DIR:INTRO",				"\n Volume in drive %c is %s\n Volume Serial Number is %04X:%04X\n Directory of %s\n\n");
	MSG_Add("ECHO:OFF",					"ECHO is off\n");
	MSG_Add("ECHO:ON",					"ECHO is on\n");
	MSG_Add("GOTO:LABEL_NOT_FOUND",		"GOTO: Label %s not found\n");
	MSG_Add("GOTO:MISSING_LABEL",		"No label supplied to GOTO command\n");
	MSG_Add("IF:ERRORLEVEL_INVALID",	"IF ERRORLEVEL: Invalid number\n");
	MSG_Add("IF:ERRORLEVEL_MISSING",	"IF ERRORLEVEL: Missing number\n");
	MSG_Add("IF:EXIST_NO_FILENAME",		"IF EXIST: Missing filename\n");
	MSG_Add("MKDIR:ERROR",				"Unable to create: %s\n");
	MSG_Add("PAUSE:INTRO",				"Press any key to continue . . .\n");
	MSG_Add("RMDIR:ERROR",				"Invalid path, not directory,\nor directory not empty\n");
	MSG_Add("SET:NOT_SET",				"Environment variable %s not defined\n");
	MSG_Add("SET:OUT_OF_SPACE",			"Not enough environment space left\n");
	MSG_Add("TIME:NOW",					"Current time is %2u:%02u:%02u.%02u\n");
	MSG_Add("VER:MESS",					"DOS Version %d.%02d\n");

	LoadMessageFile();
	}


const char * MSG_Get(char const * msg)
	{
	for(itmb tel = Lang.begin(); tel != Lang.end(); tel++)
		if ((*tel).name == msg)
			return  (*tel).val.c_str();
	return "Message not Found!\n";
	}
