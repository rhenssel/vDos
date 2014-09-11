#ifndef VDOS_CALLBACK_H
#define VDOS_CALLBACK_H

#ifndef VDOS_MEM_H
#include "mem.h"
#endif 

typedef Bitu (*CallBack_Handler)(void);
extern CallBack_Handler CallBack_Handlers[];

enum {CB_RETF, CB_RETF8, CB_IRET, CB_IRET_STI, CB_IRET_EOI_PIC1,
		CB_IRQ0, CB_IRQ1, CB_IRQ9, CB_IRQ12, CB_IRQ12_RET, CB_MOUSE,
		/*CB_INT28,*/ CB_INT29, CB_INT16, CB_HOOKABLE};

#define CB_MAX		128
#define CB_SIZE		32
#define CB_SEG		0xF000
#define CB_SOFFSET	0x1000

enum {CBRET_NONE=0, CBRET_STOP=1};

extern Bit8u lastint;

static inline RealPt CALLBACK_RealPointer(Bitu callback)
	{
	return SegOff2dWord(CB_SEG, (Bit16u)(CB_SOFFSET+callback*CB_SIZE));
	}

static inline PhysPt CALLBACK_PhysPointer(Bitu callback)
	{
	return SegOff2Ptr(CB_SEG, (Bit16u)(CB_SOFFSET+callback*CB_SIZE));
	}

static inline PhysPt CALLBACK_GetBase(void)
	{
	return (CB_SEG << 4) + CB_SOFFSET;
	}

Bitu CALLBACK_Allocate();

void CALLBACK_Idle(void);


void CALLBACK_RunRealInt(Bit8u intnum);

bool CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,const char* descr);
Bitu CALLBACK_Setup(Bitu callback,CallBack_Handler handler,Bitu type,PhysPt addr,const char* descr);

const char* CALLBACK_GetDescription(Bitu callback);
bool CALLBACK_Free(Bitu callback);

void CALLBACK_SCF(bool val);
void CALLBACK_SZF(bool val);
void CALLBACK_SIF(bool val);

extern Bitu call_priv_io;

class CALLBACK_HandlerObject
	{
private:
	bool installed;
	Bitu m_callback;
	enum {NONE,SETUP,SETUPAT} m_type;
    struct {	
		RealPt old_vector;
		Bit8u interrupt;
		bool installed;
	} vectorhandler;
public:
	CALLBACK_HandlerObject():installed(false), m_type(NONE)
		{
		vectorhandler.installed=false;
		}
	~CALLBACK_HandlerObject();

	// Install and allocate a callback.
	void Install(CallBack_Handler handler, Bitu type, const char* description);
	void Install(CallBack_Handler handler, Bitu type, PhysPt addr,const char* description);

	// Only allocate a callback number
	void Allocate(CallBack_Handler handler, const char* description = 0);
	Bit16u Get_callback()
		{
		return (Bit16u)m_callback;
		}
	RealPt Get_RealPointer()
		{
		return CALLBACK_RealPointer(m_callback);
		}
	void Set_RealVec(Bit8u vec);
	};
#endif
