/* adlist.c - A generic doubly linked list implementation
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


#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of every node need to be freed
 * by the user before to call AlFreeList().
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
list *listCreate(void)
{
    struct list *list;

    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Free the whole list.
 *
 * This function can't fail. */
void listRelease(list *list)
{
    unsigned int len;
    listNode *current, *next;
    /* set current to list->head */
    current = list->head;
    /* get the length of list */
    len = list->len;
    /* loop all the list node */
    while(len--) {
        /* get next node */
        next = current->next;
        /* free current->value by calling list->free
         * list->free is a user defined function
         **/
        if (list->free) list->free(current->value);
        zfree(current);
        /* move ahead */
        current = next;
    }
    /* free the pointer which points to the list */
    zfree(list);
}

/* Add a new node to the list, to head, contaning the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;
    /* allocate memory for a new node */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    /* assign value */
    node->value = value;
    /* if list is empty */
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        /* update the head */
        node->prev = NULL; /* set node->prev */
        node->next = list->head; /* set node->next to list->head */
        list->head->prev = node; /* set list->head's prev to node */
        list->head = node; /* set list->head to node */
    }
    /* update list->len */
    list->len++;
    return list;
}

/* Add a new node to the list, to tail, contaning the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;
    /* allocate a new node to store the value */
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    /* assign value */
    node->value = value;
    /* empty list */
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        /* set node->prev to list->tail */
        node->prev = list->tail;
        /* set node->next to NULL */
        node->next = NULL;
        /* set list->tail's next to node */
        list->tail->next = node;
        /* set list->tail to node */
        list->tail = node;
    }
    /* update list->len */
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
void listDelNode(list *list, listNode *node)
{
    /* if node is not the head of list */
    if (node->prev)
        /**
         * set node->prev's next to node->next
         * so we delete one way of the chain
         */
        node->prev->next = node->next;
    else/* node is the head of the list */
        list->head = node->next; /* update the head */
    if (node->next)/* if node is not tail of the list */
        node->next->prev = node->prev; /* delete right to left chain by setting node->next' prev to node->prev*/
    else/* node is tail of the list */
        list->tail = node->prev; /* update the tail */
    /* call the user free function to free the node->value */
    if (list->free) list->free(node->value);
    /* free the memory hold node */
    zfree(node);
    /* update the list's length */
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;
    /* allocate memory for iter */
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;
    /* move from head to tail */
    if (direction == AL_START_HEAD)
        iter->next = list->head;/* set iter to list->head */
    else /* move from tail to head */
        iter->next = list->tail; /* set iter to list->tail */
    iter->direction = direction; /* update iter->direction */
    return iter;
}

/* Release the iterator memory */
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
void listRewind(list *list) {
    /* set list->iter to the head of the list,
     * and direction left to right
     */
    list->iter.next = list->head;
    list->iter.direction = AL_START_HEAD;
}
/* Create an interator in the list private iterator structure */
void listRewindTail(list *list) {
    /* set list->iter to the tail of the list,
     * and direction right to left
     */
    list->iter.next = list->tail;
    list->iter.direction = AL_START_TAIL;
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 *
 * iter = listGetItarotr(list,<direction>);
 * while ((node = listNextIterator(iter)) != NULL) {
 *     DoSomethingWith(listNodeValue(node));
 * }
 *
 * */
listNode *listNext(listIter *iter)
{
    /* get the next node */
    listNode *current = iter->next;
    /* if next node isn't NULL */
    if (current != NULL) {
        /* go ahead */
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next;
        else /* go back */
            iter->next = current->prev;
    }
    /* return the current node */
    return current;
}

/* List Yield just call listNext() against the list private iterator */
listNode *listYield(list *list) {
    return listNext(&list->iter);
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
list *listDup(list *orig)
{
    list *copy;
    listIter *iter;
    listNode *node;

    if ((copy = listCreate()) == NULL)
        return NULL;
    /* set the auxiliary function */
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;
    /* get iter of list from head */
    iter = listGetIterator(orig, AL_START_HEAD);
    /* loop all the node of list */
    while((node = listNext(iter)) != NULL) {
        void *value;
        /* if there is a user defined copy function */
        if (copy->dup) {
            /* set value by calling user defined function */
            value = copy->dup(node->value);
            if (value == NULL) { /* opppps, we get some error */
                listRelease(copy);/* release the new list */
                listReleaseIterator(iter); /* release the iterator */
                return NULL;
            }
        } else
            value = node->value; /* shadow copy if there isn't a user defined function */
        if (listAddNodeTail(copy, value) == NULL) {
            /* if we get some error when add the new node to the end of the new list */
            listRelease(copy);/* release the new list */
            listReleaseIterator(iter); /* release the iterator */
            return NULL;
        }
    }
    listReleaseIterator(iter);/* release the iterator */
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
listNode *listSearchKey(list *list, void *key)
{
    listIter *iter;
    listNode *node;
    /* get the head iterator of list */
    iter = listGetIterator(list, AL_START_HEAD);
    /* loop all the node */
    while((node = listNext(iter)) != NULL) {
        if (list->match) {/* if there is a user defined match function */
            if (list->match(node->value, key)) {/* find the node */
                listReleaseIterator(iter); /* release the iterator */
                return node; /* return the node */
            }
        } else {/* there isn't a user defined match function */
            if (key == node->value) { /* if key equals to node->value */
                listReleaseIterator(iter); /* release the iterator */
                return node; /* return the node */
            }
        }
    }
    listReleaseIterator(iter); /*release the iterator */
    return NULL;/*didn't find the node with key */
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimante
 * and so on. If the index is out of range NULL is returned. */
listNode *listIndex(list *list, int index) {
    listNode *n;

    if (index < 0) {/* look from tail */
        index = (-index)-1; /* change index to positive */
        n = list->tail;
        while(index-- && n) n = n->prev;
    } else {/* look from head */
        n = list->head;
        while(index-- && n) n = n->next;
    }
    return n;/*return the node */
}
