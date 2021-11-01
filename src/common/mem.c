/*
 * Copyright (c) 2016 XTAO technology <www.xtaotech.com>
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

#include "mem.h"
#include "locking.h"
#include "logging.h"
#include <stdlib.h>
#include <stdarg.h>

#define XT_MEM_POOL_XLIST_BOUNDARY        (sizeof(struct xlist_head))
#define XT_MEM_POOL_PAD_BOUNDARY         (XT_MEM_POOL_XLIST_BOUNDARY + sizeof(int))
#define mem_pool_chunkhead2ptr(head)     ((head) + XT_MEM_POOL_PAD_BOUNDARY)
#define mem_pool_ptr2chunkhead(ptr)      ((ptr) - XT_MEM_POOL_PAD_BOUNDARY)
#define is_mem_chunk_in_use(ptr)         (*ptr == 1)

int
xt_vasprintf (char **string_ptr, const char *format, va_list arg)
{
        va_list arg_save;
        char    *str = NULL;
        int     size = 0;
        int     rv = 0;

        if (!string_ptr || !format)
                return -1;

        va_copy (arg_save, arg);

        size = vsnprintf (NULL, 0, format, arg);
        size++;
        str = XT_MALLOC (size);
        if (str == NULL) {
                return -1;
        }
        rv = vsnprintf (str, size, format, arg_save);

        *string_ptr = str;
        return (rv);
}

int
xt_asprintf (char **string_ptr, const char *format, ...)
{
        va_list arg;
        int     rv = 0;

        va_start (arg, format);
        rv = xt_vasprintf (string_ptr, format, arg);
        va_end (arg);

        return rv;
}

struct mem_pool *
mem_pool_new (unsigned long sizeof_type,
                 unsigned long count)
{
        struct mem_pool  *mem_pool = NULL;
        unsigned long     padded_sizeof_type = 0;
        void             *pool = NULL;
        int               i = 0;
        struct xlist_head *list = NULL;

        if (!sizeof_type || !count) {
                xt_log ("mem-pool", XT_LOG_ERROR, "invalid argument");
                return NULL;
        }
        padded_sizeof_type = sizeof_type + XT_MEM_POOL_PAD_BOUNDARY;

        mem_pool = XT_CALLOC (1, sizeof (*mem_pool));
        if (!mem_pool)
                return NULL;

        LOCK_INIT (&mem_pool->lock);
        INIT_XLIST_HEAD (&mem_pool->list);

        mem_pool->padded_sizeof_type = padded_sizeof_type;
        mem_pool->cold_count = count;
        mem_pool->real_sizeof_type = sizeof_type;

        pool = XT_CALLOC (count, padded_sizeof_type);
        if (!pool) {
                XT_FREE (mem_pool);
                return NULL;
        }

        for (i = 0; i < count; i++) {
                list = pool + (i * (padded_sizeof_type));
                INIT_XLIST_HEAD (list);
                xlist_add_tail (list, &mem_pool->list);
        }

        mem_pool->pool = pool;
        mem_pool->pool_end = pool + (count * (padded_sizeof_type));

        return mem_pool;
}

void*
mem_get0 (struct mem_pool *mem_pool)
{
        void             *ptr = NULL;

        if (!mem_pool) {
                xt_log ("mem-pool", XT_LOG_ERROR, "invalid argument");
                return NULL;
        }

        ptr = mem_get(mem_pool);

        if (ptr)
                memset(ptr, 0, mem_pool->real_sizeof_type);

        return ptr;
}

void *
mem_get (struct mem_pool *mem_pool)
{
        struct xlist_head *list = NULL;
        void             *ptr = NULL;
        int             *in_use = NULL;

        if (!mem_pool) {
                xt_log ("mem-pool", XT_LOG_ERROR, "invalid argument");
                return NULL;
        }

        LOCK (&mem_pool->lock);
        {
                if (mem_pool->cold_count) {
                        list = mem_pool->list.next;
                        xlist_del (list);

                        mem_pool->hot_count++;
                        mem_pool->cold_count--;

                        ptr = list;
                        in_use = (ptr + XT_MEM_POOL_XLIST_BOUNDARY);
                        *in_use = 1;
			

                        goto fwd_addr_out;
                }

                ptr = MALLOC (mem_pool->real_sizeof_type);

                /* Memory coming from the heap need not be transformed from a
                 * chunkhead to a usable pointer since it is not coming from
                 * the pool.
                 */
                goto unlocked_out;
        }
fwd_addr_out:
        ptr = mem_pool_chunkhead2ptr (ptr);
unlocked_out:
        UNLOCK (&mem_pool->lock);

        return ptr;
}


static int
__is_member (struct mem_pool *pool, void *ptr)
{
        if (!pool || !ptr) {
                xt_log ("mem-pool", XT_LOG_ERROR, "invalid argument");
                return -1;
        }

        if (ptr < pool->pool || ptr >= pool->pool_end)
                return 0;

        if ((mem_pool_ptr2chunkhead (ptr) - pool->pool)
            % pool->padded_sizeof_type)
                return -1;

        return 1;
}


void
mem_put (struct mem_pool *pool, void *ptr)
{
        struct xlist_head *list = NULL;
        int    *in_use = NULL;
        void   *head = NULL;

        if (!pool || !ptr) {
                xt_log ("mem-pool", XT_LOG_ERROR, "invalid argument");
                return;
        }

        LOCK (&pool->lock);
        {

                switch (__is_member (pool, ptr))
                {
                case 1:
                        list = head = mem_pool_ptr2chunkhead (ptr);
                        in_use = (head + XT_MEM_POOL_XLIST_BOUNDARY);
                        if (!is_mem_chunk_in_use(in_use)) {
                                xt_log ("mem-pool", XT_LOG_CRITICAL,
					"mem_put called on freed ptr %p of mem "
					"pool %p", ptr, pool);
                                break;
                        }
                        pool->hot_count--;
                        pool->cold_count++;
                        *in_use = 0;
                        xlist_add (list, &pool->list);
                        break;
                case -1:
                        /* For some reason, the address given is within
                         * the address range of the mem-pool but does not align
                         * with the expected start of a chunk that includes
                         * the list headers also. Sounds like a problem in
                         * layers of clouds up above us. ;)
                         */
                        abort ();
                        break;
                case 0:
                        /* The address is outside the range of the mem-pool. We
                         * assume here that this address was allocated at a
                         * point when the mem-pool was out of chunks in mem_get
                         * or the programmer has made a mistake by calling the
                         * wrong de-allocation interface. We do
                         * not have enough info to distinguish between the two
                         * situations.
                         */
                        FREE (ptr);
                        break;
                default:
                        /* log error */
                        break;
                }
        }
        UNLOCK (&pool->lock);
}


void
mem_pool_destroy (struct mem_pool *pool)
{
        if (!pool)
                return;

        LOCK_DESTROY (&pool->lock);
        XT_FREE (pool->pool);
        XT_FREE (pool);

        return;
}
