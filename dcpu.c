
#include "dcpu.h"
#include "opcode.h"
#include "cputypes.h"
#include <stdio.h>

#define DCPUMODE_SKIP 1
#define DCPUMODE_INTQ 2
#define DCPUMODE_PROCINT 4
#define DCPUMODE_EXTINT 8

static inline void DCPU_reref(int, uint16_t, DCPU*, uint16_t*);
static inline void DCPU_rerefB(int, uint16_t, DCPU*, uint16_t*);
static inline uint16_t DCPU_deref(int , DCPU* , uint16_t * );
static inline uint16_t DCPU_derefB(int , DCPU* , uint16_t * );
static int DCPU_setonfire(DCPU * );
static inline void DCPU_skipref(int , DCPU* );
static uint16_t LPC;

void DCPU_init(struct isiCPUInfo *info, uint16_t *ram)
{
	DCPU* pr = (DCPU*)info->cpustate;
	info->archtype = ARCH_DCPU;
	info->RunCycles = DCPU_run;
	memset(pr, 0, sizeof(DCPU));
	pr->memptr = ram;
}

void DCPU_reset(DCPU* pr)
{
	int i;
	for(i = 0; i < 8; i++)
	{
		pr->R[i] = 0;
	}
	pr->PC = 0;
	pr->EX = 0;
	pr->SP = 0;
	pr->IA = 0;
	pr->MODE = 0;
	pr->cycl = 0;
	pr->IQC = 0;
}

int DCPU_interupt(DCPU* pr, uint16_t msg)
{
	if(pr->IA) {
		if(pr->IQC < 256) {
			pr->IQU[pr->IQC++] = msg;
			return 1;
		} else {
			return DCPU_setonfire(pr);
		}
	} else {
		return 0;
	}
}

static const int DCPU_cycles1[] =
{
	0, 1, 2, 2, 2, 2, 3, 3,
	3, 3, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 2, 2, 2, 2
};

static const int DCPU_cycles2[] =
{
	0, 3, 0, 0, 0, 0, 0, 0,
	4, 1, 1, 3, 2, 0, 0, 0,
	2, 4, 4, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static const int DCPU_dtbl[] =
{
	0, 3, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,
	0, 0, 1, 1, 0, 0, 3, 3
};
static const int DCPU_dwtbl[] =
{
	0, 0, 1, 1, 1, 1, 2, 2,
	3, 3, 3, 3, 3, 2, 2, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 1, 0, 0, 0, 0
};

int DCPU_run(struct isiCPUInfo * l_info, struct timespec crun)
{
	DCPU *pr = (DCPU*)l_info->cpustate;
	uint16_t *ram = pr->memptr;
	size_t cycl = 0;
	int op;
	int oa;
	int ob;
	union {
		uint16_t u;
		int16_t s;
	} alu1;
	union {
		uint16_t u;
		uint32_t ui;
		int32_t si;
		int16_t s;
	} alu2;
	
	uintptr_t ccq = 0;
	if(l_info->ctl & ISICTL_STEP) {
		if(l_info->ctl & ISICTL_STEPE) {
			ccq = 1;
			l_info->ctl &= ~ISICTL_STEPE;
		} else {
			l_info->nrun.tv_sec = crun.tv_sec;
			l_info->nrun.tv_nsec = crun.tv_nsec;
			isi_addtime(&l_info->nrun, l_info->runrate);
		}
	} else {
		ccq = l_info->rate;
	}
	while(ccq && (l_info->nrun.tv_sec < crun.tv_sec || l_info->nrun.tv_nsec < crun.tv_nsec)) {

	if(pr->MODE == BURNING) {
		cycl += 3;
	}

	if((pr->MODE & DCPUMODE_EXTINT)) {
		pr->MODE ^= DCPUMODE_EXTINT;
	} else {
		op = ram[(uint16_t)(pr->PC++)];
		oa = (op >> 10) & 0x003f;
		ob = (op >> 5) & 0x001f;
		op &= 0x001f;
		if(!op) {
			if(pr->MODE & DCPUMODE_SKIP) {
				DCPU_skipref(oa, pr);
				pr->MODE ^= DCPUMODE_SKIP;
				cycl ++;
				goto ecpu;
			}
			cycl += DCPU_cycles2[ob];
			// Special opcodes
			switch(ob) {
			case 0:
				// really special opcodes
				break;
			case SOP_JSR:
				alu1.u = DCPU_deref(oa, pr, ram);
				DCPU_reref(0x18, pr->PC, pr, ram);
				pr->PC = alu1.u;
				break;
			case 0x2: // UKN
			case 0x3: // UKN
			case 0x4: // UKN
			case 0x5: // UKN
			case 0x6: // UKN
			case SOP_HCF: // HCF
				alu1.u = DCPU_deref(oa, pr, ram);
				break;
				// 0x08 - 0x0f Interupts
			case SOP_INT:
				alu1.u = DCPU_deref(oa, pr, ram);
				DCPU_interupt(pr, alu1.u);
				break;
			case SOP_IAG:
				DCPU_reref(oa, pr->IA, pr, ram);
				break;
			case SOP_IAS:
				alu1.u = DCPU_deref(oa, pr, ram);
				if(!(pr->IA = alu1.u)) pr->IQC = 0;
				break;
			case SOP_RFI:
				alu1.u = DCPU_deref(oa, pr, ram);
				pr->R[0] = DCPU_deref(0x18, pr, ram);
				pr->PC = DCPU_deref(0x18, pr, ram);
				pr->MODE &= ~2;
				break;
			case SOP_IAQ:
				alu1.u = DCPU_deref(oa, pr, ram);
				if(alu1.u) {
					pr->MODE |= 2;
				} else {
					pr->MODE &= ~2;
				}
				break;
				// 0x10 - 0x17 Hardware control
			case SOP_HWN:
				DCPU_reref(oa, pr->hwcount, pr, ram);
				break;
			case SOP_HWQ:
				alu1.u = DCPU_deref(oa, pr, ram);
				if((pr->control & 1) && (pr->hwqaf) && (op = pr->hwqaf(pr->R, alu1.u, pr))) {
				} else {
					pr->R[0] = 0;
					pr->R[1] = 0;
					pr->R[2] = 0;
					pr->R[3] = 0;
					pr->R[4] = 0;
				}
				break;
			case SOP_HWI:
				alu1.u = DCPU_deref(oa, pr, ram);
				//pr->MODE |= DCPUMODE_EXTINT;
				if((pr->control & 2) && (pr->hwiaf) && (op = pr->hwiaf(pr->R, alu1.u, pr))) {
					if(op > 0) {
						//pr->wcycl = op;
						pr->MODE |= DCPUMODE_EXTINT;
						goto ecpu;
					}
				}
				break;
			default:
				break;
			}
		} else {
			if(pr->MODE & DCPUMODE_SKIP) {
				DCPU_skipref(oa, pr);
				DCPU_skipref(ob, pr);
				if(op < OPN_IFMIN || op > OPN_IFMAX) {
					pr->MODE ^= DCPUMODE_SKIP;
				}
				cycl ++;
				goto ecpu;
			}
			alu1.u = DCPU_deref(oa, pr, ram);
			switch(DCPU_dtbl[op]) {
			case 1:
				alu2.ui = DCPU_derefB(ob, pr, ram) | (pr->EX << 16);
				break;
			case 2:
				alu2.u = DCPU_deref(ob, pr, ram);
				break;
			case 3:
				DCPU_reref(ob, alu1.u, pr, ram);
				break;
			default:
				break;
			}
			cycl += DCPU_cycles1[op];
			switch(op) {
			case OP_SET: // SET (WO)
				break;
			// Math functions:
			case OP_ADD: // ADD (R/W)
				alu2.ui = alu2.u + alu1.u;
				break;
			case OP_SUB: // SUB (R/W)
				alu2.ui = alu2.u - alu1.u;
				break;
			case OP_MUL: // MUL (R/W)
				alu2.ui = alu2.u * alu1.u;
				break;
			case OP_MLI: // MLI (R/W)
				alu2.si = alu2.s * alu1.s;
				break;
			case OP_DIV: // DIV (R/W)
				if(alu1.u) {
					alu2.ui = (alu2.ui << 16) / alu1.u;
				} else {
					alu2.ui = 0;
				}
				break;
			case OP_DVI: // DVI (R/W)
				alu2.ui = alu2.u << 16;
				{
					int sgn = (alu2.si < 0) ^ (alu1.s < 0);
					if(alu2.si < 0) {
						alu2.si = -alu2.si;
					}
					if(alu1.s < 0) {
						alu1.s = -alu1.s;
					}
					if(!alu1.u) {
						alu2.ui = 0;
					} else {
						alu2.ui = alu2.ui / alu1.u;
						if(sgn)
							alu2.ui = -alu2.ui;
					}
				}
				break;
			case OP_MOD: // MOD (R/W)
				if(alu1.u) {
					alu2.u = alu2.u % alu1.u;
				} else {
					alu2.u = 0;
				}
				break;
			case OP_MDI: // MDI (R/W)
				if(alu1.s) {
					alu2.s = alu2.s % alu1.s;
				} else {
					alu2.u = 0;
				}
				break;
			// Bits operations:
			case OP_AND: // AND (R/W)
				alu2.u &= alu1.u;
				break;
			case OP_BOR: // BOR (R/W)
				alu2.u |= alu1.u;
				break;
			case OP_XOR: // XOR (R/W)
				alu2.u ^= alu1.u;
				break;
			case OP_SHR: // SHR (R/W)
				alu2.ui = alu2.u << 16;
				alu2.ui >>= alu1.u;
				break;
			case OP_ASR: // ASR (R/W)
				alu2.ui = alu2.u << 16;
				alu2.si >>= alu1.u;
				break;
			case OP_SHL: // SHL (R/W)
				alu2.ui = alu2.u << alu1.u;
				break;
			// Conditional Operations:
			case OP_IFB: // IFB (RO)
				if(alu2.u & alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
					// fail
				}
				break;
			case OP_IFC: // IFC (RO)
				if(alu2.u & alu1.u) {
					// fail
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				} else {
				}
				break;
			case OP_IFE: // IFE (RO)
				if(alu2.u == alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFN: // IFN (RO)
				if(alu2.u != alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFG: // IFG (RO)
				if(alu2.u > alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFA: // IFA (RO)
				if(alu2.s > alu1.s) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFL: // IFL (RO)
				if(alu2.u < alu1.u) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			case OP_IFU: // IFU (RO)
				if(alu2.s < alu1.s) {
				} else {
					pr->MODE |= DCPUMODE_SKIP;
					cycl++;
				}
				break;
			// Composite operations:
			case 0x18: // UKN
				fprintf(stderr, "DCPU: Invalid op: %x\n", op);
				break;
			case 0x19: // UKN
				fprintf(stderr, "DCPU: Invalid op: (%04x->%04x) %x\n", LPC, pr->PC, op);
				break;
			case OP_ADX: // ADX (R/W)
				alu2.ui = alu2.u + alu1.u + pr->EX;
				break;
			case OP_SBX: // SBX (R/W)
				alu2.ui = alu2.u - alu1.u + pr->EX;
				break;
			case 0x1C: // UKN
				fprintf(stderr, "DCPU: Invalid op: %x\n", op);
				break;
			case 0x1D: // UKN
				fprintf(stderr, "DCPU: Invalid op: %x\n", op);
				break;
			case OP_STI: // STI (WO)
				pr->R[6]++; pr->R[7]++;
				break;
			case OP_STD: // STD (WO)
				pr->R[6]--; pr->R[7]--;
				break;
			default:
				break;
			}
			switch(DCPU_dwtbl[op]) {
			case 1:
				DCPU_rerefB(ob, alu2.u, pr, ram);
				pr->EX = alu2.ui >> 16;
				break;
			case 2:
				DCPU_rerefB(ob, alu2.ui >> 16, pr, ram);
				pr->EX = alu2.u;
				break;
			case 3:
				DCPU_rerefB(ob, alu2.u, pr, ram);
				break;
			}
		}
	}
	LPC = pr->PC;
	if(!(pr->MODE & (DCPUMODE_SKIP|DCPUMODE_INTQ)) && pr->IQC > 0) {
		DCPU_reref(0x18, pr->PC, pr, ram);
		DCPU_reref(0x18, pr->R[0], pr, ram);
		pr->R[0] = pr->IQU[0];
		pr->PC = pr->IA;
		pr->MODE |= DCPUMODE_INTQ;
		pr->IQC--;
		for(oa = pr->IQC; oa > 0; oa-- ) {
			pr->IQU[oa - 1] = pr->IQU[oa];
		}
	}
ecpu:
	cycl += pr->cycl;
	pr->cycl = 0;
	if(!cycl) cycl = 1;
	if(ccq < cycl) { // used too many
		ccq = 0;
	} else {
		ccq -= cycl; // each op uses cycles
	}
	l_info->cycl += cycl;
	isi_addtime(&l_info->nrun, cycl * l_info->runrate);
	cycl = 0;
	} // while ccq
	return 0;
}

// Skip references
static inline void DCPU_skipref(int p, DCPU* pr)
{
	if((p >= 0x10 && p < 0x18) || p == 0x1a || p == 0x1e || p == 0x1f) {
		pr->PC++;
		pr->cycl++;
	}
}

// Write referenced B operand
static inline void DCPU_reref(int p, uint16_t v, DCPU* pr, uint16_t * ram)
{
	if(p < 0x18) {
		if(p & 0x0010) {
			pr->cycl++;
			ram[ (uint16_t)(ram[pr->PC++] + pr->R[p & 0x7]) ] = v;
			return;
		} else {
			if(p & 0x8) {
				ram[ pr->R[p & 0x7] ] = v;
				return;
			} else {
				pr->R[p & 0x7] = v;
				return;
			}
		}
	} else {
		if(p < 0x20) {
			switch(p) {
			case 0x18:
				ram[--(pr->SP)] = v;
				return;
			case 0x19:
				ram[pr->SP] = v;
				return;
			case 0x1a:
				pr->cycl++;
				ram[(uint16_t)(pr->SP + ram[pr->PC++]) ] = v;
				return;
			case 0x1b:
				pr->SP = v;
				return;
			case 0x1c:
				pr->PC = v;
				return;
			case 0x1d:
				pr->EX = v;
				return;
			case 0x1e:
				pr->cycl++;
				ram[ ram[pr->PC++] ] = v;
				return;
			case 0x1f:
				pr->cycl++;
				// READ: ram[ pr->PC ];
				pr->PC++;
				// Fail silently
				return;
			default:
				fprintf(stderr, "DCPU: Bad write: %x\n", p);
				// Should never actually happen
				return ;
			}
		}
	}
}

// re-Write referenced B operand
static inline void DCPU_rerefB(int p, uint16_t v, DCPU* pr, uint16_t * ram)
{
	if(p < 0x18) {
		if(p & 0x0010) {
			pr->cycl++;
			ram[ (uint16_t)(ram[pr->PC++] + pr->R[p & 0x7]) ] = v;
			return;
		} else {
			if(p & 0x8) {
				ram[ pr->R[p & 0x7] ] = v;
				return;
			} else {
				pr->R[p & 0x7] = v;
				return;
			}
		}
	} else {
		if(p < 0x20) {
			switch(p) {
			case 0x18:
				ram[pr->SP] = v;
				return;
			case 0x19:
				ram[pr->SP] = v;
				return;
			case 0x1a:
				pr->cycl++;
				ram[(uint16_t)(pr->SP + ram[pr->PC++]) ] = v;
				return;
			case 0x1b:
				pr->SP = v;
				return;
			case 0x1c:
				pr->PC = v;
				return;
			case 0x1d:
				pr->EX = v;
				return;
			case 0x1e:
				pr->cycl++;
				ram[ ram[pr->PC++] ] = v;
				return;
			case 0x1f:
				pr->cycl++;
				// READ: ram[ pr->PC ];
				pr->PC++;
				// Fail silently
				return;
			default:
				fprintf(stderr, "DCPU: Bad write: %x\n", p);
				// Should never actually happen
				return ;
			}
		}
	}
}


// Dereference an A operand for ops
static inline uint16_t DCPU_deref(int p, DCPU* pr, uint16_t * ram)
{
	if(p < 0x18) {
		if(p & 0x0010) {
			pr->cycl++;
			return ram[ (uint16_t)(ram[pr->PC++] + pr->R[p & 0x7]) ];
		} else {
			if(p & 0x8) {
				return ram[ pr->R[p & 0x7] ];
			} else {
				return pr->R[p & 0x7];
			}
		}
	} else {
		if(p < 0x20) {
			switch(p) {
			case 0x18:
				return ram[pr->SP++];
			case 0x19:
				return ram[pr->SP];
			case 0x1a:
				pr->cycl++;
				return ram[(uint16_t)(pr->SP + ram[pr->PC++]) ];
			case 0x1b:
				return pr->SP;
			case 0x1c:
				return pr->PC;
			case 0x1d:
				return pr->EX;
			case 0x1e:
				pr->cycl++;
				return ram[ ram[pr->PC++] ];
			case 0x1f:
				pr->cycl++;
				return ram[pr->PC++];
			default:
				// Should never actually happen
				return 0;
			}
		} else {
			return (uint16_t)(p - 0x21);
			// literals
		}
	}
}

// Dereference a B operand for R/W ops:
static inline uint16_t DCPU_derefB(int p, DCPU* pr, uint16_t * ram)
{
	if(p < 0x18) {
		if(p & 0x0010) {
			pr->cycl++;
			return ram[ (uint16_t)(ram[pr->PC] + pr->R[p & 0x7]) ];
		} else {
			if(p & 0x8) {
				return ram[ pr->R[p & 0x7] ];
			} else {
				return pr->R[p & 0x7];
			}
		}
	} else {
		if(p < 0x20) {
			switch(p) {
			case 0x18:
				return ram[--(pr->SP)];
			case 0x19:
				return ram[pr->SP];
			case 0x1a:
				pr->cycl++;
				return ram[(uint16_t)(pr->SP + ram[pr->PC]) ];
			case 0x1b:
				return pr->SP;
			case 0x1c:
				return pr->PC;
			case 0x1d:
				return pr->EX;
			case 0x1e:
				pr->cycl++;
				return ram[ ram[pr->PC] ];
			case 0x1f:
				pr->cycl++;
				return ram[pr->PC];
			default:
				// Should never actually happen
				return 0;
			}
		} else {
			return (uint16_t)(p - 0x21);
			// literals
		}
	}
}


static int DCPU_setonfire(DCPU * pr)
{
	pr->MODE = BURNING;
	fprintf(stderr, "DCPU-MODULE: ur dcpu is on fire foo...\n");
	return HUGE_FIREBALL;
}

int DCPU_sethwcount(DCPU* pr, uint16_t count)
{
	pr->hwcount = count;
	return 0;
}

int DCPU_sethwqcallback(DCPU* pr, DCPUext_query efc)
{
	if(efc) {
		pr->hwqaf = efc;
		pr->control |= 1;
		return 1; 
	}
	return 0;
}

int DCPU_sethwicallback(DCPU* pr, DCPUext_interupt efc)
{
	if(efc) {
		pr->hwiaf = efc;
		pr->control |= 2;
		return 1; 
	}
	return 0;
}
