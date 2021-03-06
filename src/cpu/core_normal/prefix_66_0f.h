	CASE_0F_D(0x00)												/* GRP 6 Exxx */
		{
			if (!cpu.pmode)
				goto illegal_opcode;
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:	/* SLDT */
			case 0x01:	/* STR */
				{
					Bitu saveval;
					if (!which) saveval=CPU_SLDT();
					else saveval=CPU_STR();
					if (rm >= 0xc0) {GetEArw;*earw=(Bit16u)saveval;}
					else {GetEAa;SaveMw(eaa,saveval);}
				}
				break;
			case 0x02:case 0x03:case 0x04:case 0x05:
				{
					/* Just use 16-bit loads since were only using selectors */
					Bitu loadval;
					if (rm >= 0xc0 ) {GetEArw;loadval=*earw;}
					else {GetEAa;loadval=vPC_rLodsw(eaa);}
					switch (which) {
					case 0x02:
						if (cpu.cpl) EXCEPTION(EXCEPTION_GP);
						if (CPU_LLDT(loadval)) RUNEXCEPTION();
						break;
					case 0x03:
						if (cpu.cpl) EXCEPTION(EXCEPTION_GP);
						if (CPU_LTR(loadval)) RUNEXCEPTION();
						break;
					case 0x04:
						CPU_VERR(loadval);
						break;
					case 0x05:
						CPU_VERW(loadval);
						break;
					}
				}
				break;
			default:
				LOG(LOG_CPU,LOG_ERROR)("GRP6:Illegal call %2X",which);
				goto illegal_opcode;
			}
		}
		break;
	CASE_0F_D(0x01)												/* Group 7 Ed */
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm < 0xc0)	{ //First ones all use EA
				GetEAa;Bitu limit;
				switch (which) {
				case 0x00:										/* SGDT */
					SaveMw(eaa,(Bit16u)CPU_SGDT_limit());
					SaveMd(eaa+2,(Bit32u)CPU_SGDT_base());
					break;
				case 0x01:										/* SIDT */
					SaveMw(eaa,(Bit16u)CPU_SIDT_limit());
					SaveMd(eaa+2,(Bit32u)CPU_SIDT_base());
					break;
				case 0x02:										/* LGDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					CPU_LGDT(vPC_rLodsw(eaa),vPC_rLodsd(eaa+2));
					break;
				case 0x03:										/* LIDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					CPU_LIDT(vPC_rLodsw(eaa),vPC_rLodsd(eaa+2));
					break;
				case 0x04:										/* SMSW */
					SaveMw(eaa,(Bit16u)CPU_SMSW());
					break;
				case 0x06:										/* LMSW */
					limit=vPC_rLodsw(eaa);
					if (CPU_LMSW((Bit16u)limit)) RUNEXCEPTION();
					break;
				case 0x07:										/* INVLPG */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
//					PAGING_ClearTLB();
					break;
				}
			} else {
				GetEArd;
				switch (which) {
				case 0x02:										/* LGDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					goto illegal_opcode;
				case 0x03:										/* LIDT */
					if (cpu.pmode && cpu.cpl) EXCEPTION(EXCEPTION_GP);
					goto illegal_opcode;
				case 0x04:										/* SMSW */
					*eard=(Bit32u)CPU_SMSW();
					break;
				case 0x06:										/* LMSW */
					if (CPU_LMSW(*eard)) RUNEXCEPTION();
					break;
				default:
					LOG(LOG_CPU,LOG_ERROR)("Illegal group 7 RM subfunction %d",which);
					goto illegal_opcode;
					break;
				}

			}
		}
		break;
	CASE_0F_D(0x02)												/* LAR Gd,Ed */
		{
			if (!cpu.pmode)
				goto illegal_opcode;
			GetRMrd;Bitu ar=*rmrd;
			if (rm >= 0xc0) {
				GetEArw;CPU_LAR(*earw,ar);
			} else {
				GetEAa;CPU_LAR(vPC_rLodsw(eaa),ar);
			}
			*rmrd=(Bit32u)ar;
		}
		break;
	CASE_0F_D(0x03)												/* LSL Gd,Ew */
		{
			if (!cpu.pmode)
				goto illegal_opcode;
			GetRMrd;Bitu limit=*rmrd;
			/* Just load 16-bit values for selectors */
			if (rm >= 0xc0) {
				GetEArw;CPU_LSL(*earw,limit);
			} else {
				GetEAa;CPU_LSL(vPC_rLodsw(eaa),limit);
			}
			*rmrd=(Bit32u)limit;
		}
		break;
	CASE_0F_D(0x80)												/* JO */
		JumpCond32_d(TFLG_O);break;
	CASE_0F_D(0x81)												/* JNO */
		JumpCond32_d(TFLG_NO);break;
	CASE_0F_D(0x82)												/* JB */
		JumpCond32_d(TFLG_B);break;
	CASE_0F_D(0x83)												/* JNB */
		JumpCond32_d(TFLG_NB);break;
	CASE_0F_D(0x84)												/* JZ */
		JumpCond32_d(TFLG_Z);break;
	CASE_0F_D(0x85)												/* JNZ */
		JumpCond32_d(TFLG_NZ);break;
	CASE_0F_D(0x86)												/* JBE */
		JumpCond32_d(TFLG_BE);break;
	CASE_0F_D(0x87)												/* JNBE */
		JumpCond32_d(TFLG_NBE);break;
	CASE_0F_D(0x88)												/* JS */
		JumpCond32_d(TFLG_S);break;
	CASE_0F_D(0x89)												/* JNS */
		JumpCond32_d(TFLG_NS);break;
	CASE_0F_D(0x8a)												/* JP */
		JumpCond32_d(TFLG_P);break;
	CASE_0F_D(0x8b)												/* JNP */
		JumpCond32_d(TFLG_NP);break;
	CASE_0F_D(0x8c)												/* JL */
		JumpCond32_d(TFLG_L);break;
	CASE_0F_D(0x8d)												/* JNL */
		JumpCond32_d(TFLG_NL);break;
	CASE_0F_D(0x8e)												/* JLE */
		JumpCond32_d(TFLG_LE);break;
	CASE_0F_D(0x8f)												/* JNLE */
		JumpCond32_d(TFLG_NLE);break;
	
	CASE_0F_D(0xa0)												/* PUSH FS */		
		Push_32(SegValue(fs));break;
	CASE_0F_D(0xa1)												/* POP FS */		
		if (CPU_PopSeg(fs,true)) RUNEXCEPTION();
		break;
	CASE_0F_D(0xa3)												/* BT Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=vPC_rLodsd(eaa);
				SETFLAGBIT(CF,(old & mask));
			}
			break;
		}
	CASE_0F_D(0xa4)												/* SHLD Ed,Gd,Ib */
		RMEdGdOp3(DSHLD,Fetchb());
		break;
	CASE_0F_D(0xa5)												/* SHLD Ed,Gd,CL */
		RMEdGdOp3(DSHLD,reg_cl);
		break;
	CASE_0F_D(0xa8)												/* PUSH GS */		
		Push_32(SegValue(gs));break;
	CASE_0F_D(0xa9)												/* POP GS */		
		if (CPU_PopSeg(gs,true)) RUNEXCEPTION();
		break;
	CASE_0F_D(0xab)												/* BTS Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
				*eard|=mask;
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=vPC_rLodsd(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMd(eaa,old | mask);
			}
			break;
		}
	
	CASE_0F_D(0xac)												/* SHRD Ed,Gd,Ib */
		RMEdGdOp3(DSHRD,Fetchb());
		break;
	CASE_0F_D(0xad)												/* SHRD Ed,Gd,CL */
		RMEdGdOp3(DSHRD,reg_cl);
		break;
	CASE_0F_D(0xaf)												/* IMUL Gd,Ed */
		{
			RMGdEdOp3(DIMULD,*rmrd);
			break;
		}
	CASE_0F_D(0xb1)												/* CMPXCHG Ed,Gd */
		goto illegal_opcode;
	CASE_0F_D(0xb2)												/* LSS Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(ss,vPC_rLodsw(eaa+4))) RUNEXCEPTION();
			*rmrd=vPC_rLodsd(eaa);
			break;
		}
	CASE_0F_D(0xb3)												/* BTR Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
				*eard&= ~mask;
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=vPC_rLodsd(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMd(eaa,old & ~mask);
			}
			break;
		}
	CASE_0F_D(0xb4)												/* LFS Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(fs,vPC_rLodsw(eaa+4))) RUNEXCEPTION();
			*rmrd=vPC_rLodsd(eaa);
			break;
		}
	CASE_0F_D(0xb5)												/* LGS Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0) goto illegal_opcode;
			GetEAa;
			if (CPU_SetSegGeneral(gs,vPC_rLodsw(eaa+4))) RUNEXCEPTION();
			*rmrd=vPC_rLodsd(eaa);
			break;
		}
	CASE_0F_D(0xb6)												/* MOVZX Gd,Eb */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArb;*rmrd=*earb;}
			else {GetEAa;*rmrd=vPC_rLodsb(eaa);}
			break;
		}
	CASE_0F_D(0xb7)												/* MOVXZ Gd,Ew */
		{
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArw;*rmrd=*earw;}
			else {GetEAa;*rmrd=vPC_rLodsw(eaa);}
			break;
		}
	CASE_0F_D(0xba)												/* GRP8 Ed,Ib */
		{
			FillFlags();GetRM;
			if (rm >= 0xc0 ) {
				GetEArd;
				Bit32u mask=1 << (Fetchb() & 31);
				SETFLAGBIT(CF,(*eard & mask));
				switch (rm & 0x38) {
				case 0x20:											/* BT */
					break;
				case 0x28:											/* BTS */
					*eard|=mask;
					break;
				case 0x30:											/* BTR */
					*eard&=~mask;
					break;
				case 0x38:											/* BTC */
					if (GETFLAG(CF)) *eard&=~mask;
					else *eard|=mask;
					break;
				default:
					E_Exit("CPU:66:0F:BA: Illegal subfunction %X",rm & 0x38);
				}
			} else {
				GetEAa;Bit32u old=vPC_rLodsd(eaa);
				Bit32u mask=1 << (Fetchb() & 31);
				SETFLAGBIT(CF,(old & mask));
				switch (rm & 0x38) {
				case 0x20:											/* BT */
					break;
				case 0x28:											/* BTS */
					SaveMd(eaa,old|mask);
					break;
				case 0x30:											/* BTR */
					SaveMd(eaa,old & ~mask);
					break;
				case 0x38:											/* BTC */
					if (GETFLAG(CF)) old&=~mask;
					else old|=mask;
					SaveMd(eaa,old);
					break;
				default:
					E_Exit("CPU:66:0F:BA: Illegal subfunction %X",rm & 0x38);
				}
			}
			break;
		}
	CASE_0F_D(0xbb)												/* BTC Ed,Gd */
		{
			FillFlags();GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
				*eard^=mask;
			} else {
				GetEAa;eaa+=(((Bit32s)*rmrd)>>5)*4;
				Bit32u old=vPC_rLodsd(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMd(eaa,old ^ mask);
			}
			break;
		}
	CASE_0F_D(0xbc)												/* BSF Gd,Ed */
		{
			GetRMrd;
			Bit32u result,value;
			if (rm >= 0xc0) { GetEArd; value=*eard; } 
			else			{ GetEAa; value=vPC_rLodsd(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 0;
				while ((value & 0x01)==0) { result++; value>>=1; }
				SETFLAGBIT(ZF,false);
				*rmrd = result;
			}
			lflags.type=t_UNKNOWN;
			break;
		}
	CASE_0F_D(0xbd)												/*  BSR Gd,Ed */
		{
			GetRMrd;
			Bit32u result,value;
			if (rm >= 0xc0) { GetEArd; value=*eard; } 
			else			{ GetEAa; value=vPC_rLodsd(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 31;	// Operandsize-1
				while ((value & 0x80000000)==0) { result--; value<<=1; }
				SETFLAGBIT(ZF,false);
				*rmrd = result;
			}
			lflags.type=t_UNKNOWN;
			break;
		}
	CASE_0F_D(0xbe)												/* MOVSX Gd,Eb */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArb;*rmrd=*(Bit8s *)earb;}
			else {GetEAa;*rmrd=LoadMbs(eaa);}
			break;
		}
	CASE_0F_D(0xbf)												/* MOVSX Gd,Ew */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArw;*rmrd=*(Bit16s *)earw;}
			else {GetEAa;*rmrd=LoadMws(eaa);}
			break;
		}
	CASE_0F_D(0xc1)												/* XADD Gd,Ed */
	CASE_0F_D(0xc8)												/* BSWAP EAX */
	CASE_0F_D(0xc9)												/* BSWAP ECX */
	CASE_0F_D(0xca)												/* BSWAP EDX */
	CASE_0F_D(0xcb)												/* BSWAP EBX */
	CASE_0F_D(0xcc)												/* BSWAP ESP */
	CASE_0F_D(0xcd)												/* BSWAP EBP */
	CASE_0F_D(0xce)												/* BSWAP ESI */
	CASE_0F_D(0xcf)												/* BSWAP EDI */
		goto illegal_opcode;
