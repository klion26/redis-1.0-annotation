/* zmalloc - total amount of allocated memory aware version of malloc()
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
#include <string.h>
#include "config.h"

static size_t used_memory = 0;
/** 分配 size 大小的空间 **/
void *zmalloc(size_t size) {
    /** 申请 size + sizeof(size_t) 大小的空间 多申请的空间是用来统计 used_memory 的 **/
    void *ptr = malloc(size+sizeof(size_t));
    /** 申请空间失败 **/
    if (!ptr) return NULL;
#ifdef HAVE_MALLOC_SIZE
    /** 在苹果机器上，利用 redis_malloc_size 直接计算申请的空间大小
     * 其中 redis_malloc_size 是一个宏定义
     * 在 config.h 里面
     * **/
    used_memory += redis_malloc_size(ptr);
    return ptr;
#else
    /**
    *  非苹果机器上
    *  第一个元素存申请的空间大小
    *  更新 used_memory
    *  返回实际数据开始的首地址，
    *  这里为什么需要使用一个 size_t 的内存空间记录 size 值呢???
    **/
    *((size_t*)ptr) = size;
    used_memory += size+sizeof(size_t);
    return (char*)ptr+sizeof(size_t);
#endif
}

/** 将 ptr 指向的空间调整为 size 大小 **/
void *zrealloc(void *ptr, size_t size) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr; /** if not define HAVE_MALLOC_SIZE **/
#endif
    /** @oldsize : the size ptr allocated **/
    size_t oldsize;
    /** @newptr : the new address **/
    void *newptr;
    /** if ptr is NULL, then call zmalloc to allocate a block with size **/
    if (ptr == NULL) return zmalloc(size);
#ifdef HAVE_MALLOC_SIZE  /** define HAVE_MALLOC_SIZE **/
    oldsize = redis_malloc_size(ptr);
    newptr = realloc(ptr,size);
    if (!newptr) return NULL; /** newptr == NULL, then ptr will not be modified **/

    /** update used_memory **/
    used_memory -= oldsize;
    used_memory += redis_malloc_size(newptr);
    return newptr;
#else
    /** get the real address allocated before **/
    realptr = (char*)ptr-sizeof(size_t);
    oldsize = *((size_t*)realptr);
    newptr = realloc(realptr,size+sizeof(size_t));
    if (!newptr) return NULL;

    *((size_t*)newptr) = size;
    /** update used_memory **/
    used_memory -= oldsize;
    used_memory += size;
    return (char*)newptr+sizeof(size_t);
#endif
}
/** free the block we allocated by zmalloc/zrealloc **/
void zfree(void *ptr) {
#ifndef HAVE_MALLOC_SIZE
    /** didn't define HAVE_MALLOC_SIZE **/
    void *realptr;
    size_t oldsize;
#endif

    if (ptr == NULL) return;
#ifdef HAVE_MALLOC_SIZE
    /** update used_memory and free the memory **/
    used_memory -= redis_malloc_size(ptr);
    free(ptr);
#else
    /** update the used_memory and free the memory **/
    realptr = (char*)ptr-sizeof(size_t);
    oldsize = *((size_t*)realptr);
    used_memory -= oldsize+sizeof(size_t);
    free(realptr);
#endif
}
/** get a copy of s **/
char *zstrdup(const char *s) {
    /** allocate the length we need **/
    size_t l = strlen(s)+1;
    char *p = zmalloc(l);
    /** copy s to the destination **/
    memcpy(p,s,l);
    return p;
}

/** return the memory we have allocated **/
size_t zmalloc_used_memory(void) {
    return used_memory;
}
