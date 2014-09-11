#include "vDos.h"
#include "mem.h"
#include "dos_inc.h"
#include "callback.h"

static Bitu call_casemap;

static Bit16u dos_memseg = DOS_PRIVATE_SEGMENT;

Bit16u DOS_GetMemory(Bit16u pages)
	{
	if ((Bitu)pages + (Bitu)dos_memseg >= DOS_PRIVATE_SEGMENT_END)
		E_Exit("DOS: Not enough memory for internal tables");
	Bit16u page = dos_memseg;
	dos_memseg += pages;
	return page;
	}

static Bitu DOS_CaseMapFunc(void)
	{
	// LOG(LOG_DOSMISC,LOG_ERROR)("Case map routine called : %c",reg_al);
	return CBRET_NONE;
	}

static Bit8u country_info[0x22] = {
/* Date format      */  0x00, 0x00,
/* Currencystring   */  0x24, 0x00, 0x00, 0x00, 0x00,
/* Thousands sep    */  0x2c, 0x00,
/* Decimal sep      */  0x2e, 0x00,
/* Date sep         */  0x2d, 0x00,
/* time sep         */  0x3a, 0x00,
/* currency form    */  0x00,
/* digits after dec */  0x02,
/* Time format      */  0x00,
/* Casemap          */  0x00, 0x00, 0x00, 0x00,
/* Data sep         */  0x2c, 0x00,
/* Reservered 5     */  0x00, 0x00, 0x00, 0x00, 0x00,
/* Reservered 5     */  0x00, 0x00, 0x00, 0x00, 0x00
};

static Bit8u filenamechartable[0x18] = {
	0x16, 0x00, 0x01,
	0x00, 0xff,		// allowed chars from .. to
	0x00,
	0x00, 0x20,		// excluded chars from .. to
	0x02,
	0x0e,			// number of illegal separators
	0x2e, 0x22, 0x2f, 0x5c, 0x5b, 0x5d, 0x3a, 0x7c, 0x3c, 0x3e, 0x2b, 0x3d, 0x3b, 0x2c
};

void DOS_SetupTables(void)
	{
	Bit16u seg;
	Bitu i;
	dos.tables.mediaid = SegOff2dWord(DOS_GetMemory(4), 0);
	dos.tables.tempdta = SegOff2dWord(DOS_GetMemory(4), 0);
	dos.tables.tempdta_fcbdelete = SegOff2dWord(DOS_GetMemory(4), 0);
	vPC_rStoswb(dWord2Ptr(dos.tables.mediaid), 0, DOS_DRIVES*2);
	// Create the DOS Info Block
	dos_infoblock.SetLocation(DOS_INFOBLOCK_SEG);	// c2woody
   
	// create SDA
	DOS_SDA(DOS_SDA_SEG, 0).Init();

	// Some weird files >20 detection routine
	// Possibly obselete when SFT is properly handled
	vPC_rStosd(DOS_CONSTRING_SEG, 0x0a, 0x204e4f43);
	vPC_rStosd(DOS_CONSTRING_SEG, 0x1a, 0x204e4f43);
	vPC_rStosd(DOS_CONSTRING_SEG, 0x2a, 0x204e4f43);

	// create a CON device driver
	seg = DOS_CONDRV_SEG;
 	vPC_rStosd(seg, 0x00, 0xffffffff);	// next ptr
 	vPC_rStosw(seg, 0x04, 0x8013);		// attributes
  	vPC_rStosd(seg, 0x06, 0xffffffff);	// strategy routine
  	vPC_rStosd(seg, 0x0a, 0x204e4f43);	// driver name
  	vPC_rStosd(seg, 0x0e, 0x20202020);	// driver name
	dos_infoblock.SetDeviceChainStart(SegOff2dWord(seg, 0));
   
	// Create a fake Current Directory Structure
	seg = DOS_CDS_SEG;
	vPC_rStosd(seg, 0x00, 0x005c3a43);
	dos_infoblock.SetCurDirStruct(SegOff2dWord(seg, 0));

	// Allocate DCBS DOUBLE BYTE CHARACTER SET LEAD-BYTE TABLE
	dos.tables.dbcs = SegOff2dWord(DOS_GetMemory(12), 0);
	vPC_rStosd(dWord2Ptr(dos.tables.dbcs), 0);	// empty table
	// FILENAME CHARACTER TABLE
	dos.tables.filenamechar = SegOff2dWord(DOS_GetMemory(2), 0);
	PhysPt p_addr = dWord2Ptr(dos.tables.filenamechar);
	for (i = 0; i < sizeof(filenamechartable); i++)
		vPC_rStosb(p_addr++, filenamechartable[i]);
/*
	vPC_rStosw(dWord2Ptr(dos.tables.filenamechar)+0x00, 0x16);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x02, 0x01);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x03, 0x00);	// allowed chars from
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x04, 0xff);	// ...to
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x05, 0x00);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x06, 0x00);	// excluded chars from
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x07, 0x20);	// ...to
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x08, 0x02);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x09, 0x0e);	// number of illegal separators
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x0a, 0x2e);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x0b, 0x22);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x0c, 0x2f);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x0d, 0x5c);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x0e, 0x5b);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x0f, 0x5d);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x10, 0x3a);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x11, 0x7c);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x12, 0x3c);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x13, 0x3e);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x14, 0x2b);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x15, 0x3d);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x16, 0x3b);
	vPC_rStosb(dWord2Ptr(dos.tables.filenamechar)+0x17, 0x2c);
*/
	// COLLATING SEQUENCE TABLE + UPCASE TABLE
	// 256 bytes for col table, 128 for upcase, 4 for number of entries
	dos.tables.collatingseq = SegOff2dWord(DOS_GetMemory(25), 0);
	vPC_rStosw(dWord2Ptr(dos.tables.collatingseq), 0x100);
	for (i = 0; i < 256; i++)
		vPC_rStosb(dWord2Ptr(dos.tables.collatingseq)+i+2, i);
	dos.tables.upcase = dos.tables.collatingseq+258;
	vPC_rStosw(dWord2Ptr(dos.tables.upcase), 0x80);
	for (i = 0; i < 128; i++)
		vPC_rStosb(dWord2Ptr(dos.tables.upcase)+i+2, 0x80+i);
 
	// Create a fake FCB SFT
	seg = DOS_GetMemory(4);
	vPC_rStosd(seg, 0, 0xffffffff);			// Last File Table
	vPC_rStosw(seg, 4, 100);				// File Table supports 100 files
	dos_infoblock.SetFCBTable(SegOff2dWord(seg, 0));

	// Create a fake DPB
	dos.tables.dpb = DOS_GetMemory(2);
	for(Bitu d = 0; d < 26; d++)
		vPC_rStosb(dos.tables.dpb, d, d);

	// Create a fake disk buffer head
	seg = DOS_GetMemory(6);

	for (Bitu ct = 0; ct < 0x20; ct++)
		vPC_rStosb(seg, ct, 0);
	vPC_rStosw(seg, 0x00, 0xffff);		// forward ptr
	vPC_rStosw(seg, 0x02, 0xffff);		// backward ptr
	vPC_rStosb(seg, 0x04, 0xff);		// not in use
	vPC_rStosb(seg, 0x0a, 0x01);		// number of FATs
	vPC_rStosd(seg, 0x0d, 0xffffffff);	// pointer to DPB
	dos_infoblock.SetDiskBufferHeadPt(SegOff2dWord(seg, 0));

	// Set buffers to a nice value
	dos_infoblock.SetBuffers(50, 50);

	// case map routine INT 0x21 0x38
	call_casemap = CALLBACK_Allocate();
	CALLBACK_Setup(call_casemap, DOS_CaseMapFunc, CB_RETF, "DOS CaseMap");
	// Add it to country structure
	*(Bit32u *)(country_info + 0x12) = CALLBACK_RealPointer(call_casemap);
	dos.tables.country = country_info;
	}
