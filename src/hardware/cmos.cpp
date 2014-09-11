#include <time.h>
#include <math.h>
#include "vDos.h"
#include "timer.h"
#include "pic.h"
#include "inout.h"
#include "mem.h"

static struct {
	Bit8u regs[0x40];
	bool nmi;
	bool bcd;
	Bit8u reg;
	struct {
		bool enabled;
		Bit8u div;
		float delay;
		bool acknowledged;
	} timer;
	struct {
		double timer;
		double ended;
		double alarm;
	} last;
	bool update_ended;
} cmos;

static void cmos_timerevent(Bitu val)
	{
	if (cmos.timer.acknowledged)
		{
		cmos.timer.acknowledged = false;
		PIC_ActivateIRQ(8);
		}
	if (cmos.timer.enabled)
		{
		PIC_AddEvent(cmos_timerevent, cmos.timer.delay);
		cmos.regs[0xc] = 0xC0;		// Contraption Zack (music)
		}
	}

static void cmos_checktimer(void)
	{
	PIC_RemoveEvents(cmos_timerevent);	
	if (cmos.timer.div <= 2)
		cmos.timer.div += 7;
	cmos.timer.delay = (1000.0f/(32768.0f / (1 << (cmos.timer.div - 1))));
	if (!cmos.timer.div || !cmos.timer.enabled)
		return;
//	PIC_AddEvent(cmos_timerevent,cmos.timer.delay);
	/* A rtc is always running */
	double remd = fmod(PIC_FullIndex(),(double)cmos.timer.delay);
	PIC_AddEvent(cmos_timerevent, (float)((double)cmos.timer.delay-remd));	// Should be more like a real pc. Check
//	status reg A reading with this (and with other delays actually)
	}

void cmos_selreg(Bitu port,Bitu val, Bitu iolen)
	{
	cmos.reg = val & 0x3f;
	cmos.nmi = (val & 0x80)>0;
	}

static void cmos_writereg(Bitu port, Bitu  val, Bitu iolen)
	{
	switch (cmos.reg)
		{
	case 0x00:		// Seconds
	case 0x02:		// Minutes
	case 0x04:		// Hours
	case 0x06:		// Day of week
	case 0x07:		// Date of month
	case 0x08:		// Month
	case 0x09:		// Year
	case 0x32:		// Century
		// Ignore writes to change alarm
		break;
	case 0x01:		// Seconds Alarm
	case 0x03:		// Minutes Alarm
	case 0x05:		// Hours Alarm
		cmos.regs[cmos.reg] = val;
		break;
	case 0x0a:		// Status reg A
		cmos.regs[cmos.reg] = val & 0x7f;
		if ((val & 0x70) != 0x20)
			LOG(LOG_BIOS,LOG_ERROR)("CMOS Illegal 22 stage divider value");
		cmos.timer.div = (val & 0xf);
		cmos_checktimer();
		break;
	case 0x0b:		// Status reg B
		cmos.bcd = !(val & 0x4);
		cmos.regs[cmos.reg] = val & 0x7f;
		cmos.timer.enabled = (val & 0x40)>0;
		if (val&0x10)
			LOG(LOG_BIOS,LOG_ERROR)("CMOS:Updated ended interrupt not supported yet");
		cmos_checktimer();
		break;
	case 0x0d:		// Status reg D
		cmos.regs[cmos.reg] = val & 0x80;	// Bit 7=1:RTC Pown on
		break;
	case 0x0f:		// Shutdown status byte
		cmos.regs[cmos.reg] = val & 0x7f;
		break;
	default:
		cmos.regs[cmos.reg] = val & 0x7f;
		LOG(LOG_BIOS,LOG_ERROR)("CMOS:WRite to unhandled register %x", cmos.reg);
		}
	}

#define MAKE_RETURN(_VAL) (cmos.bcd ? ((((_VAL) / 10) << 4) | ((_VAL) % 10)) : (_VAL));

static Bitu cmos_readreg(Bitu port,Bitu iolen)
	{
	if (cmos.reg > 0x3f)
		{
		LOG(LOG_BIOS,LOG_ERROR)("CMOS:Read from illegal register %x",cmos.reg);
		return 0xff;
		}
	time_t curtime;
	struct tm *loctime;
	// Get the current time.
	curtime = time(NULL);

	// Convert it to local time representation.
	loctime = localtime(&curtime);

	switch (cmos.reg)
		{
	case 0x00:		// Seconds
		return MAKE_RETURN(loctime->tm_sec);
	case 0x02:		// Minutes
		return MAKE_RETURN(loctime->tm_min);
	case 0x04:		// Hours
		return MAKE_RETURN(loctime->tm_hour);
	case 0x06:		// Day of week
		return MAKE_RETURN(loctime->tm_wday + 1);
	case 0x07:		// Date of month
		return MAKE_RETURN(loctime->tm_mday);
	case 0x08:		// Month
		return MAKE_RETURN(loctime->tm_mon + 1);
	case 0x09:		// Year
		return MAKE_RETURN(loctime->tm_year % 100);
	case 0x32:		// Century
		return MAKE_RETURN(loctime->tm_year / 100 + 19);
	case 0x01:		// Seconds Alarm
	case 0x03:		// Minutes Alarm
	case 0x05:		// Hours Alarm
		return cmos.regs[cmos.reg];
	case 0x0a:		// Status register A
		if (PIC_TickIndex()<0.002)
			return (cmos.regs[0x0a]&0x7f) | 0x80;
		else
			return (cmos.regs[0x0a]&0x7f);
	case 0x0c:		// Status register C
		cmos.timer.acknowledged = true;
		if (cmos.timer.enabled)
			{
			// In periodic interrupt mode only care for those flags
			Bit8u val = cmos.regs[0xc];
			cmos.regs[0xc] = 0;
			return val;
			}
		else
			{
			// Give correct values at certain times
			Bit8u val = 0;
			double index = PIC_FullIndex();
			if (index >= (cmos.last.timer+cmos.timer.delay))
				{
				cmos.last.timer = index;
				val |= 0x40;
				} 
			if (index >= (cmos.last.ended+1000))
				{
				cmos.last.ended = index;
				val |= 0x10;
				} 
			return val;
			}
	case 0x10:		// Floppy size
	case 0x12:		// First harddrive info

	case 0x19:
	case 0x1b:
	case 0x1c:
	case 0x1d:
	case 0x1e:
	case 0x1f:
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	
	case 0x1a:		// Second harddrive info
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
	case 0x28:
	case 0x29:
	case 0x2a:
	case 0x2b:
	case 0x2c:
	case 0x39:
	case 0x3a:
		return 0;
//	case 0x0b:		// Status register B
//	case 0x0d:		// Status register D
//	case 0x0f:		// Shutdown status byte
//	case 0x14:		// Equipment
//	case 0x15:		// Base Memory KB Low Byte
//	case 0x16:		// Base Memory KB High Byte
//	case 0x17:		// Extended memory in KB Low Byte
//	case 0x18:		// Extended memory in KB High Byte
//	case 0x30:		// Extended memory in KB Low Byte
//	case 0x31:		// Extended memory in KB High Byte
//		return cmos.regs[cmos.reg];
	default:
		return cmos.regs[cmos.reg];
	}
}

void CMOS_SetRegister(Bitu regNr, Bit8u val)
	{
	cmos.regs[regNr] = val;
	}


static	IO_ReadHandleObject ReadHandler[2];
static	IO_WriteHandleObject WriteHandler[2];	

void CMOS_Init()
	{
	WriteHandler[0].Install(0x70, cmos_selreg, IO_MB);
	WriteHandler[1].Install(0x71, cmos_writereg, IO_MB);
	ReadHandler[0].Install(0x71, cmos_readreg, IO_MB);
	cmos.timer.enabled = false;
	cmos.timer.acknowledged = true;
	cmos.reg = 0xa;
	cmos_writereg(0x71, 0x26, 1);
	cmos.reg = 0xb;
	cmos_writereg(0x71, 0x2, 1);							// Struct tm *loctime is of 24 hour format,
	cmos.reg = 0xd;
	cmos_writereg(0x71, 0x80, 1);							// RTC power on
	// Equipment is updated from bios.cpp and bios_disk.cpp
	*(Bit32u*)(cmos.regs+0x15) = 640;						// Base memory size, it is always 640K
	*(Bit32u*)(cmos.regs+0x17) = TOT_MEM_BYTES/1024-1024;	// Extended memory size
	*(Bit32u*)(cmos.regs+0x30) = TOT_MEM_BYTES/1024-1024;
	}
