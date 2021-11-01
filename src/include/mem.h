/*
 * Copyright (c) 2016 XTAO Technology <www.xtaotech.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef __MEM_H__
#define __MEM_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xlist.h"
#include "locking.h"

#define MALLOC(size)       malloc(size)
#define CALLOC(cnt,size)   calloc(cnt,size)
#define REALLOC(ptr,size)  realloc(ptr,size)

#define FREE(ptr)                               \
        if (ptr != NULL) {                      \
                free ((void *)ptr);             \
                ptr = (void *)0xeeeeeeee;       \
        }

#define XT_CALLOC(nmemb, size) \
	CALLOC(nmemb, size)

#define XT_MALLOC(size)  MALLOC(size)

#define XT_REALLOC(ptr, size)  REALLOC(ptr, size)

#define XT_FREE(free_ptr) FREE(free_ptr)

int xt_vasprintf (char **string_ptr, const char *format, va_list arg);
int xt_asprintf (char **string_ptr, const char *format, ...);

static inline
char * xt_strdup (const char *src)
{

        char    *dup_str = NULL;
        size_t  len = 0;

	if (!src)
		return NULL;

        len = strlen (src) + 1;

        dup_str = XT_CALLOC(1, len);

        if (!dup_str)
                return NULL;

        memcpy (dup_str, src, len);

        return dup_str;
}

struct mem_pool {
        struct xlist_head  list;
        int               hot_count;
        int               cold_count;
        xt_lock_t         lock;
        unsigned long     padded_sizeof_type;
        void             *pool;
        void             *pool_end;
        int               real_sizeof_type;
};

struct mem_pool *
mem_pool_new (unsigned long sizeof_type, unsigned long count);

void mem_put (struct mem_pool *pool, void *ptr);
void *mem_get (struct mem_pool *pool);
void *mem_get0 (struct mem_pool *pool);

void mem_pool_destroy (struct mem_pool *pool);

#endif
