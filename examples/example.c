/*
 *  Sample part of a linker script
 *

-u _memset
-u _strtol

MEMORY
{
// ...

   RAML0   : origin = 0x008000, length = 0x000E00
   FLASHD  : origin = 0x3E8000, length = 0x003000

// ...
}

SECTIONS
{
   Flash28_API:
   {
        -lFlash280*_API_V30*.lib(.econst) 
        -lFlash280*_API_V30*.lib(.text)
        -lbufops.lib(.econst) 
        -lbufops.lib(.text)
        -lrts*.lib<ctype.obj>(.econst)
        -lrts*.lib<strtol.obj,memset.obj>(.text)
   }                   LOAD = FLASHD, 
                       RUN = RAML0, 
                       LOAD_START(_Flash28_API_LoadStart),
                       LOAD_END(_Flash28_API_LoadEnd),
                       RUN_START(_Flash28_API_RunStart),
                       PAGE = 0
    // ...
}

*/


#define _FLASH

#define XMODEM_USE_OLD_API 1

#include "f280x_bufops/xmodem.h"
#include "f280x_bufops/hexbuf.h"

#ifdef _FLASH
#pragma CODE_SECTION(LEVELER_local_fwup, "Flash28_API")
#endif
/*
 * Example routine to flash ourselves with Xmodem or eCAN source
 * using HEXBUF format produced from `out' file by out2hex.pl tool
 */
void MY_local_fwup(int flg)
{
	HEXBUF_Handle myHexbuf;
	XMODEM_Handle myXmodem;
	CANBUF_Handle myCanbuf;
	Uint16 status;
	FLASH_ST flash_status;

	do
	{
		__disable_interrupts();

		if (!(flg & 0x10))
		{
			// Make sure, that SciaRegs is configured to work in polling mode
			scia_msg("\r\nUsing XMODEM. ");
			myXmodem = XMODEM_init(mySci);
		}
		else
		{
			scia_msg("\r\nUsing CANBUF. ");
			myCanbuf = CANBUF_init(&ECanaRegs, CANBUF_Mode_Receive, 0x400);
		}

		Flash_CPUScaleFactor = SCALE_FACTOR;
		Flash_CallbackPtr = NULL;

		myHexbuf = HEXBUF_init();
		if (!(flg & 0xf))
		{
			scia_msg("Disable flashing\r\n");
			HEXBUF_disableFlashing(myHexbuf);
		}
		else
		{
			scia_msg("Erasing, please wait...\r\n");
			HEXBUF_enableFlashing(myHexbuf);

			status = Flash_Erase(HEXBUF_SECTORS, &flash_status);
			if (status != STATUS_SUCCESS)
			{
				scia_printf("failed: %d\r\n", status);
				break;
			}
		}
		if (!(flg & 0x10))
		{
			XMODEM_setReader(myXmodem, HEXBUF_reader, (long)myHexbuf);
			XMODEM_setEnd(myXmodem, HEXBUF_end, (long)myHexbuf);
			XMODEM_recv(myXmodem, HEXBUF_getBuffer(myHexbuf), 128);
		}
		else
		{
			CANBUF_setReader(myCanbuf, HEXBUF_reader, (long)myHexbuf);
			CANBUF_setEnd(myCanbuf, HEXBUF_end, (long)myHexbuf);
			CANBUF_recv(myCanbuf, HEXBUF_getBuffer(myHexbuf), 128);
		}
	}
	while (flg == 2);

	WDOG_reset();
}

/*
 * Example routine to update remote controller via eCAN with Xmodem source over SCI-A
 */
void MY_ecan_send()
{
	CANBUF_Handle myCanbuf;
	XMODEM_Handle myXmodem;

	myCanbuf = CANBUF_init(&ECanaRegs, CANBUF_Mode_Send, 0x400);
	myXmodem = XMODEM_init(mySci);

	XMODEM_setReader(myXmodem, CANBUF_reader, (long)myCanbuf);
	XMODEM_setEnd(myXmodem, CANBUF_end, (long)myCanbuf);
	scia_printf("\r\necan_send=%d\r\n", XMODEM_recv(myXmodem, NULL, 128));
}

void main()
{
	// ...
	memcpy(&Flash28_API_RunStart, &Flash28_API_LoadStart, (size_t)(&Flash28_API_LoadEnd-&Flash28_API_LoadStart));

	// ...
	MY_local_fwup(1);
}
