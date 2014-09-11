#include <stdlib.h>
#include <string.h>
#include <algorithm>	//std::copy
#include <iterator>		//std::front_inserter
#include "shell.h"
#include "regs.h"
#include "callback.h"
#include "support.h"

#include "timer.h"
#include "bios.h"

static char* exec_ext[] = {".COM", ".EXE", ".BAT"};

void DOS_Shell::ShowPrompt(void)
	{
	char dir[DOS_PATHLENGTH];
	dir[0] = 0;											// DOS_GetCurrentDir doesn't always return something. (if drive is messed up)
	DOS_GetCurrentDir(0, dir);
	WriteOut("%c:\\%s>", DOS_GetDefaultDrive()+'A', dir);
	}

static void outc(Bit8u c)
	{
	Bit16u n = 1;
	DOS_WriteFile(STDOUT, &c, &n);
	}

void DOS_Shell::InputCommand(char * line)
	{
	Bitu size = CMD_MAXLINE-2;							// lastcharacter+0
	Bit8u c;
	Bit16u n = 1;
	Bitu str_len = 0;
	Bitu str_index = 0;
	Bit16u len = 0;
	bool current_hist = false;							// current command stored in history?

	line[0] = '\0';
	WriteOut("\033[s");									// Save cursor position (directly after prompt)

	std::list<std::string>::iterator it_history = l_history.begin();

	while (size)
		{
		dos.echo = false;
		while(!DOS_ReadFile(input_handle, &c, &n))
			{
			Bit16u dummy;
			DOS_CloseFile(input_handle);
			DOS_OpenFile("con", 2, &dummy);
			LOG(LOG_MISC,LOG_ERROR)("Reopening the input handle.This is a bug!");
			}
		if (!n)
			{
			size = 0;									// Kill the while loop
			continue;
			}
		switch (c)
			{
		case 0:											// Extended Keys
			{
			DOS_ReadFile(input_handle, &c, &n);
			switch (c)
				{
			case 0x3d:									// F3
				if (!l_history.size())
					break;
				WriteOut("\033[u\033[K");				// Go to prompt and erase all right of it
				it_history = l_history.begin();
				strcpy(line, it_history->c_str());
				len = (Bit16u)it_history->length();
				str_len = str_index = len;
				size = CMD_MAXLINE - str_index - 2;
				DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
				it_history ++;
				break;
			case 0x4B:									// LEFT
				if (str_index)
					{
					outc(8);
					str_index--;
					}
				break;
			case 0x4D:									// RIGHT
				if (str_index < str_len)
					outc(line[str_index++]);
				break;
			case 0x47:									// HOME
				WriteOut("\033[u");						// Go to prompt
				str_index = 0;
				break;
			case 0x4F:									// END
				while (str_index < str_len)
					outc(line[str_index++]);
				break;
			case 0x48:									// UP
				if (l_history.empty() || it_history == l_history.end())
					break;
				WriteOut("\033[u\033[K");				// Go to prompt and erase all right of it
				strcpy(line, it_history->c_str());
				len = (Bit16u)it_history->length();
				str_len = str_index = len;
				size = CMD_MAXLINE - str_index - 2;
				DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
				it_history ++;
				break;
			case 0x50:									// DOWN
				if (l_history.empty() || it_history == l_history.begin())
					break;
				// not very nice but works ..
				it_history --;
				if (it_history == l_history.begin())
					{
					// no previous commands in history
					it_history ++;

					// remove current command from history
					if (current_hist)
						{
						current_hist = false;
						l_history.pop_front();
						}
					break;
					}
				else
					it_history --;

				WriteOut("\033[u\033[K");				// Go to prompt and erase all right of it
				strcpy(line, it_history->c_str());
				len = (Bit16u)it_history->length();
				str_len = str_index = len;
				size = CMD_MAXLINE - str_index - 2;
				DOS_WriteFile(STDOUT, (Bit8u *)line, &len);
				it_history ++;
				break;
			case 0x53:									// DELETE
				{
				if (str_index >= str_len)
					break;
				Bit16u a = str_len-str_index-1;
				Bit8u* text=reinterpret_cast<Bit8u*>(&line[str_index+1]);
				DOS_WriteFile(STDOUT, text, &a);		// Write buffer to screen
				outc(' ');
				outc(8);
				for (Bitu i=str_index; i<str_len-1; i++)
					{
					line[i] = line[i+1];
					outc(8);
					}
				line[--str_len] = 0;
				size++;
				}
				break;
			default:
				break;
				}
			}
			break;
		case 0x03:										// Ctrl C
			WriteOut("^C\n\n");
			if (echo)
				ShowPrompt();
			WriteOut("\033[s");							// Save cursor position (directly after prompt)
			*line = 0;									// reset the line.
			str_len = 0;
			str_index = 0;
			len = 0;
			break;
		case 0x08:										// BackSpace
			if (str_index)
				{
				outc(8);
				Bit32u str_remain = str_len - str_index;
				size++;
				if (str_remain)
					{
					memmove(&line[str_index-1], &line[str_index], str_remain);
					line[--str_len] = 0;
					str_index --;
					// Go back to redraw
					for (Bit16u i = str_index; i < str_len; i++)
						outc(line[i]);
					}
				else
					{
					line[--str_index] = '\0';
					str_len--;
					}
				outc(' ');
				outc(8);
				// moves the cursor left
				while (str_remain--)
					outc(8);
				}
			break;
		case 0x0a:										// New Line not handled
			/* Don't care */
			break;
		case 0x0d:										// Return
			outc('\n');
			size = 0;									// Kill the while loop
			break;
		case '\t':
			break;										// Maybe output a tab, not usefull at all
		case 0x1b:										// ESC, write a backslash and return to the next line
			WriteOut("\033[u\033[K");					// Go to prompt and erase all right of it
			*line = 0;									// reset the line.
			str_len = 0;
			str_index = 0;
			len = 0;
			break;
		default:
			if (str_index < str_len)
				{ // mem_readb(BIOS_KEYBOARD_FLAGS1)&0x80) dev_con.h ?
				outc(' ');								// move cursor one to the right.
				Bit16u a = str_len - str_index;
				Bit8u* text = reinterpret_cast<Bit8u*>(&line[str_index]);
				DOS_WriteFile(STDOUT, text, &a);		//write buffer to screen
				outc(8);								// undo the cursor the right.
				for (Bitu i = str_len; i > str_index; i--)
					{
					line[i] = line[i-1];				// move internal buffer
					outc(8);							// move cursor back (from write buffer to screen)
					}
				line[++str_len] = 0;					// new end (as the internal buffer moved one place to the right
				size--;
				};
		   
			line[str_index] = c;
			str_index ++;
			if (str_index > str_len)
				{ 
				line[str_index] = '\0';
				str_len++;
				size--;
				}
			DOS_WriteFile(STDOUT, &c, &n);
			break;
			}
		}

	if (!str_len)
		return;

	str_len++;

	// remove current command from history if it's there
	if (current_hist)
		{
		current_hist = false;
		l_history.pop_front();
		}

	// add command line to history
	l_history.push_front(line); it_history = l_history.begin();
	}

bool DOS_Shell::Execute(char* name, char* args)
	{
	char line[CMD_MAXLINE];
	if (strlen(args) != 0)
		{
		if (*args != ' ')												// Put a space in front (Jos: why?)
			{	
			line[0] = ' ';
			strncpy(line+1, args, CMD_MAXLINE-2);
			}
		else
			strncpy(line, args, CMD_MAXLINE);
		line[CMD_MAXLINE-1] = 0;
		}
	else
		line[0] = 0;
	
	char* fullname = Which(name);										// Check for a full name
	if (!fullname)
		return false;

	char* extension = strrchr(fullname, '.');							// Extension is eventualy added by Which
	if (stricmp(extension, ".BAT") == 0)								// Run the .bat file, delete old batch file if call is not active
		{
		bool temp_echo = echo;											// Keep the current echostate (as delete bf might change it )
		if (bf && !call)
			delete bf;
		bf = new BatchFile(this, fullname, name, line);
		echo = temp_echo;												// Restore it.
		} 
	else 
		{
		if (stricmp(extension, ".COM") && stricmp(extension, ".EXE"))	// Only .bat .exe .com extensions may be be executed by the shell
			return false;
		reg_sp -= 0x200;												// Allocate some stack space for tables in physical memory
		
		DOS_ParamBlock block(SegPhys(ss)+reg_sp);						// Add Parameter block
		block.Clear();
		RealPt file_name = RealMakeSeg(ss, reg_sp+0x20);				// Add a filename
		vPC_rBlockWrite(dWord2Ptr(file_name), fullname, (Bitu)(strlen(fullname)+1));

		CommandTail cmdtail;											// Fill the command line
		cmdtail.count = 0;
		memset(&cmdtail.buffer, 0, 127);								// Else some part of the string is unitialized (valgrind)
		if (strlen(line) > 126)
			line[126] = 0;
		cmdtail.count = (Bit8u)strlen(line);
		memcpy(cmdtail.buffer, line, strlen(line));
		cmdtail.buffer[strlen(line)] = 0xd;
		
		vPC_rBlockWrite(SegPhys(ss)+reg_sp+0x100, &cmdtail, 128);		// Copy command line in stack block too
																		// Parse FCB (first two parameters) and put them into the current DOS_PSP
		Bit8u add;
		FCB_Parsename(dos.psp(), 0x5C, 0x00, cmdtail.buffer, &add);
		FCB_Parsename(dos.psp(), 0x6C, 0x00, &cmdtail.buffer[add], &add);
		block.exec.fcb1 = SegOff2dWord(dos.psp(), 0x5C);
		block.exec.fcb2 = SegOff2dWord(dos.psp(), 0x6C);
		block.exec.cmdtail = RealMakeSeg(ss, reg_sp+0x100);				// Set the command line in the block and save it
		block.SaveData();

		reg_ax = 0x4b00;												// Start up a dos execute interrupt
		SegSet16(ds, SegValue(ss));										// Filename pointer
		reg_dx = RealOff(file_name);
		SegSet16(es, SegValue(ss));										// Paramblock
		reg_bx = reg_sp;
		SETFLAGBIT(IF, false);
		CALLBACK_RunRealInt(0x21);
		reg_sp += 0x200;												// Restore CS:IP and the stack
		}
	return true;														// Executable started
	}

char * DOS_Shell::Which(char* name)
	{
	size_t name_len = strlen(name);
	if (name_len >= DOS_PATHLENGTH)
		return 0;

	bool has_extension = false;
	bool has_path = false;
	char* pos;
	if (pos = strrchr(name, '\\'))										// Path, extension specified?
		{
		has_path = true;
		if (strchr(pos, '.'))
			has_extension = true;
		}
	else if (strchr(name, '.'))
		has_extension = true;
	if (has_extension && DOS_FileExists(name))
		return name;

	static char which_ret[DOS_PATHLENGTH+4];							// Try to find .com .exe .bat appended
	if (!has_extension)
		for (int i = 0; i < 3; i++)
			if (DOS_FileExists(strcat(strcpy(which_ret, name), exec_ext[i])))
				return which_ret;
	if (has_path || name[1] == ':')										// If path or drive included FAIL (not found above)
		return 0;

	char path[DOS_PATHLENGTH];											// No drive or path in filename, look through path environment string
	if (const char* pathenv = GetEnvStr("PATH"))
		while (*pathenv)
		{
		while (*pathenv == ';')											// Remove ;'s at the beginnings
			pathenv++;
		Bitu i_path = 0;												// Get next entry
		while (*pathenv && (*pathenv != ';') && (i_path < DOS_PATHLENGTH-1))
			path[i_path++] = *pathenv++;

		if (i_path == DOS_PATHLENGTH-1)									// If max size. move till next ;
			while (*pathenv && (*pathenv != ';')) 
				pathenv++;
		path[i_path] = 0;

		if (size_t len = strlen(path))									// Check entry
			{
			if (path[len - 1] != '\\')
				{
				strcat(path, "\\"); 
				len++;
				}
			if ((name_len + len + 1) >= DOS_PATHLENGTH)					// If name too long =>next
				continue;
			if (DOS_FileExists(strcat(path, name)))
				return strcpy(which_ret, path);
			if (!has_extension)
				for (int i = 0; i < 3; i++)
					if (DOS_FileExists(strcat(strcpy(which_ret, path), exec_ext[i])))
						return which_ret;
			}
		}
	return 0;
	}
