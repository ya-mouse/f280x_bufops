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
#ifndef XMODEM_H_
#define XMODEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "f280x_bufops/bufops.h"

#define XMODEM_DELAY		1000000L
#define XMODEM_TIMEOUT 		15
#define XMODEM_ABORT_COUNT	2

/* Packed buffer is good enough for binary data transfer directly to/from memory.
 * Use unpacked buffer for ASCII transfers */
#ifndef XMODEM_WITH_PACKEDBUF
#define XMODEM_WITH_PACKEDBUF 0
#endif

/* Use internal XMODEM library buffer to store intermediate data.
 * Usefull with update/process callbacks */
#ifndef XMODEM_WITH_BUFFER
#define XMODEM_WITH_BUFFER 0
#endif

/* Use CRC-16 checksums */
#ifndef XMODEM_WITH_CRC
#define XMODEM_WITH_CRC 1
#endif

/* Put XMODEM routines and data into RAM sections (xmodemramfuncs and xmodemramdata) */
#ifndef XMODEM_USE_RAM
#define XMODEM_USE_RAM 0
#endif

#define XMODEM_FUNCS_SECT "buframfuncs"
#define XMODEM_DATA_SECT "buframdata"

#if XMODEM_USE_RAM
#ifndef XMODEM_FUNCS_SECT
#define XMODEM_FUNCS_SECT 	"xmodemramfuncs"
#endif
#ifndef XMODEM_DATA_SECT
#define XMODEM_DATA_SECT	"xmodemramdata"
#endif
#endif

#define XMODEM_DEFAULT_RETRY 20

/* Use old-style API with SCI regs and bit fields definitions */
#ifndef XMODEM_USE_OLD_API
#define XMODEM_USE_OLD_API 0
#endif

#if !XMODEM_USE_OLD_API
#include "f280x_common/include/sci.h"
#endif


//! \brief Defines the XMODEM object
//!
typedef struct _XMODEM_Obj_
{
#if XMODEM_USE_OLD_API
	struct SCI_REGS *sci;
#else
	SCI_Handle sci;
#endif
	int retry;
	BUF_Op_cb writer;
	BUF_Op_cb reader;
	long writer_data;
	long reader_data;
} XMODEM_Obj;


//! \brief Defines the xmodem (XMODEM) handle
//!
typedef struct XMODEM_Obj *XMODEM_Handle;

#if XMODEM_USE_OLD_API
XMODEM_Handle XMODEM_init(struct SCI_REGS *sciHandle);
#else
XMODEM_Handle XMODEM_init(SCI_Handle sciHandle);
#endif

void XMODEM_setWriter(XMODEM_Handle xmodemHandle, BUF_Op_cb writerCallback, long cbData);

void XMODEM_setReader(XMODEM_Handle xmodemHandle, BUF_Op_cb readerCallback, long cbData);

void XMODEM_setRetry(XMODEM_Handle xmodemHandle, int retry);

void XMODEM_abort(XMODEM_Handle xmodemHandle, int count);

int XMODEM_send(XMODEM_Handle xmodemHandle, int *buffer, int bufsize);

int XMODEM_recv(XMODEM_Handle xmodemHandle, int *buffer, int bufsize);

#if XMODEM_WITH_PACKEDBUF
void XMODEM_unpack(int *buffer, int bufsize);
#else
#define XMODEM_unpack(buffer, bufsize)
#endif

#ifdef __cplusplus
}
#endif // extern "C"

#endif /* XMODEM_H_ */
