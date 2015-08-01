/* SDSLib, A C dynamic strings library
 *
 * Copyright (c) 2006-2009, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define SDS_ABORT_ON_OOM

#include "sds.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "zmalloc.h"

static void sdsOomAbort(void) {
    fprintf(stderr,"SDS: Out Of Memory (SDS_ABORT_ON_OOM defined)\n");
    abort();
}

/** make a new string with length of initlen, copy *init to the new string if *init is not null 
 *  be careful if strlen(init) < initlen, such as
 *  a = sdsnewlen("abc", 10);
 *  a = sdscat(a, "abc");
 *  printf("%s", a) ====> "abc", because a[4] == '\0'!!!
 ***/
sds sdsnewlen(const void *init, size_t initlen) {
    struct sdshdr *sh;
    /**
     * allocate the memory
     * sizeof(struct sdshdr) -> sizeof of struct
     * initlen  -> sizeof the string
     * 1  -> room for '\0'
     **/
    sh = zmalloc(sizeof(struct sdshdr)+initlen+1);
#ifdef SDS_ABORT_ON_OOM
    if (sh == NULL) sdsOomAbort();
#else
    if (sh == NULL) return NULL;
#endif
    /**
     * update struct sh
     * setup sh->len and sh->free
     * change size_t(initlen) to long(sh->len), there may get problem
     */
    sh->len = initlen;
    sh->free = 0;
    if (initlen) {/*initlen > 0 */
        /* setup sh->buf */
        if (init) memcpy(sh->buf, init, initlen); /* copy the content from init */
        else memset(sh->buf,0,initlen); /* set all the elements of sh->buf to 0 */
    }
    /* add '\0' at then end of sh->buf */
    sh->buf[initlen] = '\0';
    /**
     * return the new string
     * sds --> typedef of char*
     **/
    return (char*)sh->buf;
}
/* get a empty string */
sds sdsempty(void) {
    return sdsnewlen("",0);
}

/**
 * make a new string with content of *init
 */
sds sdsnew(const char *init) {
    /* calculate the length of new string */
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    /* call sdsnewlen(const void *, size_t) to new a string */
    return sdsnewlen(init, initlen);
}
/**
 * get the length of current string with O(1)
 * it uses less time than strlen(s)
 **/
size_t sdslen(const sds s) {
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    return sh->len;
}
/* get a copy of s */
sds sdsdup(const sds s) {
    return sdsnewlen(s, sdslen(s));
}
/* free the memory which contains s*/
void sdsfree(sds s) {
    if (s == NULL) return;
    /* call zfree do the really free thing */
    zfree(s-sizeof(struct sdshdr));
}
/**
 * return how many *free* memory of does s have
 */
size_t sdsavail(sds s) {
    /* get the address of struct contains s */
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    /* return sh->free which specifies the free space */
    return sh->free;
}
/**
 * update the struct element which contains s
 * why needs this function?
 **/
void sdsupdatelen(sds s) {
    /* get the start position of the struct */
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    /* get the current length of s */
    int reallen = strlen(s);
    /* update the free space and length */
    sh->free += (sh->len-reallen);
    sh->len = reallen;
}
/**
 * make room for addlen byte after s
 * @s  : the original string
 * @addlen: the length we want to add
 **/
static sds sdsMakeRoomFor(sds s, size_t addlen) {
    /* @sh : point the the struct contains s
     * @newsh : points the new struct
     */
    struct sdshdr *sh, *newsh;
    /* calculate the free space of sh */
    size_t free = sdsavail(s);
    size_t len, newlen;
    /* if there are enough free space for addlen */
    if (free >= addlen) return s;
    len = sdslen(s);
    sh = (void*) (s-(sizeof(struct sdshdr)));
    /**
     * allocate (len + addlen)*2 space
     * we want (len + addlen)
     * so we may not reallocate memory next time
     **/
    newlen = (len+addlen)*2;
    /* realloc the memory */
    newsh = zrealloc(sh, sizeof(struct sdshdr)+newlen+1);
#ifdef SDS_ABORT_ON_OOM
    if (newsh == NULL) sdsOomAbort();
#else
    if (newsh == NULL) return NULL;
#endif
    /**
     * update the struct , newsh->len doesn't change
     **/
    newsh->free = newlen - len;
    return newsh->buf;
}
/* concate len elements of t at the end of s */
sds sdscatlen(sds s, void *t, size_t len) {
    /* @sh -> struct contains s */
    struct sdshdr *sh;
    size_t curlen = sdslen(s);
    /* make enough room */
    s = sdsMakeRoomFor(s,len);
    if (s == NULL) return NULL;
    sh = (void*) (s-(sizeof(struct sdshdr)));
    /* copy the len elements after s */
    memcpy(s+curlen, t, len);
    /* update sh->len and sh->free */
    sh->len = curlen+len;
    sh->free = sh->free-len;
    /* add '\0' at the end of the string */
    s[curlen+len] = '\0';
    return s;
}
/* concate t to s */
sds sdscat(sds s, char *t) {
    /* call sdscatlen to do the work */
    return sdscatlen(s, t, strlen(t));
}
/* copy len elements of t to s*/
sds sdscpylen(sds s, char *t, size_t len) {
    /* @sh -> struct which contains s */
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    /* @totlen -> total length of sh */
    size_t totlen = sh->free+sh->len;

    /* doesn't have enough space */
    if (totlen < len) {
        /* make room for the new string */
        s = sdsMakeRoomFor(s,len-totlen);
        /* failed to make more room */
        if (s == NULL) return NULL;
        /*update sh and totlen */
        sh = (void*) (s-(sizeof(struct sdshdr)));
        totlen = sh->free+sh->len;
    }
    /* copy the contents */
    memcpy(s, t, len);
    /* add '\0' to the end of the string */
    s[len] = '\0';
    /* update sh->len and sh->free */
    sh->len = len;
    sh->free = totlen-len;
    /* return s */
    return s;
}
/* copy t to s */
sds sdscpy(sds s, char *t) {
    return sdscpylen(s, t, strlen(t));
}
/**
 * concat some format characters after s,
 * the format likes the parameter of printf
 */
sds sdscatprintf(sds s, const char *fmt, ...) {
    /* ap : va_list */
    va_list ap;
    char *buf, *t;
    /* start buf size */
    size_t buflen = 32;

    /* look for a suffient buf size */
    while(1) {
        buf = zmalloc(buflen);
#ifdef SDS_ABORT_ON_OOM
        if (buf == NULL) sdsOomAbort();
#else
        if (buf == NULL) return NULL;
#endif
        buf[buflen-2] = '\0';
        /* print the format string to buf */
        va_start(ap, fmt);
        vsnprintf(buf, buflen, fmt, ap);
        va_end(ap);
        /* if buf isn't bigger enough to hold the format string
         * keeping on looking*/
        if (buf[buflen-2] != '\0') {
            zfree(buf);
            buflen *= 2;
            continue;
        }
        break;
    }
    /* concat the format string after s */
    t = sdscat(s, buf);
    /* free the memory we allocated before */
    zfree(buf);
    return t;
}
/**
 * remove the characters at the begin and end of s
 * which in (*cset)
 */
sds sdstrim(sds s, const char *cset) {
    /* @sh : struct which contains s */
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    /* some pointers */
    char *start, *end, *sp, *ep;
    size_t len;
    /* sp and start point to the begin of string */
    sp = start = s;
    /* ep and end point to the end of string */
    ep = end = s+sdslen(s)-1;
    /* skip all the characters at the begin of s */
    while(sp <= end && strchr(cset, *sp)) sp++;
    /* skip all the characters at the end of s */
    while(ep > start && strchr(cset, *ep)) ep--;
    /* calculate the number of characters remain */
    len = (sp > ep) ? 0 : ((ep-sp)+1);
    /**
     * if we have deleted some charactes at the begin of s
     * move the remain buf to the start of string
     * */
    if (sh->buf != sp) memmove(sh->buf, sp, len);
    /* add '\0' */
    sh->buf[len] = '\0';
    /* update sh->free and sh->len */
    sh->free = sh->free+(sh->len-len);
    sh->len = len;
    return s;
}
/**
 * return the substring of s(from index start to end) and modify the original string
 * -sdslen(s) < start, end < sdslen(s)
 * make sure end >= start after shifting
 * otherwise, you'll get an empty string
 * suppose s is a string with length 10
 * then sdsrange(s, 1, 5) modifies s to s[1:5]
 * and return the modified s
 * sdsrange(s, -1, 5) modifies s to empty string!!!, because it will be sdsrange(s, -1+10, 5)
 * sdsrange(s, -5, -1) modifies s to s[5, 9], and return the modified s
 */
sds sdsrange(sds s, long start, long end) {
    /**
     * @sh --> struct which contains s
     */
    struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
    size_t newlen, len = sdslen(s);
    /* empty string */
    if (len == 0) return s;
    /* shift start */
    if (start < 0) {
        start = len+start;
        if (start < 0) start = 0;
    }
    /* shift end */
    if (end < 0) {
        end = len+end;
        if (end < 0) end = 0;
    }
    /* calculate newlen using start and end */
    newlen = (start > end) ? 0 : (end-start)+1;
    if (newlen != 0) {
        /**
         * check if start & end is out of range
         * start >= len or end >= len
         * why casting? (if we don't cast, then start will be casted to unsigned)
         * if len becomes negative, then start and end will be equal, and newlen == 1
         **/
        if (start >= (signed)len) start = len-1;
        if (end >= (signed)len) end = len-1;
        newlen = (start > end) ? 0 : (end-start)+1;
    } else {
        start = 0;
    }
    /* move the content */
    if (start != 0) memmove(sh->buf, sh->buf+start, newlen);
    /* add '\0' at the end of string */
    sh->buf[newlen] = 0;
    /* update sh->free and sh->len */
    sh->free = sh->free+(sh->len-newlen);
    sh->len = newlen;
    return s;
}
/* tolower function */
void sdstolower(sds s) {
    int len = sdslen(s), j;
    /* change each element to lower letter */
    for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}
/* toupper function */
void sdstoupper(sds s) {
    int len = sdslen(s), j;
    /* make each letter to upper letter */
    for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}
/* compare two string */
int sdscmp(sds s1, sds s2) {
    /* @l1 : length of s1
     * @l2 : length of s2
     * @minlen : minimal of l1 and l2
     */
    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    /* compare the first minlen characters */
    cmp = memcmp(s1,s2,minlen);
    /* if the first characters are the same */
    if (cmp == 0) return l1-l2;
    /* return cmp */
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
sds *sdssplitlen(char *s, int len, char *sep, int seplen, int *count) {
    /**
     * @elements : count of split string
     * @slots : count of split string we could have *at most*, used for allocating memory
     * @start : the start position of next split string
     * @j : the current index of s
     */
    int elements = 0, slots = 5, start = 0, j;
    /**
     * @tokens : store the split string
     */
    sds *tokens = zmalloc(sizeof(sds)*slots);
#ifdef SDS_ABORT_ON_OOM
    if (tokens == NULL) sdsOomAbort();
#endif
    /* empty sep, empty s or can't out of memory */
    if (seplen < 1 || len < 0 || tokens == NULL) return NULL;
    /* split the string */
    for (j = 0; j < (len-(seplen-1)); j++) {
        /* make sure there is room for the next element and the final one */
        if (slots < elements+2) {
            sds *newtokens;

            /* double slots */
            slots *= 2;
            /* realloct new room */
            newtokens = zrealloc(tokens,sizeof(sds)*slots);
            if (newtokens == NULL) {
#ifdef SDS_ABORT_ON_OOM
                sdsOomAbort();
#else
                goto cleanup;
#endif
            }
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s+j) == sep[0]) || (memcmp(s+j,sep,seplen) == 0)) {
            /* make a new sdshdr to hold new string */
            tokens[elements] = sdsnewlen(s+start,j-start);
            if (tokens[elements] == NULL) {
#ifdef SDS_ABORT_ON_OOM
                sdsOomAbort();
#else
                goto cleanup;
#endif
            }
            /* update elements, start and j */
            elements++;
            start = j+seplen;
            j = j+seplen-1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sdsnewlen(s+start,len-start);
    if (tokens[elements] == NULL) {
#ifdef SDS_ABORT_ON_OOM
                sdsOomAbort();
#else
                goto cleanup;
#endif
    }
    /* update info, and return */
    elements++;
    *count = elements;
    return tokens;

#ifndef SDS_ABORT_ON_OOM
cleanup: /* cleanup */
    {
        int i;
        for (i = 0; i < elements; i++) sdsfree(tokens[i]);
        zfree(tokens);
        return NULL;
    }
#endif
}
