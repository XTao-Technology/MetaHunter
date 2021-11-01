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


#include "rbthash.h"
#include "rb.h"
#include "locking.h"
#include "logging.h"
#include "mem.h"
#include "common.h"

#include <pthread.h>
#include <string.h>


static int
rbthash_comparator (void *entry1, void *entry2, void *param)
{
        int             ret = 0;
        rbthash_entry_t *e1 = NULL;
        rbthash_entry_t *e2 = NULL;

        if ((!entry1) || (!entry2) || (!param))
                return -1;

        e1 = (rbthash_entry_t *)entry1;
        e2 = (rbthash_entry_t *)entry2;

        if (e1->keylen != e2->keylen) {
                if (e1->keylen < e2->keylen)
                        ret = -1;
                else if (e1->keylen > e2->keylen)
                        ret = 1;
        } else
                ret = memcmp (e1->key, e2->key, e1->keylen);

        return ret;
}


int
static __rbthash_init_buckets (rbthash_table_t *tbl, int buckets)
{
        int     i = 0;
        int     ret = -1;

        if (!tbl)
                return -1;

        for (; i < buckets; i++) {
                LOCK_INIT (&tbl->buckets[i].bucketlock);
                tbl->buckets[i].bucket = rb_create ((rb_comparison_func *)rbthash_comparator, tbl, NULL);
                if (!tbl->buckets[i].bucket) {
                        xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to create rb"
                                " table bucket");
                        ret = -1;
                        goto err;
                }
        }

        ret = 0;
err:
        return ret;
}


/*
 * rbthash_table_init - Initialize a RBT based hash table
 * @buckets - Number of buckets in the hash table
 * @hfunc   - hashing function
 * @dfunc   - destroyer for data in the RBT
 * @expected_entries - Number of entries expected in RBT. Mutually exclusive
 * with entrypool.
 * @entrypool -  Memory pool in lieu of expected_entries.
 */

rbthash_table_t *
rbthash_table_init (int buckets, rbt_hasher_t hfunc,
                    rbt_data_destroyer_t dfunc,
                    unsigned long expected_entries,
                    struct mem_pool *entrypool)
{
        rbthash_table_t         *newtab = NULL;
        int                     ret = -1;

        if (!hfunc) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Hash function not given");
                return NULL;
        }

        if (!entrypool && !expected_entries) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR,
                        "Both mem-pool and expected entries not provided");
                return NULL;
        }

        if (entrypool && expected_entries) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR,
                        "Both mem-pool and expected entries are provided");
                return NULL;
        }


        newtab = XT_CALLOC (1, sizeof (*newtab));
        if (!newtab)
                return NULL;

        newtab->buckets = XT_CALLOC (buckets, sizeof (struct rbthash_bucket));
        if (!newtab->buckets) {
                goto free_newtab;
        }

        if (expected_entries) {
                newtab->entrypool = mem_pool_new(sizeof (rbthash_entry_t),
				      expected_entries);
                if (!newtab->entrypool) {
                        xt_log (XT_RBTHASH, XT_LOG_ERROR,
                                "Failed to allocate mem-pool");
                        goto free_buckets;
                }
                newtab->pool_alloced = _xt_true;
        } else {
                newtab->entrypool = entrypool;
        }

        LOCK_INIT (&newtab->tablelock);
        newtab->numbuckets = buckets;
        ret = __rbthash_init_buckets (newtab, buckets);

        if (ret == -1) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to init buckets");
                if (newtab->pool_alloced)
                        mem_pool_destroy (newtab->entrypool);
        } else {
                xt_log (XT_RBTHASH, XT_LOG_TRACE, "Inited hash table: buckets:"
                        " %d", buckets);
        }

        newtab->hashfunc = hfunc;
        newtab->dfunc = dfunc;

free_buckets:
        if (ret == -1)
                XT_FREE (newtab->buckets);

free_newtab:
        if (ret == -1) {
                XT_FREE (newtab);
                newtab = NULL;
        }

        return newtab;
}

static rbthash_entry_t *
rbthash_init_entry (rbthash_table_t *tbl, void *data, void *key, int keylen)
{
        int             ret = -1;
        rbthash_entry_t *entry = NULL;

        if ((!tbl) || (!data) || (!key))
                return NULL;

        entry = mem_get (tbl->entrypool);
        if (!entry) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to get entry from"
                        " mem-pool");
                goto ret;
        }

        entry->data = data;
        entry->key = XT_CALLOC (keylen, sizeof (char));
        if (!entry->key) {
                goto free_entry;
        }

        memcpy (entry->key, key, keylen);
        entry->keylen = keylen;
        entry->keyhash = tbl->hashfunc (entry->key, entry->keylen);
        xt_log (XT_RBTHASH, XT_LOG_TRACE, "HASH: %u", entry->keyhash);

        ret = 0;
free_entry:
        if (ret == -1) {
                mem_put (tbl->entrypool, entry);
                entry = NULL;
        }

ret:
        return entry;
}


static void
rbthash_deinit_entry (rbthash_table_t *tbl, rbthash_entry_t *entry)
{

        if (!entry)
                return;

        if (entry->key)
                XT_FREE (entry->key);

        if (tbl) {
                if ((entry->data) && (tbl->dfunc))
                        tbl->dfunc (entry->data);
                mem_put (tbl->entrypool, entry);
        }

        return;
}


static inline struct rbthash_bucket *
rbthash_entry_bucket (rbthash_table_t *tbl, rbthash_entry_t * entry)
{
        int     nbucket = 0;

        nbucket = (entry->keyhash % tbl->numbuckets);
        xt_log (XT_RBTHASH, XT_LOG_TRACE, "BUCKET: %d", nbucket);
        return &tbl->buckets[nbucket];
}


int
static rbthash_insert_entry (rbthash_table_t *tbl, rbthash_entry_t *entry)
{
        struct rbthash_bucket   *bucket = NULL;
        int                     ret = -1;

        if ((!tbl) || (!entry))
                return -1;

        bucket = rbthash_entry_bucket (tbl, entry);
        if (!bucket) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to get bucket");
                goto err;
        }

        ret = 0;
        LOCK (&bucket->bucketlock);
        {
                if (!rb_probe (bucket->bucket, (void *)entry)) {
                        xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to insert"
                                " entry");
                        ret = -1;
                }
        }
        UNLOCK (&bucket->bucketlock);

err:
        return ret;
}


int
rbthash_insert (rbthash_table_t *tbl, void *data, void *key, int keylen)
{
        rbthash_entry_t         *entry = NULL;
        int                     ret = -1;

        if ((!tbl) || (!data) || (!key))
                return -1;

        entry = rbthash_init_entry (tbl, data, key, keylen);
        if (!entry) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to init entry");
                goto err;
        }

        ret = rbthash_insert_entry (tbl, entry);

        if (ret == -1) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to insert entry");
                rbthash_deinit_entry (tbl, entry);
        }

err:
        return ret;
}

static inline struct rbthash_bucket *
rbthash_key_bucket (rbthash_table_t *tbl, void *key, int keylen)
{
        uint32_t        keyhash = 0;
        int             nbucket = 0;

        if ((!tbl) || (!key))
                return NULL;

        keyhash =  tbl->hashfunc (key, keylen);
        xt_log (XT_RBTHASH, XT_LOG_TRACE, "HASH: %u", keyhash);
        nbucket = (keyhash % tbl->numbuckets);
        xt_log (XT_RBTHASH, XT_LOG_TRACE, "BUCKET: %u", nbucket);

        return &tbl->buckets[nbucket];
}


void *
rbthash_get (rbthash_table_t *tbl, void *key, int keylen)
{
        struct rbthash_bucket           *bucket = NULL;
        rbthash_entry_t                 *entry = NULL;
        rbthash_entry_t                 searchentry = {0, };

        if ((!tbl) || (!key))
                return NULL;

        bucket = rbthash_key_bucket (tbl, key, keylen);
        if (!bucket) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to get bucket");
                return NULL;
        }

        searchentry.key = key;
        searchentry.keylen = keylen;
        LOCK (&bucket->bucketlock);
        {
                entry = rb_find (bucket->bucket, &searchentry);
        }
        UNLOCK (&bucket->bucketlock);

        if (!entry)
                return NULL;

        return entry->data;
}


void *
rbthash_remove (rbthash_table_t *tbl, void *key, int keylen)
{
        struct rbthash_bucket   *bucket = NULL;
        rbthash_entry_t         *entry = NULL;
        rbthash_entry_t         searchentry = {0, };
        void                    *dataref = NULL;

        if ((!tbl) || (!key))
                return NULL;

        bucket = rbthash_key_bucket (tbl, key, keylen);
        if (!bucket) {
                xt_log (XT_RBTHASH, XT_LOG_ERROR, "Failed to get bucket");
                return NULL;
        }

        searchentry.key = key;
        searchentry.keylen = keylen;

        LOCK (&bucket->bucketlock);
        {
                entry = rb_delete (bucket->bucket, &searchentry);
        }
        UNLOCK (&bucket->bucketlock);

        if (!entry)
                return NULL;

        XT_FREE (entry->key);
        dataref = entry->data;
        mem_put (tbl->entrypool, entry);

        return dataref;
}


static void
rbthash_entry_deiniter (void *entry, void *rbparam)
{
        if (!entry)
                return;

        rbthash_deinit_entry (rbparam, entry);
}


static void
rbthash_table_destroy_buckets (rbthash_table_t *tbl)
{
        int x = 0;
        if (!tbl)
                return;

        for (;x < tbl->numbuckets; x++) {
                LOCK_DESTROY (&tbl->buckets[x].bucketlock);
                rb_destroy (tbl->buckets[x].bucket, rbthash_entry_deiniter);
        }

        return;
}


void
rbthash_table_destroy (rbthash_table_t *tbl)
{
        if (!tbl)
                return;

        rbthash_table_destroy_buckets (tbl);
        if (tbl->pool_alloced)
                mem_pool_destroy (tbl->entrypool);

        LOCK_DESTROY (&tbl->tablelock);

        XT_FREE (tbl->buckets);
        XT_FREE (tbl);
}
