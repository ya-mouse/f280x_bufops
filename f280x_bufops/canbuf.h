#ifndef CAN_BUF_H_
#define CAN_BUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "f280x_bufops/bufops.h"

//! \brief Defines the CANBUF object
//!
typedef struct _CANBUF_Obj_
{
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
CANBUF_Handle CANBUF_init(int base, const CANBUF_Mode_e mode, int msgid);

/* Write input data via eCAN interface */
int CANBUF_reader(long data, int *buffer, int len);

int CANBUF_recv(CANBUF_Handle canbufHandle);

#ifdef __cplusplus
}
#endif // extern "C"

#endif /* CAN_BUF_H_ */
