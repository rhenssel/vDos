#include "vDos.h"
#include "dos_system.h"
#include "drives.h"
#include "support.h"

bool WildFileCmp(const char * file, const char * wild) 
	{
	char file_name[9];
	char file_ext[4];
	char wild_name[9];
	char wild_ext[4];
	const char * find_ext;
	Bitu r;

	strcpy(wild_name, strcpy(file_name, "        "));
	strcpy(wild_ext, strcpy(file_ext, "   "));

	find_ext = strrchr(file, '.');
	if (find_ext)
		{
		Bitu size = (Bitu)(find_ext-file);
		if (size > 8)
			size = 8;
		memcpy(file_name, file, size);
		find_ext++;
		memcpy(file_ext, find_ext, (strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
		}
	else
		memcpy(file_name, file, (strlen(file) > 8) ? 8 : strlen(file));
	upcase(file_name);
	upcase(file_ext);
	find_ext = strrchr(wild, '.');
	if (find_ext)
		{
		Bitu size = (Bitu)(find_ext-wild);
		if (size > 8)
			size = 8;
		memcpy(wild_name, wild,size);
		find_ext++;
		memcpy(wild_ext, find_ext, (strlen(find_ext)>3) ? 3 : strlen(find_ext));
		}
	else
		memcpy(wild_name, wild, (strlen(wild) > 8) ? 8 : strlen(wild));
	upcase(wild_name);
	upcase(wild_ext);
	// Names are right do some checking
	r = 0;
	while ( r <8)
		{
		if (wild_name[r] == '*')
			break;
		if (wild_name[r] != '?' && wild_name[r] != file_name[r])
			return false;
		r++;
		}
    r = 0;
	while (r < 3)
		{
		if (wild_ext[r] == '*')
			return true;
		if (wild_ext[r] != '?' && wild_ext[r] != file_ext[r])
			return false;
		r++;
		}
	return true;
	}

DOS_Drive::DOS_Drive()
	{
	curdir[0] = 0;
	info[0] = 0;
	}

void DOS_Drive::SetLabel(char const * const input)
	{
	strcpy(label, input);
	}

char * DOS_Drive::GetInfo(void)
	{
	return info;
	}

