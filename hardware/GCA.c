
#include "../dcpuhw.h"

/* state while this device exists/active (runtime volatile) */
struct GCA_rvstate {
	uint16_t Pressure;
	uint16_t GasCount;
	uint16_t Gas[6];
	uint16_t Concentration[6];
	uint16_t State;
	uint16_t ExtraTime;
	uint16_t Version;
};
ISIREFLECT(struct GCA_rvstate,
	ISIR(GCA_rvstate, uint16_t, GasCount)
	ISIR(GCA_rvstate, uint16_t, Pressure)
	ISIR(GCA_rvstate, uint16_t, Gas)
	ISIR(GCA_rvstate, uint16_t, Concentration)
	ISIR(GCA_rvstate, uint16_t, State)
	ISIR(GCA_rvstate, uint16_t, ExtraTime)
	ISIR(GCA_rvstate, uint16_t, Version)
)

/* state while running on this server (server volatile) */
struct GCA_svstate {
	
};
ISIREFLECT(struct GCA_svstate,
)
/* SIZE call for variable size structs */
int GCA_SIZE(int t, size_t *sz)
{
	if(t == 0) return *sz = sizeof(struct GCA_rvstate);
	if(t == 1) return sizeof(struct GCA_svstate);
	return 0;
}
static int GCA_Init(struct isiInfo *info);
/* static int GCA_PreInit(struct isiInfo *info); */
static int GCA_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg);
struct isidcpudev GCA_Meta = {0x1823,0x2FF23233,0x4C534453};
struct isiConstruct GCA_Con = {
	.objtype = 0x5000,
	.name = "GCA",
	.desc = "GCAXX23",
	.PreInit = NULL, /* GCA_PreInit, */
	.Init = GCA_Init,
	.New = GCA_New,
	.QuerySize = GCA_SIZE,
	.rvproto = &ISIREFNAME(struct GCA_rvstate),
	.svproto = &ISIREFNAME(struct GCA_svstate),
	.meta = &GCA_Meta
};

void GCA_Register()
{
	isi_register(&GCA_Con);
}

static int GCA_Reset(struct isiInfo *info)
{
	struct GCA_rvstate *dev = (struct GCA_rvstate*)info->rvstate;
	dev->GasCount = 0;
	dev->Pressure = 0;
	return 0;
}

static int GCA_OnReset(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	//struct GCA_rvstate *dev = (struct GCA_rvstate*)info->rvstate;
	return 0;
}

static int GCA_Query(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec mtime)
{
	struct GCA_rvstate *dev = (struct GCA_rvstate*)info->rvstate;
	struct isidcpudev *mid = (struct isidcpudev *)info->meta->meta;
	msg[0] = (uint16_t)(mid->devid);
	msg[1] = (uint16_t)(mid->devid >> 16);
	msg[3] = (uint16_t)(mid->mfgid);
	msg[4] = (uint16_t)(mid->mfgid >> 16);
	
	msg[2] = dev->Version;
	
	return 0;
}

static int GCA_HWI(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, struct timespec crun)
{
	struct GCA_rvstate *dev = (struct GCA_rvstate*)info->rvstate;
	switch(msg[0]) {
	case 0:
		msg[0] = dev->State;
		msg[1] = dev->GasCount;
		break;
	case 1:
		//TODO: Send message to game server
		dev->State = 1;
		info->nrun.tv_nsec = crun.tv_nsec;
		info->nrun.tv_sec = crun.tv_sec;
		isi_addtime(&info->nrun, 10000000000);
		break;
	case 2:
		if(dev->State == 0)
		{
			msg[0] = 0;
			msg[1] = 0;
		}
		if(msg[1] > dev->GasCount)
		{
			msg[0] = 0;
			msg[1] = 0;
			break;
		}
		msg[0] = dev->Gas[msg[1]];
		msg[1] = dev->Concentration[msg[1]];
		break;
	case 3:
		if(dev->State == 0)
		{
			msg[0] = 0;
		}
		msg[0] = dev->Pressure;
		break;
	case 0xFFFE:
		GCA_Query(info, src, msg, crun);
	case 0xFFFF:
		GCA_Reset(info);
		break;
	}
	return 0;
}

static int GCA_Tick(struct isiInfo *info, struct timespec crun)
{
	struct GCA_rvstate *dev = (struct GCA_rvstate*)info->rvstate;
	if(dev->State != 0)
	{
		if(!isi_time_lt(&info->nrun, &crun)) return 0; /* wait for scheduled time */
		switch(dev->State) {
			case 1:
				if(dev->ExtraTime == 0)
				{
					dev->ExtraTime = 0;
					isi_addtime(&info->nrun, 1000000000 * dev->GasCount * 2); //TODO: Adjust based on version
					return 0;
				}
				dev->State = 2;
				isi_addtime(&info->nrun, 10000000000);
				//TODO: interrupt to signal completion
				break;
			case 2:
				dev->State = 0;
		}
	}
	return 0;
}

static int GCA_MsgIn(struct isiInfo *info, struct isiInfo *src, uint16_t *msg, int len, struct timespec mtime)
{
	struct GCA_rvstate *dev = (struct GCA_rvstate*)info->rvstate;
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case 0: return GCA_OnReset(info, src, msg+2, mtime); /* CPU finished reset */
	case 1: return GCA_Query(info, src, msg+2, mtime); /* HWQ executed */
	case 2: return GCA_HWI(info, src, msg+2, mtime); /* HWI executed */
	case 3:
		if(len<3) {
			return 1;
		}
		if(dev->State == 1)
		{
			if(len<3+msg[2]*2) {
				return 1;
			}
			dev->Pressure = msg[1];
			dev->GasCount = msg[2];
			for(int l1 = 0; l1 < msg[2]; l1++)
			{
				dev->Gas[l1] = msg[3+l1*2];
				dev->Concentration[l1] = msg[4+l1*2];
			}
		}
	default: break;
	}
	return 1;
}

static struct isiInfoCalls GCA_Calls = {
	.Reset = GCA_Reset, /* power on reset */
	.MsgIn = GCA_MsgIn, /* message from CPU or network */
	.RunCycles = GCA_Tick /* scheduled runtime */
};

static int GCA_Init(struct isiInfo *info)
{
	info->c = &GCA_Calls;
	return 0;
}

static int GCA_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg)
{
	return 0;
}

