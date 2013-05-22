/*
 *  Sample part of a linker script
 *

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
        -lrts*.lib<memset.obj>(.text)
        -lrts*.lib<memcpy_ff.obj>(.text)
        -lrts*.lib<strtol.obj>(.text)
        -lrts*.lib<ctype.obj>(.econst)
        -lbufops.lib(.econst) 
        -lbufops.lib(.text)
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
void MY_local_fwup(int flg)
{
	HEXBUF_Handle myHexbuf;
	XMODEM_Handle myXmodem;

	__disable_interrupts();

	// Make sure, that SciaRegs is configured to work in polling mode
	myXmodem = XMODEM_init(SciaRegs);

	Flash_CPUScaleFactor = SCALE_FACTOR;
	Flash_CallbackPtr = NULL;

	myHexbuf = HEXBUF_init();
	if (!flg)
	{
		scia_msg("\r\nDisable flashing\r\n");
		HEXBUF_disableFlashing(myHexbuf);
	}
	else
	{
		scia_msg("\r\nEnable flashing\r\n");
		HEXBUF_enableFlashing(myHexbuf);
	}
	XMODEM_setReader(myXmodem, HEXBUF_reader, (long)myHexbuf);
	if (XMODEM_recv(myXmodem, HEXBUF_getBuffer(myHexbuf), 128))
		HEXBUF_start(myHexbuf);

	WDOG_reset();
}

void main()
{
	// ...
	memcpy(&Flash28_API_RunStart, &Flash28_API_LoadStart, (size_t)(&Flash28_API_LoadEnd-&Flash28_API_LoadStart));

	// ...
	MY_local_fwup(1);
}
