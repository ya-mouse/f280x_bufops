#include "DSP28x_Project.h"

#if BUFFER_USE_RAM
#define _INLINE 1
#endif
#include <stdlib.h>
#include <string.h>

#include "f280x_bufops/hexbuf.h"

#if 0
#if BUFFER_USE_RAM
#pragma CODE_SECTION(xmemcpy, BUF_FUNCS_SECT)
#endif
void far *xmemcpy(void far *to, const void far *from, size_t n)
{
     register char far *rto   = (char far *) to;
     register char far *rfrom = (char far *) from;
     register unsigned int rn;
     register unsigned int nn = (unsigned int) n;

     /***********************************************************************/
     /*  Assume that the size is < 64K and do the memcpy. At the end compare*/
     /*  the number of chars moved with the size n. If they are equal       */
     /*  return. Else continue to copy the remaining chars.                 */
     /***********************************************************************/
     for (rn = 0; rn < nn; rn++) *rto++ = *rfrom++;
     if (nn == n) return (to);

     /***********************************************************************/
     /*  Write the memcpy of size >64K using nested for loops to make use   */
     /*  of BANZ instrunction.                                              */
     /***********************************************************************/
     {
        register unsigned int upper = (unsigned int)(n >> 16);
        register unsigned int tmp;
        for (tmp = 0; tmp < upper; tmp++)
	{
           for (rn = 0; rn < 65535; rn++)
		*rto++ = *rfrom++;
	   *rto++ = *rfrom++;
	}
     }

     return (to);
}
#endif

#if BUFFER_USE_RAM
#pragma CODE_SECTION(HEXBUF_init, BUF_FUNCS_SECT)
#endif
HEXBUF_Handle HEXBUF_init()
{
	static struct _HEXBUF_Obj_ hexbuf;
	HEXBUF_Handle bufHandle = (struct HEXBUF_Obj *)&hexbuf;

	memset(bufHandle, 0, sizeof(HEXBUF_Obj));

	hexbuf.addr = -1;

    return (bufHandle);
}

#if BUFFER_USE_RAM
#pragma CODE_SECTION(HEXBUF_writer, BUF_FUNCS_SECT)
#endif
int HEXBUF_writer(long data, int *buffer, int len)
{
    //HEXBUF_Obj *bufobj = (HEXBUF_Obj *)data;

    return 0;
}

#if BUFFER_USE_RAM
#pragma CODE_SECTION(HEXBUF_reader, BUF_FUNCS_SECT)
#endif
int HEXBUF_reader(long data, int *buffer, int len)
{
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)data;
    char *p  = (char *)buffer;
    char *ep;
    char tok = bufobj->token;
    int bufsize = len;
    long xdig;
    long addr = bufobj->addr;

    /* Prepend last tailed buffer */
	p = bufobj->buffer;
	len += 5;

	while (len--)
    {
    	if (*p != '\0')
    	{
    		switch (tok) {
    		case '\0':
    			/* Search for the first '$' occurrence */
	    		if (*p == '$')
    				tok = '$';
    			break;

	    	case '$':
    			switch (*p) {
    			case 'A':
    			case 'E':
    				tok = *p;
    				break;

	    		default:
    			/* Wrong data, search for next '$' */
    				tok = '\0';
    				break;
    			}
    			break;

	    	case '\n':
    			/* Check for more data */
    			if (*p == '\n' || *p == '\r')
    				break;
    			else if (*p == '$')
	    			tok = '$';
	    		else if (*p == '\x1a' || *p == 0xff)
	    			goto next_buf;
	    		/* EOL reached, expect DATA then */
    			else if (addr != -1)
    			{
    				tok = 'D';
    				len++;
    				continue;
    			}
    			break;

	    	case 'D':
    			if (len < 4)
    				goto copybuf;
    		case 'A':
    		case 'E':
    			/* Not enough buffer input is available, request more */
    			if (len < 5)
	    		{
copybuf:
					memset(bufobj->buffer, '\0', 5);
    				memcpy(bufobj->buffer+(4-len), p, len+1);
    				continue;
    			}
    			ep = NULL;
	    		xdig = strtol(p, &ep, 16);
	    		if (p != ep && ep != NULL)
	    		{
	    			len -= (ep - p) - 1;
	    			p = ep;
	    			switch (tok) {
	    			case 'A':
    					addr = xdig;
    					break;

    				case 'D':
    					/* Protect stack */
#ifdef _FLASH
    					/* RAMM0 */
    					if ((addr >= 0x000 && addr < 0x400)
#else
    					/* RAMM1 */
    					if ((addr >= 0x400 && addr < 0x800)
#endif
#if 0
    							|| (addr >= (int32_t)&__protected_fn_start && addr < (int32_t)&__protected_fn_end)
    							|| (addr >= (int32_t)&__protected_dt_start && addr < (int32_t)&__protected_dt_end))
#else
    						)
#endif
    					{
    						addr++;
    						break;
    					}
    					*((int *)addr++) = xdig & 0xffff;

    					break;

    				case 'E':
    					bufobj->entry = xdig;
    					break;
    				}
	    		}

    			tok = '\n';
    			continue;
    		}
    	}

   		p++;
    }

	memset(bufobj->buffer, '\0', 5);

next_buf:
    bufobj->addr  = addr;
    bufobj->token = tok;

    return bufsize;
}

#if BUFFER_USE_RAM
#pragma CODE_SECTION(HEXBUF_setEntryPoint, BUF_FUNCS_SECT)
#endif
void HEXBUF_setEntryPoint(HEXBUF_Handle hexbufHandle, long entry)
{
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)hexbufHandle;

    bufobj->entry = entry;
}

#if BUFFER_USE_RAM
#pragma CODE_SECTION(HEXBUF_getBuffer, BUF_FUNCS_SECT)
#endif
int *HEXBUF_getBuffer(HEXBUF_Handle hexbufHandle)
{
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)hexbufHandle;

    return (int *)(bufobj->buffer+5);
}

#if BUFFER_USE_RAM
#pragma CODE_SECTION(HEXBUF_start, BUF_FUNCS_SECT)
#endif
void HEXBUF_start(HEXBUF_Handle hexbufHandle)
{
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)hexbufHandle;
    void (*start)() = (void (*)())bufobj->entry;

    // TODO: enable WDog

    /* Branch to entry point */
    if (start != NULL)
    	start();
}
