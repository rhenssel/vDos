#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "vDos.h"
#include "dos_inc.h"
#include "drives.h"
#include "support.h"
#include "inout.h"
#include <sys/stat.h>
#include <sys/locking.h>
#include <direct.h>
#include <fcntl.h>

class localFile : public DOS_File
	{
public:
	localFile(const char* name, int fd);
	bool Read(Bit8u * data, Bit16u * size);
	bool Write(Bit8u * data, Bit16u * size);
	bool Seek(Bit32u * pos, Bit32u type);
	void Close();
	bool LockFile(Bit8u mode, Bit32u pos, Bit32u size);
	Bit16u GetInformation(void);
	void UpdateDateTimeFromHost(void);   
private:
	int	fd;
	};

bool localDrive::FileCreate(DOS_File * * file, char * name, Bit16u attr/*attributes*/)
	{
	// TODO Maybe care for attributes but not likely
	char win_name[MAX_PATH_LEN];

	int fd;
	if(_sopen_s(&fd, strcat(strcpy(win_name, basedir), name), _O_CREAT | _O_TRUNC | _O_BINARY | _O_RDWR, _SH_DENYNO, _S_IREAD | _S_IWRITE))
		{
		LOG_MSG("File creation failed: %s\nErrorno: %d", win_name, errno);
		return false;
		}
	// Make the 16 bit device information
	*file = new localFile(name, fd);
	(*file)->flags = OPEN_READWRITE;
	return true;
	}

bool localDrive::FileOpen(DOS_File * * file, char * name, Bit32u flags)
	{
	int oflag, shflag;

	switch (flags&0xf)
		{
	case OPEN_READ:
		oflag = _O_RDONLY;
		break;
	case OPEN_WRITE:
		oflag = _O_RDWR;
		break;
	case OPEN_READWRITE:
		oflag = _O_RDWR;
		break;
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
		}
	switch (flags&0x70)
		{
	case 0x10:
		shflag = _SH_DENYRW;
		break;
	case 0x20:
		shflag = _SH_DENYWR;
		break;
	case 0x30:
		shflag = _SH_DENYRD;
		break;
	default:
		shflag = _SH_DENYNO;
		}
	char win_name[MAX_PATH_LEN];
	int fd;
	if(_sopen_s(&fd, strcat(strcpy(win_name, basedir), name), oflag | _O_BINARY, shflag, _S_IREAD | _S_IWRITE))
		return false;
	*file = new localFile(name, fd);
	(*file)->flags = flags;											// For the inheritance flag and maybe check for others.
	return true;
	}

bool localDrive::FileUnlink(char * name)
	{
	char win_name[MAX_PATH_LEN];
//	return unlink(strcat(strcpy(win_name, basedir), name)) == 0;
	return remove(strcat(strcpy(win_name, basedir), name)) == 0;
	}

bool localDrive::FindFirst(char* _dir, DOS_DTA & dta, bool fcb_findfirst)
	{
	if (!TestDir(_dir))
		{
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
		}
	strcat(strcpy(srch_dir, basedir), _dir);						// Should be on a per program base, but only used shortly so for now OK
	if (srch_dir[strlen(srch_dir)-1] != '\\')
		strcat(srch_dir, "\\");
	
	Bit8u sAttr;
	char srch_pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(sAttr, srch_pattern);
	if (!strcmp(srch_pattern, "        .   "))						// Special complete empty, what about abc. ? (abc.*?)
		strcat(srch_dir, "*.*");
	else
		{
		int j = strlen(srch_dir);
 		for (unsigned int i = 0; i <= strlen(srch_pattern); i++)	// Pattern had 8.3 format with embedded spaces, for now simply remove them ( "abc d   .a b"?)
			if (srch_pattern[i] != ' ')
				srch_dir[j++] = srch_pattern[i];
		}
	// Windows "finds" LPT1-9/COM1-9 which are never returned in a (DOS)DIR
	if (((!strnicmp(srch_pattern, "LPT", 3) || !strnicmp(srch_pattern, "COM", 3)) && (srch_pattern[3] > '0' && srch_pattern[3] <= '9' && srch_pattern[4] ==' '))
		|| ((!strnicmp(srch_pattern, "PRN", 3) || !strnicmp(srch_pattern, "AUX", 3)) && srch_pattern[3] ==' '))
		{
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
		}

	if (Bit32u dirID = dta.GetDirID())								// Close eventually open Find by this program
		{
//		FindClose((HANDLE)dirID);									// Jos: I would like to do this (saving resouces?)
		dta.SetDirID(0);											// But at least WP sets the DTA and copies the contents between searches (for recusive searches/subdirs?)
		}															// So the DirID gets an old value. localDrive::FindNext() uses this for FindNextFile() and vDos is terminated by Wijdwos!!!
	
	if (sAttr == DOS_ATTR_VOLUME)
		{
		if (strcmp(GetLabel(), "") == 0)
			{
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
			}
		dta.SetResult(GetLabel(), 0, 0, 0, DOS_ATTR_VOLUME);
		return true;
		}
	else if ((sAttr & DOS_ATTR_VOLUME)  && (*_dir == 0) && !fcb_findfirst)
		{ 
		// should check for a valid leading directory instead of 0
		// exists==true if the volume label matches the searchmask and the path is valid
		if (WildFileCmp(GetLabel(), srch_pattern))
			{
			dta.SetResult(GetLabel(), 0, 0, 0, DOS_ATTR_VOLUME);
			return true;
			}
		}
	return FindNext(dta);
	}

bool isDosName(char* fName)											// Check for valid DOS filename, 8.3 and no forbidden characters
	{
	if (!strcmp(fName, ".") || !strcmp(fName, ".."))				// "." and ".." specials
		return true;

	if (strpbrk(fName, "+[] "))
		return false;

	char* pos = strchr(fName, '.');
	if (pos)														// If extension
		{
		if (strlen(pos) > 4 || strchr(pos+1, '.') || pos - fName > 8)	// Max 3 chars,  max 1 "extension", max name = 8 chars
			return false;
		}
	else if (strlen(fName) > 8)										// Max name = 8 chars
		return false;
	return true;
	}

bool localDrive::FindNext(DOS_DTA & dta)
	{
	Bit8u srch_attr;
	char srch_pattern[DOS_NAMELENGTH_ASCII];

	dta.GetSearchParams(srch_attr, srch_pattern);
	srch_attr ^= (DOS_ATTR_VOLUME);
	WIN32_FIND_DATA search_data;
	Bit8u find_attr;
	if (!dta.GetDirID())											// Really a FindFirst search w/o label
		{
		HANDLE sHandle = FindFirstFile(srch_dir, &search_data);
		if (sHandle == INVALID_HANDLE_VALUE)						// Shouldn't happen
			{
			Bit16u errorno = (Bit16u)GetLastError();
			if (errorno == ERROR_FILE_NOT_FOUND)
				DOS_SetError(DOSERR_NO_MORE_FILES);					// DOS returns this
			else
				DOS_SetError(errorno);
			return false;
			}
		dta.SetDirID((Bit32u)sHandle);
		}
	else if (!FindNextFile((HANDLE)dta.GetDirID(), &search_data))
		{
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
		}
	do
		{
		if (!isDosName(search_data.cFileName))
			continue;
//		if (!(search_data.dwFileAttributes & srch_attr))
//			continue;
		if (search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			find_attr = DOS_ATTR_DIRECTORY;
		else
			find_attr = DOS_ATTR_ARCHIVE;
 		if (~srch_attr & find_attr & (DOS_ATTR_DIRECTORY | DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM))
			continue;
		// file is okay, setup everything to be copied in DTA Block
		char find_name[DOS_NAMELENGTH_ASCII];
		Bit16u find_date,find_time;
		Bit32u find_size;
		strcpy(find_name, search_data.cFileName);
		upcase(find_name);
		find_size = search_data.nFileSizeLow;
		FILETIME fTime;
		FileTimeToLocalFileTime(&search_data.ftLastWriteTime, &fTime);
		FileTimeToDosDateTime(&fTime, &find_date, &find_time);

//		FileTimeToDosDateTime(&search_data.ftLastWriteTime, &find_date, &find_time);
//		FileTimeToSystemTime(&search_data.ftLastWriteTime, &sTime);
//		find_date = DOS_PackDate((Bit16u)(sTime.wYear), (Bit16u)(sTime.wMonth), (Bit16u)sTime.wDay);
//		find_time = DOS_PackTime((Bit16u)sTime.wHour, (Bit16u)sTime.wMinute, (Bit16u)sTime.wSecond);
		dta.SetResult(find_name, find_size, find_date, find_time, find_attr);
		return true;
		}
	while (FindNextFile((HANDLE)dta.GetDirID(), &search_data));
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
	}

bool localDrive::GetFileAttr(char* name, Bit16u* attr)
	{
	char win_name[MAX_PATH_LEN];
	struct stat status;
	if (stat(strcat(strcpy(win_name, basedir), name), &status) == 0)
		{
		*attr = DOS_ATTR_ARCHIVE;
		if (status.st_mode & S_IFDIR)
			*attr |= DOS_ATTR_DIRECTORY;
		return true;
		}
	*attr = 0;
	return false; 
	}

bool localDrive::MakeDir(char * dir)
	{
	char win_dir[MAX_PATH_LEN];
	return (_mkdir(strcat(strcpy(win_dir, basedir), dir)) == 0);
	}

bool localDrive::RemoveDir(char * dir)
	{
	char win_dir[MAX_PATH_LEN];
	return (_rmdir(strcat(strcpy(win_dir, basedir), dir)) == 0);
	}

bool localDrive::TestDir(char* dir)
	{
	char win_dir[MAX_PATH_LEN];
	strcat(strcpy(win_dir, basedir), dir);
	size_t len = strlen(win_dir);
	if (len && (win_dir[len-1] != '\\'))							// Skip directory test, if "\"
		{
		// It has to be a directory !
		struct stat test;
		if (stat(win_dir, &test) || (test.st_mode & S_IFDIR) == 0)
			return false;
		}
	return (access(win_dir, 0) == 0);
	}

bool localDrive::Rename(char* oldname, char* newname)
	{
	char winold[MAX_PATH_LEN];
	char winnew[MAX_PATH_LEN];
	return (rename(strcat(strcpy(winold, basedir), oldname), strcat(strcpy(winnew, basedir), newname)) == 0);
	}

bool localDrive::AllocationInfo(Bit16u * _bytes_sector, Bit8u * _sectors_cluster, Bit16u * _total_clusters, Bit16u * _free_clusters)
	{
	*_bytes_sector = allocation.bytes_sector;
	*_sectors_cluster = allocation.sectors_cluster;
	*_total_clusters = allocation.total_clusters;
	*_free_clusters = allocation.free_clusters;
	return true;
	}

bool localDrive::FileExists(const char* name)
	{
	char win_name[MAX_PATH_LEN];
	struct stat temp_stat;
	return (stat(strcat(strcpy(win_name, basedir), name), &temp_stat) == 0) && !(temp_stat.st_mode & S_IFDIR);
	}

Bit8u localDrive::GetMediaByte(void)
	{
	return allocation.mediaid;
	}

localDrive::localDrive(const char* startdir, Bit16u _bytes_sector, Bit8u _sectors_cluster, Bit16u _total_clusters, Bit16u _free_clusters, Bit8u _mediaid)
	{
	strcpy(basedir, startdir);
	strcpy(info, startdir);
	allocation.bytes_sector = _bytes_sector;
	allocation.sectors_cluster = _sectors_cluster;
	allocation.total_clusters = _total_clusters;
	allocation.free_clusters = _free_clusters;
	allocation.mediaid = _mediaid;
	while (Bit8u c = *startdir++)
		VolSerial = c + (VolSerial << 6) + (VolSerial << 16) - VolSerial;
	}

bool localFile::Read(Bit8u* data, Bit16u* size)
	{
	if ((this->flags & 0xf) == OPEN_WRITE)							// Check if file opened in write-only mode
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
		}
	int bytesread = (Bit16u)_read(fd, data, *size);
	if (bytesread == -1)
		{
		DOS_SetError((Bit16u)_doserrno);
		*size = 0;													// Is this OK ??
		return false;
		}
	*size = bytesread;
	return true;
	}

bool localFile::Write(Bit8u* data, Bit16u* size)
	{
	if ((this->flags & 0xf) == OPEN_READ)							// Check if file opened in read-only mode
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
		}
	if (*size == 0)
        return (!_chsize(fd, _tell(fd)));

//	if (*size == 0)													// TODO ???
//        return (!ftruncate(fileno(fhandle), ftell(fhandle)));
	int byteswritten = (Bit16u)_write(fd, data, *size);
	if (byteswritten == -1)
		{
		DOS_SetError((Bit16u)_doserrno);
		*size = 0;													// Is this OK ??
		return false;
		}
	*size = byteswritten;
	return true;
	}

bool localFile::LockFile(Bit8u mode, Bit32u pos, Bit32u size)
	{
	HANDLE hFile = (HANDLE)_get_osfhandle(fd);
	BOOL bRet;

	switch (mode)
		{
	case 0: bRet = ::LockFile (hFile, pos, 0, size, 0); break;
	case 1: bRet = ::UnlockFile(hFile, pos, 0, size, 0); break;
	default: 
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
		}

	if (!bRet)
		{
		switch (GetLastError())
			{
		case ERROR_ACCESS_DENIED:
		case ERROR_LOCK_VIOLATION:
		case ERROR_NETWORK_ACCESS_DENIED:
		case ERROR_DRIVE_LOCKED:
		case ERROR_SEEK_ON_DEVICE:
		case ERROR_NOT_LOCKED:
		case ERROR_LOCK_FAILED:
			DOS_SetError(0x21);
			break;
		case ERROR_INVALID_HANDLE:
			DOS_SetError(DOSERR_INVALID_HANDLE);
			break;
		case ERROR_INVALID_FUNCTION:
		default:
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			break;
			}
		return false;
		}
	return true;
	}

bool localFile::Seek(Bit32u* pos, Bit32u type)
	{
	if (type > 2)
		{
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
		}
	int newpos = _lseek(fd, *pos, type);
	if (newpos != -1)
		{
		*pos = newpos;
		return true;
		}
	*pos = _tell(fd);
	return false;
	}

void localFile::Close()
	{
	if (refCtr == 1)												// Only close if one reference left
		_close(fd);
	}

Bit16u localFile::GetInformation(void)
	{
	return 0;
	}

localFile::localFile(const char* _name, int _fd)
	{
	fd = _fd;
	UpdateDateTimeFromHost();

	attr = DOS_ATTR_ARCHIVE;

	name = 0;
	SetName(_name);
	}

void localFile::UpdateDateTimeFromHost(void)
	{
	struct _stat temp_stat;
	_fstat(fd, &temp_stat);
	struct tm * ltime;

	if ((ltime = localtime(&temp_stat.st_mtime)) != 0)
		{
		time = DOS_PackTime((Bit16u)ltime->tm_hour, (Bit16u)ltime->tm_min, (Bit16u)ltime->tm_sec);
		date = DOS_PackDate((Bit16u)(ltime->tm_year+1900), (Bit16u)(ltime->tm_mon+1), (Bit16u)ltime->tm_mday);
		}
	else
		{
		time = 1;
		date = 1;
		}
	}
