	case 0x00:													// ADD Eb,Gb
		RMEbGb(ADDB);
		break;
	case 0x01:													// ADD Ew,Gw
		RMEwGw(ADDW);
		break;	
	case 0x02:													// ADD Gb,Eb
		RMGbEb(ADDB);
		break;
	case 0x03:													// ADD Gw,Ew
		RMGwEw(ADDW);
		break;
	case 0x04:													// ADD AL,Ib
		ALIb(ADDB);
		break;
	case 0x05:													// ADD AX,Iw
		AXIw(ADDW);
		break;
	case 0x06:													// PUSH ES		
		Push_16(SegValue(es));
		break;
	case 0x07:													// POP ES
		if (CPU_PopSeg(es, false))
			RUNEXCEPTION();
		break;
	case 0x08:													// OR Eb,Gb
		RMEbGb(ORB);
		break;
	case 0x09:													// OR Ew,Gw
		RMEwGw(ORW);
		break;
	case 0x0a:													// OR Gb,Eb
		RMGbEb(ORB);
		break;
	case 0x0b:													// OR Gw,Ew
		RMGwEw(ORW);
		break;
	case 0x0c:													// OR AL,Ib
		ALIb(ORB);
		break;
	case 0x0d:													// OR AX,Iw
		AXIw(ORW);
		break;
	case 0x0e:													// PUSH CS
		Push_16(SegValue(cs));
		break;
	case 0x0f:													// 2 byte opcodes
		core.opcode_index |= OPCODE_0F;
		goto restart_opcode;
	case 0x10:													// ADC Eb,Gb
		RMEbGb(ADCB);
		break;
	case 0x11:													// ADC Ew,Gw
		RMEwGw(ADCW);
		break;	
	case 0x12:													// ADC Gb,Eb
		RMGbEb(ADCB);
		break;
	case 0x13:													// ADC Gw,Ew
		RMGwEw(ADCW);
		break;
	case 0x14:													// ADC AL,Ib
		ALIb(ADCB);
		break;
	case 0x15:													// ADC AX,Iw
		AXIw(ADCW);
		break;
	case 0x16:													// PUSH SS
		Push_16(SegValue(ss));
		break;
	case 0x17:													// POP SS
		if (CPU_PopSeg(ss, false))
			RUNEXCEPTION();
		CPU_Cycles++;		// Always do another instruction
		break;
	case 0x18:													// SBB Eb,Gb
		RMEbGb(SBBB);
		break;
	case 0x19:													// SBB Ew,Gw
		RMEwGw(SBBW);
		break;
	case 0x1a:													// SBB Gb,Eb
		RMGbEb(SBBB);
		break;
	case 0x1b:													// SBB Gw,Ew
		RMGwEw(SBBW);
		break;
	case 0x1c:													// SBB AL,Ib
		ALIb(SBBB);
		break;
	case 0x1d:													// SBB AX,Iw
		AXIw(SBBW);
		break;
	case 0x1e:													// PUSH DS
		Push_16(SegValue(ds));
		break;
	case 0x1f:													// POP DS
		if (CPU_PopSeg(ds, false))
			RUNEXCEPTION();
		break;
	case 0x20:													// AND Eb,Gb
		RMEbGb(ANDB);
		break;
	case 0x21:													// AND Ew,Gw
		RMEwGw(ANDW);
		break;	
	case 0x22:													// AND Gb,Eb
		RMGbEb(ANDB);
		break;
	case 0x23:													// AND Gw,Ew
		RMGwEw(ANDW);
		break;
	case 0x24:													// AND AL,Ib
		ALIb(ANDB);
		break;
	case 0x25:													// AND AX,Iw
		AXIw(ANDW);
		break;
	case 0x26:													// SEG ES:
		DO_PREFIX_SEG(es);
		break;
	case 0x27:													// DAA
		DAA();
		break;
	case 0x28:													// SUB Eb,Gb
		RMEbGb(SUBB);
		break;
	case 0x29:													// SUB Ew,Gw
		RMEwGw(SUBW);
		break;
	case 0x2a:													// SUB Gb,Eb
		RMGbEb(SUBB);
		break;
	case 0x2b:													// SUB Gw,Ew
		RMGwEw(SUBW);
		break;
	case 0x2c:													// SUB AL,Ib
		ALIb(SUBB);
		break;
	case 0x2d:													// SUB AX,Iw
		AXIw(SUBW);
		break;
	case 0x2e:													// SEG CS:
		DO_PREFIX_SEG(cs);
		break;
	case 0x2f:													// DAS
		DAS();
		break;  
	case 0x30:													// XOR Eb,Gb
		RMEbGb(XORB);
		break;
	case 0x31:													// XOR Ew,Gw
		RMEwGw(XORW);
		break;	
	case 0x32:													// XOR Gb,Eb
		RMGbEb(XORB);
		break;
	case 0x33:													// XOR Gw,Ew
		RMGwEw(XORW);
		break;
	case 0x34:													// XOR AL,Ib
		ALIb(XORB);
		break;
	case 0x35:													// XOR AX,Iw
		AXIw(XORW);
		break;
	case 0x36:													// SEG SS:
		DO_PREFIX_SEG(ss);
		break;
	case 0x37:													// AAA
		AAA();
		break;  
	case 0x38:													// CMP Eb,Gb
		RMEbGb(CMPB);
		break;
	case 0x39:													// CMP Ew,Gw
		RMEwGw(CMPW);
		break;
	case 0x3a:													// CMP Gb,Eb
		RMGbEb(CMPB);
		break;
	case 0x3b:													// CMP Gw,Ew
		RMGwEw(CMPW);
		break;
	case 0x3c:													// CMP AL,Ib
		ALIb(CMPB);
		break;
	case 0x3d:													// CMP AX,Iw
		AXIw(CMPW);
		break;
	case 0x3e:													// SEG DS:
		DO_PREFIX_SEG(ds);
		break;
	case 0x3f:													// AAS
		AAS();
		break;
	case 0x40:													// INC AX
		INCW(reg_ax, LoadRw, SaveRw);
		break;
	case 0x41:													// INC CX
		INCW(reg_cx, LoadRw, SaveRw);
		break;
	case 0x42:													// INC DX
		INCW(reg_dx, LoadRw, SaveRw);
		break;
	case 0x43:													// INC BX
		INCW(reg_bx, LoadRw, SaveRw);
		break;
	case 0x44:													// INC SP
		INCW(reg_sp, LoadRw, SaveRw);
		break;
	case 0x45:													// INC BP
		INCW(reg_bp, LoadRw, SaveRw);
		break;
	case 0x46:													// INC SI
		INCW(reg_si, LoadRw, SaveRw);
		break;
	case 0x47:													// INC DI
		INCW(reg_di, LoadRw, SaveRw);
		break;
	case 0x48:													// DEC AX
		DECW(reg_ax, LoadRw, SaveRw);
		break;
	case 0x49:													// DEC CX
  		DECW(reg_cx, LoadRw, SaveRw);
		break;
	case 0x4a:													// DEC DX
		DECW(reg_dx, LoadRw, SaveRw);
		break;
	case 0x4b:													// DEC BX
		DECW(reg_bx, LoadRw, SaveRw);
		break;
	case 0x4c:													// DEC SP
		DECW(reg_sp, LoadRw, SaveRw);
		break;
	case 0x4d:													// DEC BP
		DECW(reg_bp, LoadRw, SaveRw);
		break;
	case 0x4e:													// DEC SI
		DECW(reg_si, LoadRw, SaveRw);
		break;
	case 0x4f:													// DEC DI
		DECW(reg_di, LoadRw, SaveRw);
		break;
	case 0x50:													// PUSH AX
		Push_16(reg_ax);
		break;
	case 0x51:													// PUSH CX
		Push_16(reg_cx);
		break;
	case 0x52:													// PUSH DX
		Push_16(reg_dx);
		break;
	case 0x53:													// PUSH BX
		Push_16(reg_bx);
		break;
	case 0x54:													// PUSH SP
		Push_16(reg_sp);
		break;
	case 0x55:													// PUSH BP
		Push_16(reg_bp);
		break;
	case 0x56:													// PUSH SI
		Push_16(reg_si);
		break;
	case 0x57:													// PUSH DI
		Push_16(reg_di);
		break;
	case 0x58:													// POP AX
		reg_ax = Pop_16();
		break;
	case 0x59:													// POP CX
		reg_cx = Pop_16();
		break;
	case 0x5a:													// POP DX
		reg_dx = Pop_16();
		break;
	case 0x5b:													// POP BX
		reg_bx = Pop_16();
		break;
	case 0x5c:													// POP SP
		reg_sp = Pop_16();
		break;
	case 0x5d:													// POP BP
		reg_bp = Pop_16();
		break;
	case 0x5e:													// POP SI
		reg_si = Pop_16();
		break;
	case 0x5f:													// POP DI
		reg_di = Pop_16();
		break;
	case 0x60:													// PUSHA
		{
		Bit16u old_sp = reg_sp;
		Push_16(reg_ax);
		Push_16(reg_cx);
		Push_16(reg_dx);
		Push_16(reg_bx);
		Push_16(old_sp);
		Push_16(reg_bp);
		Push_16(reg_si);
		Push_16(reg_di);
		}
		break;
	case 0x61:													// POPA
		reg_di = Pop_16();
		reg_si = Pop_16();
		reg_bp = Pop_16();
		Pop_16();			// Don't pop SP
		reg_bx = Pop_16();
		reg_dx = Pop_16();
		reg_cx = Pop_16();
		reg_ax = Pop_16();
		break;
	case 0x62:													// BOUND
		{
		Bit16s bound_min, bound_max;
		GetRMrw;
		GetEAa;
		bound_min = vPC_rLodsw(eaa);
		bound_max = vPC_rLodsw(eaa+2);
		if ((((Bit16s)*rmrw) < bound_min) || (((Bit16s)*rmrw) > bound_max))
			EXCEPTION(5);
		}
		break;
	case 0x63:													// ARPL Ew,Rw
		{
		if (!cpu.pmode)
			goto illegal_opcode;
		GetRMrw;
		if (rm >= 0xc0 )
			{
			GetEArw;
			Bitu new_sel = *earw;
			CPU_ARPL(new_sel, *rmrw);
			*earw = (Bit16u)new_sel;
			}
		else
			{
			GetEAa;
			Bitu new_sel = vPC_rLodsw(eaa);
			CPU_ARPL(new_sel, *rmrw);
			SaveMw(eaa, (Bit16u)new_sel);
			}
		}
		break;
	case 0x64:													// SEG FS:
		DO_PREFIX_SEG(fs);
		break;
	case 0x65:													// SEG GS:
		DO_PREFIX_SEG(gs);
		break;
	case 0x66:													// Operand Size Prefix
		core.opcode_index = (cpu.code.big^0x1)*0x200;
		goto restart_opcode;
	case 0x67:													// Address Size Prefix
		DO_PREFIX_ADDR();
	case 0x68:													// PUSH Iw
		Push_16(Fetchw());
		break;
	case 0x69:													// IMUL Gw,Ew,Iw
		RMGwEwOp3(DIMULW, Fetchws());
		break;
	case 0x6a:													// PUSH Ib
		Push_16(Fetchbs());
		break;
	case 0x6b:													// IMUL Gw,Ew,Ib
		RMGwEwOp3(DIMULW, Fetchbs());
		break;
	case 0x6c:													// INSB
		if (CPU_IO_Exception(reg_dx, 1))
			RUNEXCEPTION();
		DoString(R_INSB);
		break;
	case 0x6d:													// INSW
		if (CPU_IO_Exception(reg_dx, 2))
			RUNEXCEPTION();
		DoString(R_INSW);
		break;
	case 0x6e:													// OUTSB
		if (CPU_IO_Exception(reg_dx, 1))
			RUNEXCEPTION();
		DoString(R_OUTSB);
		break;
	case 0x6f:													// OUTSW
		if (CPU_IO_Exception(reg_dx, 2))
			RUNEXCEPTION();
		DoString(R_OUTSW);
		break;
	case 0x70:													// JO
		JumpCond16_b(TFLG_O);
	case 0x71:													// JNO
		JumpCond16_b(TFLG_NO);
	case 0x72:													// JB
		JumpCond16_b(TFLG_B);
	case 0x73:													// JNB
		JumpCond16_b(TFLG_NB);
	case 0x74:													// JZ
  		JumpCond16_b(TFLG_Z);
	case 0x75:													// JNZ
		JumpCond16_b(TFLG_NZ);
	case 0x76:													// JBE
		JumpCond16_b(TFLG_BE);
	case 0x77:													// JNBE
		JumpCond16_b(TFLG_NBE);
	case 0x78:													// JS
		JumpCond16_b(TFLG_S);
	case 0x79:													// JNS
		JumpCond16_b(TFLG_NS);
	case 0x7a:													// JP
		JumpCond16_b(TFLG_P);
	case 0x7b:													// JNP
		JumpCond16_b(TFLG_NP);
	case 0x7c:													// JL
		JumpCond16_b(TFLG_L);
	case 0x7d:													// JNL
		JumpCond16_b(TFLG_NL);
	case 0x7e:													// JLE
		JumpCond16_b(TFLG_LE);
	case 0x7f:													// JNLE
		JumpCond16_b(TFLG_NLE);
	case 0x80:													// Grpl Eb,Ib
	case 0x82:													// Grpl Eb,Ib Mirror instruction
		{
		GetRM;
		Bitu which = (rm>>3)&7;
		if (rm >= 0xc0)
			{
			GetEArb;
			Bit8u ib = Fetchb();
			switch (which)
				{
				case 0x00:ADDB(*earb,ib,LoadRb,SaveRb);break;
				case 0x01: ORB(*earb,ib,LoadRb,SaveRb);break;
				case 0x02:ADCB(*earb,ib,LoadRb,SaveRb);break;
				case 0x03:SBBB(*earb,ib,LoadRb,SaveRb);break;
				case 0x04:ANDB(*earb,ib,LoadRb,SaveRb);break;
				case 0x05:SUBB(*earb,ib,LoadRb,SaveRb);break;
				case 0x06:XORB(*earb,ib,LoadRb,SaveRb);break;
				case 0x07:CMPB(*earb,ib,LoadRb,SaveRb);break;
				}
			}
		else
			{
			GetEAa;
			Bit8u ib = Fetchb();
			switch (which)
				{
				case 0x00:ADDB(eaa,ib,vPC_rLodsb,SaveMb);break;
				case 0x01: ORB(eaa,ib,vPC_rLodsb,SaveMb);break;
				case 0x02:ADCB(eaa,ib,vPC_rLodsb,SaveMb);break;
				case 0x03:SBBB(eaa,ib,vPC_rLodsb,SaveMb);break;
				case 0x04:ANDB(eaa,ib,vPC_rLodsb,SaveMb);break;
				case 0x05:SUBB(eaa,ib,vPC_rLodsb,SaveMb);break;
				case 0x06:XORB(eaa,ib,vPC_rLodsb,SaveMb);break;
				case 0x07:CMPB(eaa,ib,vPC_rLodsb,SaveMb);break;
				}
			}
		break;
		}
	case 0x81:													// Grpl Ew,Iw
		{
		GetRM;
		Bitu which = (rm>>3)&7;
		if (rm >= 0xc0)
			{
			GetEArw;
			Bit16u iw = Fetchw();
			switch (which)
				{
				case 0x00:ADDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x01: ORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x02:ADCW(*earw,iw,LoadRw,SaveRw);break;
				case 0x03:SBBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x04:ANDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x05:SUBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x06:XORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x07:CMPW(*earw,iw,LoadRw,SaveRw);break;
				}
			}
		else
			{
			GetEAa;
			Bit16u iw = Fetchw();
			switch (which)
				{
				case 0x00:ADDW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x01: ORW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x02:ADCW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x03:SBBW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x04:ANDW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x05:SUBW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x06:XORW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x07:CMPW(eaa,iw,vPC_rLodsw,SaveMw);break;
				}
			}
		break;
		}
	case 0x83:													// Grpl Ew,Ix
		{
		GetRM;
		Bitu which = (rm>>3)&7;
		if (rm >= 0xc0)
			{
			GetEArw;
			Bit16u iw = (Bit16s)Fetchbs();
			switch (which)
				{
				case 0x00:ADDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x01: ORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x02:ADCW(*earw,iw,LoadRw,SaveRw);break;
				case 0x03:SBBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x04:ANDW(*earw,iw,LoadRw,SaveRw);break;
				case 0x05:SUBW(*earw,iw,LoadRw,SaveRw);break;
				case 0x06:XORW(*earw,iw,LoadRw,SaveRw);break;
				case 0x07:CMPW(*earw,iw,LoadRw,SaveRw);break;
				}
			}
		else
			{
			GetEAa;
			Bit16u iw = (Bit16s)Fetchbs();
			switch (which)
				{
				case 0x00:ADDW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x01: ORW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x02:ADCW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x03:SBBW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x04:ANDW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x05:SUBW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x06:XORW(eaa,iw,vPC_rLodsw,SaveMw);break;
				case 0x07:CMPW(eaa,iw,vPC_rLodsw,SaveMw);break;
				}
			}
		break;
		}
	case 0x84:													// TEST Eb,Gb
		RMEbGb(TESTB);
		break;
	case 0x85:													// TEST Ew,Gw
		RMEwGw(TESTW);
		break;
	case 0x86:													// XCHG Eb,Gb
		{	
		GetRMrb;
		Bit8u oldrmrb = *rmrb;
		if (rm >= 0xc0)
			{
			GetEArb;
			*rmrb = *earb;
			*earb = oldrmrb;
			}
		else
			{
			GetEAa;
			*rmrb = vPC_rLodsb(eaa);
			SaveMb(eaa, oldrmrb);
			}
		break;
		}
	case 0x87:													// XCHG Ew,Gw
		{	
		GetRMrw;
		Bit16u oldrmrw = *rmrw;
		if (rm >= 0xc0)
			{
			GetEArw;
			*rmrw = *earw;
			*earw = oldrmrw;
			}
		else
			{
			GetEAa;
			*rmrw = vPC_rLodsw(eaa);
			SaveMw(eaa, oldrmrw);
			}
		break;
		}
	case 0x88:													// MOV Eb,Gb
		{	
		GetRMrb;
		if (rm >= 0xc0 )
			{
			GetEArb;
			*earb = *rmrb;
			}
		else
			{
			if (cpu.pmode)
				{
				if ((rm == 0x05) && (!cpu.code.big))
					{
					Descriptor desc;
					cpu.gdt.GetDescriptor(SegValue(core.base_val_ds), desc);
					if ((desc.Type() == DESC_CODE_R_NC_A) || (desc.Type() == DESC_CODE_R_NC_NA))
						{
						CPU_Exception(EXCEPTION_GP, SegValue(core.base_val_ds) & 0xfffc);
						continue;
						}
					}
				}
			GetEAa;
			SaveMb(eaa, *rmrb);
			}
			break;
		}
	case 0x89:													// MOV Ew,Gw
		{	
		GetRMrw;
		if (rm >= 0xc0 )
			{
			GetEArw;
			*earw = *rmrw;
			}
		else
			{
			GetEAa;
			SaveMw(eaa, *rmrw);
			}
		break;
		}
	case 0x8a:													// MOV Gb,Eb
		{	
		GetRMrb;
		if (rm >= 0xc0)
			{
			GetEArb;
			*rmrb = *earb;
			}
		else
			{
			GetEAa;
			*rmrb = vPC_rLodsb(eaa);
			}
		break;
		}
	case 0x8b:													// MOV Gw,Ew
		{	
		GetRMrw;
		if (rm >= 0xc0)
			{
			GetEArw;
			*rmrw = *earw;
			}
		else
			{
			GetEAa;
			*rmrw = vPC_rLodsw(eaa);
			}
		break;
		}
	case 0x8c:													// Mov Ew,Sw
		{
		GetRM;
		Bit16u val;
		Bitu which = (rm>>3)&7;
		switch (which)
			{
		case 0x00:					/* MOV Ew,ES */
			val = SegValue(es); break;
		case 0x01:					/* MOV Ew,CS */
			val = SegValue(cs); break;
		case 0x02:					/* MOV Ew,SS */
			val = SegValue(ss); break;
		case 0x03:					/* MOV Ew,DS */
			val = SegValue(ds); break;
		case 0x04:					/* MOV Ew,FS */
			val = SegValue(fs); break;
		case 0x05:					/* MOV Ew,GS */
			val = SegValue(gs); break;
		default:
			LOG(LOG_CPU,LOG_ERROR)("CPU:8c:Illegal RM Byte");
			goto illegal_opcode;
			}
		if (rm >= 0xc0)
			{
			GetEArw;
			*earw = val;
			}
		else
			{
			GetEAa;
			SaveMw(eaa, val);
			}
		break;
		}
	case 0x8d:													// LEA Gw
		{
		// Little hack to always use segprefixed version
		BaseDS = BaseSS = 0;
		GetRMrw;
		if (TEST_PREFIX_ADDR)
			*rmrw = (Bit16u)(*EATable[256+rm])();
		else
			*rmrw = (Bit16u)(*EATable[rm])();
		break;
		}
	case 0x8e:													// MOV Sw,Ew
		{
		GetRM;
		Bit16u val;
		Bitu which = (rm>>3)&7;
		if (rm >= 0xc0)
			{
			GetEArw;
			val = *earw;
			}
		else
			{
			GetEAa;
			val = vPC_rLodsw(eaa);
			}
		switch (which)
			{
		case 0x02:					/* MOV SS,Ew */
			CPU_Cycles++;	// Always do another instruction
		case 0x00:					/* MOV ES,Ew */
		case 0x03:					/* MOV DS,Ew */
		case 0x05:					/* MOV GS,Ew */
		case 0x04:					/* MOV FS,Ew */
			if (CPU_SetSegGeneral((SegNames)which, val))
				RUNEXCEPTION();
			break;
		default:
			goto illegal_opcode;
			}
		break;
		}							
	case 0x8f:													// POP Ew
		{
		Bit16u val = Pop_16();
		GetRM;
		if (rm >= 0xc0)
			{
			GetEArw;
			*earw = val;
			}
		else
			{
			GetEAa;
			SaveMw(eaa, val);
			}
		break;
		}
	case 0x90:													// NOP
		break;
	case 0x91:													// XCHG CX,AX
		{
		Bit16u temp = reg_ax;
		reg_ax = reg_cx;
		reg_cx = temp;
		}
		break;
	case 0x92:													// XCHG DX,AX
		{
		Bit16u temp = reg_ax;
		reg_ax = reg_dx;
		reg_dx = temp;
		}
		break;
	case 0x93:													// XCHG BX,AX
		{
		Bit16u temp = reg_ax;
		reg_ax = reg_bx;
		reg_bx = temp;
		}
		break;
	case 0x94:													// XCHG SP,AX
		{
		Bit16u temp = reg_ax;
		reg_ax = reg_sp;
		reg_sp = temp;
		}
		break;
	case 0x95:													// XCHG BP,AX
		{
		Bit16u temp = reg_ax;
		reg_ax = reg_bp;
		reg_bp = temp;
		}
		break;
	case 0x96:													// XCHG SI,AX
		{
		Bit16u temp = reg_ax;
		reg_ax = reg_si;
		reg_si = temp;
		}
		break;
	case 0x97:													// XCHG DI,AX
		{
		Bit16u temp = reg_ax;
		reg_ax = reg_di;
		reg_di = temp;
		}
		break;
	case 0x98:													// CBW
		reg_ax = (Bit8s)reg_al;
		break;
	case 0x99:													// CWD
		if (reg_ax & 0x8000)
			reg_dx = 0xffff;
		else
			reg_dx = 0;
		break;
	case 0x9a:													// CALL Ap
		{ 
		FillFlags();
		Bit16u newip = Fetchw();
		Bit16u newcs = Fetchw();
		CPU_CALL(false, newcs, newip, GETIP);
#if CPU_TRAP_CHECK
		if (GETFLAG(TF))
			{	
			cpudecoder = CPU_Core_Normal_Trap_Run;
			return CBRET_NONE;
			}
#endif
		continue;
		}
	case 0x9b:													// WAIT
		break;		// No waiting here
	case 0x9c:													// PUSHF
		CPU_PUSHF(false);
		break;
	case 0x9d:													// POPF
		CPU_POPF(false);
#if CPU_TRAP_CHECK
		if (GETFLAG(TF))
			{	
			cpudecoder = CPU_Core_Normal_Trap_Run;
			goto decode_end;
			}
#endif
#if	CPU_PIC_CHECK
		if (GETFLAG(IF) && PIC_IRQCheck)
			goto decode_end;
#endif
		break;
	case 0x9e:													// SAHF
		SETFLAGSb(reg_ah);
		break;
	case 0x9f:													// LAHF
		FillFlags();
		reg_ah = reg_flags&0xff;
		break;
	case 0xa0:													// MOV AL,Ob
		{
		GetEADirect;
		reg_al = vPC_rLodsb(eaa);
		}
		break;
	case 0xa1:													// MOV AX,Ow
		{
		GetEADirect;
		reg_ax = vPC_rLodsw(eaa);
		}
		break;
	case 0xa2:													// MOV Ob,AL
		{
		GetEADirect;
		SaveMb(eaa, reg_al);
		}
		break;
	case 0xa3:													// MOV Ow,AX
		{
		GetEADirect;
		SaveMw(eaa, reg_ax);
		}
		break;
	case 0xa4:													// MOVSB
		DoString(R_MOVSB);
		break;
	case 0xa5:													// MOVSW
		DoString(R_MOVSW);
		break;
	case 0xa6:													// CMPSB
		DoString(R_CMPSB);
		break;
	case 0xa7:													// CMPSW
		DoString(R_CMPSW);
		break;
	case 0xa8:													// TEST AL,Ib
		ALIb(TESTB);
		break;
	case 0xa9:													// TEST AX,Iw
		AXIw(TESTW);
		break;
	case 0xaa:													// STOSB
		DoString(R_STOSB);
		break;
	case 0xab:													// STOSW
		DoString(R_STOSW);
		break;
	case 0xac:													// LODSB
		DoString(R_LODSB);
		break;
	case 0xad:													// LODSW
		DoString(R_LODSW);
		break;
	case 0xae:													// SCASB
		DoString(R_SCASB);
		break;
	case 0xaf:													// SCASW
		DoString(R_SCASW);
		break;
	case 0xb0:													// MOV AL,Ib
		reg_al = Fetchb();
		break;
	case 0xb1:													// MOV CL,Ib
		reg_cl = Fetchb();
		break;
	case 0xb2:													// MOV DL,Ib
		reg_dl = Fetchb();
		break;
	case 0xb3:													// MOV BL,Ib
		reg_bl = Fetchb();
		break;
	case 0xb4:													// MOV AH,Ib
		reg_ah = Fetchb();
		break;
	case 0xb5:													// MOV CH,Ib
		reg_ch = Fetchb();
		break;
	case 0xb6:													// MOV DH,Ib
		reg_dh = Fetchb();
		break;
	case 0xb7:													// MOV BH,Ib
		reg_bh = Fetchb();
		break;
	case 0xb8:													// MOV AX,Iw
		reg_ax = Fetchw();
		break;
	case 0xb9:													// MOV CX,Iw
		reg_cx = Fetchw();
		break;
	case 0xba:													// MOV DX,Iw
		reg_dx = Fetchw();
		break;
	case 0xbb:													// MOV BX,Iw
		reg_bx = Fetchw();
		break;
	case 0xbc:													// MOV SP,Iw
		reg_sp = Fetchw();
		break;
	case 0xbd:													// MOV BP.Iw
		reg_bp = Fetchw();
		break;
	case 0xbe:													// MOV SI,Iw
		reg_si = Fetchw();
		break;
	case 0xbf:													// MOV DI,Iw
		reg_di = Fetchw();
		break;
	case 0xc0:													// GRP2 Eb,Ib
		GRP2B(Fetchb());
		break;
	case 0xc1:													// GRP2 Ew,Ib
		GRP2W(Fetchb());
		break;
	case 0xc2:													// RETN Iw
		reg_eip = Pop_16();
		reg_esp += Fetchw();
		continue;
	case 0xc3:													// RETN
		reg_eip = Pop_16();
		continue;
	case 0xc4:													// LES
		{	
		GetRMrw;
		if (rm >= 0xc0)
			goto illegal_opcode;
		GetEAa;
		if (CPU_SetSegGeneral(es, vPC_rLodsw(eaa+2)))
			RUNEXCEPTION();
		*rmrw = vPC_rLodsw(eaa);
		break;
		}
	case 0xc5:													// LDS
		{	
		GetRMrw;
		if (rm >= 0xc0)
			goto illegal_opcode;
		GetEAa;
		if (CPU_SetSegGeneral(ds, vPC_rLodsw(eaa+2)))
			RUNEXCEPTION();
		*rmrw = vPC_rLodsw(eaa);
		break;
		}
	case 0xc6:													// MOV Eb,Ib
		{
		GetRM;
		if (rm >= 0xc0)
			{
			GetEArb;
			*earb = Fetchb();
			}
		else
			{
			GetEAa;
			SaveMb(eaa, Fetchb());
			}
		break;
		}
	case 0xc7:													// MOV EW,Iw
		{
		GetRM;
		if (rm >= 0xc0)
			{
			GetEArw;
			*earw = Fetchw();
			}
		else
			{
			GetEAa;
			SaveMw(eaa, Fetchw());
			}
		break;
		}
	case 0xc8:													// ENTER Iw,Ib
		{
		Bitu bytes = Fetchw();
		Bitu level = Fetchb();
		CPU_ENTER(false, bytes, level);
		}
		break;
	case 0xc9:													// LEAVE
		reg_esp &= cpu.stack.notmask;
		reg_esp |= (reg_ebp&cpu.stack.mask);
		reg_bp = Pop_16();
		break;
	case 0xca:													// RETF Iw
		{
		Bitu words = Fetchw();
		FillFlags();
		CPU_RET(false, words, GETIP);
		continue;
		}
	case 0xcb:													// RETF
		FillFlags();
		CPU_RET(false, 0, GETIP);
		continue;
	case 0xcc:													// INT3
		CPU_SW_Interrupt_NoIOPLCheck(3, GETIP);
#if CPU_TRAP_CHECK
		cpu.trap_skip = true;
#endif
		continue;
	case 0xcd:													// INT Ib
		{
		Bit8u num = Fetchb();
		CPU_SW_Interrupt(num, GETIP);
#if CPU_TRAP_CHECK
		cpu.trap_skip = true;
#endif
		continue;
		}
	case 0xce:													// INTO
		if (get_OF())
			{
			CPU_SW_Interrupt(4, GETIP);
#if CPU_TRAP_CHECK
			cpu.trap_skip = true;
#endif
			continue;
			}
		break;
	case 0xcf:													// IRET
		{
		CPU_IRET(false, GETIP);
#if CPU_TRAP_CHECK
		if (GETFLAG(TF))
			{	
			cpudecoder = CPU_Core_Normal_Trap_Run;
			return CBRET_NONE;
			}
#endif
#if CPU_PIC_CHECK
		if (GETFLAG(IF) && PIC_IRQCheck)
			return CBRET_NONE;
#endif
		continue;
		}
	case 0xd0:													// GRP2 Eb,1
		GRP2B(1);
		break;
	case 0xd1:													// GRP2 Ew,1
		GRP2W(1);
		break;
	case 0xd2:													// GRP2 Eb,CL
		GRP2B(reg_cl);
		break;
	case 0xd3:													// GRP2 Ew,CL
		GRP2W(reg_cl);
		break;
	case 0xd4:													// AAM Ib
		AAM(Fetchb());
		break;
	case 0xd5:													// AAD Ib
		AAD(Fetchb());
		break;
	case 0xd6:													// SALC
		reg_al = get_CF() ? 0xFF : 0;
		break;
	case 0xd7:													// XLAT
		if (TEST_PREFIX_ADDR)
			reg_al = vPC_rLodsb(BaseDS+(Bit32u)(reg_ebx+reg_al));
		else
			reg_al = vPC_rLodsb(BaseDS+(Bit16u)(reg_bx+reg_al));
		break;
	case 0xd8:													// FPU ESC 0
	case 0xd9:													// FPU ESC 1
	case 0xda:													// FPU ESC 2
	case 0xdb:													// FPU ESC 3
	case 0xdc:													// FPU ESC 4
	case 0xdd:													// FPU ESC 5
	case 0xde:													// FPU ESC 6
	case 0xdf:													// FPU ESC 7
		{
		Bit8u rm = Fetchb();
		if (rm < 0xc0)
			GetEAa;
		}
		break;
	case 0xe0:													// LOOPNZ
		SAVEIP;
		if ((TEST_PREFIX_ADDR ? --reg_ecx : --reg_cx) && !get_ZF())
			reg_ip += Fetchbs();
		reg_ip += 1;
		continue;
//		if (TEST_PREFIX_ADDR)
//			{
//			JumpCond16_b(--reg_ecx && !get_ZF());
//			}
//		else
//			{
//			JumpCond16_b(--reg_cx && !get_ZF());
//			}
//		break;
	case 0xe1:													// LOOPZ
		SAVEIP;
		if ((TEST_PREFIX_ADDR ? --reg_ecx : --reg_cx) && get_ZF())
			reg_ip += Fetchbs();
		reg_ip += 1;
		continue;
//		if (TEST_PREFIX_ADDR)
//			{
//			JumpCond16_b(--reg_ecx && get_ZF());
//			}
//		else
//			{
//			JumpCond16_b(--reg_cx && get_ZF());
//			}
//		break;
	case 0xe2:													// LOOP
		SAVEIP;
		if ((TEST_PREFIX_ADDR ? --reg_ecx : --reg_cx))
			reg_ip += Fetchbs();
		reg_ip += 1;
		continue;
//		if (TEST_PREFIX_ADDR)
//			{
//			JumpCond16_b(--reg_ecx);
//			}
//		else
//			{
//			JumpCond16_b(--reg_cx);
//			}
//		break;
	case 0xe3:													// JCXZ
		JumpCond16_b(!(reg_ecx & AddrMaskTable[core.prefixes & PREFIX_ADDR]));
	case 0xe4:													// IN AL,Ib
		{	
		Bitu port = Fetchb();
		if (CPU_IO_Exception(port, 1))
			RUNEXCEPTION();
		reg_al = IO_ReadB(port);
		break;
		}
	case 0xe5:													// IN AX,Ib
		{	
		Bitu port = Fetchb();
		if (CPU_IO_Exception(port, 2))
			RUNEXCEPTION();
		reg_al = IO_ReadW(port);
		break;
		}
	case 0xe6:													// OUT Ib,AL
		{
		Bitu port = Fetchb();
		if (CPU_IO_Exception(port, 1))
			RUNEXCEPTION();
		IO_WriteB(port, reg_al);
		break;
		}		
	case 0xe7:													// OUT Ib,AX
		{
		Bitu port = Fetchb();
		if (CPU_IO_Exception(port, 2))
			RUNEXCEPTION();
		IO_WriteW(port, reg_ax);
		break;
		}
	case 0xe8:													// CALL Jw
		{ 
		Bit16u addip = Fetchws();
		SAVEIP;
		Push_16(reg_eip);
		reg_eip = (Bit16u)(reg_eip+addip);
		continue;
		}
	case 0xe9:													// JMP Jw
		{ 
		Bit16u addip = Fetchws();
		SAVEIP;
		reg_eip = (Bit16u)(reg_eip+addip);
		continue;
		}
	case 0xea:													// JMP Ap
		{ 
		Bit16u newip = Fetchw();
		Bit16u newcs = Fetchw();
		FillFlags();
		CPU_JMP(false, newcs, newip, GETIP);
#if CPU_TRAP_CHECK
		if (GETFLAG(TF))
			{	
			cpudecoder = CPU_Core_Normal_Trap_Run;
			return CBRET_NONE;
			}
#endif
		continue;
		}
	case 0xeb:													// JMP Jb
		{ 
		Bit16s addip = Fetchbs();
		SAVEIP;
		reg_eip = (Bit16u)(reg_eip+addip);
		continue;
		}
	case 0xec:													// IN AL,DX
		if (CPU_IO_Exception(reg_dx, 1))
			RUNEXCEPTION();
		reg_al = IO_ReadB(reg_dx);
		break;
	case 0xed:													// IN AX,DX
		if (CPU_IO_Exception(reg_dx, 2))
			RUNEXCEPTION();
		reg_ax = IO_ReadW(reg_dx);
		break;
	case 0xee:													// OUT DX,AL
		if (CPU_IO_Exception(reg_dx,1)) RUNEXCEPTION();
		IO_WriteB(reg_dx,reg_al);
		break;
	case 0xef:													// OUT DX,AX
		if (CPU_IO_Exception(reg_dx,2)) RUNEXCEPTION();
		IO_WriteW(reg_dx,reg_ax);
		break;
	case 0xf0:													// LOCK
		break;						// FIXME: see case D_LOCK in core_full/load.h
	case 0xf1:													// ICEBP
		CPU_SW_Interrupt_NoIOPLCheck(1, GETIP);
#if CPU_TRAP_CHECK
		cpu.trap_skip = true;
#endif
		continue;
	case 0xf2:													// REPNZ
		DO_PREFIX_REP(false);	
		break;		
	case 0xf3:													// REPZ
		DO_PREFIX_REP(true);	
		break;		
	case 0xf4:													// HLT
		if (cpu.pmode && cpu.cpl)
			EXCEPTION(EXCEPTION_GP);
		FillFlags();
		CPU_HLT(GETIP);
		return CBRET_NONE;		// Needs to return for hlt cpu core
	case 0xf5:													// CMC
		FillFlags();
		SETFLAGBIT(CF, !(reg_flags & FLAG_CF));
		break;
	case 0xf6:													// GRP3 Eb(,Ib)
		{	
		GetRM;
		Bitu which = (rm>>3)&7;
		switch (which)
			{
		case 0x00:											/* TEST Eb,Ib */
		case 0x01:											/* TEST Eb,Ib Undocumented*/
			{
			if (rm >= 0xc0)
				{
				GetEArb;
				TESTB(*earb, Fetchb(), LoadRb, 0)
				}
			else
				{
				GetEAa;
				TESTB(eaa, Fetchb(), vPC_rLodsb, 0);
				}
			break;
			}
		case 0x02:											/* NOT Eb */
			{
			if (rm >= 0xc0)
				{
				GetEArb;
				*earb = ~*earb;
				}
			else
				{
				GetEAa;
				SaveMb(eaa, ~vPC_rLodsb(eaa));
				}
			break;
			}
		case 0x03:											/* NEG Eb */
			{
			lflags.type = t_NEGb;
			if (rm >= 0xc0)
				{
				GetEArb;
				lf_var1b = *earb;
				lf_resb = 0-lf_var1b;
				*earb = lf_resb;
				}
			else
				{
				GetEAa;
				lf_var1b = vPC_rLodsb(eaa);
				lf_resb = 0-lf_var1b;
 				SaveMb(eaa, lf_resb);
				}
			break;
			}
		case 0x04:											/* MUL AL,Eb */
			RMEb(MULB);
			break;
		case 0x05:											/* IMUL AL,Eb */
			RMEb(IMULB);
			break;
		case 0x06:											/* DIV Eb */
			RMEb(DIVB);
			break;
		case 0x07:											/* IDIV Eb */
			RMEb(IDIVB);
			break;
			}
		break;
		}
	case 0xf7:													// GRP3 Ew(,Iw)
		{ 
		GetRM;
		Bitu which = (rm>>3)&7;
		switch (which)
			{
		case 0x00:											/* TEST Ew,Iw */
		case 0x01:											/* TEST Ew,Iw Undocumented*/
			{
			if (rm >= 0xc0)
				{
				GetEArw;
				TESTW(*earw, Fetchw(), LoadRw, SaveRw);
				}
			else
				{
				GetEAa;
				TESTW(eaa, Fetchw(), vPC_rLodsw, SaveMw);
				}
			break;
			}
		case 0x02:											/* NOT Ew */
			{
			if (rm >= 0xc0)
				{
				GetEArw;
				*earw = ~*earw;
				}
			else
				{
				GetEAa;
				SaveMw(eaa, ~vPC_rLodsw(eaa));
				}
			break;
			}
		case 0x03:											/* NEG Ew */
			{
			lflags.type = t_NEGw;
			if (rm >= 0xc0)
				{
				GetEArw;
				lf_var1w = *earw;
				lf_resw = 0-lf_var1w;
				*earw = lf_resw;
				}
			else
				{
				GetEAa;
				lf_var1w = vPC_rLodsw(eaa);
				lf_resw = 0-lf_var1w;
 				SaveMw(eaa, lf_resw);
				}
			break;
			}
		case 0x04:											/* MUL AX,Ew */
			RMEw(MULW);
			break;
		case 0x05:											/* IMUL AX,Ew */
			RMEw(IMULW)
			break;
		case 0x06:											/* DIV Ew */
			RMEw(DIVW)
			break;
		case 0x07:											/* IDIV Ew */
			RMEw(IDIVW)
			break;
			}
		break;
		}
	case 0xf8:													// CLC
		FillFlags();
		SETFLAGBIT(CF, false);
		break;
	case 0xf9:													// STC
		FillFlags();
		SETFLAGBIT(CF, true);
		break;
	case 0xfa:													// CLI
		if (CPU_CLI())
			RUNEXCEPTION();
		break;
	case 0xfb:													// STI
		if (CPU_STI())
			RUNEXCEPTION();
#if CPU_PIC_CHECK
		if (GETFLAG(IF) && PIC_IRQCheck)
			goto decode_end;
#endif
		break;
	case 0xfc:													// CLD
		SETFLAGBIT(DF, false);
		cpu.direction = 1;
		break;
	case 0xfd:													// STD
		SETFLAGBIT(DF, true);
		cpu.direction = -1;
		break;
	case 0xfe:													// GRP4 Eb
		{
		GetRM;
		Bitu which = (rm>>3)&7;
		switch (which)
			{
		case 0x00:										/* INC Eb */
			RMEb(INCB);
			break;		
		case 0x01:										/* DEC Eb */
			RMEb(DECB);
			break;
		case 0x07:										/* CallBack */
			{
			Bitu cb = Fetchw();
			FillFlags();
			SAVEIP;
			return cb;
			}
		default:
			E_Exit("Illegal GRP4 Call %d",(rm>>3) & 7);
			break;
			}
		break;
		}
	case 0xff:													// GRP5 Ew
		{
		GetRM;
		Bitu which = (rm>>3)&7;
		switch (which)
			{
		case 0x00:										/* INC Ew */
			RMEw(INCW);
			break;		
		case 0x01:										/* DEC Ew */
			RMEw(DECW);
			break;		
		case 0x02:										/* CALL Ev */
			if (rm >= 0xc0)
				{
				GetEArw;
				reg_eip = *earw;
				}
			else
				{
				GetEAa;
				reg_eip = vPC_rLodsw(eaa);
				}
			Push_16(GETIP);
			continue;
		case 0x03:										/* CALL Ep */
			{
			if (rm >= 0xc0)
				goto illegal_opcode;
			GetEAa;
			Bit16u newip = vPC_rLodsw(eaa);
			Bit16u newcs = vPC_rLodsw(eaa+2);
			FillFlags();
			CPU_CALL(false, newcs, newip, GETIP);
#if CPU_TRAP_CHECK
			if (GETFLAG(TF))
				{	
				cpudecoder = CPU_Core_Normal_Trap_Run;
				return CBRET_NONE;
				}
#endif
			continue;
			}
		case 0x04:										/* JMP Ev */	
			if (rm >= 0xc0)
				{
				GetEArw;
				reg_eip = *earw;
				}
			else
				{
				GetEAa;
				reg_eip = vPC_rLodsw(eaa);
				}
			continue;
		case 0x05:										/* JMP Ep */	
			{
			if (rm >= 0xc0)
				goto illegal_opcode;
			GetEAa;
			Bit16u newip = vPC_rLodsw(eaa);
			Bit16u newcs = vPC_rLodsw(eaa+2);
			FillFlags();
			CPU_JMP(false, newcs, newip, GETIP);
#if CPU_TRAP_CHECK
			if (GETFLAG(TF))
				{	
				cpudecoder = CPU_Core_Normal_Trap_Run;
				return CBRET_NONE;
				}
#endif
			continue;
			}
		case 0x06:										/* PUSH Ev */
			if (rm >= 0xc0)
				{
				GetEArw;
				Push_16(*earw);
				}
			else
				{
				GetEAa;
				Push_16(vPC_rLodsw(eaa));
				}
			break;
		default:
			LOG(LOG_CPU,LOG_ERROR)("CPU:GRP5:Illegal Call %2X", which);
			goto illegal_opcode;
			}
		break;
		}
