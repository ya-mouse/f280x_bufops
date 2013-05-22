#include "DSP28x_Project.h"

#include "f280x_bufops/hexbuf.h"

#if BUFFER_USE_RAM
#define _INLINE 1
#endif
#include <stdlib.h>
#include <string.h>

HEXBUF_Handle HEXBUF_init()
{
	static struct _HEXBUF_Obj_ hexbuf;
	HEXBUF_Handle bufHandle = (struct HEXBUF_Obj *)&hexbuf;

	memset(bufHandle, 0, sizeof(HEXBUF_Obj));

	hexbuf.addr = -1;

    return (bufHandle);
}

int HEXBUF_writer(long data, int *buffer, int len)
{
    //HEXBUF_Obj *bufobj = (HEXBUF_Obj *)data;

    return 0;
}

#if HEXBUF_USE_FLASHAPI
static int _flash_erase(HEXBUF_Obj *bufobj)
{
	Uint16 status = STATUS_SUCCESS;
	FLASH_ST flash_status;

	if (bufobj->flashing_disabled)
		return STATUS_SUCCESS;

	status = Flash_Erase(HEXBUF_SECTORS, &flash_status);
	if (status == STATUS_SUCCESS)
		bufobj->erased = 1;

	return (status);
}

static int _flash_write(HEXBUF_Obj *bufobj)
{
	int len;
	Uint16 status;
	FLASH_ST flash_status;

	len = bufobj->outidx;
	bufobj->outidx = 0;

	if (bufobj->flashing_disabled)
		goto increment_outaddr;

	status = Flash_Program((Uint16 *)bufobj->outaddr,
						   bufobj->output,
						   len,
						   &flash_status);

	if (status != STATUS_SUCCESS)
		return -status;

	status = Flash_Verify((Uint16 *)bufobj->outaddr,
						  bufobj->output,
						  len,
						  &flash_status);

	if (status != STATUS_SUCCESS)
		return -status;

increment_outaddr:
	bufobj->outaddr += len;

	return (len);
}
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
#ifdef HEXBUF_USE_FLASHAPI
    int rc;

    bufobj->outidx = 0;
#endif

    /* Prepend last tailed buffer */
	p = bufobj->buffer;
	len += 5;
	p[len] = '\0';

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
    				goto copy_buf;
    		case 'A':
    		case 'E':
    			/* Not enough buffer input is available, request more */
    			if (len < 5)
	    		{
copy_buf:
					memset(bufobj->buffer, '\0', 5);
    				xmemcpy(bufobj->buffer+(4-len), p, len+1);
    				goto next_buf;
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
#if HEXBUF_USE_FLASHAPI
    					if (!bufobj->erased)
    					{
    						rc = _flash_erase(bufobj);
    						if (rc < 0)
    							return rc;
    					}

    					/* Flush data to the Flash */
    					if (bufobj->outidx != 0)
    					{
   							rc = _flash_write(bufobj);
   							if (rc < 0)
   								return rc;
    					}
   						bufobj->outaddr = addr;
#endif
    					break;

    				case 'D':
    					/* Protect stack */
#if 1
    					/* RAMM0 */
    					if ((addr >= 0x3B0 && addr < 0x400)
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
    					if (addr >= FLASH_START_ADDR && addr <= FLASH_END_ADDR)
    					{
#if HEXBUF_USE_FLASHAPI
    						/* Flush data to the Flash */
    						if (bufobj->outidx == sizeof(bufobj->output) || bufobj->outaddr+bufobj->outidx != addr)
    						{
    							rc = _flash_write(bufobj);
    							if (rc < 0)
    								return rc;
    						}

    						bufobj->output[bufobj->outidx++] = xdig & 0xffff;
#endif
    						addr++;
    					}
    					else
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

#if HEXBUF_USE_FLASHAPI
	/* Flush data to the Flash */
    if (bufobj->outidx != 0)
    {
    	rc = _flash_write(bufobj);
		if (rc < 0)
			return rc;
    }
#endif

    return bufsize;
}

void HEXBUF_setEntryPoint(HEXBUF_Handle hexbufHandle, long entry)
{
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)hexbufHandle;

    bufobj->entry = entry;
}

void HEXBUF_disableFlashing(HEXBUF_Handle hexbufHandle)
{
#if HEXBUF_USE_FLASHAPI
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)hexbufHandle;

    bufobj->flashing_disabled = 1;
#endif
}

int *HEXBUF_getBuffer(HEXBUF_Handle hexbufHandle)
{
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)hexbufHandle;

    return (int *)(bufobj->buffer+5);
}

void HEXBUF_start(HEXBUF_Handle hexbufHandle)
{
    HEXBUF_Obj *bufobj = (HEXBUF_Obj *)hexbufHandle;
    void (*start)() = (void (*)())bufobj->entry;

    // TODO: enable WDog

    /* Branch to entry point */
    if (start != NULL)
    	start();
}
