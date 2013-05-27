#include "DSP28x_Project.h"

#include "f280x_bufops/canbuf.h"

#if BUFFER_USE_RAM
#define _INLINE 1
#endif
#include <stdlib.h>
#include <string.h>

#define MBX_SEND			15
#define MBX_RECV			14
#if CANBUF_WITH_ASCIIBUF
#define PKT_SHIFT 3
/* Maximum size of ASCII buffer */
#define PACKET_SIZE			128
#else
#define PKT_SHIFT 2
/* Maxium size of binary buffer */
#define PACKET_SIZE			64
#endif

/* Check for new incoming control message */
#define MBX_HAS_COMMAND(obj)						\
	(obj->regs->CANRMP.all & (1 << MBX_RECV))

#define CAN_SET_VAL(reg, val)						\
	v = (uint32_t)val;								\
	canbuf->regs->reg.all = v

#define CAN_GET_VAL(reg)							\
	canbuf->regs->reg.all

#define CAN_CLEAR_REG(reg, val) 					\
	v = canbuf.regs->reg.all;						\
	v &= ~((uint32_t)val);							\
	canbuf.regs->reg.all = v

#define CAN_SET_REG(reg, val) 						\
	v = canbuf.regs->reg.all;						\
	v |= ((uint32_t)val);							\
	canbuf.regs->reg.all = v

#define CAN_CLEAR_AND_SET_REG(reg, valc, vals) 		\
	v = canbuf.regs->reg.all;						\
	v &= ~((uint32_t)valc);							\
	v |= ((uint32_t)vals);							\
	canbuf.regs->reg.all = v


typedef enum {
	CANBUF_Command_UNK = 0,
	CANBUF_Command_SOH = 0x01,
	CANBUF_Command_EOT = 0x04,
	CANBUF_Command_ACK = 0x06,
	CANBUF_Command_NAK = 0x15,
	CANBUF_Command_CAN = 0x18
} CANBUF_Command_e;

struct canbuf_soh
{
	uint16_t seq1:8;
	uint16_t seq2:8;
	uint16_t len;
};

typedef struct
{
	CANBUF_Command_e cmd;
	union {
		uint32_t data;
		struct canbuf_soh soh;
	} u;
} canbuf_cmd;

// #define DEB 1
extern int scia_printf(const char *_format, ...);

#if CANBUF_SCIA_PROGRESS
static int _cscia_putc(char c)
{
	int i = 15*1000L;

	while (i--)
	{
		if (SciaRegs.SCICTL2.bit.TXRDY) {
			SciaRegs.SCITXBUF = c;
			return 1;
		}

		DELAY_US(10L);
	}
	return 0;
}
#else
#define _cscia_putc(c) while (0) { }
#endif

static int _canbuf_recv(CANBUF_Obj *canbuf, int mbx, uint32_t *mdl, uint32_t *mdh)
{
	volatile struct MBOX *mbox;
	long i = CANBUF_TIMEOUT*100000L; // *500L;
	uint32_t v;

	mbox = &canbuf->mbx->MBOX0 + mbx;

	*mdl = 0;
	*mdh = 0;
	while (i--)
	{
		if (canbuf->regs->CANRMP.all & ((uint32_t)1 << mbx))
		{
			*mdl = mbox->MDL.all;
			*mdh = mbox->MDH.all;
			CAN_SET_VAL(CANRMP, 1 << mbx);
			return 1;
		}

		DELAY_US(1L);
	}

	return 0;
}


static uint32_t _canbuf_get_cmd(CANBUF_Obj *canbuf, CANBUF_Command_e *cmd)
{
	uint32_t mdl;
	uint32_t mdh;

	if (_canbuf_recv(canbuf, MBX_RECV, &mdl, &mdh))
	{
		*cmd = (CANBUF_Command_e)(mdl & 0xff);
#ifdef DEB
		scia_printf("< %04x\r\n", *cmd);
#endif
		return mdh;
	}

	*cmd = CANBUF_Command_UNK;
#ifdef DEB
	scia_printf("< %04x\r\n", *cmd);
#endif
	return 0;
}

static int _canbuf_send(CANBUF_Obj *canbuf, int mbx, uint32_t mdl, uint32_t mdh)
{
	volatile struct MBOX *mbox;
    int retries = 0;
    uint32_t v;

    if (mbx > 0 && mbx < 32)
    {
    	mbox = &canbuf->mbx->MBOX0 + mbx;
        mbox->MDL.all = (uint32_t)mdl;
        mbox->MDH.all = (uint32_t)mdh;

        CAN_SET_VAL(CANTRS, 1 << mbx);
        do {
        	v = CAN_GET_VAL(CANTA);
        	// CANES = 0x80010
        	retries++;
        } while((v & ((uint32_t)1 << mbx)) == 0 && retries != 255);
        CAN_SET_VAL(CANTA, 1 << mbx);
    }
    return (retries == 255);
}

static int _canbuf_recv_data(CANBUF_Obj *ecan, int *buffer, int bufsize)
{
	int i;
	int rc;
	uint32_t mdl;
	uint32_t mdh;

	/* Each MBOX can transmit 8 bytes */
	for (i = 0; i < 16; i++)
	{
		if (! _canbuf_recv(ecan, 16+i, &mdl, &mdh))
			return -1;

#if CANBUF_WITH_ASCIIBUF
		buffer[i << 3] = mdl & 0xff;
		buffer[(i << 3) + 1] = ((uint16_t)mdl >> 8) & 0xff;
		buffer[(i << 3) + 2] = ((uint32_t)mdl >> 16) & 0xff;
		buffer[(i << 3) + 3] = ((uint32_t)mdl >> 24) & 0xff;
		buffer[(i << 3) + 4] = mdh & 0xff;
		buffer[(i << 3) + 5] = ((uint16_t)mdh >> 8) & 0xff;
		buffer[(i << 3) + 6] = ((uint32_t)mdh >> 16) & 0xff;
		buffer[(i << 3) + 7] = ((uint32_t)mdh >> 24) & 0xff;
#else
		buffer[i << 2] = mdl & 0xffff;
		buffer[(i << 2) + 1] = ((uint32_t)mdl >> 16) & 0xffff;
		buffer[(i << 2) + 2] = mdh & 0xffff;
		buffer[(i << 2) + 3] = ((uint32_t)mdh >> 16) & 0xffff;
#endif
		if (rc == 255)
			return -1;
	}
	return (i << PKT_SHIFT);
}

static int _canbuf_send_data(CANBUF_Obj *ecan, int *buffer, int bufsize)
{
	int i;
	int rc;
	uint32_t mdl;
	uint32_t mdh;

	/* Each MBOX can transmit 8 bytes */
	for (i = 0; i < 16; i++)
	{
#if CANBUF_WITH_ASCIIBUF
		mdl  = (uint32_t)buffer[i << 3]
		    | (uint16_t)(buffer[(i << 3) + 1] << 8);
		mdl |= ((uint32_t)buffer[(i << 3) + 2] << 16)
		     | ((uint32_t)buffer[(i << 3) + 3] << 24);
		mdh  = (uint32_t)buffer[(i << 3) + 4]
		    | (uint16_t)(buffer[(i << 3) + 5] << 8);
		mdh |= ((uint32_t)buffer[(i << 3) + 6] << 16)
		     | ((uint32_t)buffer[(i << 3) + 7] << 24);
#else
		mdl = (uint32_t)buffer[(i << 2) + 0] | (uint32_t)(buffer[(i << 2) + 1] << 16);
		mdh = (uint32_t)buffer[(i << 2) + 2] | (uint32_t)(buffer[(i << 2) + 3] << 16);
#endif
		rc = _canbuf_send(ecan, 16+i, mdl, mdh);
		if (rc == 255)
			return -1;
	}
	return (i << PKT_SHIFT);
}

static int _canbuf_send_cmd(CANBUF_Obj *ecan, CANBUF_Command_e cmd, uint32_t data)
{
	int i = CANBUF_TIMEOUT*1000L;

#ifdef DEB
	scia_printf("> %04x\r\n", cmd);
#endif
	while (i--)
	{
		if (_canbuf_send(ecan, MBX_SEND, cmd, data) != 255)
			return 1;

		DELAY_US(10L);
	}
	return 0;
}

CANBUF_Handle CANBUF_init(volatile struct ECAN_REGS *regs, const CANBUF_Mode_e mode, int msgid)
{
	static struct _CANBUF_Obj_ canbuf;
	union CANMSGID_REG msgid_r;
	volatile struct MBOX* mbox;
	CANBUF_Handle bufHandle = (struct CANBUF_Obj *)&canbuf;
	int i;
	uint32_t v;

	memset(bufHandle, 0, sizeof(CANBUF_Obj));

	canbuf.msgid = msgid;
	canbuf.retry = CANBUF_DEFAULT_RETRY;
	canbuf.regs = regs;
	canbuf.sequence = 1;
	canbuf.mbx  = (volatile struct ECAN_MBOXES *)((int16_t *)regs+0x100);

    EALLOW;

    /* Disable MBOX 14..31 */
    CAN_CLEAR_REG(CANME, 0x3ffff << 14);
	CAN_CLEAR_REG(CANMIM, 0x3ffff << 14);

	msgid_r.all = 0;
    for (i = -1, mbox = &canbuf.mbx->MBOX14; i < 17; i++, mbox++)
    {
    	/* Set Command MBX 14 and 15 with the same msgid */
        msgid_r.bit.STDMSGID = (i > 0 ? msgid+i : msgid);
        mbox->MDL.all = 0;
        mbox->MDH.all = 0;
    	mbox->MSGID.all = msgid_r.all;
    	mbox->MSGCTRL.bit.DLC = 8;
    }

    if (mode == CANBUF_Mode_Send)
    {
    	/* Set MBOX 15..31 as transmit
    	 * Set MBOX14 as receive
    	 */
    	CAN_CLEAR_AND_SET_REG(CANMD, 0x1ffff << 15, 1 << MBX_RECV);
    }
    else
    {
    	/* Set MBOX 14, 16..31 as receive
    	 * Set MBOX15 as transmit
    	 */
    	CAN_CLEAR_AND_SET_REG(CANMD, 1 << MBX_SEND, 0x3fffd << 14);
    }

    /* Clean RMP */
    CAN_SET_REG(CANRMP, 0x3fffd << 14);

    /* Enable MBOX 14..31 */
    CAN_SET_REG(CANME, 0x3ffff << 14);

    /* Enable interrupts for MBox 14..31 */
    CAN_SET_REG(CANMIM, 0x3ffff << 14);

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
    // CANBUF_Obj *bufobj = (CANBUF_Obj *)data;

    return CANBUF_send((CANBUF_Handle)data, buffer, len);
}

int CANBUF_end(long data, int *buffer, int len)
{
    CANBUF_Obj *ecan = (CANBUF_Obj *)data;

    /* End of transmission */
    _canbuf_send_cmd(ecan, CANBUF_Command_EOT, 0);

    ecan->handshaked = 0;
    ecan->sequence = 1;
    return 1;
}

void CANBUF_setWriter(CANBUF_Handle canbufHandle, BUF_Op_cb writerCallback, long cbData)
{
    CANBUF_Obj *ecan = (CANBUF_Obj *)canbufHandle;

    ecan->writer = writerCallback;
    ecan->writer_data = cbData;
}

void CANBUF_setReader(CANBUF_Handle canbufHandle, BUF_Op_cb readerCallback, long cbData)
{
    CANBUF_Obj *ecan = (CANBUF_Obj *)canbufHandle;

    ecan->reader = readerCallback;
    ecan->reader_data = cbData;
}

void CANBUF_setEnd(CANBUF_Handle canbufHandle, BUF_Op_cb endCallback, long cbData)
{
    CANBUF_Obj *ecan = (CANBUF_Obj *)canbufHandle;

    ecan->end = endCallback;
    ecan->end_data = cbData;
}

int CANBUF_send(CANBUF_Handle canbufHandle, int *buffer, int bufsize)
{
    CANBUF_Obj *ecan = (CANBUF_Obj *)canbufHandle;

    int error_count = 0;
    int cancel = 0;
    int len = bufsize;
    int offset;
    int retry_count;
    canbuf_cmd cmd;

    /* Check for handshake status */
    if (ecan->handshaked)
    	goto init_done;

    /* Initiate protocol */
    for (;;)
    {
    	cmd.u.data = _canbuf_get_cmd(ecan, &cmd.cmd);
    	switch (cmd.cmd)
    	{
    	case CANBUF_Command_NAK:
    		goto init_done;

    	case CANBUF_Command_CAN:
    		if (cancel)
    			return 0;
    		else
    			cancel = 1;
    	}
    	error_count++;

        if (error_count >= ecan->retry)
        	goto abort;
    }
init_done:

    /* Read data */
	ecan->handshaked = 1;
    error_count = 0;

    if (ecan->writer)
		len = ecan->writer(ecan->writer_data, buffer, bufsize);

    for (;;)
    {
        offset = 0;
    	/* Emit packet */
    	for (;;)
    	{
#if 0
        	if (len - offset < PACKET_SIZE)
        		memset(buffer + offset + len, '\0', PACKET_SIZE - len - offset);
#else
        	if (len < PACKET_SIZE)
        		memset(buffer + offset + len, '\0', PACKET_SIZE - len);
#endif

        	cmd.u.soh.seq1 = ecan->sequence;
    		cmd.u.soh.seq2 = 0xff - cmd.u.soh.seq1;
    		cmd.u.soh.len  = len > PACKET_SIZE ? PACKET_SIZE : len;

    		if (_canbuf_send_data(ecan, buffer + offset, PACKET_SIZE) < 0)
    			goto has_error;

    		_canbuf_send_cmd(ecan, CANBUF_Command_SOH, cmd.u.data);

    		retry_count = 0;
    		/*
    		 * Other end may take a while to process input data.
    		 * Relax, and wait a bit.
    		 */
			do
			{
				cmd.u.data = _canbuf_get_cmd(ecan, &cmd.cmd);
				retry_count++;
			} while (cmd.cmd == CANBUF_Command_UNK && retry_count < 20);
            if (cmd.cmd == CANBUF_Command_ACK)
            {
#if 0
            	/* Not a whole packet has been sent, transmit the tail */
            	if (sent < len)
            	{
            		offset += sent;
            		goto seq_inc;
            	}
#endif
            	break;
            }
            else if (cmd.cmd == CANBUF_Command_NAK)
            {
has_error:
            	error_count++;
            	if (error_count >= ecan->retry)
            		goto abort;
            }
            else /* Unwanted response, retry */
            	goto has_error;
    	}

    	/* Call callback to get more data */
    	if (ecan->writer)
    	{
    		len = ecan->writer(ecan->writer_data, buffer, bufsize);
    	}
    	else if (len > PACKET_SIZE)
    	{
    		buffer += PACKET_SIZE;
    		len -= PACKET_SIZE;
    	}
    	else
    		len = 0;

// seq_inc:
    	/* Keep track of sequence */
    	ecan->sequence = (ecan->sequence + 1) & 0xff;

    	if (len == 0)
    		break;
    }

    if (ecan->end)
    {
    	ecan->end(ecan->end_data, buffer, bufsize);
    }

    return bufsize;

abort:

	_canbuf_send_cmd(ecan, CANBUF_Command_CAN, 0);
	return -1;
}

int CANBUF_recv(CANBUF_Handle canbufHandle, int *buffer, int bufsize)
{
    CANBUF_Obj *ecan = (CANBUF_Obj *)canbufHandle;

    int error_count = 0;
    long income_size;
    int cancel = 0;
    canbuf_cmd cmd;

    /* Initiate protocol */
    for (;;)
    {
    	if (error_count >= ecan->retry)
    		goto abort;

   		if (!_canbuf_send_cmd(ecan, CANBUF_Command_NAK, 0))
   		{
   			DELAY_US(CANBUF_DELAY);
   			error_count++;
   		}

    	cmd.u.data = _canbuf_get_cmd(ecan, &cmd.cmd);
    	switch (cmd.cmd)
    	{
    	case CANBUF_Command_SOH:
    		goto init_done;

    	case CANBUF_Command_CAN:
    		if (cancel)
    			return 0;
    		else
    			cancel = 1;
    		break;

    	default:
    		error_count++;
    	}
    }
init_done:

    /* Read data */
    error_count = 0;
    income_size = 0;
    cancel = 0;

	if (!bufsize || buffer == NULL)
		goto abort;

	for (;;)
    {
    	for (;;)
    	{
    		if (cmd.cmd == CANBUF_Command_SOH)
    		{
    			/* SOH received */
    			break;
    		}
    		else if (cmd.cmd == CANBUF_Command_EOT)
    		{
				_canbuf_send_cmd(ecan, CANBUF_Command_ACK, 0);
	   			_cscia_putc('+');
			    if (ecan->end)
			    {
			    	ecan->end(ecan->end_data, buffer, bufsize);
			    }
    			return (uint32_t)income_size;
    		}
    		else if (cmd.cmd == CANBUF_Command_CAN)
    		{
    			if (cancel)
    				return 0;
    			else
    				cancel = 1;
    		}
    		else
    		{
    			cmd.u.data = _canbuf_get_cmd(ecan, &cmd.cmd);
    			error_count++;
	   			_cscia_putc('E');
    			if (error_count >= ecan->retry)
    				goto abort;
    		}
    	}

    	/* Read sequence */
    	error_count = 0;
    	cancel = 0;
   		if (cmd.u.soh.seq1 == ecan->sequence && cmd.u.soh.seq2 == (0xff - ecan->sequence))
   		{
   			/* Sequence is ok, read packet */
			if (bufsize < 0)
			{
				/* No more buffer available */
	   			_cscia_putc('0');
				goto abort;
			}

   			if (_canbuf_recv_data(ecan, buffer, cmd.u.soh.len) != cmd.u.soh.len)
   			{
   				error_count++;
	   			_cscia_putc('E');
       			if (error_count >= ecan->retry)
       				goto abort;
   				goto nak;
   			}

//   			income_size += cmd.u.soh.len;
   			income_size++;
   			_cscia_putc('.');
   			if (ecan->reader)
   			{
   				bufsize = ecan->reader(ecan->reader_data, buffer, cmd.u.soh.len);
   				/* Error has been occurred, re-request the packet */
   				if (bufsize < 0) {
   		   			_cscia_putc('R');
   					goto nak;
   				}
   			}
   			else if (bufsize >= cmd.u.soh.len)
			{
   				bufsize -= cmd.u.soh.len;
				buffer += cmd.u.soh.len;
   			}
   			_canbuf_send_cmd(ecan, CANBUF_Command_ACK, 0);
			ecan->sequence = (ecan->sequence + 1) & 0xff;

			cmd.u.data = _canbuf_get_cmd(ecan, &cmd.cmd);
   			continue;
   		}
   		else
   		{
   			_cscia_putc('S');
   		}

nak:
		_cscia_putc('?');
    	/* Something wen wrong, request retransmission */
    	_canbuf_send_cmd(ecan, CANBUF_Command_NAK, 0);
    	cmd.u.data = _canbuf_get_cmd(ecan, &cmd.cmd);
    }

abort:

	_cscia_putc('A');
	_canbuf_send_cmd(ecan, CANBUF_Command_CAN, 0);
	return 0;
}
