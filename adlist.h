/* adlist.h - A generic doubly linked list implementation
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */

typedef struct listNode {
    struct listNode *prev; /* pointer of previous node */
    struct listNode *next; /* pointer of next node */
    void *value; /* pointer of value */
} listNode;

typedef struct listIter {
    listNode *next;   /* pointer of next node */
    int direction;    /* */
} listIter;

typedef struct list {
    listNode *head;    /* head of list */
    listNode *tail;    /* tail of list */
    void *(*dup)(void *ptr); /* */
    void (*free)(void *ptr); /* */
    int (*match)(void *ptr, void *key); /* */
    unsigned int len;   /* */
    listIter iter;   /* */
} list;

/* Functions implemented as macros */
#define listLength(l) ((l)->len)
#define listFirst(l) ((l)->head)
#define listLast(l) ((l)->tail)
#define listPrevNode(n) ((n)->prev)
#define listNextNode(n) ((n)->next)
#define listNodeValue(n) ((n)->value)

#define listSetDupMethod(l,m) ((l)->dup = (m))
#define listSetFreeMethod(l,m) ((l)->free = (m))
#define listSetMatchMethod(l,m) ((l)->match = (m))

#define listGetDupMethod(l) ((l)->dup)
#define listGetFree(l) ((l)->free)
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
list *listCreate(void);  /*OK*/
void listRelease(list *list);  /*OK*/
list *listAddNodeHead(list *list, void *value);  /*OK*/
list *listAddNodeTail(list *list, void *value);  /*OK*/
void listDelNode(list *list, listNode *node);  /*OK*/
listIter *listGetIterator(list *list, int direction);  /*OK*/
listNode *listNext(listIter *iter);  /*OK*/
void listReleaseIterator(listIter *iter);  /*OK*/
list *listDup(list *orig);  /*OK*/
listNode *listSearchKey(list *list, void *key);  /*OK*/
listNode *listIndex(list *list, int index);  /*OK*/
void listRewind(list *list);  /*OK*/
void listRewindTail(list *list);  /*OK*/
listNode *listYield(list *list);  /*OK*/

/* Directions for iterators */
#define AL_START_HEAD 0
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */
