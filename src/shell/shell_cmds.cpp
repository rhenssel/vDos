#include "vDos.h"
#include "shell.h"
#include "callback.h"
#include "regs.h"
#include "../dos/drives.h"
#include "../src/ints/int10.h"
#include "support.h"
#include "control.h"
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <string>
#include <time.h>
#include "../src/ints/xms.h"
#include "render.h"

static SHELL_Cmd cmd_list[]={
{	"MEM",		&DOS_Shell::CMD_MEM			},
{	"USE",		&DOS_Shell::CMD_USE			},
{	"CALL",		&DOS_Shell::CMD_CALL		},
{	"CD",		&DOS_Shell::CMD_CHDIR		},
{	"CHDIR",	&DOS_Shell::CMD_CHDIR		},
{	"CLS",		&DOS_Shell::CMD_CLS			},
{	"COPY",		&DOS_Shell::CMD_COPY		},
{	"DATE",		&DOS_Shell::CMD_DATE		},
{	"DEL",		&DOS_Shell::CMD_DELETE		},
{	"DELETE",	&DOS_Shell::CMD_DELETE		},
{	"DIR",		&DOS_Shell::CMD_DIR			},
{	"ECHO",		&DOS_Shell::CMD_ECHO		},
{	"ERASE",	&DOS_Shell::CMD_DELETE		},
{	"EXIT",		&DOS_Shell::CMD_EXIT		},	
{	"GOTO",		&DOS_Shell::CMD_GOTO		},
{	"IF",		&DOS_Shell::CMD_IF			},
{	"LH",		&DOS_Shell::CMD_LOADHIGH	},
{	"LOADHIGH",	&DOS_Shell::CMD_LOADHIGH	},
{	"MD",		&DOS_Shell::CMD_MKDIR		},
{	"MKDIR",	&DOS_Shell::CMD_MKDIR		},
{	"PATH",		&DOS_Shell::CMD_PATH		},
{	"PAUSE",	&DOS_Shell::CMD_PAUSE		},
{	"PROMPT",	&DOS_Shell::CMD_PROMPT		},
{	"RD",		&DOS_Shell::CMD_RMDIR		},
{	"RMDIR",	&DOS_Shell::CMD_RMDIR		},
{	"TIME",		&DOS_Shell::CMD_TIME		},
{	"REM",		&DOS_Shell::CMD_REM			},
{	"REN",		&DOS_Shell::CMD_RENAME		},
{	"RENAME",	&DOS_Shell::CMD_RENAME		},
{	"SET",		&DOS_Shell::CMD_SET			},
{	"SHIFT",	&DOS_Shell::CMD_SHIFT		},
{	"TYPE",		&DOS_Shell::CMD_TYPE		},
{	"VER",		&DOS_Shell::CMD_VER			},
{0,0,0}
}; 

bool do_break = false;

/* support functions */
static char empty_char = 0;
static char* empty_string = &empty_char;

static void StripSpaces(char*&args)
	{
	while (isspace(*args))
		args++;
	}

static void StripSpaces(char*&args, char also)
	{
	while (isspace(*args) || (*args == also))
		args++;
	}

static char* ExpandDot(char* args, char* buffer)
	{
	if (*args == '.')
		{
		if (*(args+1) == 0)
			return strcpy(buffer, "*.*");
		if( (*(args+1) != '.') && (*(args+1) != '\\'))
			return strcat(strcpy(buffer, "*"), args);
		}
	return strcpy(buffer, args);
	}

void DOS_Shell::DoCommand(char * line)
	{
	// Parse the line first for % stuff
	char * cmdOrg = line;
	char cmdNew[CMD_MAXLINE];
	char *cmdPtr = cmdNew;
	while (*cmdOrg)
		{
		if (*cmdOrg == '%')
			{
			char nextChar = *(++cmdOrg);
			if (nextChar == '%' || (!bf && nextChar >= '0'&& nextChar <= '9'))	// Skip %% and %0-%9 if not in batchfile
				*cmdPtr++ = '%';
			else if (nextChar == '0')								// Handle %0
				{
				cmdOrg++;
				strcpy(cmdPtr, bf->cmd->GetFileName());
				cmdPtr += strlen(cmdPtr);
				}
			else if (nextChar > '0' && nextChar <= '9')				// Handle %1..%9
				{  
				strcpy(cmdPtr, bf->cmd->FindCommand(nextChar-'0'));
				if (*cmdPtr)										// If substituted
					{
					cmdOrg++;
					cmdPtr += strlen(cmdPtr);
					}
				}
			else													// Not a command line number has to be an environment string
				{
				char * percPtr = strchr(cmdOrg, '%');
				if (!percPtr)										// No environment variable afterall
					*cmdPtr++ = '%';
				else
					{
					*percPtr = 0;
					if (const char *substStr = this->GetEnvStr(cmdOrg))
						{
						strcpy(cmdPtr, substStr);
						cmdPtr += strlen(cmdPtr);
						}
					else											// No DOS variable, try Windows
						{
						*percPtr = '%';
						char afterVar = percPtr[1];
						percPtr[1] = 0;								// Have to limit it to the variable only, else all will be translated/copied
						if (ExpandEnvironmentStrings(cmdOrg-1, cmdPtr, CMD_MAXLINE-(cmdPtr-cmdNew)))
							if (strcmp(cmdOrg-1, cmdPtr))			// Also succesful if nothing done!
								cmdPtr += strlen(cmdPtr);
							else if (!bf)							// At the commandline it's not wiped out
								{
								percPtr[1] = afterVar;
								*cmdPtr++ = '%';
								continue;
								}
						percPtr[1] = afterVar;
						}
					cmdOrg = percPtr+1;
					}
				}
			}
		else
			*cmdPtr++ = *cmdOrg++;
		}
	*cmdPtr = 0;

	// Next split the line into command and arguments
	line = trim(cmdNew);
	char cmdBuff[CMD_MAXLINE];
	char * cmd_write = cmdBuff;
	while (*line)
		{
		if (*line == 32 || *line == '/' || *line == '\t' || *line == '=')
			break;
		if (*line == '.' || *line == '\\')											// allow stuff like cd.. and dir.exe cd\kees
			{
			*cmd_write = 0;
			for (Bit32u cmd_index = 0; cmd_list[cmd_index].name; cmd_index++)
				if (stricmp(cmd_list[cmd_index].name, cmdBuff) == 0)
					{
					(this->*(cmd_list[cmd_index].handler))(ltrim(line));
			 		return;
					}
			}
		*cmd_write++ = *line++;
		}
	*cmd_write = 0;
	if (*cmdBuff == 0)
		return;
	
	for (Bit32u cmd_index = 0; cmd_list[cmd_index].name; cmd_index++)				// Check the internal list
		if (stricmp(cmd_list[cmd_index].name, cmdBuff) == 0)
			{
			(this->*(cmd_list[cmd_index].handler))(ltrim(line));
	 		return;
			}

	if (cmdBuff[1] == ':')					// drive specifified?
		{	
		if (!isalpha(*cmdBuff) || !DOS_DriveIsMounted(toupper(*cmdBuff) -'A'))
			{
			WriteOut(MSG_Get("INVALID_DRIVE"));
			return;
			}
		// check for a drive change (A-Z: or A-Z:\)
		if ((cmdBuff[2] == 0) || (strspn(cmdBuff+2, "\\") == strlen(cmdBuff)-2))	// DOS treates multiple \'s as single '\', args are ignored, so OK
			{
			DOS_SetDefaultDrive(toupper(*cmdBuff)-'A');
			return;
			}
		}
	if (!Execute(cmdBuff, ltrim(line)))												// It isn't an internal command execute it
		WriteOut(MSG_Get("ILLEGAL_COMMAND"));
	}

#define HELP(command) \
	if (strstr(args, "/?")) { \
		WriteOut(MSG_Get(command "?")); \
		return; \
	}

void DOS_Shell::CMD_MEM(char * args)
	{
	HELP("MEM");
	if (*args)
		{
		WriteOut(MSG_Get("INVALID_PARAMETER"), args);
		return;
		}

	// Show conventional Memory
	Bit16u segment;
	Bit16u total = 0xffff;
	DOS_AllocateMemory(&segment, &total);
	WriteOut(MSG_Get("MEM:CONVEN"), total*16/1024);

	// show upper memory
	Bit16u largest, count;
	if (DOS_GetFreeUMB(&total, &largest, &count))
		if (count == 1)
			WriteOut(MSG_Get("MEM:UPPER1"), total*16/1024);
		else
			WriteOut(MSG_Get("MEM:UPPER2"), total*16/1024, count, largest*16/1024);

	// Show free XMS
	Bit16u largestFree, totalFree;
	if (!XMS_QueryFreeMemory(largestFree, totalFree))
		WriteOut(MSG_Get("MEM:EXTEND"), totalFree);
	}

void DOS_Shell::CMD_USE(char *args)
	{
	HELP("USE");
	if (!*args)						// If no arguments show active assignments
		{
		WriteOut(MSG_Get("USE:MOUNTED"));
		for (int d = 0; d < DOS_DRIVES - 1; d++)
			if (Drives[d])
				WriteOut("%c: => %s\n", d+'A', Drives[d]->GetInfo());
		WriteOut("Z: %s\n", Drives[25]->GetInfo());
		return;
		}

	if (strlen(args) < 2 || !isalpha(*args) || args[1] != ':')
		{
		WriteOut(MSG_Get("INVALID_DRIVE"), args);
		return;
		}
	char label[] = " _DRIVE";
	label[0] = toupper(*args);
	if (Drives[label[0]-'A'])
		{
		WriteOut(MSG_Get("USE:ALREADY_USED"), label[0]);
		return;
		}

	char* rem = ltrim(args+2);
	if (!strlen(rem))
		{
		WriteOut(MSG_Get("MISSING_PARAMETER"));
		return;
		}
	while (strlen(rem) > 1 && *rem == '"' && rem[strlen(rem)-1] == '"')			// surrounding by "'s not needed/wanted
		{
		rem[strlen(rem)-1] = 0;													// remove them, eventually '\\' is appended
		rem++;
		}

	char pathBuf[512];															// 512-1 should do!
	pathBuf[0] = 0;																// For eventual error message to test if used
	int len = GetFullPathName(rem, 511, pathBuf, NULL);							// optional lpFilePath is of no use
	if (len != 0 && len < 511)
		{
		int attr = GetFileAttributes(pathBuf);
		if (attr != INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_DIRECTORY))
			{
			if (pathBuf[strlen(pathBuf)-1] != '\\')
				strcat(pathBuf, "\\");

			Bit8u mediaid = 0xF8;												// Hard Disk
			DOS_Drive *newdrive = new localDrive(pathBuf, 512, 127, 1230, 615, mediaid);	// 512*127*1230 == ~80 MB total size, *615  == ~40 MB free size
			if (!newdrive)
				{
				WriteOut(MSG_Get("USE:ERROR"), label[0], pathBuf);
				return;
				}
			Drives[label[0]-'A'] = newdrive;

			vPC_rStosb(dWord2Ptr(dos.tables.mediaid)+(label[0]-'A')*2, newdrive->GetMediaByte());	// Set the correct media byte in the table
			WriteOut("%c => %s\n", label[0], newdrive->GetInfo());
			newdrive->SetLabel(label);
			return;
			}
		}
	if (pathBuf[0] != 0)
		WriteOut(MSG_Get("USE:NODIR"), pathBuf);
	else
		WriteOut(MSG_Get("USE:NODIR"), rem);
	return;
	}

void DOS_Shell::CMD_CLS(char * args)
	{
	HELP("CLS");
	FinishSetMode(true);
	}

void DOS_Shell::CMD_DELETE(char * args)
	{
	HELP("DELETE");
	if (!*args)
		{
		WriteOut(MSG_Get("MISSING_PARAMETER"));
		return;
		}
	if (char* rem = ScanCMDRemain(args))
		{
		WriteOut(MSG_Get("INVALID_SWITCH"), rem);
		return;
		}
	//	Command uses dta so set it to our internal dta
	RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);

	char full[DOS_PATHLENGTH];
	char buffer[MAX_PATH_LEN];
	args = ExpandDot(args, buffer);
	StripSpaces(args);
	if (!DOS_Canonicalize(args, full))
		{
		WriteOut(MSG_Get("ILLEGAL_PATH"));
		return;
		}
// TODO Maybe support confirmation for *.* like dos does.	
	bool res = DOS_FindFirst(args, 0xffff & ~DOS_ATTR_VOLUME);
	if (!res)
		{
		WriteOut(MSG_Get("DEL:ERROR"),args);
		dos.dta(save_dta);
		return;
		}
	// end can't be 0, but if it is we'll get a nice crash, who cares :)
	char * end = strrchr(full,'\\')+1;
	*end = 0;
	char name[DOS_NAMELENGTH_ASCII];
	Bit32u size;
	Bit16u time, date;
	Bit8u attr;
	DOS_DTA dta(dos.dta());
	while (res)
		{
		dta.GetResult(name, size, date, time,attr);	
		if (!(attr & (DOS_ATTR_DIRECTORY|DOS_ATTR_READ_ONLY)))
			{
			strcpy(end, name);
			if (!DOS_UnlinkFile(full))
				WriteOut(MSG_Get("DEL:ERROR"), full);
			}
		res = DOS_FindNext();
		}
	dos.dta(save_dta);
	}

void DOS_Shell::CMD_RENAME(char * args)
	{
	HELP("RENAME");
	bool renOK;
	if (!*args)
		{
		WriteOut(MSG_Get("MISSING_PARAMETER"));
		return;
		}
	if (strchr(args,'*') || strchr(args,'?'))
		{
		WriteOut(MSG_Get("NO_WILD"));
		return;
		}
	char * arg1 = StripWord(args);
	char* slash = strrchr(arg1, '\\');
	if (slash)
		{ 
		slash++;
		/* If directory specified (crystal caves installer)
		 * rename from c:\X : rename c:\abc.exe abc.shr. 
		 * File must appear in C:\ */ 
		
		char dir_source[DOS_PATHLENGTH] = {0};
		//Copy first and then modify, makes GCC happy
		strcpy(dir_source, arg1);
		char* dummy = strrchr(dir_source, '\\');
		*dummy = 0;

		if ((strlen(dir_source) == 2) && (dir_source[1] == ':')) 
			strcat(dir_source,"\\"); // X: add slash

		char dir_current[DOS_PATHLENGTH + 1];
		dir_current[0] = '\\';	//Absolute addressing so we can return properly
		DOS_GetCurrentDir(0, dir_current + 1);
		if(!DOS_ChangeDir(dir_source))
			{
			WriteOut(MSG_Get("ILLEGAL_PATH"));
			return;
			}
		renOK = DOS_Rename(slash, args);
		DOS_ChangeDir(dir_current);
		}
	else
		renOK = DOS_Rename(arg1, args);
	if (!renOK)
		switch (dos.errorcode)
			{
		case DOSERR_FILE_NOT_FOUND:
			WriteOut(MSG_Get("FILE_NOT_FOUND"));
			break;
		case DOSERR_NOT_SAME_DEVICE:
			WriteOut(MSG_Get("INVALID_DRIVE"));
			break;
			}
	}

void DOS_Shell::CMD_ECHO(char * args)
	{
	HELP("ECHO");
	if (!*args)
		{
		WriteOut(MSG_Get(echo ? "ECHO:ON" : "ECHO:OFF"));
		return;
		}
	if (!stricmp(args, "OFF"))
		{
		echo = false;		
		return;
		}
	if (!stricmp(args, "ON"))
		{
		echo = true;		
		return;
		}

	if (*args == '.' || *args=='/')
		args++;
	WriteOut("%s\r\n", args);
	}


void DOS_Shell::CMD_EXIT(char * args)
	{
	HELP("EXIT");
	exit = true;
	}

void DOS_Shell::CMD_CHDIR(char * args)
	{
	HELP("CHDIR");
	if (char* rem = ScanCMDRemain(args))
		{
		WriteOut(MSG_Get("INVALID_SWITCH"), rem);
		return;
		}
	if (!*args)
		{
		Bit8u drive = DOS_GetDefaultDrive()+'A';
		char dir[DOS_PATHLENGTH];
		DOS_GetCurrentDir(0, dir);
		WriteOut("%c:\\%s\n", drive, dir);
		}
	else if (strlen(args) == 2 && args[1] == ':')
		{
		int drive = (args[0] | 0x20)-'a';
		if (drive >= DOS_DRIVES || !Drives[drive])
			{
			WriteOut(MSG_Get("INVALID_DRIVE"));
			return; 
			}
		WriteOut("%c:\\%s\n", drive+'A', Drives[drive]->curdir);
		}
	else if (!DOS_ChangeDir(args))
		WriteOut(MSG_Get("INVALID_DIRECTORY"));
	}

void DOS_Shell::CMD_MKDIR(char * args)
	{
	HELP("MKDIR");
	if (!*args)
		{
		WriteOut(MSG_Get("MISSING_PARAMETER"));
		return;
		}
	if (char* rem = ScanCMDRemain(args))
		{
		WriteOut(MSG_Get("INVALID_SWITCH"), rem);
		return;
		}
	if (!DOS_MakeDir(args))
		WriteOut(MSG_Get("MKDIR:ERROR"),args);
	}

void DOS_Shell::CMD_RMDIR(char * args)
	{
	HELP("RMDIR");
	if (!*args)
		{
		WriteOut(MSG_Get("MISSING_PARAMETER"));
		return;
		}
	if (char* rem = ScanCMDRemain(args))
		{
		WriteOut(MSG_Get("INVALID_SWITCH"), rem);
		return;
		}
	if (!DOS_RemoveDir(args))
		WriteOut(MSG_Get("RMDIR:ERROR"));
	}

char *commaprint(Bitu n)
	{
	static char comma = 0;
	static char retbuf[30];

	if (comma == 0)
		{
		struct lconv *lcp = localeconv();
		comma = *lcp->thousands_sep != 0 ? *lcp->thousands_sep : ',';		// ATTENTION: it's always 0, why????
		}

	char *p = &retbuf[sizeof(retbuf)-1];
	*p = '\0';
	int i = 0;
	do
		{
		if (i%3 == 0 && i != 0)
			*--p = comma;
		*--p = '0' + n % 10;
		n /= 10;
		i++;
		}
	while(n != 0);

	return p;
	}

void DOS_Shell::CMD_DIR(char * args)		// TODO: disgard files size >= 1GB
	{
	HELP("DIR");

	char path[DOS_PATHLENGTH];

	if (char * found = GetEnvStr("DIRCMD"))
		{
		std::string line = args;
		line += " ";
		while (*found)
			line += toupper(*found++);
		args = const_cast<char*>(line.c_str());
		}
   
	bool optW = ScanCMDBool(args, "W");
	ScanCMDBool(args, "S");
	bool optP = ScanCMDBool(args, "P");
	bool optB = ScanCMDBool(args, "B");
	bool optAD = ScanCMDBool(args, "AD");
	if (char* rem = ScanCMDRemain(args))
		{
		WriteOut(MSG_Get("INVALID_SWITCH"), rem);
		return;
		}
	Bit32u byte_count, file_count, dir_count;
	Bitu w_count = 0;
	Bitu p_count = 0;
	Bitu w_size = optW ? 5 : 1;
	byte_count = file_count = dir_count = 0;

	char buffer[MAX_PATH_LEN];
	args = trim(args);
	size_t argLen = strlen(args);
	if (argLen == 0)
		strcpy(args, "*.*");								// no arguments.
	else if (args[argLen-1] == '\\' || args[argLen-1] == ':')
		strcat(args, "*.*");								// handle \, C:\, C: etc.
	args = ExpandDot(args, buffer);

	if (!strrchr(args,'*') && !strrchr(args,'?'))
		{
		Bit16u attribute = 0;
		if (DOS_GetFileAttr(args, &attribute) && (attribute&DOS_ATTR_DIRECTORY))
			strcat(args,"\\*.*");							// if no wildcard and a directory, get its files
		}
	if (!strrchr(args,'.'))
		strcat(args,".*");									// if no extension, get them all

	// Make a full path in the args
	if (!DOS_Canonicalize(args, path))
		{
		WriteOut(MSG_Get("ILLEGAL_PATH"));
		return;
		}
	*(strrchr(path, '\\')+1) = 0;
	if (strlen(path) > 3)									// remove trailling '\\' from subdirs
		path[strlen(path)-1] = 0;
	// Command uses dta so set it to our internal dta
	RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	bool ret = DOS_FindFirst(args, 0xffff & ~DOS_ATTR_VOLUME & ~DOS_ATTR_DEVICE);
	if (!optB)
		{
		Bit8u drive=dta.GetSearchDrive();
		WriteOut(MSG_Get("DIR:INTRO"), drive+'A', Drives[drive]->GetLabel(), Drives[drive]->VolSerial>>16, Drives[drive]->VolSerial&0xffff, path);
		p_count = 3 * w_size;
		}
	if (!ret)
		{
		if (!optB)
			WriteOut(MSG_Get("FILE_NOT_FOUND"));
		dos.dta(save_dta);
		return;
		}
 
	do_break = false;
	do
		{	// File name and extension
		char name[DOS_NAMELENGTH_ASCII];
		Bit32u size;
		Bit16u date;
		Bit16u time;
		Bit8u attr;
		dta.GetResult(name, size, date, time,attr);

		if (optAD && !(attr & DOS_ATTR_DIRECTORY))			// Skip non-directories if option AD is present
			continue;

		// output the file
		if (optB)
			{
			if (strcmp(".", name) && strcmp("..", name))	// this overrides pretty much everything
				WriteOut("%s\n", name);
			}
		else
			{
			char* ext = empty_string;
			if (!optW && (name[0] != '.'))
				{
				ext = strrchr(name, '.');
				if (!ext)
					ext = empty_string;
				else
					*ext++ = 0;
				}
			Bit8u day	= (Bit8u)(date & 0x001f);
			Bit8u month	= (Bit8u)((date >> 5) & 0x000f);
			Bit16u year = (Bit16u)((date >> 9) + 1980);
			Bit8u hour	= (Bit8u)((time >> 5 ) >> 6);
			Bit8u minute = (Bit8u)((time >> 5) & 0x003f);

			if (attr & DOS_ATTR_DIRECTORY)
				{
				if (optW)
					{
					WriteOut("[%s]", name);
					if (!(++w_count%5))
						WriteOut("\n");
					else
						for (int i = 14-strlen(name); i > 0; i--)
							WriteOut(" ");
					}
				else
					WriteOut("%-8s %-3s %-13s %02d-%02d-%02d  %2d:%02d\n", name,ext, "<DIR>", day, month, year%100, hour, minute);
				dir_count++;
				}
			else
				{
				if (optW)
					{
					if (!(++w_count%5))
						WriteOut("%s\n", name);
					else
						WriteOut("%-16s", name);
					}
				else
					WriteOut("%-8s %-3s   %11s %02d-%02d-%02d  %2d:%02d\n", name, ext, commaprint(size), day, month, year%100, hour, minute);
				file_count++;
				byte_count += size;
				}
			}
		if (optP && !(++p_count%((ttf.lins-3)*w_size)))
			{
			CMD_PAUSE(empty_string);
			if (do_break)
				return;
			WriteOut(MSG_Get("DIR:CONTINUING"), path);
			}
		}
	while ((ret = DOS_FindNext()));

	if (optW && w_count%5)
		{
		WriteOut("\n");
		if (optP && !(++p_count%((ttf.lins-3)*w_size)))
			{
			CMD_PAUSE(empty_string);
			if (do_break)
				return;
			WriteOut(MSG_Get("DIR:CONTINUING"), path);
			}
		}
	if (!optB)
		{
		// Show the summary of results
		WriteOut(MSG_Get("DIR:BYTES_USED"), file_count + dir_count, commaprint(byte_count));
		// No freepsace shown, would just be a fake
		}
	dos.dta(save_dta);
	}

struct copysource {
	std::string filename;
	bool concat;
	copysource(std::string filein, bool concatin):
	filename(filein), concat(concatin){ };
	copysource():filename(""), concat(false){ };
};


void DOS_Shell::CMD_COPY(char * args)
	{
	HELP("COPY");
	static char defaulttarget[] = ".";
	// Command uses dta so set it to our internal dta
	RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	DOS_DTA dta(dos.dta());
	Bit32u size;
	Bit16u date;
	Bit16u time;
	Bit8u attr;
	char name[DOS_NAMELENGTH_ASCII];
	std::vector<copysource> sources;
	// ignore /a and /b switches: always copy binary
	while (ScanCMDBool(args, "B")) ;
	while (ScanCMDBool(args, "A")) ;
	ScanCMDBool(args, "Y");
	ScanCMDBool(args, "-Y");
	ScanCMDBool(args, "V");

	if (char* rem = ScanCMDRemain(args))
		{
		WriteOut(MSG_Get("INVALID_SWITCH"), rem);
		dos.dta(save_dta);
		return;
		}
	// Gather all sources (extension to copy more then 1 file specified at commandline)
	// Concatating files goes as follows: All parts except for the last bear the concat flag.
	// This construction allows them to be counted (only the non concat set)
	char* source_p = NULL;
	char source_x[DOS_PATHLENGTH+MAX_PATH_LEN];
	while ((source_p = StripWord(args)) && *source_p)
		{
		do
			{
			char* plus = strchr(source_p, '+');
			if (plus)
				*plus++ = 0;
			safe_strncpy(source_x, source_p, MAX_PATH_LEN);
			bool has_drive_spec = false;
			size_t source_x_len = strlen(source_x);
			if (source_x_len > 0)
				if (source_x[source_x_len-1] == ':')
					has_drive_spec = true;
			if (!has_drive_spec)
				if (DOS_FindFirst(source_p, 0xffff & ~DOS_ATTR_VOLUME))
					{
					dta.GetResult(name, size, date, time,attr);
					if (attr & DOS_ATTR_DIRECTORY && !strstr(source_p, "*.*"))
						strcat(source_x, "\\*.*");
					}
			sources.push_back(copysource(source_x, (plus) ? true : false));
			source_p = plus;
			}
		while(source_p && *source_p);
		}
	// At least one source has to be there
	if (!sources.size() || !sources[0].filename.size())
		{
		WriteOut(MSG_Get("MISSING_PARAMETER"));
		dos.dta(save_dta);
		return;
		}

	copysource target;
	// If more then one object exists and last target is not part of a 
	// concat sequence then make it the target.
	if (sources.size() > 1 && !sources[sources.size()-2].concat)
		{
		target = sources.back();
		sources.pop_back();
		}
	// If no target => default target with concat flag true to detect a+b+c
	if (target.filename.size() == 0)
		target = copysource(defaulttarget, true);

	copysource oldsource;
	copysource source;
	Bit32u count = 0;
	while (sources.size())
		{
		// Get next source item and keep track of old source for concat start end
		oldsource = source;
		source = sources[0];
		sources.erase(sources.begin());

		// Skip first file if doing a+b+c. Set target to first file
		if (!oldsource.concat && source.concat && target.concat)
			{
			target = source;
			continue;
			}

		// Make a full path in the args
		char pathSource[DOS_PATHLENGTH];
		char pathTarget[DOS_PATHLENGTH];

		if (!DOS_Canonicalize(const_cast<char*>(source.filename.c_str()), pathSource))
			{
			WriteOut(MSG_Get("ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
			}
		// cut search pattern
		char* pos = strrchr(pathSource, '\\');
		if (pos)
			*(pos+1) = 0;

		if (!DOS_Canonicalize(const_cast<char*>(target.filename.c_str()), pathTarget))
			{
			WriteOut(MSG_Get("ILLEGAL_PATH"));
			dos.dta(save_dta);
			return;
			}
		char* temp = strstr(pathTarget, "*.*");
		if (temp)
			*temp = 0;	//strip off *.* from target
	
		// add '\\' if target is a directoy
		if (pathTarget[strlen(pathTarget)-1] != '\\')
			if (DOS_FindFirst(pathTarget, 0xffff & ~DOS_ATTR_VOLUME))
				{
				dta.GetResult(name, size, date, time,attr);
				if (attr & DOS_ATTR_DIRECTORY)	
					strcat(pathTarget, "\\");
				}

		// Find first sourcefile
		bool ret = DOS_FindFirst(const_cast<char*>(source.filename.c_str()), 0xffff & ~DOS_ATTR_VOLUME);
		if (!ret)
			{
			WriteOut(MSG_Get("FILE_NOT_FOUND"));
			dos.dta(save_dta);
			return;
			}

		Bit16u sourceHandle, targetHandle;
		char nameTarget[DOS_PATHLENGTH];
		char nameSource[DOS_PATHLENGTH];

		while (ret)
			{
			dta.GetResult(name, size, date, time,attr);

			if ((attr & DOS_ATTR_DIRECTORY) == 0)
				{
				strcpy(nameSource, pathSource);
				strcat(nameSource, name);
				// Open Source
				if (DOS_OpenFile(nameSource, 0, &sourceHandle))
					{
					// Create Target or open it if in concat mode
					strcpy(nameTarget, pathTarget);
					if (nameTarget[strlen(nameTarget)-1] == '\\')
						strcat(nameTarget, name);
					if (!count && !stricmp(nameSource, nameTarget))				// File cannot be copied nto itself
						{
						WriteOut(MSG_Get("COPY:SAMEFILE"));
						dos.dta(save_dta);
						return;
						}

					// Don't create a newfile when in concat mode
					if (oldsource.concat || DOS_CreateFile(nameTarget, 0, &targetHandle))
						{
						Bit32u dummy = 0;
						// In concat mode. Open the target and seek to the eof
						if (!oldsource.concat || (DOS_OpenFile(nameTarget, OPEN_READWRITE, &targetHandle) && DOS_SeekFile(targetHandle, &dummy, DOS_SEEK_END)))
							{
							// Copy 
							static Bit8u buffer[0x8000];	// static, otherwise stack overflow possible.
							Bit16u toread = 0x8000;
							do
								{
								DOS_ReadFile(sourceHandle, buffer, &toread);
								DOS_WriteFile(targetHandle, buffer, &toread);
								}
							while (toread == 0x8000);
							DOS_CloseFile(sourceHandle);
							DOS_CloseFile(targetHandle);
							if (!source.concat)
								count++;	// Only count concat files once
							}
						else
							{
							DOS_CloseFile(sourceHandle);
							WriteOut(MSG_Get("COPY:FAILURE"), const_cast<char*>(target.filename.c_str()));
							}
						}
					else
						{
						DOS_CloseFile(sourceHandle);
						WriteOut(MSG_Get("COPY:FAILURE"), const_cast<char*>(target.filename.c_str()));
						}
					}
				else
					WriteOut(MSG_Get("COPY:FAILURE"), const_cast<char*>(source.filename.c_str()));
				}
			// On the next file
			ret = DOS_FindNext();
			}
		}

	WriteOut(MSG_Get("COPY:SUCCESS"), count);
	dos.dta(save_dta);
	}

void DOS_Shell::CMD_SET(char * args)
	{
	HELP("SET");

	if (!*args)							// No command line show all environment lines
		{
		char * found;
		for (Bitu a = 0; found = GetEnvNum(a) ; a++)
			WriteOut("%s\n", found);			
		return;
		}

	char * p = strpbrk(args, "=");
	if (!p)
		{
		if (char * found = GetEnvStr(args))
			WriteOut("%s\n", found);
		else
			WriteOut(MSG_Get("SET:NOT_SET"), args);
		return;
		}

	*p++ = 0;
	// parse p for envirionment variables
	char parsed[CMD_MAXLINE];
	char* p_parsed = parsed;
	while (*p)
		{
		if (*p != '%')
			*p_parsed++ = *p++;					// Just add it (most likely path)
		else if (*(p+1) == '%')
			{
			*p_parsed++ = '%';
			p += 2;								// %% => % 
			}
		else
			{
			char * second = strchr(++p, '%');
			if (!second)
				continue;
			*second++ = 0;
			if (char * found = GetEnvStr(p))
				{
				strcpy(p_parsed, found);
				p_parsed += strlen(p_parsed);
				}
			p = second;
			}
		}
	*p_parsed = 0;
	// Try setting the variable
	if (!SetEnv(args, parsed))
		WriteOut(MSG_Get("SET:OUT_OF_SPACE"));
	}

void DOS_Shell::CMD_IF(char * args)
	{
	HELP("IF");
	StripSpaces(args, '=');
	bool has_not = false;

	while (strnicmp(args, "NOT", 3) == 0)
		{
		if (!isspace(args[3]) && (args[3] != '='))
			break;
		args += 3;	// skip text
		// skip more spaces
		StripSpaces(args, '=');
		has_not = true;
		}

	if (strncasecmp(args, "ERRORLEVEL", 10) == 0)
		{
		args += 10;		// skip text
		// Strip spaces and ==
		StripSpaces(args, '=');
		char* word = StripWord(args);
		if (!isdigit(*word))
			{
			WriteOut(MSG_Get("IF:ERRORLEVEL_MISSING"));
			return;
			}

		Bit8u n = 0;
		do
			n = n * 10 + (*word - '0');
		while (isdigit(*++word));
		if (*word && !isspace(*word))
			{
			WriteOut(MSG_Get("IF:ERRORLEVEL_INVALID"));
			return;
			}
		// Read the error code from DOS
		if ((dos.return_code >= n) == (!has_not))
			DoCommand(args);
		return;
		}

	if (strnicmp(args,"EXIST ", 6) == 0)
		{
		args += 6;	// Skip text
		StripSpaces(args);
		char* word = StripWord(args);
		if (!*word)
			{
			WriteOut(MSG_Get("IF:EXIST_NO_FILENAME"));
			return;
			}

		// DOS_FindFirst uses dta so set it to our internal dta
		RealPt save_dta = dos.dta();
		dos.dta(dos.tables.tempdta);
		bool ret = DOS_FindFirst(word, 0xffff & ~DOS_ATTR_VOLUME);
		dos.dta(save_dta);
		if (ret == (!has_not))
			DoCommand(args);
		return;
		}

	// Normal if string compare
	char* word1 = args;
	// first word is until space or =
	while (*args && !isspace(*args) && (*args != '='))
		args++;
	char* end_word1 = args;

	// scan for =
	while (*args && (*args != '='))
		args++;
	// check for ==
	if ((*args==0) || (args[1] != '='))
		{
		WriteOut(MSG_Get("SYNTAXERROR"));
		return;
		}
	args += 2;
	StripSpaces(args, '=');

	char* word2 = args;
	// second word is until space or =
	while (*args && !isspace(*args) && (*args != '='))
		args++;

	if (*args)
		{
		*end_word1 = 0;		// mark end of first word
		*args++ = 0;		// mark end of second word
		StripSpaces(args, '=');

		if ((strcmp(word1, word2) == 0) == (!has_not))
			DoCommand(args);
		}
	}

void DOS_Shell::CMD_GOTO(char * args)
	{
	HELP("GOTO");
	if (!bf)
		return;
	if (*args && (*args==':'))
		args++;
	// label ends at the first space
	char* non_space = args;
	while (*non_space)
		if ((*non_space == ' ') || (*non_space == '\t')) 
			*non_space = 0; 
		else
			non_space++;
	if (!*args)
		{
		WriteOut(MSG_Get("GOTO:MISSING_LABEL"));
		return;
		}
	if (!bf->Goto(args))
		{
		WriteOut(MSG_Get("GOTO:LABEL_NOT_FOUND"), args);
		return;
		}
	}

void DOS_Shell::CMD_SHIFT(char * args)
	{
	HELP("SHIFT");
	if (bf)
		bf->Shift();
	}

void DOS_Shell::CMD_TYPE(char * args)
	{
	HELP("TYPE");
	if (!*args)
		{
		WriteOut(MSG_Get("MISSING_PARAMETER"));
		return;
		}
	char * word;
	if (word = ScanCMDRemain(args))			// no switches !!
		{
		WriteOut(MSG_Get("INVALID_SWITCH"), word);
		return;
		}
	word = StripWord(args);
	if (*args)								// only one parameter
		{
		WriteOut(MSG_Get("TOO_MANY_PARAMETERS"), StripWord(args));
		return;
		}
	Bit16u handle;
	if (!DOS_OpenFile(word, OPEN_READ, &handle))
		{
		WriteOut(MSG_Get("FILE_NOT_FOUND"));
		return;
		}
	Bit16u n = 1;
	Bit8u c;
	do
		{
		DOS_ReadFile(handle, &c, &n);
		DOS_WriteFile(STDOUT, &c, &n);
		}
	while (n);
	DOS_CloseFile(handle);
	}

void DOS_Shell::CMD_REM(char* args)
	{
	HELP("REM");
	}

void DOS_Shell::CMD_PAUSE(char* args)
	{
	HELP("PAUSE");
	WriteOut(MSG_Get("PAUSE:INTRO"));
	Bit8u c;
	Bit16u n = 1;
	DOS_ReadFile (STDIN, &c, &n);
	do_break = c == 3 ? true : false;
	WriteOut("\n%s", do_break ? "^C" : "");
	}

void DOS_Shell::CMD_CALL(char * args)
	{
	HELP("CALL");
	this->call = true;						// else the old batchfile will be closed first
	this->ParseLine(args);					// OK, but where is call tested?
	this->call = false;
	}

void DOS_Shell::CMD_DATE(char * args)
	{
	HELP("DATE");	
	if (*args)								// set date not supported, always return error
		{
		WriteOut(MSG_Get("NO_PARAMS"));
		return;
		}
	_SYSTEMTIME systime;
	GetLocalTime(&systime);					// return the Windows localdate

	char buffer[21] = {0};
	const char* datestring = MSG_Get("DATE:DAYS");
	Bit8u bufferptr = *datestring-'0';
	strncpy(buffer, datestring+((Bit8u)systime.wDayOfWeek)*bufferptr+1, bufferptr);		// NB Sunday = 0, despite of MSDN documentation
	const char* formatstring = MSG_Get("DATE:FORMAT");
	buffer[bufferptr++] = ' ';
	for (Bitu i = 0; i < 5; i++)
		if (i == 1 || i == 3)
			buffer[bufferptr++] = formatstring[i];
		else
			switch (formatstring[i])
				{
			case 'M':
				bufferptr += sprintf(buffer+bufferptr, "%02u", systime.wMonth);
				break;
			case 'D':
				bufferptr += sprintf(buffer+bufferptr, "%02u", systime.wDay);
				break;
			case 'Y':
				bufferptr += sprintf(buffer+bufferptr, "%04u", systime.wYear);
				break;
				}
	WriteOut(MSG_Get("DATE:NOW"), buffer);
	}

void DOS_Shell::CMD_TIME(char * args)
	{
	HELP("TIME");
	if (*args)										// set time not supported always return error
		{
		WriteOut(MSG_Get("NO_PARAMS"));
		return;
		}
	_SYSTEMTIME systime;
	GetLocalTime(&systime);							// return Windows localtime
	WriteOut(MSG_Get("TIME:NOW"), systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds/10);
	}

void DOS_Shell::CMD_LOADHIGH(char* args)
	{
	HELP("LOADHIGH");
	Bit16u umb_start = dos_infoblock.GetStartOfUMBChain();
	if (umb_start == 0x9fff)
		{
		Bit8u umb_flag = dos_infoblock.GetUMBChainState();
		Bit8u old_memstrat = (Bit8u)(DOS_GetMemAllocStrategy()&0xff);
		if ((umb_flag&1) == 0)
			DOS_LinkUMBsToMemChain(1);
		DOS_SetMemAllocStrategy(0x80);				// search in UMBs first
		this->ParseLine(args);
		Bit8u current_umb_flag = dos_infoblock.GetUMBChainState();
		if ((current_umb_flag&1) != (umb_flag&1))
			DOS_LinkUMBsToMemChain(umb_flag);
		DOS_SetMemAllocStrategy(old_memstrat);		// restore strategy
		}
	else
		this->ParseLine(args);
	}

void DOS_Shell::CMD_PROMPT(char *args)
	{
	HELP("PROMPT");
	SetEnv("PROMPT", *args ? args : "$P$G");
	}

void DOS_Shell::CMD_PATH(char* args)
	{
	HELP("PATH");
	if (*args)
		{
		char pathstring[DOS_PATHLENGTH+MAX_PATH_LEN+20] = { 0 };
		strcpy(pathstring,"set PATH=");
		while (args && *args && (*args == '='|| *args == ' ')) 
			args++;
		strcat(pathstring, args);
		this->ParseLine(pathstring);
		return;
		}
	else
		{
		if (char * found = GetEnvStr("PATH"))
        	WriteOut("%s", found);
		else
			WriteOut("No search path defined");
		}
	}

void DOS_Shell::CMD_VER(char* args)
	{
	HELP("VER");
	if (*args)
		{
		char* word = StripWord(args);
		dos.version.major = (Bit8u)(atoi(word));
		dos.version.minor = (Bit8u)(atoi(args));
		}
	else
		WriteOut(MSG_Get("VER:MESS"), vDosVersion, dos.version.major, dos.version.minor);
	}
