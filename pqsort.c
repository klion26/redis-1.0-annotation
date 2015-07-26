/* The following is the NetBSD libc qsort implementation modified in order to
 * support partial sorting of ranges for Redis.
 *
 * Copyright(C) 2009 Salvatore Sanfilippo. All rights reserved.
 *
 * The original copyright notice follows. */


/*	$NetBSD: qsort.c,v 1.19 2009/01/30 23:38:44 lukem Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)qsort.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: qsort.c,v 1.19 2009/01/30 23:38:44 lukem Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

/* med3 : return the middle of the three value, the fourth parameter
 * is the compare function */
static inline char	*med3 (char *, char *, char *,
    int (*)(const void *, const void *));
static inline void	 swapfunc (char *, char *, size_t, int);

/** return the less one **/
#define min(a, b)	(a) < (b) ? a : b

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
/** swap n bytes each from parmi and parmj,
 * all the elements are type TYPE
 */
#define swapcode(TYPE, parmi, parmj, n) { 		\
	size_t i = (n) / sizeof (TYPE); 		\
	TYPE *pi = (TYPE *)(void *)(parmi); 		\
	TYPE *pj = (TYPE *)(void *)(parmj); 		\
	do { 						\
		TYPE	t = *pi;			\
		*pi++ = *pj;				\
		*pj++ = t;				\
        } while (--i > 0);				\
}

/*
 * calculate swaptype
 * if the start position of the array is multiply of sizeof(long)
 * or the array's element is long type, then we could swap as long,
 * otherwise, we will swap element as char.
 * swaptype :
 *          0 -> ((char *)a - (char *)0) % sizeof(long == 0 && es == sizeof(long)
 *          1 -> ((char *)a - (char *)0) % sizeof(long == 0 && es != sizeof(long) && es % sizeof(long) == 0 
 *          2 -> other
 */
#define SWAPINIT(a, es) swaptype = ((char *)a - (char *)0) % sizeof(long) || \
	es % sizeof(long) ? 2 : es == sizeof(long)? 0 : 1;

static inline void
swapfunc(char *a, char *b, size_t n, int swaptype)
{
    /**
     * swaptype <= 1
     *     can swap as long (alignment with long, and es % sizeof(long) == 0)
     */
	if (swaptype <= 1) 
		swapcode(long, a, b, n)
	else /* all the other solutions */
		swapcode(char, a, b, n)
}
/* swap the value pointed by a and b */
#define swap(a, b)						\
	if (swaptype == 0) {					\
		long t = *(long *)(void *)(a);			\
		*(long *)(void *)(a) = *(long *)(void *)(b);	\
		*(long *)(void *)(b) = t;			\
	} else							\
		swapfunc(a, b, es, swaptype)
/** vector swap 
 * swap the n elements from a and b
 */
#define vecswap(a, b, n) if ((n) > 0) swapfunc((a), (b), (size_t)(n), swaptype)


/* *
 * this functions returns the middle of the three values pointed by a, b and c
 * the fourth parameter is the compare function
 * */
static inline char *
med3(char *a, char *b, char *c,
    int (*cmp) (const void *, const void *))
{

	return cmp(a, b) < 0 ?
	       (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a ))
              :(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c ));
}

/*****
 * @param: a -> array will be sorted
 * @param: n -> number of elements will be sorted
 * @param: es -> size of each element, all the data will be change to char, so we need a parameter to specify the size.
 * @param: cmp -> compare function
 * @param: lrange -> the address of the left bound of the sort range
 * @param: rrange -> the address of the right bound of the sort range
 */
static void
_pqsort(void *a, size_t n, size_t es,
    int (*cmp) (const void *, const void *), void *lrange, void *rrange)
{
	char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
	size_t d, r;
	int swaptype, swap_cnt, cmp_result; /** swap_cnt used for what??? **/

    /** calculates swaptype **/
loop:	SWAPINIT(a, es);
	swap_cnt = 0;
    /**
     * small amount of elements
     * use bubble sort
     */
	if (n < 7) { /* why 7?????  a magical number*/
		/** use bubble sort */
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
		return;
	}
    /** middle position of array **/
	pm = (char *) a + (n / 2) * es;
    /**  why skip off 7 ??? **/
	if (n > 7) {
		pl = (char *) a;
		pn = (char *) a + (n - 1) * es;
		if (n > 40) { /** where does this magic number come from ?? **/
			d = (n / 8) * es;
			pl = med3(pl, pl + d, pl + 2 * d, cmp);
			pm = med3(pm - d, pm, pm + d, cmp);
			pn = med3(pn - 2 * d, pn - d, pn, cmp);
		}
		pm = med3(pl, pm, pn, cmp);
	}
	swap(a, pm);
	pa = pb = (char *) a + es;

	pc = pd = (char *) a + (n - 1) * es;
	for (;;) {
		/* a quick sort loop */
		while (pb <= pc && (cmp_result = cmp(pb, a)) <= 0) {
			if (cmp_result == 0) {
				swap_cnt = 1; 
				swap(pa, pb);
				pa += es;
			}
			pb += es;
		}
		while (pb <= pc && (cmp_result = cmp(pc, a)) >= 0) {
			if (cmp_result == 0) {
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
        /** {a[0]}  {a[1]...a[k]}  {a[k+1]...a[n-1]}
         * a[i] < a[0]  for 1<= i < k+1
         * a[i] > a[0]  for k+1 <= i < n
         **/
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0; 
			     pl -= es)
				swap(pl, pl - es);
		return;
	}

    /** change the array to
     *  {a[0]...a[s]}  {a[s+1]...a[t]}  {a[t+1]...a[n-1]}
     *  a[i] = a[t] for s+1 <= i < t+1
     *  a[i] < a[t] for i < s+1
     *  a[i] > a[t] for i >= t+1
     **/
    /**
     * pa points to the first element less than (*a)
     * pb points to the first element bigger than (*a)
     * pc points to the last element less than (*a)
     * pd points to the last element bigger than (*a)
     */
	pn = (char *) a + n * es;
	r = min(pa - (char *) a, pb - pa);
	vecswap(a, pb - r, r);
	r = min((size_t)(pd - pc), pn - pd - es);
	vecswap(pb, pn - r, r);
    /***
     * swap over
     */
    /** more than one element less than (*a) **/
	if ((r = pb - pa) > es) {
                /**
                 * _l --> points to the start of the element less than (*a)
                 * _r --> points to the end of the element bigger than (*a)
                 */
                void *_l = a, *_r = ((unsigned char*)a)+r-1;
                /**
                 * test this interval needs to be sorted or not
                 */
                if (!((lrange < _l && rrange < _l) ||
                    (lrange > _r && rrange > _r)))
		    _pqsort(a, r / es, es, cmp, lrange, rrange); /** sort the left part **/
        }
	if ((r = pd - pc) > es) { 
                void *_l, *_r;
                
		/* Iterate rather than recurse to save stack space */
        /**
         * updates a and n to use iterates the function
         * @a: the beginning of the array needs to be sorted
         * @n: the elements need to be sorted
         */
		a = pn - r;
		n = r / es;
                /**
                 * @_l  points to the start of the elements bigger than the first element of the array
                 * @_r  points to the end of the elements
                 */
                _l = a;
                _r = ((unsigned char*)a)+r-1;
                /**
                 * test the interval needs to be sorted or not
                 */
                if (!((lrange < _l && rrange < _l) ||
                    (lrange > _r && rrange > _r)))
		    goto loop;
	}
/*		qsort(pn - r, r / es, es, cmp);*/
}

/*****
 * @param: a -> array will be sorted
 * @param: n -> number of elements will be sorted
 * @param: es -> size of each element
 * @param: cmp -> compare function
 * @param: lrange -> offset of left bound of the sort range, start with 0
 * @param: rrange -> offset of right bound of the sort range, start with 0
 */
void
pqsort(void *a, size_t n, size_t es,
    int (*cmp) (const void *, const void *), size_t lrange, size_t rrange)
{
	/**
	 * change (void *)a to (unsigned char*)a,
	 * and plus offset * es,
	 * so we don't need to know the type of the element in a
	 * the plus arithmetic of pointer will add n * sizeof(element)
	 */
    _pqsort(a,n,es,cmp,((unsigned char*)a)+(lrange*es),
                       ((unsigned char*)a)+((rrange+1)*es)-1);
}
