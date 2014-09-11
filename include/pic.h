#ifndef VDOS_PIC_H
#define VDOS_PIC_H


/* CPU Cycle Timing */
extern Bit32s CPU_Cycles;
extern Bit32s CPU_CycleLeft;
//extern Bit32s CPU_CycleMax;

typedef void (PIC_EOIHandler) (void);
typedef void (* PIC_EventHandler)(Bitu val);


#define PIC_MAXIRQ 15
#define PIC_NOIRQ 0xFF

extern Bitu PIC_IRQCheck;
extern Bitu PIC_IRQActive;
extern Bitu PIC_Ticks;

static inline float PIC_TickIndex(void)
	{
	return (CPU_CycleMax-CPU_CycleLeft-CPU_Cycles)/(float)CPU_CycleMax;
	}

static inline Bits PIC_MakeCycles(double amount)
	{
	return (Bits)(CPU_CycleMax*amount);
	}

static inline double PIC_FullIndex(void)
	{
	return PIC_Ticks+(double)PIC_TickIndex();
	}

void PIC_ActivateIRQ(Bitu irq);
void PIC_DeActivateIRQ(Bitu irq);

void PIC_runIRQs(void);
bool PIC_RunQueue(void);

//Delay in milliseconds
void PIC_AddEvent(PIC_EventHandler handler, float delay, Bitu val=0);
void PIC_RemoveEvents(PIC_EventHandler handler);

void PIC_SetIRQMask(Bitu irq, bool masked);
#endif
