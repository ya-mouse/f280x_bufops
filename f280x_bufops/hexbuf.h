#ifndef HEX_BUF_H_
#define HEX_BUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "f280x_bufops/bufops.h"

#define HEXBUF_USE_FLASHAPI 1

#if HEXBUF_USE_FLASHAPI
#define HEXBUF_SECTORS	(SECTORC|SECTORD)
#endif

//! \brief Defines the HEXBUF object
//!
typedef struct _HEXBUF_Obj_
{
	long entry;
	long addr;
	char token;
	char buffer[5+130];
#if HEXBUF_USE_FLASHAPI
	Uint16 output[16];
	int outidx;
	long outaddr;
	int erased;
	int flashing_disabled;
#endif
} HEXBUF_Obj;


//! \brief Defines the hexbuf (HEXBUF) handle
//!
typedef struct HEXBUF_Obj *HEXBUF_Handle;


/* Initialize HEXBUF object */
HEXBUF_Handle HEXBUF_init();

/* Read data from memory and output it as HEX-format */
int HEXBUF_writer(long data, int *buffer, int len);

/* Write HEX-formatted data to the memory / flash */
int HEXBUF_reader(long data, int *buffer, int len);

/* Set EntryPoint address */
void HEXBUF_setEntryPoint(HEXBUF_Handle hexbufHandle, long entry);

/* Disable flashing */
void HEXBUF_disableFlashing(HEXBUF_Handle hexbufHandle);

/* Enable flashing */
void HEXBUF_enableFlashing(HEXBUF_Handle hexbufHandle);

/* Return pointer to the internal buffer with 4-char offset for the previous tail */
int *HEXBUF_getBuffer(HEXBUF_Handle hexbufHandle);

void HEXBUF_start(HEXBUF_Handle hexbufHandle);

#ifdef __cplusplus
}
#endif // extern "C"

#endif /* HEX_BUF_H_ */
