#include "vDos.h"
#include "support.h"
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <stdlib.h>
#include <stdio.h>

#ifndef vDOS_PROGRAMS_H
#include "programs.h"
#endif

using namespace std;

bool CommandLine::FirstStart()
	{
	cmd_it it;
	if (!(FindEntry("/INIT", it)))
		return false;
	cmds.erase(it);
	return true;
	}

const char * CommandLine::GetVarNum(unsigned int which)
	{
	if (which > cmds.size())
		return "";
	cmd_it it = cmds.begin();
	for (; which > 1; which--)
		it++;
	return (*it).c_str();
	}

bool CommandLine::FindEntry(char const * const name, cmd_it & it)
	{
	for (it = cmds.begin(); it != cmds.end(); it++)
		if (!stricmp((*it).c_str(), name))
			return true;
	return false;
	}

bool CommandLine::GetCommand(std::string & value)
	{
	for (cmd_it it = cmds.begin(); it != cmds.end(); it++)
		if (strnicmp("/C", (*it).c_str(), 2) == 0)
			{
			value = ((*it).c_str() + 2);
			while (++it != cmds.end())
				{
				value += " ";
				value += (*it);
				}
			return true;
			}
	return false;
	}

CommandLine::CommandLine(char const * const name, char const * const cmdline)
	{
	if (name)
		file_name = name;
	// Parse the cmds and put them in the list
	bool inword, inquote;
	char c;
	inword = false;
	inquote = false;
	std::string str;
	const char* c_cmdline = cmdline;
	while ((c = *c_cmdline) != 0)
		{
		if (inquote)
			{
			if (c != '"')
				str += c;
			else
				{
				inquote = false;
				cmds.push_back(str);
				str.erase();
				}
			}
		else if (inword)
			{
			if (c != ' ')
				str += c;
			else
				{
				inword = false;
				cmds.push_back(str);
				str.erase();
				}
			} 
		else if (c == '"')
			inquote = true;
		else if	(c != ' ')
			{
			str += c;
			inword = true;
			}
		c_cmdline++;
		}
	if (inword || inquote)
		cmds.push_back(str);
	}

void CommandLine::Shift(unsigned int amount)
	{
	while (amount--)
		{
		file_name = cmds.size() ? (*(cmds.begin())) : "";
		if (cmds.size())
			cmds.erase(cmds.begin());
		}
	}
