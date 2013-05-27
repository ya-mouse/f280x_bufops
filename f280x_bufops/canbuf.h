#ifndef CAN_BUF_H_
#define CAN_BUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "f280x_bufops/bufops.h"

#define CANBUF_DELAY		 1000000L
#define CANBUF_TIMEOUT 		 15
#define CANBUF_ABORT_COUNT	 2
#define CANBUF_DEFAULT_RETRY 20

/* Buffer contains ASCII characters. Pack them. */
#ifndef CANBUF_WITH_ASCIIBUF
#define CANBUF_WITH_ASCIIBUF 1
#endif

//! \brief Defines the CANBUF object
//!
typedef struct _CANBUF_Obj_
{
	int msgid;
	int retry;
	BUF_Op_cb writer;
	BUF_Op_cb reader;
	BUF_Op_cb end;
	long writer_data;
	long reader_data;
	long end_data;
	int handshaked;
	int sequence;
	volatile struct ECAN_REGS *regs;
	volatile struct ECAN_MBOXES *mbx;
} CANBUF_Obj;


//! \brief Defines the hexbuf (CANBUF) handle
//!
typedef struct CANBUF_Obj *CANBUF_Handle;

typedef enum {
	CANBUF_Mode_Send = 0,
	CANBUF_Mode_Receive
} CANBUF_Mode_e;

/* Initialize CANBUF object */
/* mode is send or receive */
/* msgid is the first MBX MSGID for the next 16 MBOXes */
/* takes 16-31 MBOX */
CANBUF_Handle CANBUF_init(volatile struct ECAN_REGS *regs, const CANBUF_Mode_e mode, int msgid);

/* Write input data via eCAN interface */
int CANBUF_reader(long data, int *buffer, int len);

/* Write input data via eCAN interface */
int CANBUF_writer(long data, int *buffer, int len);

/* To be called at the end of transmission */
int CANBUF_end(long data, int *buffer, int len);

void CANBUF_setWriter(CANBUF_Handle canbufHandle, BUF_Op_cb writerCallback, long cbData);

void CANBUF_setReader(CANBUF_Handle canbufHandle, BUF_Op_cb writerCallback, long cbData);

void CANBUF_setEnd(CANBUF_Handle canbufHandle, BUF_Op_cb endCallback, long cbData);

int CANBUF_send(CANBUF_Handle canbufHandle, int *buffer, int bufsize);

int CANBUF_recv(CANBUF_Handle canbufHandle, int *buffer, int bufsize);

#ifdef __cplusplus
}
#endif // extern "C"

#endif /* CAN_BUF_H_ */
