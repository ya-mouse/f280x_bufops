/*
 * XMODEM protocol implementation for TMS320F28x
 * 		  with optional CRC-16 support
 * 		  and ring-buffer processor/updater callbacks
 *
 * NOTE:  1024 packet size is not supported
 *
 * XMODEM code flow based on Python's xmodem-0.2.4 package
 *        by Wijnand Modderman <maze@pyth0n.org>
 *
 * License: MIT
 *
 * Copyright:
 *
 *    2011, Wijnand Modderman <maze@pyth0n.org>
 *    2013, Anton D. Kachalov <mouse@yandex.ru>
 *
 */
#include "DSP28x_Project.h"

#include <string.h>

#include "f280x_bufops/xmodem.h"

#if BUFFER_USE_RAM
#define _INLINE 1
#endif
#include <string.h>

#define SOH	0x01
#define STX	0x02
#define EOT	0x04
#define ACK	0x06
#define NAK	0x15
#define CAN	0x18
#define CRC	0x43

#if XMODEM_WITH_PACKEDBUF
#define PACKET_SIZE (packet_size >> 1)
#define BUFLEN_DIV	1
#else
#define PACKET_SIZE packet_size
#define BUFLEN_DIV	0
#endif

#if BUFFER_USE_RAM || XMODEM_USE_OLD_API
#define SCI_PUT_DATA(sci, data, action)							\
		struct SCI_REGS *regs = (struct SCI_REGS *)sci; 		\
		if (regs->SCICTL2.bit.TXRDY) {							\
			regs->SCITXBUF = data;								\
			action;												\
		}

#define SCI_GET_DATA(sci, data, success)						\
		struct SCI_REGS *regs = (struct SCI_REGS *)sci; 		\
		if (regs->SCIFFRX.bit.RXFFST) {							\
			data = regs->SCIRXBUF.bit.RXDT;						\
			*success = 1;										\
		} else {												\
			data = 0;											\
			*success = 0;										\
		}

#else
#define SCI_PUT_DATA(sci, data, action)							\
		if (SCI_putDataNonBlocking(sci, c))	{					\
			action;												\
		}

#define SCI_GET_DATA(sci, data, success)						\
		data = SCI_getDataNonBlocking(sci, success)
#endif


/* crctab calculated by Mark G. Mendel, Network Systems Corporation */
#if XMODEM_WITH_CRC
const uint16_t crctable[] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
    };
#endif

#if XMODEM_WITH_BUFFER
static int _xmodem_buffer[128];
#endif

static int _xmodem_putc(XMODEM_Obj *x, char c)
{
	int i = XMODEM_TIMEOUT*1000L;

	while (i--)
	{
		SCI_PUT_DATA(x->sci, c, return 1);

		DELAY_US(10L);
	}
	return 0;
}

static void _xmodem_putm(XMODEM_Obj *x, int *buffer, int len)
{
	int i;
	for (i = 0; i < (len >> BUFLEN_DIV); i++)
	{
		_xmodem_putc(x, buffer[i] & 0xff);
#if XMODEM_WITH_PACKEDBUF
		_xmodem_putc(x, buffer[i] >> 8);
#endif
	}
}

static inline int _xmodem_getc(XMODEM_Obj *x, uint16_t *success)
{
	int16_t c;
	long i = XMODEM_TIMEOUT*1000L*(200/CPU_RATE)*10L;

	while (i--)
	{
		SCI_GET_DATA(x->sci, c, success);
		if (*success) {
//			SCI_clearRxFifoOvf(x->sci);
//			SCI_clearRxFifoInt(x->sci);
			return c;
		}
	}

	return 0;

//	return SCI_getDataBlocking(x->sci);
}

static int _xmodem_getc_t(XMODEM_Obj *x, uint16_t *success)
{
	int c;
	long i = XMODEM_TIMEOUT*100000L; // *500L;

	while (i--)
	{
		SCI_GET_DATA(x->sci, c, success);
		if (*success) {
//			SCI_clearRxFifoOvf(x->sci);
//			SCI_clearRxFifoInt(x->sci);
			return c;
		}

		DELAY_US(1L);
	}

	return 0;
}

static int _xmodem_getm(XMODEM_Obj *x, int *buffer, int len)
{
	int i;
	int16_t c;
	uint16_t success;

	for (i = 0; i < len; i++)
	{
		c = _xmodem_getc_t(x, &success);
		if (!success)
			return i;
#if XMODEM_WITH_PACKEDBUF
		if (buffer)
			buffer[i >> 1] = c;
		if (i == len-1)
			return i+1;
		c = _xmodem_getc_t(x, &success);
		if (!success)
			return i+1;
		if (buffer)
			buffer[i >> 1] |= c << 8;
		i++;
#else
		if (buffer)
			buffer[i] = c;
#endif
	}

	return i;
}

static uint16_t _xmodem_calc_checksum(int *buffer, int len)
{
	int i;
	uint16_t sum = 0;

#if XMODEM_WITH_PACKEDBUF
	for (i = 0; i < (len >> 1); i++)
	{
		sum += (buffer[i] & 0xff) + (buffer[i] >> 8);
	}
#else
	for (i = 0; i < len; i++)
	{
		sum += buffer[i] & 0xff;
	}
#endif

	return sum % 256;
}

#if XMODEM_WITH_CRC
static uint16_t _xmodem_calc_crc(int *buffer, int len)
{
	int i;
	uint16_t crc = 0;

#if XMODEM_WITH_PACKEDBUF
	for (i = 0; i < (len >> 1); i++)
	{
		crc = (crc << 8) ^ crctable[((crc >> 8) ^ (buffer[i] & 0xff)) & 0xff];
		crc = (crc << 8) ^ crctable[((crc >> 8) ^ (buffer[i] >> 8)) & 0xff];
	}
#else
	for (i = 0; i < len; i++)
	{
		crc = (crc << 8) ^ crctable[((crc >> 8) ^ (buffer[i] & 0xff)) & 0xff];
	}
#endif

	return crc;
}
#endif


#if XMODEM_USE_OLD_API
XMODEM_Handle XMODEM_init(struct SCI_REGS *sciHandle);
#else
XMODEM_Handle XMODEM_init(SCI_Handle sciHandle)
#endif
{
	static struct _XMODEM_Obj_ x;
	XMODEM_Handle xmodemHandle = (struct XMODEM_Obj *)&x;

	memset(xmodemHandle, 0, sizeof(XMODEM_Obj));
    x.sci = sciHandle;
    x.retry = XMODEM_DEFAULT_RETRY;

    return (xmodemHandle);
}

void XMODEM_setWriter(XMODEM_Handle xmodemHandle, BUF_Op_cb writerCallback, long cbData)
{
    XMODEM_Obj *x = (XMODEM_Obj *)xmodemHandle;

    x->writer = writerCallback;
    x->writer_data = cbData;
}

void XMODEM_setReader(XMODEM_Handle xmodemHandle, BUF_Op_cb readerCallback, long cbData)
{
    XMODEM_Obj *x = (XMODEM_Obj *)xmodemHandle;

    x->reader = readerCallback;
    x->reader_data = cbData;
}

void XMODEM_setEnd(XMODEM_Handle xmodemHandle, BUF_Op_cb endCallback, long cbData)
{
    XMODEM_Obj *x = (XMODEM_Obj *)xmodemHandle;

    x->end = endCallback;
    x->end_data = cbData;
}

void XMODEM_setRetry(XMODEM_Handle xmodemHandle, int retry)
{
    XMODEM_Obj *x = (XMODEM_Obj *)xmodemHandle;

    x->retry = retry;
}

void XMODEM_abort(XMODEM_Handle xmodemHandle, int count)
{
    int i;
    XMODEM_Obj *x = (XMODEM_Obj *)xmodemHandle;

    for (i = 0; i < count; i++)
    {
        _xmodem_putc(x, CAN);
    }
}

int XMODEM_end(long data, int *buffer, int bufsize)
{
    /* End of transmission */
    _xmodem_putc((XMODEM_Obj *)data, EOT);

    /* Wait a bit */
	DELAY_US(2000000L);

	return 1;
}

int XMODEM_send(XMODEM_Handle xmodemHandle, int *buffer, int bufsize)
{
    XMODEM_Obj *x = (XMODEM_Obj *)xmodemHandle;
    int error_count = 0;
    int packet_size;
#if XMODEM_WITH_CRC
    int crc_mode = 0;
#endif
    int len = bufsize;
    int cancel = 0;
    int sequence;
    uint16_t success;
    uint16_t crc;
    uint16_t c;

    /* Initialize protocol */
    for (;;)
    {
        c = _xmodem_getc_t(x, &success);
        if (success != 0)
        {
        	switch (c)
        	{
        	case NAK:
#if XMODEM_WITH_CRC
        		crc_mode = 0;
            	goto init_done;

	        case CRC:
    	        crc_mode = 1;
#endif
        	    goto init_done;

	        case CAN:
    	        if (cancel)
        	        return 0;
            	else
                	cancel = 1;
        	}
        }
    	error_count++;

        if (error_count >= x->retry)
        	goto abort;
    }
init_done:

    /* Send data */
    error_count = 0;
    packet_size = 128;
    sequence = 1;

#if XMODEM_WITH_BUFFER
	if (buffer == NULL)
	{
		buffer = _xmodem_buffer;
		bufsize = PACKET_SIZE;
	}
#else
	if (!len || buffer == NULL)
		return 0;
#endif
	if (x->writer)
		len = x->writer(x->writer_data, buffer, bufsize);

    for (;;)
    {
    	if (len < packet_size)
    		memset(buffer+len, '\x1a', packet_size-len);
#if XMODEM_WITH_CRC
    	if (crc_mode)
    		crc = _xmodem_calc_crc(buffer, packet_size);
    	else
#endif
    		crc = _xmodem_calc_checksum(buffer, packet_size);

    	/* Emit packet */
    	for (;;)
    	{
    		_xmodem_putc(x, SOH);
    		_xmodem_putc(x, sequence);
    		_xmodem_putc(x, 0xff - sequence);
    		_xmodem_putm(x, buffer, packet_size);
#if XMODEM_WITH_CRC
    		if (crc_mode)
    		{
    			_xmodem_putc(x, crc >> 8);
    			_xmodem_putc(x, crc & 0xff);
    		}
    		else
#endif
    			_xmodem_putc(x, crc);

            c = _xmodem_getc_t(x, &success);
            if (c == ACK)
            	break;
            else if (c == NAK)
            {
            	error_count++;
            	if (error_count >= x->retry)
            		goto abort;
            }
            else /* Protocol abort */
            	goto abort;
    	}

    	/* Keep track of sequence */
    	sequence = (sequence +1 ) & 0xff;

    	/* Call callback to get more data */
    	if (x->writer)
    	{
    		len = x->writer(x->writer_data, buffer, bufsize);
    	}
    	else if (len > packet_size)
    	{
    		buffer += PACKET_SIZE;
    		len -= packet_size;
    	}
    	else
    		len = 0;

    	if (len == 0)
    		break;
    }

    if (x->end)
    {
    	x->end(x->end_data, buffer, bufsize);
    }

    return XMODEM_end((long)xmodemHandle, NULL, 0);

abort:
	XMODEM_abort(xmodemHandle, XMODEM_ABORT_COUNT);

    return 0;
}

int XMODEM_recv(XMODEM_Handle xmodemHandle, int *buffer, int bufsize)
{
    XMODEM_Obj *x = (XMODEM_Obj *)xmodemHandle;
    int error_count = 0;
    int packet_size;
    long income_size;
    int cancel = 0;
    int sequence;
    int seq1, seq2;
    int valid;
#if XMODEM_WITH_CRC
    int crc_mode = 1;
#endif
    uint16_t success;
    uint16_t csum;
    uint16_t c;

    /* Initiate protocol */
    for (;;)
    {
    	/* First try CRC mode, if this fails,
    	 * fall back to checksum mode */
    	if (error_count >= x->retry)
    		goto abort;
#if XMODEM_WITH_CRC
    	if (crc_mode && (error_count < (x->retry >> 1)))
    	{
    		if (!_xmodem_putc(x, CRC))
    		{
    			DELAY_US(XMODEM_DELAY);
    			error_count++;
    		}
    	}
    	else
#endif
    	{
#if XMODEM_WITH_CRC
    		crc_mode = 0;
#endif
    		if (!_xmodem_putc(x, NAK))
    		{
    			DELAY_US(XMODEM_DELAY);
    			error_count++;
    		}
    	}

    	c = _xmodem_getc(x, &success);
    	if (!success)
    	{
    		error_count++;
    		continue;
    	}
    	switch (c)
    	{
    	case SOH:
    		/* crc_mode = 0 */
    		goto init_done;

    	case STX:
//    	case CAN:
    		goto init_done;

    	case CAN:
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
    packet_size = 128;
    sequence = 1;
    cancel = 0;

#if XMODEM_WITH_BUFFER
	if (buffer == NULL)
	{
		buffer = _xmodem_buffer;
		bufsize = PACKET_SIZE;
	}
#else
	if (!len || buffer == NULL)
		return 0;
#endif

	for (;;)
    {
    	for (;;)
    	{
    		if (c == SOH)
    		{
    			packet_size = 128;
    			break;
    		}
#if 0
    		/* NOTE: 1k block is not supported */
    		else if (c == STX)
    		{
    			packet_size = 1024;
    			break;
    		}
#endif
    		else if (c == EOT)
    		{
				_xmodem_putc(x, ACK);
			    if (x->end)
			    {
			    	x->end(x->end_data, buffer, bufsize);
			    }
    			DELAY_US(2000000L);
    			return income_size;
    		}
    		else if (c == CAN)
    		{
    			if (cancel)
    				return 0;
    			else
    				cancel = 1;
    		}
    		else
    		{
    	    	c = _xmodem_getc_t(x, &success);
    			error_count++;
    			if (error_count >= x->retry)
    				goto abort;
    		}
    	}

    	/* Read sequence */
    	error_count = 0;
    	cancel = 0;
    	seq1 = _xmodem_getc(x, &success);
    	seq2 = 0xff - _xmodem_getc(x, &success);
   		if (seq1 == sequence && seq2 == sequence)
   		{
   			/* Sequence is ok, read packet
    		 * packet_size + checksum */
			if (bufsize < 0)
			{
				/* No more buffer available */
				goto abort;
			}
#if XMODEM_WITH_CRC

    		if (_xmodem_getm(x, buffer, packet_size + 1 + crc_mode) != packet_size + 1 + crc_mode)
#else
   			if (_xmodem_getm(x, buffer, packet_size + 1) != packet_size + 1)
#endif
   			{
   				error_count++;
       			if (error_count >= x->retry)
       				goto abort;
   				goto nak;
   			}
#if XMODEM_WITH_CRC
   			if (crc_mode)
   			{
#if XMODEM_WITH_PACKEDBUF
   				csum  = (uint16_t)buffer[packet_size >> 1] >> 8;
   				csum |= (buffer[packet_size >> 1] & 0xff) << 8;
#else
   				csum = buffer[packet_size] << 8 | buffer[packet_size+1];
#endif
   				valid = csum == _xmodem_calc_crc(buffer, packet_size);
   			}
   			else
#endif
   			{
   				csum = buffer[PACKET_SIZE] & 0xff;
   				valid = csum == _xmodem_calc_checksum(buffer, packet_size);
   			}

   			if (valid)
   			{
//   				income_size += packet_size;
   				income_size++;
   				if (x->reader)
   				{
   					bufsize = x->reader(x->reader_data, buffer, packet_size);
   					/* Error has been occurred, re-request the packet */
   					if (bufsize < 0)
   						goto nak;
   				}
   				else if (bufsize >= packet_size)
   				{
					bufsize -= packet_size;
					buffer += PACKET_SIZE;
   				}
   				_xmodem_putc(x, ACK);
   				sequence = (sequence + 1) & 0xff;

   				c = _xmodem_getc_t(x, &success);
   				continue;
   			}
   		}
   		else
   		{
   			/* Consume data */
#if XMODEM_WITH_CRC
   			_xmodem_getm(x, NULL, packet_size + 1 + crc_mode);
#else
   			_xmodem_getm(x, NULL, packet_size + 1);
#endif
   		}

nak:
    	/* Something wen wrong, request retransmission */
    	_xmodem_putc(x, NAK);
    	c = _xmodem_getc_t(x, &success);
    }

abort:
    XMODEM_abort(xmodemHandle, XMODEM_ABORT_COUNT);
    return 0;
}

#if XMODEM_WITH_PACKEDBUF
// TODO: …to be completed
void XMODEM_unpack(int16_t *buffer, int bufsize)
{
	while (bufsize-- > 1)
	{
		buffer[bufsize] = buffer[bufsize >> 1];
	}
}
#endif
