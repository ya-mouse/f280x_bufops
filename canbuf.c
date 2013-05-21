#include "DSP28x_Project.h"

#include "f280x_bufops/canbuf.h"

#if BUFFER_USE_RAM
#define _INLINE 1
#endif
#include <stdlib.h>
#include <string.h>

CANBUF_Handle CANBUF_init(int base, const CANBUF_Mode_e mode, int msgid)
{
	static struct _CANBUF_Obj_ canbuf;
	union CANMSGID_REG msgid_r;
	volatile struct MBOX* mbox;
	CANBUF_Handle bufHandle = (struct CANBUF_Obj *)&canbuf;
	int i;

	canbuf.regs = (struct ECAN_REGS *)base;
	canbuf.mbx  = (struct ECAN_MBOXES *)base+0x100;

    canbuf.regs->CANME.all &= ~((uint32_t)0x3ffff << 14);
    canbuf.regs->CANMIM.all &= ~((uint32_t)0x3ffff << 14);

	msgid_r.all = 0;
    for (i = -2, mbox = &canbuf.mbx->MBOX14; i < 16; i++, mbox++)
    {
        msgid_r.bit.STDMSGID = msgid+i;
    	mbox->MSGID.all = msgid_r.all;
    	mbox->MSGCTRL.bit.DLC = 8;
    }

    if (mode == CANBUF_Mode_Send)
    	canbuf.regs->CANMD.all &= ~((uint32_t)0xffff << 16);
    else
        canbuf.regs->CANMD.all |= (uint32_t)0xffff << 16;

    canbuf.regs->CANMD.all &= ~((uint32_t)1 << 15);
    canbuf.regs->CANMD.all |= (uint32_t)1 << 14;

    canbuf.regs->CANME.all |= (uint32_t)0x3ffff << 14;

    EALLOW;

    /* Enable interrupts for MBox 14..31 */
    canbuf.regs->CANMIM.all |= (uint32_t)0x3ffff << 14;

    EDIS;

    return (bufHandle);
}

int CANBUF_writer(long data, int *buffer, int len)
{
    //CANBUF_Obj *bufobj = (CANBUF_Obj *)data;

    return 0;
}

int CANBUF_reader(long data, int *buffer, int len)
{
    //CANBUF_Obj *bufobj = (CANBUF_Obj *)data;

    return len;
}

int CANBUF_recv(CANBUF_Handle canbufHandle)
{
    //CANBUF_Obj *bufobj = (CANBUF_Obj *)canbufHandle;

    return 0;
}
