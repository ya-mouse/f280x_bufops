#ifndef BUFOPS_H_
#define BUFOPS_H_

#include "f280x_bufops/Flash280x_API_Library.h"

/* Define prototype for writer and reader callbacks
 * to update output buffer and
 * to process input buffer respectively
 */
typedef int (*BUF_Op_cb)(long data, int *buffer, int len);

/* Put Buffer routines and data into RAM sections (buframfuncs and buframdata) */
#define BUFFER_USE_RAM 0

#if BUFFER_USE_RAM
#define BUF_FUNCS_SECT	"buframfuncs"
#define BUF_DATA_SECT 	"buframdata"
#endif

extern int __protected_fn_start;
extern int __protected_fn_end;
extern int __protected_dt_start;
extern int __protected_dt_end;

extern void far *xmemcpy(void far *to, const void far *from, size_t n);
extern long xstrtol(const char *st, char **endptr, int base);

extern const _DATA_ACCESS unsigned char _xctypes_[];

/************************************************************************/
/*  MACRO DEFINITIONS                                                   */
/************************************************************************/
#define _U_   0x01       /* control chars     */
#define _L_   0x02       /* lower case letter */
#define _N_   0x04       /* digit             */
#define _S_   0x08       /* white space       */
#define _P_   0x10       /* punctuation       */
#define _C_   0x20       /* control chars     */
#define _H_   0x40       /* A-F, a-f and 0-9  */
#define _B_   0x80       /* blank             */

#define _isalnum(a)  (_xctypes_[(a)+1] & (_U_ | _L_ | _N_))
#define _isalpha(a)  (_xctypes_[(a)+1] & (_U_ | _L_))
#define _iscntrl(a)  (_xctypes_[(a)+1] & _C_)
#define _isdigit(a)  (_xctypes_[(a)+1] & _N_)
#define _isgraph(a)  (_xctypes_[(a)+1] & (_U_ | _L_ | _N_ | _P_))
#define _islower(a)  (_xctypes_[(a)+1] & _L_)
#define _isprint(a)  (_xctypes_[(a)+1] & (_B_ | _U_ | _L_ | _N_ | _P_))
#define _ispunct(a)  (_xctypes_[(a)+1] & _P_)
#define _isspace(a)  (_xctypes_[(a)+1] & _S_)
#define _isupper(a)  (_xctypes_[(a)+1] & _U_)
#define _isxdigit(a) (_xctypes_[(a)+1] & _H_)


#define _isascii(a)  (((a) & ~0x7F) == 0)
#define _toupper(b)  ((_islower(b)) ? (b) - ('a' - 'A') : (b))
#define _tolower(b)  ((_isupper(b)) ? (b) + ('a' - 'A') : (b))
#define _toascii(a)  ((a) & 0x7F)

#endif /* BUFOPS_H_ */
