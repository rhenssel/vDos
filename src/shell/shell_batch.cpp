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
	if (!DOS_OpenFile(filename.c_str(), 128, &file_handle))			// Open the batchfile and seek to stored postion
		{
		delete this;
		return false;
		}
	DOS_SeekFile(file_handle, &(this->location), DOS_SEEK_SET);
emptyline:
	Bit8u c;
	Bit16u n;
	char * cmd_write = line;
	do
		{
		n = 1;
		DOS_ReadFile(file_handle, &c, &n);
		if (n > 0)
			*cmd_write++ = c;
		}
	while (c != '\n' && n);
	*cmd_write = 0;
	if (!n && cmd_write == line)
		{
		DOS_CloseFile(file_handle);									// Close file and delete batchfile
		delete this;
		return false;	
		}
	if (*line == 0 || *line == ':')
		goto emptyline;

	this->location = 0;												// Store current location and close bat file
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
