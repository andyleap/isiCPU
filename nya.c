
#include "dcpuhw.h"
#include <unistd.h>

// TODO Not quite "Offical"
static unsigned short nya_deffont[] = {
0xffff,0xffff, 0xffff,0xffff, 0xffff,0xffff, 0xffff,0xffff,
0x242e,0x2400, 0x082A,0x0800, 0x0008,0x0000, 0x0808,0x0808,
0x00ff,0x0000, 0x00f8,0x0808, 0x08f8,0x0000, 0x080f,0x0000,
0x000f,0x0808, 0x00ff,0x0808, 0x08f8,0x0808, 0x08ff,0x0000,
0x080f,0x0808, 0x08ff,0x0808, 0x6633,0x99cc, 0x9933,0x66cc,
0xfef8,0xe080, 0x7f1f,0x0701, 0x0107,0x1f7f, 0x80e0,0xf8fe,
0x5500,0xAA00, 0x55AA,0x55AA, 0xffAA,0xff55, 0x0f0f,0x0f0f,
0xf0f0,0xf0f0, 0x0000,0xffff, 0xffff,0x0000, 0xffff,0xffff,
0x0000,0x0000, 0x005f,0x0000, 0x0300,0x0300, 0x3e14,0x3e00,
0x266b,0x3200, 0x611c,0x4300, 0x3629,0x7650, 0x0002,0x0100,
0x1c22,0x4100, 0x4122,0x1c00, 0x1408,0x1400, 0x081C,0x0800,
0x4020,0x0000, 0x0808,0x0800, 0x0040,0x0000, 0x601c,0x0300,
0x3e49,0x3e00, 0x427f,0x4000, 0x6259,0x4600, 0x2249,0x3600,
0x0f08,0x7f00, 0x2745,0x3900, 0x3e49,0x3200, 0x6119,0x0700,
0x3649,0x3600, 0x2649,0x3e00, 0x0024,0x0000, 0x4024,0x0000,
0x0814,0x2241, 0x1414,0x1400, 0x4122,0x1408, 0x0259,0x0600,
0x3e59,0x5e00, 0x7e09,0x7e00, 0x7f49,0x3600, 0x3e41,0x2200,
0x7f41,0x3e00, 0x7f49,0x4100, 0x7f09,0x0100, 0x3e41,0x7a00,
0x7f08,0x7f00, 0x417f,0x4100, 0x2040,0x3f00, 0x7f08,0x7700,
0x7f40,0x4000, 0x7f06,0x7f00, 0x7f01,0x7e00, 0x3e41,0x3e00,
0x7f09,0x0600, 0x3e41,0xbe00, 0x7f09,0x7600, 0x2649,0x3200,
0x017f,0x0100, 0x3f40,0x3f00, 0x1f60,0x1f00, 0x7f30,0x7f00,
0x7708,0x7700, 0x0778,0x0700, 0x7149,0x4700, 0x007f,0x4100,
0x031c,0x6000, 0x0041,0x7f00, 0x0201,0x0200, 0x8080,0x8000,
0x0001,0x0200, 0x2454,0x7800, 0x7f44,0x3800, 0x3844,0x2800,
0x3844,0x7f00, 0x3854,0x5800, 0x087e,0x0900, 0x4854,0x3c00,
0x7f04,0x7800, 0x447d,0x4000, 0x2040,0x3d00, 0x7f10,0x6c00,
0x417f,0x4000, 0x7c18,0x7c00, 0x7c04,0x7800, 0x3844,0x3800,
0x7c14,0x0800, 0x0814,0x7c00, 0x7c04,0x0800, 0x4854,0x2400,
0x043e,0x4400, 0x3c40,0x7c00, 0x1c60,0x1c00, 0x7c30,0x7c00,
0x6c10,0x6c00, 0x4c50,0x3c00, 0x6454,0x4c00, 0x0836,0x4100,
0x0077,0x0000, 0x4136,0x0800, 0x0201,0x0201, 0x0205,0x0200};
static unsigned short nya_defpal[] = {
0x0000,0x000a,0x00a0,0x00aa,
0x0a00,0x0a0a,0x0a50,0x0aaa,
0x0555,0x055f,0x05f5,0x05ff,
0x0f55,0x0f5f,0x0ff5,0x0fff
};

struct NyaLEM {
	struct timespec lupdate;
	unsigned short dspmem;
	unsigned short fontmem;
	unsigned short palmem;
	unsigned short border;
	unsigned short version;
	int state;
	int dlyu;
	int dlln;
	unsigned short cachepal[16];
	unsigned short cachefont[256];
	unsigned short cachedisp[384]; // Used for sending delta values
};

static int Nya_LEM_voiddisp(struct NyaLEM *dsp, struct systemhwstate *isi);

int Nya_LEM_SIZE()
{
	return sizeof(struct NyaLEM);
}

int Nya_LEM_Init(struct NyaLEM* dsp, int i)
{
	if(!dsp) return -1;
	dsp->dspmem = 0;
	dsp->fontmem = 0;
	dsp->version = 0x1802;
	dsp->lupdate.tv_sec = 0;
	dsp->lupdate.tv_nsec = 0;
	return 0;
}

int Nya_LEM_Query(void * hwd, struct systemhwstate * isi)
{
	struct NyaLEM* dsp;
	if(!hwd) return -1;
	dsp = (struct NyaLEM*)hwd;
	fprintf(stderr, "NYA LEM - HWQ\n");
	isi->regs[0] = 0xf615;
	isi->regs[1] = 0x7349;
	dsp->version = 0x1802;
	isi->regs[2] = dsp->version;
	isi->regs[3] = NYAE_LO;
	isi->regs[4] = NYAE_HI;
	return HWQ_SUCCESS;
}

int Nya_LEM_HWI(void * hwd, struct systemhwstate * isi)
{
	struct NyaLEM* dsp;
	unsigned short dma;
	int i;
	if(!hwd) return -1;
	dsp = (struct NyaLEM*)hwd;
	// TODO - this
	//fprintf(stderr, "NYALEM: HWI %04x \n", isi->regs[0]);
	switch(isi->regs[0]) {
	case 0:
		dsp->dspmem = isi->regs[1];
		//fprintf(stderr, "NYALEM: Video set to %04x \n", dsp->dspmem);
		Nya_LEM_voiddisp(dsp,isi);
		break;
	case 1: // Map Font
		dsp->fontmem = isi->regs[1];
		fprintf(stderr, "NYALEM: Font set to %04x \n", dsp->fontmem);
		break;
	case 2: // Map Palette
		dsp->palmem = isi->regs[1];
		fprintf(stderr, "NYALEM: Palette set to %04x \n", dsp->palmem);
		break;
	case 3: // Set border
		dsp->border = isi->regs[1];
		break;
	case 4: // Mem Dump Font
		dma = isi->regs[1];
		for(i = 0; i < 256; i++) isi->mem[dma++] = nya_deffont[i];
		return 256;
	case 5: // Mem Dump Pal
		dma = isi->regs[1];
		for(i = 0; i < 16; i++) isi->mem[dma++] = nya_defpal[i];
		return 16;
	default:
		break;
	}
	return 0;
}

static int Nya_LEM_voiddisp(struct NyaLEM *dsp, struct systemhwstate *isi)
{
	int dsl;
	uint16_t dsa;
	if(dsp->dspmem) {
		dsa = dsp->dspmem;
		for(dsl = 0; dsl < 384; dsl++) {
			dsp->cachedisp[dsl] = isi->mem[dsa + dsl] ^ 1;
		}
	}
	return 0;
}

int Nya_LEM_Tick(void * hwd, struct systemhwstate * isi)
{
	struct NyaLEM* dsp;
	uint16_t dsa;
	int dsl, dur;
	uint16_t ddb[400];
	int ddi, dse, dss;
	if(!hwd) return -1;
	dsp = (struct NyaLEM*)hwd;
	if(isi->msg == 0x3400) {
		fprintf(stderr, "M: %04x \n", dsp->dspmem);
		for(dur = 0; dur < 384; dur++) {
			fprintf(stderr,"%04x ",dsp->cachedisp[dur]);
			if(dur % 16 == 15) fprintf(stderr,"\n");
		}
	}
	if(!isi->netwfd) return 0; // quit if no network
	if(dsp->lupdate.tv_sec) {
		if(isi->crun.tv_sec < dsp->lupdate.tv_sec
			|| isi->crun.tv_nsec < dsp->lupdate.tv_nsec) {
			return 0;
		}
		isi_addtime(&dsp->lupdate, 50000000);
	} else {
		dsp->lupdate.tv_sec = isi->crun.tv_sec;
		dsp->lupdate.tv_nsec = isi->crun.tv_nsec;
		isi_addtime(&dsp->lupdate, 50000000);
	}
	if(dsp->dspmem) {
		dsa = dsp->dspmem;
		dur = 0;
		ddi = 0;
		dss = 0;
		dse = 0;

		ddb[0] = 0xE7AA; // Type code

		// Simple display delta protocol
		for(dsl = 0; dsl < 384; dsl++) {
			dss = dsl;
			while(dsl < 384 && isi->mem[dsa+dsl] != dsp->cachedisp[dsl]) {
				ddb[5+ddi] = isi->mem[dsa+dsl];
				dsp->cachedisp[dsl] = isi->mem[dsa+dsl];
				ddi++; dsl++;
			}
			if(ddi) {
				ddb[1] = 0; // ID
				ddb[2] = 0; // ID
				ddb[3] = dss; // address
				ddb[4] = ddi; // length (words)
				if(isi->netwfd) {
					write(isi->netwfd, (char*)ddb, 10+(ddi*2));
				}
				ddi = 0;
			}
		}
	}

	return 0;
}

