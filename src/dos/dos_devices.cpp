#include <string.h>
#include "vDos.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"
#include "parport.h"
#include "drives.h"			//Wildcmp
// Include all the devices

#include "dev_con.h"

DOS_Device * Devices[DOS_DEVICES];

class device_NUL : public DOS_Device
	{
public:
	device_NUL()
		{
		SetName("NUL");
		};
	bool Read(Bit8u * data, Bit16u * size)
		{
		for (Bitu i = 0; i < *size; i++) 
			data[i] = 0; 
		return true;
		}
	bool Write(Bit8u * data, Bit16u * size)
		{
		return true;
		}
	void Close()
		{
		}
	virtual Bit16u GetInformation(void)
		{
		return 0x8084;
		}
	};

bool DOS_Device::Read(Bit8u * data, Bit16u * size)
	{
	return Devices[devnum]->Read(data, size);
	}

bool DOS_Device::Write(Bit8u * data, Bit16u * size)
	{
	return Devices[devnum]->Write(data, size);
	}

void DOS_Device::Close()
	{
	Devices[devnum]->Close();
	}

Bit16u DOS_Device::GetInformation(void)
	{ 
	return Devices[devnum]->GetInformation();
	}

DOS_File::DOS_File(const DOS_File& orig)
	{
	flags = orig.flags;
	time = orig.time;
	date = orig.date;
	attr = orig.attr;
	refCtr = orig.refCtr;
	name = 0;
	if (orig.name)
		{
		name = new char [strlen(orig.name) + 1];
		strcpy(name, orig.name);
		}
	}

DOS_File & DOS_File::operator= (const DOS_File & orig)
	{
	flags = orig.flags;
	time = orig.time;
	date = orig.date;
	attr = orig.attr;
	refCtr = orig.refCtr;
	if (name)
		{
		delete [] name;
		name = 0;
		}
	if (orig.name)
		{
		name = new char [strlen(orig.name) + 1];
		strcpy(name, orig.name);
		}
	return *this;
	}

Bit8u DOS_FindDevice(char const * name)
	{
	// should only check for the names before the dot and spacepadded
	char fullname[DOS_PATHLENGTH];
	Bit8u drive;
//	if(!name || !(*name)) return DOS_DEVICES; //important, but makename does it
	if (!DOS_MakeName(name, fullname, &drive))
		return DOS_DEVICES;

	char* name_part = strrchr(fullname,'\\');
	if (name_part)
		{
		*name_part++ = 0;
		// Check validity of leading directory.
		if (!Drives[drive]->TestDir(fullname))
			return DOS_DEVICES;
		}
	else
		name_part = fullname;
   
	char* dot = strrchr(name_part, '.');
	if (dot)
		*dot = 0;	// no ext checking

	static char com[] = "COM1";
	static char lpt[] = "LPT1";
	// AUX is alias for COM1 and PRN for LPT1
	// A bit of a hack. (but less then before).
	if (!stricmp(name_part, "AUX"))
		name_part = com;
	else if (!stricmp(name_part, "PRN"))
		name_part = lpt;

	// loop through devices
	for (Bit8u index = 0; index < DOS_DEVICES; index++)
		if (Devices[index])
			if (WildFileCmp(name_part, Devices[index]->name))
				return index;
	return DOS_DEVICES;
	}

void DOS_AddDevice(DOS_Device * adddev)
	{
	// Caller creates the device. We store a pointer to it
	// TODO Give the Device a real handler in low memory that responds to calls
	for (Bitu i = 0; i < DOS_DEVICES; i++)
		if (!Devices[i]) 
			{
			Devices[i] = adddev;
			Devices[i]->SetDeviceNumber(i);
			return;
			}
	E_Exit("DOS: Too many devices");
	}

void DOS_DelDevice(DOS_Device * dev)
	{
	// We will destroy the device if we find it in our list.
	// TODO: The file table is not checked to see the device is opened somewhere!
	for (Bitu i = 0; i < DOS_DEVICES; i++)
		if (Devices[i] && !stricmp(Devices[i]->name, dev->name))
			{
			delete Devices[i];
			Devices[i] = 0;
			return;
			}
	}

void DOS_SetupDevices(void)
	{
	DOS_AddDevice(new device_CON());
	DOS_AddDevice(new device_NUL());
//	DOS_AddDevice(new device_PRN());
	}
