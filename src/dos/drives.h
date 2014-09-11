#ifndef _DRIVES_H__
#define _DRIVES_H__

#include <vector>
#include <sys/types.h>
#include "dos_system.h"
#include "shell.h" /* for DOS_Shell */

bool WildFileCmp(const char * file, const char * wild);

class localDrive : public DOS_Drive
	{
public:
	localDrive(const char * startdir, Bit16u _bytes_sector, Bit8u _sectors_cluster, Bit16u _total_clusters, Bit16u _free_clusters, Bit8u _mediaid);
	virtual bool FileOpen(DOS_File* * file, char* name, Bit32u flags);
	virtual bool FileCreate(DOS_File* * file, char* name, Bit16u attributes);
	virtual bool FileUnlink(char* name);
	virtual bool RemoveDir(char* dir);
	virtual bool MakeDir(char* dir);
	virtual bool TestDir(char* dir);
	virtual bool FindFirst(char* _dir, DOS_DTA & dta, bool fcb_findfirst = false);
	virtual bool FindNext(DOS_DTA & dta);
	virtual bool GetFileAttr(char* name, Bit16u * attr);
	virtual bool Rename(char* oldname, char* newname);
	virtual bool AllocationInfo(Bit16u* _bytes_sector, Bit8u* _sectors_cluster, Bit16u* _total_clusters, Bit16u* _free_clusters);
	virtual bool FileExists(const char* name);
	virtual Bit8u GetMediaByte(void);
private:
	char basedir[MAX_PATH_LEN];
	char srch_dir[MAX_PATH_LEN];

	struct {
		Bit16u bytes_sector;
		Bit8u sectors_cluster;
		Bit16u total_clusters;
		Bit16u free_clusters;
		Bit8u mediaid;
	} allocation;
	};

struct VFILE_Block;

class Virtual_Drive: public DOS_Drive
	{
public:
	Virtual_Drive();
	bool FileOpen(DOS_File * * file, char * name, Bit32u flags);
	bool FileCreate(DOS_File * * file, char * name, Bit16u attributes);
	bool FileUnlink(char * name);
	bool RemoveDir(char * dir);
	bool MakeDir(char * dir);
	bool TestDir(char * dir);
	bool FindFirst(char * _dir, DOS_DTA & dta, bool fcb_findfirst);
	bool FindNext(DOS_DTA & dta);
	bool GetFileAttr(char * name, Bit16u * attr);
	bool Rename(char * oldname, char * newname);
	bool AllocationInfo(Bit16u * _bytes_sector, Bit8u * _sectors_cluster, Bit16u * _total_clusters, Bit16u * _free_clusters);
	bool FileExists(const char* name);
	Bit8u GetMediaByte(void);
private:
	VFILE_Block * search_file;
	};

#endif
