
#include "../dcpuhw.h"

/* state while this device exists/active (runtime volatile) */
struct HICProxy_rvstate {
};
ISIREFLECT(struct HICProxy_rvstate,
)

/* state while running on this server (server volatile) */
struct HICProxy_svstate {
};
ISIREFLECT(struct HICProxy_svstate,
)
/* SIZE call for variable size structs */
int HICProxy_SIZE(int t, size_t *sz)
{
	if(t == 0) return *sz = sizeof(struct HICProxy_rvstate);
	if(t == 1) return sizeof(struct HICProxy_svstate);
	return 0;
}
static int HICProxy_Init(struct isiInfo *info);
/* static int HICProxy_PreInit(struct isiInfo *info); */
static int HICProxy_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg);
struct isidcpudev HICProxy_Meta = {0x0,0x0,0x0};
struct isiConstruct HICProxy_Con = {
	.objtype = 0x5000,
	.name = "HICProxy",
	.desc = "HIC Proxy Device",
	.PreInit = NULL, /* HICProxy_PreInit, */
	.Init = HICProxy_Init,
	.New = HICProxy_New,
	.QuerySize = HICProxy_SIZE,
	.rvproto = &ISIREFNAME(struct HICProxy_rvstate),
	.svproto = &ISIREFNAME(struct HICProxy_svstate),
	.meta = &HICProxy_Meta
};

void HICProxy_Register()
{
	isi_register(&HICProxy_Con);
}

static int HICProxy_QueryAttach(struct isiInfo *info, int32_t point, struct isiInfo *dev, int32_t devpoint)
{
	if(!dev || !info) return -1;
	if(dev->id.objtype < 0x2f00) return -1;
	return 0;
}

static int HICProxy_Reset(struct isiInfo *info)
{
	//struct HICProxy_rvstate *dev = (struct HICProxy_rvstate*)info->rvstate;
	return 0;
}

static int HICProxy_MsgIn(struct isiInfo *info, struct isiInfo *host, int32_t lsindex, uint16_t *msg, int len, struct timespec mtime)
{
	//struct HICProxy_rvstate *dev = (struct HICProxy_rvstate*)info->rvstate;
	switch(msg[0]) { /* message type, msg[1] is device index */
		/* these should return 0 if they don't have function calls */
	case ISE_RESET: return 0;
	case ISE_DPSI:
		if(host == info)
		{
			isi_message_dev(info, ISIAT_UP, msg, len, mtime);
		} else {
			isi_message_dev(info, ISIAT_SESSION, msg, len, mtime);
		}
	default: break;
	}
	return 1;
}

static struct isiInfoCalls HICProxy_Calls = {
	.QueryAttach = HICProxy_QueryAttach,
	.Reset = HICProxy_Reset, /* power on reset */
	.MsgIn = HICProxy_MsgIn, /* message from CPU or network */
};

static int HICProxy_Init(struct isiInfo *info)
{
	info->c = &HICProxy_Calls;
	return 0;
}

static int HICProxy_New(struct isiInfo *info, const uint8_t * cfg, size_t lcfg)
{
	return 0;
}

