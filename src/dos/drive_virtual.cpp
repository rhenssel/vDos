#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "vDos.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "callback.h"

struct VFILE_Block {
	const char * name;
	Bit8u * data;
	Bit16u size;
	Bit16u date;
	Bit16u time;
	VFILE_Block * next;
};

static VFILE_Block * first_file;	

void VFILE_Register(const char * name, Bit8u * data, Bit16u size)
	{
	VFILE_Block * new_file = new VFILE_Block;
	new_file->name = name;
	new_file->data = data;
	new_file->size = size;

	_SYSTEMTIME systime;
	GetLocalTime(&systime);						// Windows localdate/time
	new_file->date = DOS_PackDate(systime.wYear, systime.wMonth, systime.wDay);
	new_file->time = DOS_PackTime(systime.wHour, systime.wMinute, systime.wSecond);

	new_file->next = first_file;
	first_file = new_file;
	}

class Virtual_File : public DOS_File
	{
public:
	Virtual_File(VFILE_Block* cur_file);
	bool Read(Bit8u* data, Bit16u * size);
	bool Write(Bit8u* data, Bit16u * size);
	bool Seek(Bit32u* pos, Bit32u type);
	Bit16u GetInformation(void);
private:
	Bit32u file_size;
	Bit32u file_pos;
	Bit8u* file_data;
	};

Virtual_File::Virtual_File(VFILE_Block* new_file)
	{
	file_size = new_file->size;
	file_data = new_file->data;
	file_pos = 0;
	date = new_file->date;
	time = new_file->time;
	}

bool Virtual_File::Read(Bit8u* data, Bit16u * size)
	{
	Bit32u left = file_size-file_pos;
	if (left < *size)
		*size = (Bit16u)left;
	memcpy(data, file_data+file_pos, *size);
	file_pos += *size;
	return true;
	}

bool Virtual_File::Write(Bit8u* data, Bit16u* size)
	{
	return false;	// Not really writeable
	}

bool Virtual_File::Seek(Bit32u* new_pos, Bit32u type)
	{
	switch (type)
		{
	case DOS_SEEK_SET:
		if (*new_pos > file_size)
			return false;
		file_pos = *new_pos;
		break;
	case DOS_SEEK_CUR:
		if ((*new_pos+file_pos) > file_size)
			return false;
		file_pos = *new_pos+file_pos;
		break;
	case DOS_SEEK_END:
		if (*new_pos > file_size)
			return false;
		file_pos = file_size-*new_pos;
		break;
	default:
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
		}
	*new_pos = file_pos;
	return true;
	}

Bit16u Virtual_File::GetInformation(void)
	{
	return 0x40;	// read-only drive
	}

Virtual_Drive::Virtual_Drive()
	{
	strcpy(info, "Reserved virtual bootdisk");
	search_file = 0;
	this->SetLabel("Z_Drive");
	}

bool Virtual_Drive::FileOpen(DOS_File* * file, char* name, Bit32u flags)
	{
	// Scan through the internal list of files
	for (VFILE_Block* cur_file = first_file; cur_file; cur_file = cur_file->next)
		if (!stricmp(name, cur_file->name))			// We have a match
			{
			*file = new Virtual_File(cur_file);
			(*file)->flags = flags;
			return true;
			}
	return false;
	}

bool Virtual_Drive::FileCreate(DOS_File * * file, char * name, Bit16u attributes)
	{
	return false;
	}

bool Virtual_Drive::FileUnlink(char * name)
	{
	return false;
	}

bool Virtual_Drive::RemoveDir(char * dir)
	{
	return false;
	}

bool Virtual_Drive::MakeDir(char * dir)
	{
	return false;
	}

bool Virtual_Drive::TestDir(char * dir)
	{
	if (!dir[0])
		return true;		// only valid dir is empty/root dir
	return false;
	}

bool Virtual_Drive::FileExists(const char* name)
	{
	for (VFILE_Block* cur_file = first_file; cur_file; cur_file = cur_file->next)
		if (!stricmp(name, cur_file->name))
			return true;
	return false;
	}

bool Virtual_Drive::FindFirst(char* _dir, DOS_DTA & dta, bool fcb_findfirst)
	{
	search_file = first_file;
	Bit8u attr;
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	if (attr == DOS_ATTR_VOLUME)
		{
		dta.SetResult("vDOS", 0, 0, 0, DOS_ATTR_VOLUME);
		return true;
		}
	if ((attr & DOS_ATTR_VOLUME) && !fcb_findfirst)
		{
		if (WildFileCmp("vDOS", pattern))
			{
			dta.SetResult("vDOS", 0, 0, 0, DOS_ATTR_VOLUME);
			return true;
			}
		}
	return FindNext(dta);
	}

bool Virtual_Drive::FindNext(DOS_DTA & dta)
	{
	Bit8u attr;
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	while (search_file)
		{
		if (WildFileCmp(search_file->name, pattern))
			{
			dta.SetResult(search_file->name, stricmp(search_file->name, "AUTOEXEC.BAT") ? 0 : search_file->size, search_file->date, search_file->time, DOS_ATTR_ARCHIVE);
			search_file = search_file->next;
			return true;
			}
		search_file = search_file->next;
		}
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
	}

bool Virtual_Drive::GetFileAttr(char* name, Bit16u* attr)
	{
	for (VFILE_Block* cur_file = first_file; cur_file; cur_file = cur_file->next)
		if (!stricmp(name, cur_file->name))
			{ 
			*attr = DOS_ATTR_READ_ONLY;
			return true;
			}
	return false;
	}

bool Virtual_Drive::Rename(char* oldname, char * newname)
	{
	return false;
	}

bool Virtual_Drive::AllocationInfo(Bit16u* _bytes_sector, Bit8u* _sectors_cluster, Bit16u* _total_clusters, Bit16u* _free_clusters)
	{
	// Always report 0 bytes free
	// Total size is always 1 gb
	*_bytes_sector = 512;
	*_sectors_cluster = 127;
	*_total_clusters = 16513;
	*_free_clusters = 0;
	return true;
	}

Bit8u Virtual_Drive::GetMediaByte(void)
	{
	return 0xF8;
	}
