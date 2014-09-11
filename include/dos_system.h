#ifndef vDOS_DOS_SYSTEM_H
#define vDOS_DOS_SYSTEM_H

#include <vector>
#include "vDos.h"
#include "support.h"
#include "mem.h"

#define DOS_NAMELENGTH 12
#define DOS_NAMELENGTH_ASCII (DOS_NAMELENGTH+1)
#define DOS_FCBNAME 15
#define DOS_PATHLENGTH 80

enum {
	DOS_ATTR_READ_ONLY=	0x01,
	DOS_ATTR_HIDDEN=	0x02,
	DOS_ATTR_SYSTEM=	0x04,
	DOS_ATTR_VOLUME=	0x08,
	DOS_ATTR_DIRECTORY=	0x10,
	DOS_ATTR_ARCHIVE=	0x20,
	DOS_ATTR_DEVICE=	0x40
};

class DOS_DTA;

class DOS_File
	{
public:
	DOS_File():flags(0)	{ name = 0; refCtr = 0; hdrive = 0xff; };
	DOS_File(const DOS_File& orig);
	DOS_File & operator= (const DOS_File & orig);
	virtual	~DOS_File()	{ if (name) delete [] name; };
	virtual bool	Read(Bit8u * data, Bit16u * size) = 0;
	virtual bool	Write(Bit8u * data, Bit16u * size) = 0;
	virtual bool	Seek(Bit32u * pos, Bit32u type) = 0;
	virtual void	Close() {};
	virtual bool    LockFile(Bit8u mode, Bit32u pos, Bit32u size) { return false; };
	virtual Bit16u	GetInformation(void) = 0;
	virtual void	SetName(const char* _name)	{ if (name) delete[] name; name = new char[strlen(_name)+1]; strcpy(name, _name); }
	virtual char*	GetName(void)				{ return name; };
	virtual bool	IsName(const char* _name)	{ if (!name) return false; return stricmp(name, _name) == 0; };
	virtual void	AddRef()					{ refCtr++; };
	virtual Bits	RemoveRef()					{ return --refCtr; };
	virtual void	UpdateDateTimeFromHost()	{}
	void SetDrive(Bit8u drv) { hdrive = drv;}
	Bit8u GetDrive(void) { return hdrive;}
	Bit32u flags;
	Bit16u time;
	Bit16u date;
	Bit16u attr;
	Bits refCtr;
	char* name;
private:
	Bit8u hdrive;
	};

class DOS_Device : public DOS_File
	{
public:
	DOS_Device(const DOS_Device& orig):DOS_File(orig)
		{
		devnum = orig.devnum;
		}
	DOS_Device & operator= (const DOS_Device & orig)
		{
		DOS_File::operator = (orig);
		devnum = orig.devnum;
		return *this;
		}
	DOS_Device():DOS_File(), devnum(0){};
	virtual bool	Read(Bit8u * data,Bit16u * size);
	virtual bool	Write(Bit8u * data,Bit16u * size);
	bool	Seek(Bit32u * pos, Bit32u type)
		{
		*pos = 0;
		return true;
		}
	virtual void	Close();
	virtual Bit16u	GetInformation(void);
	void SetDeviceNumber(Bitu num)
		{
		devnum = num;
		}
	Bitu GetDeviceNumber(void)
		{
		return devnum;
		}
	Bit64u timeOutAt;
	bool ffWasLast;
private:
	Bitu devnum;
	};

class DOS_Drive
	{
public:
	DOS_Drive();
	virtual ~DOS_Drive()	{ };
	virtual bool FileOpen(DOS_File * * file, char * name, Bit32u flags) = 0;
	virtual bool FileCreate(DOS_File * * file, char * name, Bit16u attributes) = 0;
	virtual bool FileUnlink(char * _name) = 0;
	virtual bool RemoveDir(char * _dir) = 0;
	virtual bool MakeDir(char * _dir) = 0;
	virtual bool TestDir(char * _dir) = 0;
	virtual bool FindFirst(char * _dir, DOS_DTA & dta, bool fcb_findfirst = false) = 0;
	virtual bool FindNext(DOS_DTA & dta) = 0;
	virtual bool GetFileAttr(char * name, Bit16u * attr) = 0;
	virtual bool Rename(char * oldname, char * newname) = 0;
	virtual bool AllocationInfo(Bit16u * _bytes_sector, Bit8u * _sectors_cluster, Bit16u * _total_clusters, Bit16u * _free_clusters) = 0;
	virtual bool FileExists(const char* name) = 0;
	virtual Bit8u GetMediaByte(void) = 0;

	void		SetLabel(const char* name);
	char*		GetLabel(void) { return label; };
	Bit32u		VolSerial;
	char *		GetInfo(void);
	char		curdir[DOS_PATHLENGTH];
	char		info[256];
private:
	char		label[14];
	};

enum {OPEN_READ = 0, OPEN_WRITE = 1, OPEN_READWRITE = 2, DOS_NOT_INHERIT = 128};
enum {DOS_SEEK_SET = 0, DOS_SEEK_CUR = 1, DOS_SEEK_END = 2};


/*
 A multiplex handler should read the registers to check what function is being called
 If the handler returns false dos will stop checking other handlers
*/

typedef bool (MultiplexHandler)(void);

// AddDevice stores the pointer to a created device
void DOS_AddDevice(DOS_Device * adddev);

void VFILE_Register(const char * name, Bit8u * data, Bit16u size);
#endif
