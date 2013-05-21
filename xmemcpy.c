#include "DSP28x_Project.h"

#include "f280x_bufops/bufops.h"

void far *xmemcpy(void far *to, const void far *from, size_t n)
{
     register char far *rto   = (char far *) to;
     register char far *rfrom = (char far *) from;
     register unsigned int rn;
     register unsigned int nn = (unsigned int) n;

     /***********************************************************************/
     /*  Assume that the size is < 64K and do the memcpy. At the end compare*/
     /*  the number of chars moved with the size n. If they are equal       */
     /*  return. Else continue to copy the remaining chars.                 */
     /***********************************************************************/
     for (rn = 0; rn < nn; rn++) *rto++ = *rfrom++;
     if (nn == n) return (to);

     /***********************************************************************/
     /*  Write the memcpy of size >64K using nested for loops to make use   */
     /*  of BANZ instrunction.                                              */
     /***********************************************************************/
     {
        register unsigned int upper = (unsigned int)(n >> 16);
        register unsigned int tmp;
        for (tmp = 0; tmp < upper; tmp++)
	{
           for (rn = 0; rn < 65535; rn++)
		*rto++ = *rfrom++;
	   *rto++ = *rfrom++;
	}
     }

     return (to);
}
