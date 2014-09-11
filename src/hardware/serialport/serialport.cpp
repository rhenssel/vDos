#include <string.h>
#include "vdos.h"
#include "pic.h"
#include "bios.h"					// SetComPorts(..)

#include "serialport.h"

void CSerial::Putchar(Bit8u val)
	{
	Bit16u size = 1;
	mydosdevice->Write(&val, &size);
	}

static Bitu SERIAL_Read (Bitu port, Bitu iolen)
	{
	return 0xff;
	}

static void SERIAL_Write (Bitu port, Bitu val, Bitu)
	{
	for (Bitu i = 0; i < 4; i++)
		if (serial_baseaddr[i] == port && serialPorts[i])
			{
			serialPorts[i]->Putchar(val);
			return;
			}
	}

// Initialisation
CSerial::CSerial(Bitu portno, device_PRT* dosdevice)
	{
	Bit16u base = serial_baseaddr[portno];
	
	for (Bitu i = 0; i <= 7; i++)
		{
		WriteHandler[i].Install (i + base, SERIAL_Write, IO_MB);
		ReadHandler[i].Install (i + base, SERIAL_Read, IO_MB);
		}
	mydosdevice = dosdevice;
	}

CSerial::~CSerial(void)
	{
	};

bool CSerial::Getchar(Bit8u* data)
	{
	return false;			// TODO actualy read something
	}

CSerial* serialPorts[4] = {0, 0, 0, 0};
static device_PRT* dosdevices[9];

void SERIAL_Destroy (Section * sec)
	{
	for (Bitu i = 0; i < 9; i++)
		{
		if (i < 4 && serialPorts[i])
			{
			delete serialPorts[i];
			serialPorts[i] = 0;
			}
		if (dosdevices[i])
			{
			DOS_DelDevice(dosdevices[i]);
			dosdevices[i] = 0;
			}
		}
	}

void SERIAL_Init (Section * sec)
	{
	Section_prop *section = static_cast <Section_prop*>(sec);
	char sname[] = "COMx";
	// iterate through first 4 com ports and the rest (COM5-9)
	for (Bitu i = 0; i < 9; i++)
		{
		sname[3] = '1' + i;
		dosdevices[i] = new device_PRT(sname, section->Get_string(sname));
		DOS_AddDevice(dosdevices[i]);
		if (i < 4)
			serialPorts[i] = new CSerial(i, dosdevices[i]);
		}
	BIOS_SetComPorts((Bit16u*)serial_baseaddr);
	}

