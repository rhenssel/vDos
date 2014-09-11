#define GetEAa	PhysPt eaa = EALookupTable[rm]();

#define GetRMEAa															\
	GetRM;																	\
	GetEAa;											


#define RMEbGb(inst)														\
	{																		\
	GetRMrb;																\
	if (rm >= 0xc0 ) {GetEArb; inst(*earb, *rmrb, LoadRb, SaveRb);}			\
	else {GetEAa;inst(eaa,*rmrb,vPC_rLodsb,SaveMb);}						\
	}

#define RMGbEb(inst)														\
	{																		\
	GetRMrb;																\
	if (rm >= 0xc0 ) {GetEArb; inst(*rmrb, *earb, LoadRb, SaveRb);}			\
	else {GetEAa; inst(*rmrb, vPC_rLodsb(eaa), LoadRb, SaveRb);}			\
	}

#define RMEb(inst)															\
	{																		\
	if (rm >= 0xc0 ) {GetEArb; inst(*earb, LoadRb, SaveRb);}				\
	else {GetEAa; inst(eaa, vPC_rLodsb, SaveMb);}							\
	}

#define RMEwGw(inst)														\
	{																		\
	GetRMrw;																\
	if (rm >= 0xc0 ) {GetEArw; inst(*earw, *rmrw, LoadRw, SaveRw);}			\
	else {GetEAa; inst(eaa, *rmrw, vPC_rLodsw, SaveMw);}					\
	}

#define RMEwGwOp3(inst, op3)												\
	{																		\
	GetRMrw;																\
	if (rm >= 0xc0 ) {GetEArw; inst(*earw, *rmrw, op3, LoadRw, SaveRw);}	\
	else {GetEAa; inst(eaa, *rmrw, op3, vPC_rLodsw, SaveMw);}				\
	}

#define RMGwEw(inst)														\
	{																		\
	GetRMrw;																\
	if (rm >= 0xc0 ) {GetEArw; inst(*rmrw, *earw, LoadRw, SaveRw);}			\
	else {GetEAa; inst(*rmrw, vPC_rLodsw(eaa), LoadRw, SaveRw);}			\
	}																

#define RMGwEwOp3(inst, op3)												\
	{																		\
	GetRMrw;																\
	if (rm >= 0xc0 ) {GetEArw; inst(*rmrw, *earw, op3, LoadRw, SaveRw);}	\
	else {GetEAa; inst(*rmrw, vPC_rLodsw(eaa), op3, LoadRw, SaveRw);}		\
	}																

#define RMEw(inst)															\
	{																		\
	if (rm >= 0xc0 ) {GetEArw; inst(*earw, LoadRw, SaveRw);}				\
	else {GetEAa; inst(eaa, vPC_rLodsw, SaveMw);}							\
	}

#define RMEdGd(inst)														\
	{																		\
	GetRMrd;																\
	if (rm >= 0xc0 ) {GetEArd; inst(*eard, *rmrd, LoadRd, SaveRd);}			\
	else {GetEAa; inst(eaa, *rmrd, vPC_rLodsd, SaveMd);}					\
	}

#define RMEdGdOp3(inst,op3)													\
	{																		\
	GetRMrd;																\
	if (rm >= 0xc0 ) {GetEArd; inst(*eard, *rmrd, op3, LoadRd, SaveRd);}	\
	else {GetEAa; inst(eaa, *rmrd, op3, vPC_rLodsd, SaveMd);}				\
	}


#define RMGdEd(inst)														\
	{																		\
	GetRMrd;																\
	if (rm >= 0xc0 ) {GetEArd; inst(*rmrd, *eard, LoadRd, SaveRd);}			\
	else {GetEAa; inst(*rmrd, vPC_rLodsd(eaa), LoadRd, SaveRd);}			\
	}																

#define RMGdEdOp3(inst, op3)												\
	{																		\
	GetRMrd;																\
	if (rm >= 0xc0 ) {GetEArd; inst(*rmrd, *eard, op3, LoadRd, SaveRd);}	\
	else {GetEAa; inst(*rmrd, vPC_rLodsd(eaa), op3, LoadRd, SaveRd);}		\
	}																

#define RMEw(inst)															\
	{																		\
	if (rm >= 0xc0 ) {GetEArw; inst(*earw, LoadRw, SaveRw);}				\
	else {GetEAa; inst(eaa, vPC_rLodsw, SaveMw);}							\
	}

#define RMEd(inst)															\
	{																		\
	if (rm >= 0xc0 ) {GetEArd; inst(*eard, LoadRd, SaveRd);}				\
	else {GetEAa;inst(eaa, vPC_rLodsd, SaveMd);}							\
	}

#define ALIb(inst)															\
	{ inst(reg_al, Fetchb(), LoadRb, SaveRb)}

#define AXIw(inst)															\
	{ inst(reg_ax, Fetchw(), LoadRw, SaveRw);}

#define EAXId(inst)															\
	{ inst(reg_eax, Fetchd(), LoadRd, SaveRd);}

#define CASE_W(_WHICH)														\
	case (OPCODE_NONE+_WHICH):

#define CASE_D(_WHICH)														\
	case (OPCODE_SIZE+_WHICH):

#define CASE_B(_WHICH)														\
	CASE_W(_WHICH)															\
	CASE_D(_WHICH)

#define CASE_0F_W(_WHICH)													\
	case ((OPCODE_0F|OPCODE_NONE)+_WHICH):

#define CASE_0F_D(_WHICH)													\
	case ((OPCODE_0F|OPCODE_SIZE)+_WHICH):

#define CASE_0F_B(_WHICH)													\
	CASE_0F_W(_WHICH)														\
	CASE_0F_D(_WHICH)
