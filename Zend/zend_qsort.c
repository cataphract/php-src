/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2013 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        | 
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Sterling Hughes <sterling@php.net>                          |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "zend.h"
#include "zend_qsort.h"

#include <sys/types.h>
#include <stdlib.h>

static __inline char	*med3(char *, char *, char *, compare_r_func_t, void * TSRMLS_DC);
static __inline void	 swapfunc(char *, char *, int, int);

#define min(a, b)	(a) < (b) ? a : b

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) {		\
	long i = (n) / sizeof (TYPE);			\
	TYPE *pi = (TYPE *) (parmi);			\
	TYPE *pj = (TYPE *) (parmj);			\
	do {						\
		TYPE	t = *pi;			\
		*pi++ = *pj;				\
		*pj++ = t;				\
        } while (--i > 0);				\
}

#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static __inline void
swapfunc(char *a, char *b, int n, int swaptype)
{
	if (swaptype <= 1)
		swapcode(long, a, b, n)
	else
		swapcode(char, a, b, n)
}

#define swap(a, b)					\
	if (swaptype == 0) {				\
		long t = *(long *)(a);			\
		*(long *)(a) = *(long *)(b);		\
		*(long *)(b) = t;			\
	} else						\
		swapfunc(a, b, es, swaptype)

#define vecswap(a, b, n)	if ((n) > 0) swapfunc(a, b, n, swaptype)

static __inline char *
med3(char *a, char *b, char *c, compare_r_func_t cmp, void *arg TSRMLS_DC) 
{
	return cmp(a, b TSRMLS_CC, arg) < 0 ?
	       (cmp(b, c TSRMLS_CC, arg) < 0 ? b : (cmp(a, c TSRMLS_CC, arg) < 0 ? c : a ))
              :(cmp(b, c TSRMLS_CC, arg) > 0 ? b : (cmp(a, c TSRMLS_CC, arg) < 0 ? a : c ));
}

ZEND_API void
zend_qsort_r(void *aa, size_t n, size_t es, compare_r_func_t cmp, void *arg TSRMLS_DC)
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	int d, r, swaptype, swap_cnt;
	char *a = aa;

loop:	SWAPINIT(a, es);
	swap_cnt = 0;
	if (n < 7) {
		for (pm = (char *)a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl TSRMLS_CC, arg) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}
	pm = (char *)a + (n / 2) * es;
	if (n > 7) {
		pl = (char *)a;
		pn = (char *)a + (n - 1) * es;
		if (n > 40) {
			d = (n / 8) * es;
			pl = med3(pl, pl + d, pl + 2 * d, cmp, arg TSRMLS_CC);
			pm = med3(pm - d, pm, pm + d, cmp, arg TSRMLS_CC);
			pn = med3(pn - 2 * d, pn - d, pn, cmp, arg TSRMLS_CC);
		}
		pm = med3(pl, pm, pn, cmp, arg TSRMLS_CC);
	}
	swap(a, pm);
	pa = pb = (char *)a + es;

	pc = pd = (char *)a + (n - 1) * es;
	for (;;) {
		while (pb <= pc && (r = cmp(pb, a TSRMLS_CC, arg)) <= 0) {
			if (r == 0) {
				swap_cnt = 1;
				swap(pa, pb);
				pa += es;
			}
			pb += es;
				}
		while (pb <= pc && (r = cmp(pc, a TSRMLS_CC, arg)) >= 0) {
			if (r == 0) {
				swap_cnt = 1;
				swap(pc, pd);
				pd -= es;
			}
			pc -= es;
				}
		if (pb > pc)
			break;
		swap(pb, pc);
		swap_cnt = 1;
		pb += es;
		pc -= es;
			}
	if (swap_cnt == 0) {  /* Switch to insertion sort */
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl TSRMLS_CC, arg) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
		}

	pn = (char *)a + n * es;
	r = min(pa - (char *)a, pb - pa);
	vecswap(a, pb - r, r);
	r = min(pd - pc, pn - pd - (int)es);
	vecswap(pb, pn - r, r);
	if ((r = pb - pa) > (int)es)
		zend_qsort_r(a, r / es, es, cmp, arg TSRMLS_CC);
	if ((r = pd - pc) > (int)es) {
		/* Iterate rather than recurse to save stack space */
		a = pn - r;
		n = r / es;
		goto loop;
	}
	/* qsort(pn - r, r / es, es, cmp); */
}

ZEND_API void zend_qsort(void *base, size_t nmemb, size_t siz, compare_func_t compare TSRMLS_DC)
{
	zend_qsort_r(base, nmemb, siz, (compare_r_func_t)compare, NULL TSRMLS_CC);
}

/* 
 * Local Variables:
 * c-basic-offset: 4 
 * tab-width: 4
 * End:
 * vim600: fdm=marker
 * vim: noet sw=4 ts=4
 */
