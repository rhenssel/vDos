#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "vDos.h"
#include "bios.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "drives.h"
#include "Shlwapi.h"

#define FCB_SUCCESS			0
#define FCB_READ_NODATA		1
#define FCB_READ_PARTIAL	3
#define FCB_ERR_NODATA		1
#define FCB_ERR_EOF			3
#define FCB_ERR_WRITE		1

Bitu DOS_FILES = 127;
DOS_File ** Files;
DOS_Drive * Drives[DOS_DRIVES];

// FCB drive:DOS 8.3 filename to string, padding spaces from name and extension removed
void FCBNameToStr(char *name)
	{
	int i, j;
	for (i = 9; i > 1; i--)
		if (name[i] != ' ')
			break;
	for (j = 13; j > 9; j--)
		if (name[j] != ' ')
			break;
	for (int k = 10; k <= j; k++)
		name[++i] = name[k];
	name[++i] = 0;
	}

bool NormalizePath(char * path)									// strips names to max 8 and extensions to max 3 positions
	{
	if (strpbrk(path, "+[] "))									// return false if forbidden chars
		return false;

	// no checking for all invalid sequences, just strip to 8.3 and return into passed string (result will never be longer)
	char * output = path;
	bool inExt = false;
	int count = 0;
	while (char c = *path++)
		{
		if (c == '\\' || c == '/')
			{
			count = 0;
			inExt = false;
			*output++ = '\\';
			}
		else if (inExt)
			{
			if ((!count && c == '.') || (count++ < 3 && c != '.'))	// misses nnn.e.e, what to do with that?
				*output++ = c;
			}
		else if (c == '.')
			{
			count = 0;
			inExt = true;
			*output++ = c;
			}
		else if (count++ < 8)
			*output++ = c;
		}
	*output = 0;
	return true;
	}

Bit8u DOS_GetDefaultDrive(void)
	{
	Bit8u d = DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).GetDrive();
	if (d != dos.current_drive)
		LOG(LOG_DOSMISC,LOG_ERROR)("SDA drive %d not the same as dos.current_drive %d", d, dos.current_drive);
	return dos.current_drive;
	}

void DOS_SetDefaultDrive(Bit8u drive)
	{
	if (drive <= DOS_DRIVES && Drives[drive])
		{
		dos.current_drive = drive;
		DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).SetDrive(drive);
		}
	}

char makename_out[DOS_PATHLENGTH * 2];				// DOS_Makename is called sometimes repeatively for the same file
char makename_in[DOS_PATHLENGTH * 2];				// so keep result to speed up things

bool DOS_MakeName(char const* const name, char* fullname, Bit8u* drive)	// Nb, returns path w/o leading '\\'!
	{
	if (!name || *name == 0 || *name == ' ')		// Both \0 and space are seperators and empty filenames report file not found
		{
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
		}
	char const* name_ptr = name;
	// First get the drive
	if (name_ptr[1] == ':')
		{
		*drive = (*name_ptr | 0x20)-'a';
		name_ptr += 2;
		}
	else
		*drive = DOS_GetDefaultDrive();
	if (*drive >= DOS_DRIVES || !Drives[*drive] || strlen(name_ptr) >= DOS_PATHLENGTH)
		{ 
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false; 
		}
//	if (strcmp(name_ptr, makename_in))				// if different from last call		// REMOVED messes up chdir abc followed by chdir abc
//		{
		strcpy(makename_in, name_ptr);
		char tempin[DOS_PATHLENGTH * 2];			// times 2 to hold both curdir and name
		if (*name_ptr != '\\' && *name_ptr != '/' && *Drives[*drive]->curdir)
			strcat(strcat(strcat(strcpy(tempin, "\\"), Drives[*drive]->curdir), "\\"), name_ptr);
		else
			strcpy(tempin, name_ptr);
		char *p = strchr(tempin, '\0');				// right trim spaces
		while (--p >= tempin && isspace(*p)) {};
		p[1] = '\0';
		if (!(PathCanonicalize(makename_out, tempin) && NormalizePath(makename_out)))
			{ 
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false; 
			}
//		}
	strcpy(fullname, makename_out + (*makename_out =='\\' ? 1 : 0));			// leading '\\' dropped again
	return true;	
	}

bool DOS_GetCurrentDir(Bit8u drive, char * const buffer)
	{
	if (drive == 0)
		drive = DOS_GetDefaultDrive();
	else
		drive--;
	if (drive >= DOS_DRIVES || !Drives[drive])
		{
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
		}
	strcpy(buffer, Drives[drive]->curdir);
	return true;
	}

bool DOS_ChangeDir(char const* const dir)
	{
	Bit8u drive;
	const char *testdir = dir;
	if (*testdir && testdir[1] == ':')
		{
		drive = (*dir | 0x20)-'a';
		if (drive >= DOS_DRIVES || !Drives[drive])
			{
			DOS_SetError(DOSERR_INVALID_DRIVE);
			return false;
			}
		testdir += 2;
		}
	size_t len = strlen(testdir);
	char fulldir[DOS_PATHLENGTH];
	if (!len || !DOS_MakeName(dir, fulldir, &drive) || (*fulldir && testdir[len-1] == '\\'))
		{ }
	else if (Drives[drive]->TestDir(fulldir))
		{
		for (int i = 0; fulldir[i]; i++)			// names in MS-DOS are allways uppercase
			fulldir[i] = toupper(fulldir[i]);
		strcpy(Drives[drive]->curdir, fulldir);
		return true;
		}
	DOS_SetError(DOSERR_PATH_NOT_FOUND);
	return false;
	}

bool DOS_MakeDir(char const* const dir)
	{
	Bit8u drive;
	const char *testdir = dir;
	if (*testdir && testdir[1] == ':')
		{
		drive = (*testdir | 0x20)-'a';
		if (drive >= DOS_DRIVES || !Drives[drive])
			{
			DOS_SetError(DOSERR_INVALID_DRIVE);
			return false;
			}
		testdir += 2;
		}
	size_t len = strlen(testdir);
	char fulldir[DOS_PATHLENGTH];
	if (!len || !DOS_MakeName(dir, fulldir, &drive) || (*fulldir && testdir[len-1] == '\\'))
		{
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
		}
	if (Drives[drive]->MakeDir(fulldir))
		return true;   

	// Determine reason for failing
	if (Drives[drive]->TestDir(fulldir)) 
		DOS_SetError(DOSERR_ACCESS_DENIED);
	else
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
	return false;
	}

bool DOS_RemoveDir(char const * const dir)
	{
	/* We need to do the test before the removal as can not rely on
	 * the host to forbid removal of the current directory.
	 * We never change directory. Everything happens in the drives.
	 */
	Bit8u drive;
	const char *testdir = dir;
	if (*testdir && testdir[1] == ':')
		{
		drive = (*testdir | 0x20)-'a';
		if (drive >= DOS_DRIVES || !Drives[drive])
			{
			DOS_SetError(DOSERR_INVALID_DRIVE);
			return false;
			}
		testdir += 2;
		}
	size_t len = strlen(testdir);
	char fulldir[DOS_PATHLENGTH];
	if (!len || !DOS_MakeName(dir, fulldir, &drive) || (*fulldir && testdir[len-1] == '\\'))
		{
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
		}
	// Check if exists
	if (!Drives[drive]->TestDir(fulldir))
		{
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
		}
	if (strcmp(Drives[drive]->curdir, fulldir) == 0)		// Test if it's current directory
		{
		DOS_SetError(DOSERR_REMOVE_CURRENT_DIRECTORY);
		return false;
		}
	if (Drives[drive]->RemoveDir(fulldir))
		return true;
	// Failed. We know it exists and it's not the current dir assume non empty
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
	}

bool DOS_Rename(char const * const oldname, char const * const newname)
	{
	Bit8u driveold;
	char fullold[DOS_PATHLENGTH];
	if (!DOS_MakeName(oldname, fullold, &driveold))
		{
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
		}
	Bit8u drivenew;
	char fullnew[DOS_PATHLENGTH];
	if (!DOS_MakeName(newname, fullnew, &drivenew))
		return false;
	// No tricks with devices
	if ((DOS_FindDevice(oldname) != DOS_DEVICES) || (DOS_FindDevice(newname) != DOS_DEVICES))
		{
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
		}
	// Must be on the same drive
	if (driveold != drivenew)
		{
		DOS_SetError(DOSERR_NOT_SAME_DEVICE);
		return false;
		}
	// Test if target exists => no access
	Bit16u attr;
	if (Drives[drivenew]->GetFileAttr(fullnew, &attr))
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
		}
	// Source must exist, check for path ?
	if (!Drives[driveold]->GetFileAttr(fullold, &attr ))
		{
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
		}
	if (Drives[drivenew]->Rename(fullold, fullnew))
		return true;
	// If it still fails. which error should we give ? PATH NOT FOUND or EACCESS
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
	}

bool DOS_FindFirst(char * search, Bit16u attr, bool fcb_findfirst)
	{
	DOS_DTA dta(dos.dta());
	size_t len = strlen(search);
	if (len && search[len - 1] == '\\' && !((len > 2) && (search[len - 2] == ':') && (attr == DOS_ATTR_VOLUME)))
		{ 
		// Dark Forces installer, but c:\ is allright for volume labels(exclusively set)
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
		}
	char fullsearch[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(search, fullsearch, &drive))
		return false;
	// Split the search in dir and pattern
	char dir[DOS_PATHLENGTH];
	char pattern[DOS_PATHLENGTH];
	char * find_last = strrchr(fullsearch, '\\');
	if (!find_last)		// No dir
		{
		strcpy(pattern, fullsearch);
		dir[0] = 0;
		}
	else
		{
		*find_last = 0;
		strcpy(pattern, find_last+1);
		strcpy(dir, fullsearch);
		}

	dta.SetupSearch(drive, (Bit8u)attr, pattern);

	// Check for devices. FindDevice checks for leading subdir as well
	if (bool device = (DOS_FindDevice(search) != DOS_DEVICES))
		{
		if (!(attr & DOS_ATTR_DEVICE))
			return false;
		if (find_last = strrchr(pattern, '.'))
			*find_last = 0;
		// TODO use current date and time
		dta.SetResult(pattern, 0, 0, 0, DOS_ATTR_DEVICE);
		return true;
		}
	 return Drives[drive]->FindFirst(dir, dta, fcb_findfirst);
	}

bool DOS_FindNext(void)
	{
	DOS_DTA dta(dos.dta());
	Bit8u i = dta.GetSearchDrive();
	if (i >= DOS_DRIVES || !Drives[i])	// Corrupt search.
		{
		LOG(LOG_FILES,LOG_ERROR)("Corrupt search!!!!");
		DOS_SetError(DOSERR_NO_MORE_FILES); 
		return false;
		} 
	return Drives[i]->FindNext(dta);
	}

bool DOS_ReadFile(Bit16u entry, Bit8u* data, Bit16u* amount)
	{
	Bit32u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	return Files[handle]->Read(data, amount);
}

bool DOS_WriteFile(Bit16u entry, Bit8u* data, Bit16u* amount)
	{
	Bit32u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
/*
	if ((Files[handle]->flags & 0x0f) == OPEN_READ)) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
*/
	return Files[handle]->Write(data, amount);
	}

bool DOS_SeekFile(Bit16u entry, Bit32u* pos, Bit32u type)
	{
	Bit32u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	return Files[handle]->Seek(pos, type);
	}

bool DOS_LockFile(Bit16u entry, Bit8u mode, Bit32u pos, Bit32u size)
	{
	Bit32u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	return Files[handle]->LockFile(mode, pos, size);
	}

bool DOS_CloseFile(Bit16u entry)
	{
	Bit32u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	Files[handle]->Close();
	DOS_PSP psp(dos. psp());
	psp.SetFileHandle(entry, 0xff);

	if (Files[handle]->RemoveRef() <= 0)
		{
		delete Files[handle];
		Files[handle] = 0;
		}
	return true;
	}

bool DOS_FlushFile(Bit16u entry)
	{
	Bit32u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	return true;
	}

static bool PathExists(char const* const name)
	{
	char temp[MAX_PATH_LEN];
	char* lead = strrchr(strcpy(temp, name), '\\');
	if (!lead || lead == temp)
		return true;
	*lead = 0;
	Bit8u drive;
	char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(temp, fulldir, &drive) || !Drives[drive]->TestDir(fulldir))
		return false;
	return true;
	}

bool DOS_CreateFile(char const* name, Bit16u attributes, Bit16u* entry)
	{
	// Creation of a device is the same as opening it
	if (DOS_FindDevice(name) != DOS_DEVICES)
		return DOS_OpenFile(name, OPEN_READWRITE, entry);

	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;
	// Check for a free file handle
	Bit8u handle;
	for (handle = 0; handle < DOS_FILES; handle++)
		if (!Files[handle])
			break;
	if (handle == DOS_FILES)
		{
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
		}
	// We have a position in the main table, now find one in the psp table
	DOS_PSP psp(dos.psp());
	*entry = psp.FindFreeFileEntry();
	if (*entry == 0xff)
		{
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
		}
	// Don't allow directories to be created
	if (attributes&DOS_ATTR_DIRECTORY)
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
		}
	if (Drives[drive]->FileCreate(&Files[handle], fullname, attributes))
		{ 
		Files[handle]->SetDrive(drive);
		Files[handle]->AddRef();
		psp.SetFileHandle(*entry, handle);
		return true;
		}
	DOS_SetError(PathExists(name) ? DOSERR_FILE_ALREADY_EXISTS : DOSERR_PATH_NOT_FOUND);
	return false;
	}

static int wpOpenDevCount = 0;								// For WP - write file to clipboard, we have to return File not found once and twice normal
															// WP opens the file (device) to check if it exists - creates/closes - opens/writes/closes - opens/seeks/closes
static DWORD wpOpenDevTime = 0;

bool DOS_OpenFile(char const* name, Bit8u flags, Bit16u* entry)
	{
	Bit16u attr;
	Bit8u devnum = DOS_FindDevice(name);					// First check for devices
	bool device = (devnum != DOS_DEVICES);

	if (device && wpVersion)								// WP - clipboard
		{
		if (GetTickCount() > wpOpenDevTime + 1000)			// recalibrate after some (1/2 sec) time
			{
			wpOpenDevCount = 0;
			wpOpenDevTime = GetTickCount();
			}
		if ((devnum == 8 || devnum == 17) && !(++wpOpenDevCount&2))	// LPT9/COM9 (1-4 rejected by WP, 9 reserved for Edward Mendelson's macros)
			{
			DOS_SetError(PathExists(name) ? DOSERR_FILE_NOT_FOUND : DOSERR_PATH_NOT_FOUND);
			return false;
			}
		}

	if (!device && DOS_GetFileAttr(name, &attr))			// DON'T ALLOW directories to be openened.(skip test if file is device).
		if ((attr & DOS_ATTR_DIRECTORY) || (attr & DOS_ATTR_VOLUME))
			{
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
			}
	// Check if the name is correct
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;
	// Check for a free file handle
	Bit8u handle;
	for (handle = 0; handle < DOS_FILES; handle++)
		if (!Files[handle])
			break;
	if (handle == DOS_FILES)
		{
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
		}
	// We have a position in the main table, now find one in the psp table
	DOS_PSP psp(dos.psp());
	*entry = psp.FindFreeFileEntry();
	if (*entry == 0xff)
		{
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
		}
	bool exists = false;
	if (device)
		Files[handle] = new DOS_Device(*Devices[devnum]);
	else if (exists = Drives[drive]->FileOpen(&Files[handle], fullname, flags))
		Files[handle]->SetDrive(drive);
	if (exists || device)
		{
		Files[handle]->AddRef();
		psp.SetFileHandle(*entry, handle);
		return true;
		}
	// Test if file exists, but opened in read-write mode (and writeprotected)
	if (((flags&3) != OPEN_READ) && Drives[drive]->FileExists(fullname))
		DOS_SetError(DOSERR_ACCESS_DENIED);
	else
		DOS_SetError(PathExists(name) ? DOSERR_FILE_NOT_FOUND : DOSERR_PATH_NOT_FOUND);
	return false;
	}

bool DOS_OpenFileExtended(char const* name, Bit16u flags, Bit16u createAttr, Bit16u action, Bit16u* entry, Bit16u* status)
	{
	// FIXME: Not yet supported : Bit 13 of flags (int 0x24 on critical error)
	Bit16u result = 0;
	if (action == 0 || action&0xffec)
		{
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
		}
	if (DOS_OpenFile(name, (Bit8u)(flags&0xff), entry))		// File already exists
		{
		switch (action & 0x0f)
			{
		case 0:		// failed
			DOS_CloseFile(*entry);
			DOS_SetError(DOSERR_FILE_ALREADY_EXISTS);
			return false;
		case 1:		// file open (already done)
			result = 1;
			break;
		case 2:		// replace
			DOS_CloseFile(*entry);
			if (!DOS_CreateFile(name, createAttr, entry))
				return false;
			result = 3;
			break;
			}
		}
	else													// File doesn't exist
		{
		if ((action & 0xf0) == 0)
			return false;	// uses error code from failed open
		if (!DOS_CreateFile(name, createAttr, entry))		// Create File
			return false;	// uses error code from failed create
		result = 2;
		}
	*status = result;			// success
	return true;
	}

bool DOS_UnlinkFile(char const* const name)
	{
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;
	if (Drives[drive]->FileUnlink(fullname))
		return true;
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
	}

bool DOS_GetFileAttr(char const* const name, Bit16u* attr)
	{
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;
	if (Drives[drive]->GetFileAttr(fullname, attr))
		return true;
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
	}

bool DOS_SetFileAttr(char const* const name, Bit16u /*attr*/) 
	// this function does not change the file attributs
	// it just does some tests if file is available 
	// returns false when using on cdrom (stonekeep)
	{
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;	
	Bit16u attrTemp;
	return Drives[drive]->GetFileAttr(fullname, &attrTemp);
	}

bool DOS_Canonicalize(char const* const name, char* const big)
	{
	// TODO Add Better support for devices and shit but will it be needed i doubt it :) 
	Bit8u drive;
	if (!DOS_MakeName(name, &big[3], &drive))
		return false;
	big[0] = drive+'A';
	big[1] = ':';
	big[2] = '\\';
	return true;
	}

bool DOS_GetFreeDiskSpace(Bit8u drive, Bit16u* bytes, Bit8u* sectors, Bit16u* clusters, Bit16u* free)
	{
	if (drive == 0)
		drive = DOS_GetDefaultDrive();
	else
		drive--;
	if (drive >= DOS_DRIVES || !Drives[drive])
		{
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
		}
	return Drives[drive]->AllocationInfo(bytes, sectors, clusters, free);
	}

bool DOS_DuplicateEntry(Bit16u entry, Bit16u* newentry)
	{
	// Dont duplicate console handles
/*	if (entry<=STDPRN) {
		*newentry = entry;
		return true;
	};
*/	
	Bit8u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	DOS_PSP psp(dos.psp());
	*newentry = psp.FindFreeFileEntry();
	if (*newentry == 0xff)
		{
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
		}
	Files[handle]->AddRef();	
	psp.SetFileHandle(*newentry, handle);
	return true;
	}

bool DOS_ForceDuplicateEntry(Bit16u entry, Bit16u newentry)
	{
	if (entry == newentry)
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	Bit8u orig = RealHandle(entry);
	if (orig >= DOS_FILES || !Files[orig])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	Bit8u newone = RealHandle(newentry);
	if (newone < DOS_FILES && Files[newone])
		DOS_CloseFile(newentry);
	DOS_PSP psp(dos.psp());
	Files[orig]->AddRef();
	psp.SetFileHandle(newentry, orig);
	return true;
	}

bool DOS_CreateTempFile(char* const name, Bit16u* entry)	// check to see if indeed 13 0 bytes at end to receive temp filename?
	{
	size_t namelen = strlen(name);
	char * tempname = name+namelen;
	if (namelen == 0 || name[namelen-1] != '\\')		// no ending '\' append it, but is this correct (should be error) ?
		*tempname++ = '\\';
	tempname[8] = 0;			// no extension?
	do							// add random crap to the end of the path and try to open
		{
		dos.errorcode = 0;
		for (Bit32u i = 0; i < 8; i++)
			tempname[i] = (rand()%26)+'A';
		}
	while ((!DOS_CreateFile(name, 0, entry)) && (dos.errorcode == DOSERR_FILE_ALREADY_EXISTS));
	return dos.errorcode == 0;
	}

static bool isvalid(const char in)
	{
	return (Bit8u(in) > 0x1F) && (!strchr(":.;,=+ \t/\"[]<>|", in));
	}

#define PARSE_SEP_STOP          0x01
#define PARSE_DFLT_DRIVE        0x02
#define PARSE_BLNK_FNAME        0x04
#define PARSE_BLNK_FEXT         0x08

#define PARSE_RET_NOWILD        0
#define PARSE_RET_WILD          1
#define PARSE_RET_BADDRIVE      0xff

Bit8u FCB_Parsename(Bit16u seg, Bit16u offset, Bit8u parser, char* string, Bit8u* change)
	{
	char* string_begin = string;
	Bit8u ret = 0;
	if (!(parser & PARSE_DFLT_DRIVE))		// default drive forced, this intentionally invalidates an extended FCB
		vPC_rStosb(SegOff2Ptr(seg, offset), 0);
	DOS_FCB fcb(seg, offset, false);		// always a non-extended FCB
	// First get the old data from the fcb
#pragma pack (1)
	union {
		struct {
			char drive[2];
			char name[9];
			char ext[4];
		} part;
		char full[DOS_FCBNAME];
	} fcb_name;
#pragma pack()
	// Get the old information from the previous fcb
	fcb.GetName(fcb_name.full);
	fcb_name.part.drive[0] -= 'A'-1;
	fcb_name.part.drive[1] = 0;
	fcb_name.part.name[8] = 0;
	fcb_name.part.ext[3] = 0;
	// Strip of the leading sepetaror
	if ((parser & PARSE_SEP_STOP) && *string)		// ignore leading seperator
		if (strchr(":.;,=+", *string))
			string++;
	// strip leading spaces
	while ((*string == ' ') || (*string == '\t'))
		string++;
	bool hasdrive, hasname, hasext, finished;
	hasdrive = hasname = hasext = finished = false;
	// Check for a drive
	if (string[1] == ':')
		{
		fcb_name.part.drive[0] = 0;
		hasdrive = true;
		if (isalpha(string[0]) && Drives[toupper(string[0])-'A'])
			fcb_name.part.drive[0] = (char)(toupper(string[0])-'A'+1);
		else
			ret = 0xff;
		string += 2;
		}
	// Special checks for . and ..
	if (string[0] == '.')
		{
		string++;
		if (!string[0])
			{
			hasname = true;
			ret = PARSE_RET_NOWILD;
			strcpy(fcb_name.part.name, ".       ");
			goto savefcb;
			}
		if (string[1] == '.' && !string[1])
			{
			string++;
			hasname = true;
			ret = PARSE_RET_NOWILD;
			strcpy(fcb_name.part.name, "..      ");
			goto savefcb;
			}
		goto checkext;
		}
	// Copy the name
	hasname = true;
	finished = false;
	Bit8u fill = ' ';
	Bitu index = 0;
	while (index < 8)
		{
		if (!finished)
			{
			if (string[0] == '*')
				{
				fill = '?';
				fcb_name.part.name[index] = '?';
				if (!ret)
					ret = 1;
				finished = true;
				}
			else if (string[0] == '?')
				{
				fcb_name.part.name[index] = '?';
				if (!ret)
					ret = 1;
				}
			else if (isvalid(string[0]))
				fcb_name.part.name[index] = (char)(toupper(string[0]));
			else
				{
				finished = true;
				continue;
				}
			string++;
			}
		else
			fcb_name.part.name[index] = fill;
		index++;
		}
	if (!(string[0] == '.'))
		goto savefcb;
	string++;
checkext:
	// Copy the extension
	hasext = true;
	finished = false;
	fill = ' ';
	index = 0;
	while (index < 3)
		{
		if (!finished)
			{
			if (string[0] == '*')
				{
				fill = '?';
				fcb_name.part.ext[index] = '?';
				finished = true;
				}
			else if (string[0] == '?')
				{
				fcb_name.part.ext[index] = '?';
				if (!ret)
					ret = 1;
				}
			else if (isvalid(string[0]))
				fcb_name.part.ext[index] = (char)(toupper(string[0]));
			else
				{
				finished = true;
				continue;
				}
			string++;
			}
		else
			fcb_name.part.ext[index] = fill;
		index++;
		}
savefcb:
	if (!hasdrive & !(parser & PARSE_DFLT_DRIVE))
		fcb_name.part.drive[0] = 0;
	if (!hasname & !(parser & PARSE_BLNK_FNAME))
		strcpy(fcb_name.part.name, "        ");
	if (!hasext & !(parser & PARSE_BLNK_FEXT))
		strcpy(fcb_name.part.ext, "   ");
	fcb.SetName(fcb_name.part.drive[0], fcb_name.part.name, fcb_name.part.ext);
	*change = (Bit8u)(string-string_begin);
	return ret;
	}

static void DTAExtendName(char* const name, char* const filename, char* const ext)
	{
	strcpy(filename, "        ");
	strcpy(ext, "   ");
	char * pos = name;
	int i = 0;
	bool in_ext = false;
	for (char * pos = name; *pos; pos++)
		{
		if (!in_ext)
			if (*pos == '.')
				{
				in_ext = true;
				i = 0;
				}
			else if (i < 8)
				filename[i++] = *pos;
		else if (i < 3)
			ext[i++] = *pos;
		}
	}

static void SaveFindResult(DOS_FCB & find_fcb)
	{
	DOS_DTA find_dta(dos.tables.tempdta);
	char name[DOS_NAMELENGTH_ASCII];
	Bit32u size;
	Bit16u date;
	Bit16u time;
	Bit8u attr;
	Bit8u drive;
	char file_name[9];
	char ext[4];
	find_dta.GetResult(name, size, date, time, attr);
	drive = find_fcb.GetDrive()+1;
	// Create a correct file and extention
	DTAExtendName(name, file_name, ext);	
	DOS_FCB fcb(RealSeg(dos.dta()), RealOff(dos.dta()));	// TODO
	fcb.Create(find_fcb.Extended());
	fcb.SetName(drive, file_name, ext);
	fcb.SetAttr(attr);		// Only adds attribute if fcb is extended
	fcb.SetSizeDateTime(size, date, time);
	}

bool DOS_FCBCreate(Bit16u seg, Bit16u offset)
	{ 
	DOS_FCB fcb(seg, offset);
	char shortname[DOS_FCBNAME];
	Bit16u handle;
	fcb.GetName(shortname);
	FCBNameToStr(shortname);
	if (!DOS_CreateFile(shortname, DOS_ATTR_ARCHIVE, &handle))
		return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
	}

bool DOS_FCBOpen(Bit16u seg, Bit16u offset)
	{
	DOS_FCB fcb(seg, offset);
	char shortname[DOS_FCBNAME];
	Bit16u handle;

	fcb.GetName(shortname);
	FCBNameToStr(shortname);

	// First check if the name is correct
	Bit8u drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(shortname, fullname, &drive))
		return false;
	
	// Check, if file is already opened
	for (Bit8u i = 0; i < DOS_FILES; i++)
		{
		DOS_PSP psp(dos.psp());
		if (Files[i] && Files[i]->IsName(fullname))
			{
			handle = psp.FindEntryByHandle(i);
			if (handle == 0xFF)			// This shouldnt happen
				{
				LOG(LOG_FILES,LOG_ERROR)("DOS: File %s is opened but has no psp entry.", shortname);
				return false;
				}
			fcb.FileOpen((Bit8u)handle);
			return true;
			}
		}
	if (!DOS_OpenFile(shortname, OPEN_READWRITE, &handle))
		return false;
	fcb.FileOpen((Bit8u)handle);
	return true;
	}

bool DOS_FCBClose(Bit16u seg, Bit16u offset)
	{
	DOS_FCB fcb(seg, offset);
	if (!fcb.Valid())
		return false;
	Bit8u fhandle;
	fcb.FileClose(fhandle);			// Sets fhandle
	DOS_CloseFile(fhandle);
	return true;
	}

bool DOS_FCBFindFirst(Bit16u seg, Bit16u offset)
	{
	DOS_FCB fcb(seg, offset);
	RealPt old_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	char name[DOS_FCBNAME];
	fcb.GetName(name);
	FCBNameToStr(name);
	Bit8u attr = DOS_ATTR_ARCHIVE;
	fcb.GetAttr(attr);				// Gets search attributes if extended
	bool ret = DOS_FindFirst(name, attr, true);
	dos.dta(old_dta);
	if (ret)
		SaveFindResult(fcb);
	return ret;
	}

bool DOS_FCBFindNext(Bit16u seg, Bit16u offset)
	{
	DOS_FCB fcb(seg, offset);
	RealPt old_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	bool ret = DOS_FindNext();
	dos.dta(old_dta);
	if (ret)
		SaveFindResult(fcb);
	return ret;
	}

Bit8u DOS_FCBRead(Bit16u seg, Bit16u offset, Bit16u recno)
	{
	DOS_FCB fcb(seg, offset);
	Bit8u fhandle, cur_rec;
	Bit16u cur_block, rec_size;
	fcb.GetSeqData(fhandle, rec_size);
	fcb.GetRecord(cur_block, cur_rec);
	Bit32u pos = ((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle, &pos, DOS_SEEK_SET))
		return FCB_READ_NODATA; 
	Bit16u toread = rec_size;
	if (!DOS_ReadFile(fhandle, dos_copybuf, &toread) || toread == 0)
		return FCB_READ_NODATA;
	if (toread < rec_size)		// Zero pad copybuffer to rec_size
		memset(dos_copybuf+toread, 0, rec_size-toread);
	vPC_rBlockWrite(dWord2Ptr(dos.dta())+recno*rec_size, dos_copybuf, rec_size);
	if (++cur_rec > 127)
		{
		cur_block++;
		cur_rec = 0;
		}
	fcb.SetRecord(cur_block, cur_rec);
	if (toread == rec_size)
		return FCB_SUCCESS;
	return FCB_READ_PARTIAL;
	}

Bit8u DOS_FCBWrite(Bit16u seg, Bit16u offset, Bit16u recno)
	{
	DOS_FCB fcb(seg, offset);
	Bit8u fhandle, cur_rec;
	Bit16u cur_block, rec_size;
	fcb.GetSeqData(fhandle, rec_size);
	fcb.GetRecord(cur_block, cur_rec);
	Bit32u pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle, &pos, DOS_SEEK_SET))
		return FCB_ERR_WRITE; 
	vPC_rBlockRead(dWord2Ptr(dos.dta())+recno*rec_size, dos_copybuf, rec_size);
	Bit16u towrite = rec_size;
	if (!DOS_WriteFile(fhandle, dos_copybuf, &towrite))
		return FCB_ERR_WRITE;
	Bit32u size;
	Bit16u date, time;
	fcb.GetSizeDateTime(size, date, time);
	if (pos+towrite > size)
		size = pos+towrite;
	// time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year, dos.date.month, dos.date.day);
	Bit32u ticks = vPC_rLodsd(BIOS_TIMER);
	Bit32u seconds = (ticks*10)/182;
	Bit16u hour = (Bit16u)(seconds/3600);
	Bit16u min = (Bit16u)((seconds % 3600)/60);
	Bit16u sec = (Bit16u)(seconds % 60);
	time = DOS_PackTime(hour, min, sec);
	Bit8u temp = RealHandle(fhandle);
	Files[temp]->time = time;
	Files[temp]->date = date;
	fcb.SetSizeDateTime(size, date, time);
	if (++cur_rec > 127)
		{
		cur_block++;
		cur_rec = 0;
		}	
	fcb.SetRecord(cur_block, cur_rec);
	return FCB_SUCCESS;
	}

Bit8u DOS_FCBIncreaseSize(Bit16u seg, Bit16u offset)
	{
	DOS_FCB fcb(seg,offset);
	Bit8u fhandle, cur_rec;
	Bit16u cur_block, rec_size;
	fcb.GetSeqData(fhandle, rec_size);
	fcb.GetRecord(cur_block, cur_rec);
	Bit32u pos = ((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle, &pos, DOS_SEEK_SET))
		return FCB_ERR_WRITE; 
	Bit16u towrite = 0;
	if (!DOS_WriteFile(fhandle, dos_copybuf, &towrite))
		return FCB_ERR_WRITE;
	Bit32u size;
	Bit16u date, time;
	fcb.GetSizeDateTime(size, date, time);
	if (pos+towrite > size)
		size = pos+towrite;
	// time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year, dos.date.month, dos.date.day);
	Bit32u ticks = vPC_rLodsd(BIOS_TIMER);
	Bit32u seconds = (ticks*10)/182;
	Bit16u hour = (Bit16u)(seconds/3600);
	Bit16u min = (Bit16u)((seconds % 3600)/60);
	Bit16u sec = (Bit16u)(seconds % 60);
	time = DOS_PackTime(hour, min, sec);
	Bit8u temp = RealHandle(fhandle);
	Files[temp]->time = time;
	Files[temp]->date = date;
	fcb.SetSizeDateTime(size, date, time);
	fcb.SetRecord(cur_block, cur_rec);
	return FCB_SUCCESS;
	}

Bit8u DOS_FCBRandomRead(Bit16u seg, Bit16u offset, Bit16u numRec, bool restore)
	{
	/* if restore is true :random read else random blok read. 
	 * random read updates old block and old record to reflect the random data
	 * before the read!!!!!!!!! and the random data is not updated! (user must do this)
	 * Random block read updates these fields to reflect the state after the read!
	 */

	/* BUG: numRec should return the amount of records read! 
	 * Not implemented yet as I'm unsure how to count on error states (partial/failed) 
	 */

	DOS_FCB fcb(seg,offset);
	Bit32u random;
	Bit16u old_block = 0;
	Bit8u old_rec = 0;
	Bit8u error = 0;

	// Set the correct record from the random data
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128), (Bit8u)(random & 127));
	if (restore)
		fcb.GetRecord(old_block, old_rec);	//store this for after the read.
	// Read records
	for (int i = 0; i < numRec; i++)
		{
		error = DOS_FCBRead(seg, offset, (Bit16u)i);
		if (error != 0)
			break;
		}
	Bit16u new_block;
	Bit8u new_rec;
	fcb.GetRecord(new_block, new_rec);
	if (restore)
		fcb.SetRecord(old_block, old_rec);
	// Update the random record pointer with new position only when restore is false
	if (!restore)
		fcb.SetRandom(new_block*128+new_rec); 
	return error;
	}

Bit8u DOS_FCBRandomWrite(Bit16u seg, Bit16u offset, Bit16u numRec, bool restore)
	{
	// see FCB_RandomRead
	DOS_FCB fcb(seg,offset);
	Bit32u random;
	Bit16u old_block = 0;
	Bit8u old_rec = 0;
	Bit8u error = 0;

	// Set the correct record from the random data
	fcb.GetRandom(random);
	fcb.SetRecord((Bit16u)(random / 128), (Bit8u)(random & 127));
	if (restore)
		fcb.GetRecord(old_block, old_rec);
	if (numRec > 0)
		for (int i = 0; i < numRec; i++)		// Write records
			{
			error = DOS_FCBWrite(seg, offset, (Bit16u)i);	// dos_fcbwrite return 0 false when true...
			if (error != 0)
				break;
			}
	else
		DOS_FCBIncreaseSize(seg, offset);
	Bit16u new_block;
	Bit8u new_rec;
	fcb.GetRecord(new_block, new_rec);
	if (restore)
		fcb.SetRecord(old_block, old_rec);
	// Update the random record pointer with new position only when restore is false
	if (!restore)
		fcb.SetRandom(new_block*128+new_rec); 
	return error;
	}

bool DOS_FCBGetFileSize(Bit16u seg, Bit16u offset)
	{
	char shortname[DOS_PATHLENGTH];
	Bit16u entry;
	Bit8u handle;
	Bit16u rec_size;
	DOS_FCB fcb(seg, offset);
	fcb.GetName(shortname);
	FCBNameToStr(shortname);
	if (!DOS_OpenFile(shortname, OPEN_READ, &entry))
		return false;
	handle = RealHandle(entry);
	Bit32u size = 0;
	Files[handle]->Seek(&size, DOS_SEEK_END);
	DOS_CloseFile(entry);
	fcb.GetSeqData(handle, rec_size);
	Bit32u random = (size/rec_size);
	if (size % rec_size)
		random++;
	fcb.SetRandom(random);
	return true;
	}

bool DOS_FCBDeleteFile(Bit16u seg, Bit16u offset)
	{
	/* FCB DELETE honours wildcards. it will return true if one or more
	 * files get deleted. 
	 * To get this: the dta is set to temporary dta in which found files are
	 * stored. This can not be the tempdta as that one is used by fcbfindfirst
	 */
	RealPt old_dta = dos.dta();
	dos.dta(dos.tables.tempdta_fcbdelete);
	DOS_FCB fcb(RealSeg(dos.dta()), RealOff(dos.dta()));
	bool return_value = false;
	bool nextfile = DOS_FCBFindFirst(seg, offset);
	while (nextfile)
		{
		char shortname[DOS_FCBNAME] = { 0 };
		fcb.GetName(shortname);
		FCBNameToStr(shortname);
		bool res = DOS_UnlinkFile(shortname);
		if (!return_value && res)
			return_value = true;	// at least one file deleted
		nextfile = DOS_FCBFindNext(seg, offset);
		}
	dos.dta(old_dta);	// Restore dta
	return return_value;
}

bool DOS_FCBRenameFile(Bit16u seg, Bit16u offset)
	{
	DOS_FCB fcbold(seg, offset);
	DOS_FCB fcbnew(seg, offset+16);
	if (!fcbold.Valid())
		return false;
	char oldname[DOS_FCBNAME];
	char newname[DOS_FCBNAME];
	fcbold.GetName(oldname);
	FCBNameToStr(oldname);
	fcbnew.GetName(newname);
	FCBNameToStr(newname);
	// Rename the file
	return DOS_Rename(oldname, newname);
	}

void DOS_FCBSetRandomRecord(Bit16u seg, Bit16u offset)
	{
	DOS_FCB fcb(seg, offset);
	Bit16u block;
	Bit8u rec;
	fcb.GetRecord(block, rec);
	fcb.SetRandom(block*128+rec);
	}

bool DOS_FileExists(char const* const name)
	{
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;
	return Drives[drive]->FileExists(fullname);
	}

bool DOS_GetAllocationInfo(Bit8u drive, Bit16u* _bytes_sector, Bit8u* _sectors_cluster, Bit16u* _total_clusters)
	{
	if (!drive)
		drive =  DOS_GetDefaultDrive();
	else
		drive--;
	if (drive >= DOS_DRIVES || !Drives[drive])
		return false;
	Bit16u _free_clusters;
	Drives[drive]->AllocationInfo(_bytes_sector, _sectors_cluster, _total_clusters, &_free_clusters);
	SegSet16(ds,RealSeg(dos.tables.mediaid));
	reg_bx = RealOff(dos.tables.mediaid+drive*2);
	return true;
	}

bool DOS_DriveIsMounted(Bit8u drive)
	{
	return Drives[drive] ? true : false;
	}

bool DOS_GetFileDate(Bit16u entry, Bit16u* otime, Bit16u* odate)
	{
	Bit32u handle = RealHandle(entry);
	if (handle >= DOS_FILES || !Files[handle])
		{
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
		}
	Files[handle]->UpdateDateTimeFromHost();
	*otime = Files[handle]->time;
	*odate = Files[handle]->date;
	return true;
	}

void DOS_SetupFiles(void)
	{
	// Setup the File Handles
	Files = new DOS_File* [DOS_FILES];
	for (Bit32u i = 0; i < DOS_FILES; i++)
		Files[i] = 0;
	// Setup the Virtual Disk System
	for (Bit32u i = 0; i < DOS_DRIVES-1; i++)
		Drives[i] = 0;
	Drives[DOS_DRIVES-1] = new Virtual_Drive();
	}
