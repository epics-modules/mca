#include <vxWorks.h>
#include <types.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <stdioLib.h>

#include <dbEvent.h>
#include <dbDefs.h>
#include <dbCommon.h>
#include <recSup.h>
#include <genSubRecord.h>

#define NINT(f)     (int)((f)>0 ? (f)+0.5 : (f)-0.5)
#define MAXDETS 16

volatile int mcaSumDebug=0;

static double shift[MAXDETS], min_shift, max_shift;
static short op[MAXDETS];
static double fact[MAXDETS];

long ndets(genSubRecord *pgsub)
{
	long nDets, *pnDets = (long *)pgsub->u;

	nDets = *pnDets;
	if (nDets < 0) {
		printf("mcaSum:%s # of detectors is %ld.  It must be non-negative.\n",
			pgsub->name, nDets);
		nDets = 0;
	} else if (nDets > MAXDETS) {
		printf("mcaSum:%s # of detectors is %ld.  It can't be greater than %d.\n",
			pgsub->name, nDets, MAXDETS);
		nDets = MAXDETS;
	}
	return(nDets);
}

long mcaSum_init(genSubRecord *pgsub)
{
	float	*n;
	int i;
	long nDets = ndets(pgsub);	/* just to get any error message there might be */

	n = (float *)pgsub->valn;
	for (i=0; i<pgsub->novn; i++) {
		n[i] = 0.;
	}
	return(0);
}

long mcaSum_do_shift(genSubRecord *pgsub)
{
	int i;
	double **ppdata = (double **)&(pgsub->a);
	long nDets = ndets(pgsub);

	min_shift = 1.e6;
	max_shift = -1.e6;
	for (i=0; i<nDets; i++) {
		shift[i] = (double) *(ppdata[i]);
		if (shift[i] < min_shift) min_shift = shift[i];
		if (shift[i] > max_shift) max_shift = shift[i];
	}
	return(0);
}

long mcaSum_do_op(genSubRecord *pgsub)
{
	int i;
	short **ppdata = (short **)&(pgsub->a);
	long nDets = ndets(pgsub);

	for (i=0; i<nDets; i++) {
		op[i] = (short) *(ppdata[i]);
		if (op[i] < 0) op[i] = 0;
		if (op[i] > 2) op[i] = 2;
	}
	return(0);
}

long mcaSum_do_fact(genSubRecord *pgsub)
{
	int i;
	double **ppdata = (double **)&(pgsub->a);
	long nDets = ndets(pgsub);

	for (i=0; i<nDets; i++) {
		fact[i] = (double) *(ppdata[i]);
	}
	return(0);
}


long mcaSum_do(genSubRecord *pgsub)
{
	float	*n, **ppdata, *pdata, *pdest, *pd;
	int		max, i, is, ix, ix_start, ix_end, src_start;
	double	f, q, s, floor_s;
	long nDets = ndets(pgsub);

	ppdata = (float **)&(pgsub->a);
	n = (float *)pgsub->valn;
	max = (int) pgsub->novn;
	if (mcaSumDebug) printf("mcaSum_do: ppdata=%p,n=%p, max=%d\n", ppdata, n, max);
	for (ix = 0; ix < max; ix++) n[ix] = 0;
	for (i=0; i<nDets; i++) {
		if (op[i]) {
			pdata = (float *)ppdata[i];
			if (mcaSumDebug) printf("mcaSum_do: pdata=%p,pdata[0]=%f\n", pdata, pdata[0]);

			s = shift[i];
			is = NINT(s);
			if (fabs(s-is) < .000001) {
				/* integer shift */
				f = fact[i];
				ix_start = (s > 0) ? is : 0;
				ix_end = (is > 0) ? max : max + is;
				pd = &pdata[ix_start - is];
				if (mcaSumDebug) printf("ix_start=%d; ix_end=%d; s=%f\n",
					ix_start, ix_end, s);
				for (pdest = n + ix_start; pdest < n + ix_end; ) {
					*pdest++ += f * (*pd++);
					if (mcaSumDebug > 10) printf("%f, ", pdest[-1]);
				}
			} else {
				/* fractional shift */
				f = fact[i];
				floor_s = floor(s);
				ix_start = (s > 0) ? floor_s+1 : 0;
				ix_end = (s > 0) ? max-1 : (max-1) + floor_s;
				src_start = (int) (ix_start-s);
				pd = &pdata[src_start];
				q = s - floor_s;
				if (mcaSumDebug) printf("ix_start=%d; ix_end=%d; src_start=%d; s=%f\n",
					ix_start, ix_end, src_start, s);
				for (pdest = n + ix_start; pdest < n + ix_end; pd++) {
					*pdest++ += f * (pd[0] * q + pd[1] * (1-q));
				}
			}
		}
	}
	/* clear borders, where not all arrays got added because of shifting */
	for (ix = NINT(max + min_shift); ix < max; ix++) n[ix] = 0;
	for (ix = 0; ix < max_shift; ix++) n[ix] = 0;

	return(0);
}

long mcaSumROI_init(genSubRecord *pgsub)
{
	float	*n;
	long nDets = ndets(pgsub);	/* just to get any error message there might be */

	n = (float *)pgsub->valn;
	*n = 0.;
	return(0);
}

long mcaSumROI_do(genSubRecord *pgsub)
{
	float	*n, **ppdata, *pdata;
	int		i;
	long nDets = ndets(pgsub);

	ppdata = (float **)&(pgsub->a);
	n = (float *)pgsub->valn;
	if (mcaSumDebug) printf("mcaSumROI_do: ppdata=%p,n=%p\n", ppdata, n);
	*n = 0;
	for (i=0; i<nDets; i++) {
		pdata = (float *)ppdata[i];
		if (mcaSumDebug) printf("mcaSumROI_do: pdata=%p,*pdata=%f\n", pdata, *pdata);
		*n += *pdata;
	}
	return(0);
}
