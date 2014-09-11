enum STRING_OP {
	R_OUTSB, R_OUTSW, R_OUTSD,
	R_INSB, R_INSW, R_INSD,
	R_MOVSB, R_MOVSW, R_MOVSD,
	R_LODSB, R_LODSW, R_LODSD,
	R_STOSB, R_STOSW, R_STOSD,
	R_SCASB, R_SCASW, R_SCASD,
	R_CMPSB, R_CMPSW, R_CMPSD
};

#define LoadD(_BLAH) _BLAH

static void DoString(STRING_OP type)
	{
	PhysPt  si_base, di_base;
	Bitu	si_index, di_index;
	Bitu	add_mask;
	Bitu	count, count_left;

	si_base = BaseDS;
	di_base = SegBase(es);
	add_mask = AddrMaskTable[core.prefixes & PREFIX_ADDR];
	si_index = reg_esi & add_mask;
	di_index = reg_edi & add_mask;
	if (!TEST_PREFIX_REP)
		count = 1;
	else
		{
		count = reg_ecx & add_mask;
		CPU_Cycles++;
		// Calculate amount of ops to do before cycles run out
		if ((count > (Bitu)CPU_Cycles) && (type < R_SCASB))
			{
			if (count-(Bitu)CPU_Cycles > (Bitu)CPU_CycleMax/10)
				{
				count_left = count-CPU_Cycles;
				count = CPU_Cycles;
				LOADIP;			// RESET IP to the start
				}
			else
				count_left = 0;
			CPU_Cycles = 0;
			}
		else
			{
			if ((count <= 1) && (CPU_Cycles <= 1))							// Won't interrupt scas and cmps instruction since they can interrupt themselves
				CPU_Cycles--;
			else if (type < R_SCASB)
//				CPU_Cycles -= count;
				CPU_Cycles -= (min(count, 2)+count/8);						// For the time being (the whole Cycles modus will be dropped) some mix of optimized functions
			count_left = 0;
			}
		}
	Bits add_index = cpu.direction;
	if (count)
		switch (type)
			{
		case R_LODSB:
			for (; count; count--)
				{
				reg_al = vPC_rLodsb(si_base+si_index);
				si_index = (si_index+add_index) & add_mask;
				}
			break;
		case R_STOSW:
			add_index <<= 1;
			if (count > 5)											// try to optimize if more than 5 words
				{
				Bitu di_newindex = di_index+(add_index*count)-add_index;
				if ((di_newindex&add_mask) == di_newindex)			// if no wraparound, use vPC_rStoswb()
					{
					if (add_index > 0)
						vPC_rStoswb(di_base+di_index, reg_ax, count+count);
					else
						vPC_rStoswb(di_base+di_newindex, reg_ax, count+count);
					di_index = (di_newindex + add_index)&add_mask;
					count = 0;
					break;
					}
				}
			for (; count; count--)	
				{
				vPC_rStosw(di_base+di_index, reg_ax);
				di_index = (di_index+add_index) & add_mask;
				}
			break;
		case R_STOSB:
			{
			if (count > 5)											// try to optimize if more than 5 bytes
				{
				Bitu di_newindex = di_index+(add_index*count)-add_index;
				if ((di_newindex&add_mask) == di_newindex)			// if no wraparound, use vPC_rStoswb()
					{
					if (add_index > 0)
						vPC_rStoswb(di_base+di_index, reg_al+(reg_al<<8), count);
					else
						vPC_rStoswb(di_base+di_newindex, reg_al+(reg_al<<8), count);
					di_index = (di_newindex + add_index)&add_mask;
					count = 0;
					break;
					}
				}
			for (; count; count--)									// less than 6 bytes or DI wraps around
				{
				vPC_rStosb(di_base+di_index, reg_al);
				di_index = (di_index+add_index) & add_mask;
				}
			}
			break;
		case R_MOVSB:
			if (count > 5)											// try to optimize if more than 5 bytes
				{
				Bitu di_newindex = di_index+(add_index*count)-add_index;
				Bitu si_newindex = si_index+(add_index*count)-add_index;
				if ((di_newindex&add_mask) == di_newindex &&
					(si_newindex&add_mask) == si_newindex)			// if no wraparounds, use vPC_rMovsb()
					{
					if (add_index == 1)
						vPC_rMovsb(di_base+di_index, si_base+si_index, count);
					else
						vPC_rMovsbDn(di_base+di_index, si_base+si_index, count);
					di_index = (di_newindex + add_index)&add_mask;
					si_index = (si_newindex + add_index)&add_mask;
					count = 0;
					break;
					}
				}
			for (; count; count--)									// Less than 6 bytes or SI/DI wraps around
				{
				vPC_rStosb(di_base+di_index, vPC_rLodsb(si_base+si_index));
				di_index = (di_index+add_index)&add_mask;
				si_index = (si_index+add_index)&add_mask;
				}
			break;
		case R_LODSW:
			add_index <<= 1;
			for (; count; count--)
				{
				reg_ax = vPC_rLodsw(si_base+si_index);
				si_index = (si_index+add_index) & add_mask;
				}
			break;
		case R_MOVSW:
			{
			add_index <<= 1;
			if (count > 5)											// Try to optimize if more than 5 words
				{
				Bitu di_newindex = di_index+(add_index*count)-add_index;
				Bitu si_newindex = si_index+(add_index*count)-add_index;
				if ((di_newindex&add_mask) == di_newindex &&
					(si_newindex&add_mask) == si_newindex)			// If no wraparounds, use vPC_rMovsw()
					{
					if (add_index == 2)
						vPC_rMovsw(di_base+di_index, si_base+si_index, count);
					else
						vPC_rMovswDn(di_base+di_index, si_base+si_index, count);
					di_index = (di_newindex + add_index)&add_mask;
					si_index = (si_newindex + add_index)&add_mask;
					count = 0;
					break;
					}
				}
			for (; count; count--)									// Less than 6 words or SI/DI wraps around
				{
				vPC_rStosw(di_base+di_index, vPC_rLodsw(si_base+si_index));
				di_index = (di_index+add_index) & add_mask;
				si_index = (si_index+add_index) & add_mask;
				}
			}
			break;
		case R_OUTSB:
			for (; count; count--)
				{
				IO_WriteB(reg_dx, vPC_rLodsb(si_base+si_index));
				si_index = (si_index+add_index) & add_mask;
				}
			break;
		case R_OUTSW:
			add_index <<= 1;
			for (; count; count--)
				{
				IO_WriteW(reg_dx, vPC_rLodsw(si_base+si_index));
				si_index = (si_index+add_index) & add_mask;
			}
			break;
		case R_OUTSD:
			add_index <<= 2;
			for (; count; count--)
				{
				IO_WriteD(reg_dx, vPC_rLodsd(si_base+si_index));
				si_index = (si_index+add_index) & add_mask;
				}
			break;
		case R_INSB:
			for (; count; count--)
				{
				SaveMb(di_base+di_index, IO_ReadB(reg_dx));
				di_index = (di_index+add_index) & add_mask;
				}
			break;
		case R_INSW:
			add_index <<= 1;
			for (; count; count--)
				{
				SaveMw(di_base+di_index, IO_ReadW(reg_dx));
				di_index = (di_index+add_index) & add_mask;
				}
			break;
		case R_STOSD:
			add_index <<= 2;
			for (; count; count--)
				{
				vPC_rStosd(di_base+di_index, reg_eax);
				di_index = (di_index+add_index) & add_mask;
				}
			break;
		case R_MOVSD:
			add_index <<= 2;
			for (; count; count--)
				{
				vPC_rStosd(di_base+di_index, vPC_rLodsd(si_base+si_index));
				di_index = (di_index+add_index) & add_mask;
				si_index = (si_index+add_index) & add_mask;
				}
			break;
		case R_LODSD:
			add_index <<= 2;
			for (; count; count--)
				{
				reg_eax = vPC_rLodsd(si_base+si_index);
				si_index = (si_index+add_index) & add_mask;
				}
			break;
		case R_SCASB:
			{
			Bit8u val2;
			for (; count;)
				{
				count--;
				CPU_Cycles--;
				val2 = vPC_rLodsb(di_base+di_index);
				di_index = (di_index+add_index) & add_mask;
				if ((reg_al == val2) != core.rep_zero)
					break;
				}
			CMPB(reg_al, val2, LoadD, 0);
			}
			break;
		case R_SCASW:
			{
			add_index <<= 1;
			Bit16u val2;
			for (; count;)
				{
				count--;
				CPU_Cycles--;
				val2 = vPC_rLodsw(di_base+di_index);
				di_index = (di_index+add_index) & add_mask;
				if ((reg_ax == val2) != core.rep_zero)
					break;
				}
			CMPW(reg_ax, val2, LoadD, 0);
			}
			break;
		case R_SCASD:
			{
			add_index <<= 2;
			Bit32u val2;
			for (; count;)
				{
				count--;
				CPU_Cycles--;
				val2 = vPC_rLodsd(di_base+di_index);
				di_index = (di_index+add_index) & add_mask;
				if ((reg_eax == val2) != core.rep_zero)
					break;
				}
			CMPD(reg_eax, val2, LoadD, 0);
			}
			break;
		case R_CMPSB:
			{
			Bit8u val1, val2;
			for (; count;)
				{
				count--;
				CPU_Cycles--;
				val1 = vPC_rLodsb(si_base+si_index);
				val2 = vPC_rLodsb(di_base+di_index);
				si_index = (si_index+add_index) & add_mask;
				di_index = (di_index+add_index) & add_mask;
				if ((val1 == val2) != core.rep_zero)
					break;
				}
			CMPB(val1, val2, LoadD, 0);
			}
			break;
		case R_CMPSW:
			{
			add_index <<= 1;
			Bit16u val1, val2;
			for (; count;)
				{
				count--;
				CPU_Cycles--;
				val1 = vPC_rLodsw(si_base+si_index);
				val2 = vPC_rLodsw(di_base+di_index);
				si_index = (si_index+add_index) & add_mask;
				di_index = (di_index+add_index) & add_mask;
				if ((val1 == val2) != core.rep_zero)
					break;
				}
			CMPW(val1, val2, LoadD, 0);
			}
			break;
		case R_CMPSD:
			{
			add_index <<= 2;
			Bit32u val1, val2;
			for (; count;)
				{
				count--;
				CPU_Cycles--;
				val1 = vPC_rLodsd(si_base+si_index);
				val2 = vPC_rLodsd(di_base+di_index);
				si_index = (si_index+add_index) & add_mask;
				di_index = (di_index+add_index) & add_mask;
				if ((val1 == val2) != core.rep_zero)
					break;
				}
			CMPD(val1, val2, LoadD, 0);
			}
			break;
		default:
			LOG(LOG_CPU, LOG_ERROR)("Unhandled string op %d", type);
			}
	// Clean up after certain amount of instructions
	reg_esi &= (~add_mask);
	reg_esi |= (si_index & add_mask);
	reg_edi &= (~add_mask);
	reg_edi |= (di_index & add_mask);
	if (TEST_PREFIX_REP)
		{
		count += count_left;
		reg_ecx &= (~add_mask);
		reg_ecx |= (count & add_mask);
		}
	}
