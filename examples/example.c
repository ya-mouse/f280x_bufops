// #define _FLASH

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
