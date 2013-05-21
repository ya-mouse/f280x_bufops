#ifndef BUFOPS_H_
#define BUFOPS_H_

#include "f280x_bufops/Flash280x_API_Library.h"

/* Define prototype for writer and reader callbacks
 * to update output buffer and
 * to process input buffer respectively
 */
typedef int (*BUF_Op_cb)(long data, int *buffer, int len);

/* Put Buffer routines and data into RAM sections (buframfuncs and buframdata) */
#define BUFFER_USE_RAM 1

extern int __protected_fn_start;
extern int __protected_fn_end;
extern int __protected_dt_start;
extern int __protected_dt_end;

extern void far *xmemcpy(void far *to, const void far *from, size_t n);

#endif /* BUFOPS_H_ */
