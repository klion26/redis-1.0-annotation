/* The following is the NetBSD libc qsort implementation modified in order to
 * support partial sorting of ranges for Redis.
 *
 * Copyright(C) 2009 Salvatore Sanfilippo. All rights reserved.
 *
 * See the pqsort.c file for the original copyright notice. */

#ifndef __PQSORT_H
#define __PQSORT_H

/**
 * annotation by klion26
 * a quick sort implementation
 * @a: points to the start the array
 * @n: number of elements will be sorted
 * @es: size of each element
 * @cmp: a recall function used to compare
 * @lrange: the left bound of the interval will be sorted
 * @rrange: the right bound of the interval will be sorted
 */
void
pqsort(void *a, size_t n, size_t es,
    int (*cmp) (const void *, const void *), size_t lrange, size_t rrange);

#endif
