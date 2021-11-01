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

#ifndef __RBTHASH_TABLE_H_
#define __RBTHASH_TABLE_H_
#include "rb.h"
#include "logging.h"
#include "common.h"
#include <pthread.h>

#define XT_RBTHASH_MEMPOOL      16384 //1048576
#define XT_RBTHASH              "rbthash"

struct rbthash_bucket {
        struct rb_table *bucket;
        xt_lock_t       bucketlock;
};

typedef struct rbthash_entry {
        void            *data;
        void            *key;
        int             keylen;
        uint32_t        keyhash;
} rbthash_entry_t;

typedef uint32_t (*rbt_hasher_t) (void *data, int len);
typedef void (*rbt_data_destroyer_t) (void *data);

typedef struct rbthash_table {
        int                     size;
        int                     numbuckets;
        struct mem_pool         *entrypool;
        xt_lock_t               tablelock;
        struct rbthash_bucket   *buckets;
        rbt_hasher_t            hashfunc;
        rbt_data_destroyer_t    dfunc;
        xt_boolean_t            pool_alloced;
} rbthash_table_t;

extern rbthash_table_t *
rbthash_table_init (int buckets, rbt_hasher_t hfunc,
                    rbt_data_destroyer_t dfunc, unsigned long expected_entries,
                    struct mem_pool *entrypool);

extern int
rbthash_insert (rbthash_table_t *tbl, void *data, void *key, int keylen);

extern void *
rbthash_get (rbthash_table_t *tbl, void *key, int keylen);

extern void *
rbthash_remove (rbthash_table_t *tbl, void *key, int keylen);

extern void *
rbthash_replace (rbthash_table_t *tbl, void *key, int keylen, void *newdata);

extern void
rbthash_table_destroy (rbthash_table_t *tbl);
#endif
