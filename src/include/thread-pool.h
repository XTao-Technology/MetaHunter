#include "common.h"
#include "xlist.h"
#include <pthread.h>
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

#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <sys/types.h>
#include <pthread.h>
#include "xlist.h"
#include "logging.h"
typedef struct tp_s tp_t;
typedef struct tp_ctrl_s tp_ctrl_t;

typedef int (*thread_handler_t) (tp_t *tp);
typedef xt_boolean_t (*tp_match_t) (tp_t *tp, void *data);

struct tp_s {
        struct xlist_head        list;
        thread_handler_t         handler;
        thread_handler_t         cleanup;
};


struct tp_ctrl_s {
        int32_t                 idle_time;
        pthread_attr_t          w_attr;

        char                    *name;
        int32_t                 stack_size;
        int32_t                 refcount;
        xt_boolean_t            exit_asked; 

        int32_t                 min_thread_count;
        int32_t                 max_thread_count;
        int32_t                 curr_thread_count;
        int32_t                 sleep_thread_count;

	struct xlist_head       queue;
        pthread_cond_t          cond;
        int32_t                 qsize;
        uint64_t                tp_processed;

        pthread_mutex_t         mutex;

};

int tp_enqueue (tp_ctrl_t *ctr, tp_t *tp);

tp_ctrl_t *tp_ctrl_new (char *name, int32_t stack_size,
                        int32_t idle_time, int32_t min_thread_count,
			int32_t max_thread_count);

#define tp_ctrl_ref(ctrl) \
        atomic_add_and_fetch(&((ctrl)->refcount), 1)

int tp_threads_start (tp_ctrl_t *ctrl); 
int tp_threads_finish (tp_ctrl_t *ctrl);
int tp_purge (tp_ctrl_t *ctrl, tp_match_t match, void *data);
int tp_ctrl_free(tp_ctrl_t *ctrl);

#endif
