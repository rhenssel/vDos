#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "support.h"

BatchFile::BatchFile(DOS_Shell * host, char const * const resolved_name, char const * const entered_name, char const * const cmd_line)
	{
	location = 0;
	prev = host->bf;
	echo = host->echo;
	shell = host;
	char totalname[DOS_PATHLENGTH+4];
	DOS_Canonicalize(resolved_name, totalname);		// Get fullname including drive specificiation
	cmd = new CommandLine(entered_name, cmd_line);
	filename = totalname;

	// Test if file is openable
	if (!DOS_OpenFile(totalname,128, &file_handle))
		{
		// TODO Come up with something better
		E_Exit("SHELL: Can't open BatchFile %s", totalname);
		}
	DOS_CloseFile(file_handle);
	}

BatchFile::~BatchFile()
	{
	delete cmd;
	shell->bf = prev;
	shell->echo = echo;
	}

bool BatchFile::ReadLine(char * line)
	{
	// Open the batchfile and seek to stored postion
	if (!DOS_OpenFile(filename.c_str(), 128, &file_handle))
		{
		LOG(LOG_MISC,LOG_ERROR)("ReadLine Can't open BatchFile %s", filename.c_str());
		delete this;
		return false;
		}
	DOS_SeekFile(file_handle, &(this->location), DOS_SEEK_SET);
	Bit8u c = 0;
	Bit16u n = 1;
	char temp[CMD_MAXLINE];
emptyline:
	char * cmd_write = temp;
	do
		{
		n = 1;
		DOS_ReadFile(file_handle, &c, &n);
		if (n > 0)
			{
			/* Why are we filtering this ?
			 * Exclusion list: tab for batch files 
			 * escape for ansi
			 * backspace for alien odyssey */
			if (c > 31 || c == 0x1b || c == '\t' || c == 8)
				*cmd_write++ = c;
			}
		}
	while (c != '\n' && n);
	*cmd_write = 0;
	if (!n && cmd_write == temp)
		{
		// Close file and delete batchfile
		DOS_CloseFile(file_handle);
		delete this;
		return false;	
		}
	if (*temp == 0 || *temp == ':')
		goto emptyline;

	// Now parse the line read from the bat file for % stuff
	cmd_write = line;
	char * cmd_read = temp;
	char env_name[256];
	char * env_write;
	while (*cmd_read)
		{
		env_write = env_name;
		if (*cmd_read == '%')
			{
			cmd_read++;
			if (cmd_read[0] == '%')
				{
				cmd_read++;
				*cmd_write++ = '%';
				continue;
				}
			if (cmd_read[0] == '0')
				{		// Handle %0
				const char *file_name = cmd->GetFileName();
				cmd_read++;
				strcpy(cmd_write, file_name);
				cmd_write += strlen(file_name);
				continue;
				}
			char next = cmd_read[0];
			if (next > '0' && next <= '9')
				{  
				// Handle %1 %2 .. %9
				cmd_read++;		// Progress reader
				next -= '0';
				if (cmd->GetCount() < (unsigned int)next)
					continue;
				std::string word;
				if (!cmd->FindCommand(next, word))
					continue;
				strcpy(cmd_write, word.c_str());
				cmd_write += strlen(word.c_str());
				continue;
				}
			else
				{
				// Not a command line number has to be an environment string
				char * first = strchr(cmd_read, '%');
				// No env afterall. Somewhat of a hack though as %% and % aren't handled consistent in vDos. Maybe echo needs to parse % and %% as well.
				if (!first)
					{
					*cmd_write++ = '%';
					continue;
					}
				*first++ = 0;
				std::string env;
				if (const char *equals = shell->GetEnvStr(cmd_read))
					{
					strcpy(cmd_write, equals);
					cmd_write += strlen(equals);
					}
				cmd_read = first;
				}
			}
		else
			*cmd_write++ = *cmd_read++;
		}
	*cmd_write = 0;
	// Store current location and close bat file
	this->location = 0;
	DOS_SeekFile(file_handle, &(this->location), DOS_SEEK_CUR);
	DOS_CloseFile(file_handle);
	return true;	
	}

bool BatchFile::Goto(char * where)
	{
	// Open bat file and search for the where string
	if (!DOS_OpenFile(filename.c_str(), 128, &file_handle))
		{
		LOG(LOG_MISC,LOG_ERROR)("SHELL:Goto Can't open BatchFile %s", filename.c_str());
		delete this;
		return false;
		}

	char cmd_buffer[CMD_MAXLINE];
	char * cmd_write;

	// Scan till we have a match or return false
	Bit8u c;
	Bit16u n;
again:
	cmd_write = cmd_buffer;
	do
		{
		n = 1;
		DOS_ReadFile(file_handle, &c, &n);
		if (n > 0 && c > 31)
			*cmd_write++ = c;
		}
		while (c != '\n' && n);
	*cmd_write++ = 0;
	char *nospace = trim(cmd_buffer);
	if (nospace[0] == ':')
		{
		nospace++;		// Skip :
		// Strip spaces and = from it.
		while (isspace(*nospace) || (*nospace == '='))
			nospace++;
		// label is until space/=/eol
		char* const beginlabel = nospace;
		while (*nospace && !isspace(*nospace) && (*nospace != '=')) 
			nospace++;

		*nospace = 0;
		if (stricmp(beginlabel, where) == 0)
			{
			// Found it! Store location and continue
			this->location = 0;
			DOS_SeekFile(file_handle, &(this->location), DOS_SEEK_CUR);
			DOS_CloseFile(file_handle);
			return true;
			}
		}
	if (n)
		goto again;
	DOS_CloseFile(file_handle);
	delete this;
	return false;	
	}

void BatchFile::Shift(void)
	{
	cmd->Shift(1);
	}
