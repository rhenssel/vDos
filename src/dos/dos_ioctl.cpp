#include <string.h>
#include "vDos.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"

bool DOS_IOCTL(void)
	{
	Bitu handle = 0;
	Bit8u drive = 0;
	// calls 0-4,6,7,10,12,16 use a file handle
	if ((reg_al < 4) || (reg_al == 0x06) || (reg_al == 0x07) || (reg_al == 0x0a) || (reg_al == 0x0c) || (reg_al == 0x10))
		{
		handle = RealHandle(reg_bx);
		if (handle >= DOS_FILES || !Files[handle])
			{
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
			}
		}
	else if (reg_al < 0x12)
		{ 				// those use a diskdrive except 0x0b
		if (reg_al != 0x0b)
			{
			drive = reg_bl;
			if (!drive)
				drive = DOS_GetDefaultDrive();
			else
				drive--;
			if (!((drive < DOS_DRIVES) && Drives[drive]))
				{
				DOS_SetError(DOSERR_INVALID_DRIVE);
				return false;
				}
			}
		}
	else
		{
		LOG(LOG_DOSMISC, LOG_ERROR)("DOS:IOCTL Call %2X unhandled", reg_al);
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
		}
	switch (reg_al)
		{
	case 0:		// Get Device Information
		if (Files[handle]->GetInformation() & 0x8000)	// Check for device
			reg_dx = Files[handle]->GetInformation();
		else
			{
			Bit8u hdrive = Files[handle]->GetDrive();
			if (hdrive == 0xff)
				hdrive = 2;		// defaulting to C:
			// return drive number in lower 5 bits for block devices
			reg_dx = (Files[handle]->GetInformation()&0xffe0)|hdrive;
			}
		reg_ax = reg_dx;	// Destroyed officially
		return true;
	case 1:		// Set Device Information
		if (reg_dh != 0)
			{
			DOS_SetError(DOSERR_DATA_INVALID);
			return false;
			}
		if (Files[handle]->GetInformation() & 0x8000)	// Check for device
			reg_al = (Bit8u)(Files[handle]->GetInformation() & 0xff);
		else
			{
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
			}
		return true;
	case 2:		// Read from Device Control Channel
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	case 3:		// Write to Device Control Channel
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	case 6:      // Get Input Status
		if (Files[handle]->GetInformation() & 0x8000)		// Check for device
			reg_al = (Files[handle]->GetInformation() & 0x40) ? 0 : 0xff;
		else
			{	// FILE
			Bit32u oldlocation = 0;
			Files[handle]->Seek(&oldlocation, DOS_SEEK_CUR);
			Bit32u endlocation = 0;
			Files[handle]->Seek(&endlocation, DOS_SEEK_END);
			if (oldlocation < endlocation)
				reg_al = 0xff;		// Still data available
			else
				reg_al = 0;			//EOF or beyond
			Files[handle]->Seek(&oldlocation, DOS_SEEK_SET);	// restore filelocation
			}
		return true;
	case 7:		// Get Output Status
		reg_al = 0xff;
		return true;
	case 8:		// Check if block device removable
		reg_ax = 1;
		return true;
	case 9:		// Check if block device remote
		reg_dx = 0x0802;	// Open/Close supported; 32bit access supported (any use? fixes Fable installer)
		// undocumented bits from device attribute word
		// TODO Set bit 9 on drives that don't support direct I/O
		reg_ax = 0x300;
		return true;
	case 0x0A:		// Is Device or Handle Remote?
		reg_dx = 0;			// No!
		return true;
	case 0x0B:		// Set sharing retry count
		return true;
	case 0x0D:		// Generic block device request
		{
		PhysPt ptr = SegPhys(ds)+reg_dx;
		switch (reg_cl)
			{
		case 0x60:		// Get Device parameters
			vPC_rStosb(ptr, 3);								// special function
			vPC_rStosb(ptr+1, (drive>=2) ? 0x05 : 0x14);	// fixed disc(5), 1.44 floppy(14)
			vPC_rStosw(ptr+2, drive >= 2);					// nonremovable ?
			vPC_rStosw(ptr+4, 0);							// num of cylinders
			vPC_rStosb(ptr+6, 0);							// media type (00=other type)
			// drive parameter block following
			vPC_rStosb(ptr+7, drive);						// drive
			vPC_rStosb(ptr+8, 0);							// unit number
			vPC_rStosd(ptr+0x1f, 0xffffffff);				// next parameter block
			break;
		case 0x46:
		case 0x66:	// Volume label
			{			
			char const* bufin = Drives[drive]->GetLabel();
			char buffer[11] ={' '};

			char const* find_ext = strchr(bufin,'.');
			if (find_ext)
				{
				Bitu size = (Bitu)(find_ext-bufin);
				if (size > 8)
					size = 8;
				memcpy(buffer, bufin,size);
				find_ext++;
				memcpy(buffer+size, find_ext, (strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
				}
			else
				memcpy(buffer,bufin, (strlen(bufin) > 8) ? 8 : strlen(bufin));
			
			char buf2[8]={ 'F','A','T','1','6',' ',' ',' '};
			if (drive < 2)
				buf2[4] = '2';						// FAT12 for floppies
			vPC_rStosw(ptr+0, 0);					// 0
			vPC_rStosd(ptr+2, 0x1234);				// Serial number
			vPC_rBlockWrite(ptr+6, buffer,11);		// volumename
			if (reg_cl == 0x66)
				vPC_rBlockWrite(ptr+0x11, buf2, 8);	// filesystem
			}
			break;
		default	:	
			LOG(LOG_IOCTL, LOG_ERROR)("DOS:IOCTL Call 0D:%2X Drive %2X unhandled", reg_cl, drive);
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
			}
		return true;
		}
	case 0x0E:										// Get Logical Drive Map
		reg_al = 0;									// Only 1 logical drive assigned
		return true;
	default:
		LOG(LOG_DOSMISC, LOG_ERROR)("DOS:IOCTL Call %2X unhandled", reg_al);
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		break;
		}
	return false;
	}

bool DOS_GetSTDINStatus(void)
	{
	Bit32u handle = RealHandle(STDIN);
	if (handle == 0xFF)
		return false;
	if (Files[handle] && (Files[handle]->GetInformation() & 64))
		return false;
	return true;
	}
