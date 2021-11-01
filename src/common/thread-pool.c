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
#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include "common.h"
#include "mem.h"
#include "thread-pool.h"
#include "logging.h"

static int
log_base2 (unsigned long x)
{
        int val = 0;

        while (x > 1) {
                x /= 2;
                val++;
        }

        return val;
};

static tp_t *
__tp_dequeue (tp_ctrl_t *ctrl)
{
	tp_t *tp;

	if (xlist_empty(&ctrl->queue))
		return NULL;

	tp = xlist_entry (&ctrl->queue, tp_t, list);
	ctrl->qsize--;
	xlist_del_init(&tp->list);
	return tp;
}

static void *
tp_worker (void *data)
{
        tp_ctrl_t *ctrl = NULL;
        tp_t *tp = NULL;
        struct timespec sleep_till = {0, };
        int timeout = 0;
        int ret = 0;
        int bye = 0;

        if (data == NULL) {
                xt_log ("tp", XT_LOG_ERROR, "invalid argument");
                return NULL;
        }

        ctrl = (tp_ctrl_t*)data;

        while (1) {
                tp = NULL;
                sleep_till.tv_sec = time (NULL) + (uint32_t) ctrl->idle_time;

                pthread_mutex_lock (&ctrl->mutex);
                if (ctrl->exit_asked) {
                        ctrl->curr_thread_count -- ;
                        pthread_mutex_unlock (&ctrl->mutex);
                        break;
                }

                while (ctrl->qsize == 0) {
                        ctrl->sleep_thread_count ++;
                        ret = pthread_cond_timedwait (&ctrl->cond, 
                                                      &ctrl->mutex,
                                                      &sleep_till);
                        ctrl->sleep_thread_count --;
                        if (ret == ETIMEDOUT) {
                                timeout = 1;
                                break;
                        }
                }

                if (timeout) {
                        if (ctrl->curr_thread_count > ctrl->min_thread_count) {
                                ctrl->curr_thread_count --;
                                bye = 1;
                        } else {
                                timeout = 0;
                        }
                } 

                if (ctrl->qsize > 0) {
                        tp = __tp_dequeue (ctrl);
                        ctrl->tp_processed ++;
                }
                pthread_mutex_unlock (&ctrl->mutex);

                if (tp && tp->handler) {
                        tp->handler (tp);
                }

                if (bye) break;
        }

        tp_ctrl_free (ctrl);

        return NULL;
}

static inline int
__tp_threads_scale (tp_ctrl_t *ctrl, int task_count)
{
        int log2 = 0;
        int scale = 0;
        int diff = 0;
        int ret = 0;
        pthread_t thread;

        log2 = log_base2 (task_count);
        scale = log2;
        if (scale < ctrl->min_thread_count) {
                scale = ctrl->min_thread_count;
        }
        if (scale > ctrl->max_thread_count) {
                scale = ctrl->max_thread_count;
        }

        if ( scale > ctrl->curr_thread_count) {
                diff = scale - ctrl->curr_thread_count;
        }
        while (diff > 0) {
                diff --;

                tp_ctrl_ref (ctrl);
                ret = pthread_create (&thread, &ctrl->w_attr, 
                                      tp_worker, ctrl);
                if (ret == 0) {
                        ctrl->curr_thread_count ++;
                        xt_log ("thread-pool", XT_LOG_INFO, 
                                "scaled threads for %s to %d",
                                ctrl->name, ctrl->curr_thread_count);
                } else {
                        tp_ctrl_free (ctrl);
                        return -1;
                }
        }

        return diff;
}

static int
__tp_enqueue (tp_ctrl_t *ctrl, tp_t *tp)
{

	struct xlist_head *queue = &ctrl->queue;
 
        xlist_add_tail (&tp->list, queue);
        ctrl->qsize ++;
        xt_log ("thread-pool", XT_LOG_TRACE,
                "%s ctrl->qsize++ %d. curr_count %d",
                ctrl->name, ctrl->qsize,
                ctrl->curr_thread_count);

        pthread_cond_signal (&ctrl->cond);
        __tp_threads_scale (ctrl, ctrl->qsize);

        return 0;
}

int tp_enqueue (tp_ctrl_t *ctrl, tp_t *tp)
{
	int ret;

	pthread_mutex_lock (&ctrl->mutex);
	ret = __tp_enqueue(ctrl, tp);
	pthread_mutex_unlock (&ctrl->mutex);

	return ret;
}

tp_ctrl_t *
tp_ctrl_new (char *name, int32_t stack_size, int32_t idle_time,
	     int32_t min_thread_count, int32_t max_thread_count)
{
        tp_ctrl_t *ctrl = NULL;
        int ret;

        ctrl = CALLOC (1, sizeof(tp_ctrl_t));
        if (ctrl == NULL) return NULL;

        pthread_mutex_init (&ctrl->mutex, NULL);
        ctrl->stack_size = stack_size;
        ctrl->idle_time = idle_time;
        ctrl->max_thread_count = max_thread_count;
        ctrl->min_thread_count = min_thread_count;
        ctrl->refcount = 1;
        ctrl->name = name? xt_strdup (name) : xt_strdup ("anon-worker");
        ctrl->exit_asked = _xt_false;

        INIT_XLIST_HEAD (&ctrl->queue);

        if (!ctrl->name) {
                tp_ctrl_free (ctrl);
                return NULL;
        }

        pthread_attr_init (&ctrl->w_attr);
        ret = pthread_attr_setstacksize (&ctrl->w_attr, ctrl->stack_size);
        if (ret == EINVAL) {
                xt_log ("", XT_LOG_WARNING,
                        "Using default thread stack size");
        }

        xt_log ("thread-pool", XT_LOG_INFO,
                "%s, idle-time: %d, max-count: %d", 
                ctrl->name, ctrl->idle_time, ctrl->max_thread_count);

        return ctrl;
}

int
tp_ctrl_free (tp_ctrl_t *ctrl) 
{
        int refcount;
	tp_t *tp = NULL;

        refcount = atomic_sub_and_fetch(&((ctrl)->refcount), 1);
        if (refcount > 0) {
                return 0;
        }

	pthread_mutex_lock(&ctrl->mutex);

	while (ctrl->qsize > 0) {
		tp = __tp_dequeue(ctrl);
		if (tp && tp->cleanup) {
			tp->cleanup(tp);
		}
	}

        if (ctrl->name) {
                XT_FREE (ctrl->name);
        }

	pthread_mutex_unlock(&ctrl->mutex);
	pthread_cond_destroy(&ctrl->cond);
        FREE(ctrl);

        return 0;
}

int
tp_threads_start (tp_ctrl_t *ctrl)
{
        if (!ctrl) return -1;

        pthread_mutex_lock (&ctrl->mutex);
        __tp_threads_scale (ctrl, 0);
        pthread_mutex_unlock (&ctrl->mutex);

        return 0;
}

int
tp_threads_finish (tp_ctrl_t *ctrl)
{
        if (!ctrl) return -1;

        pthread_mutex_lock (&ctrl->mutex);
        ctrl->exit_asked = _xt_true;
	pthread_cond_broadcast (&ctrl->cond);
        pthread_mutex_unlock (&ctrl->mutex);

        return 0;
}

int
tp_purge (tp_ctrl_t *ctrl, tp_match_t match, void *data)
{
        tp_t *tp = NULL;
        tp_t *tmp = NULL;
         struct xlist_head purge_list;

        INIT_XLIST_HEAD (&purge_list);

        pthread_mutex_lock (&ctrl->mutex);
        xlist_for_each_entry (tp, &ctrl->queue, list) {
		if (match (tp, data)) {
			xlist_move_tail(&tp->list, &purge_list);
			ctrl->qsize--;
		}
        }
        pthread_mutex_unlock (&ctrl->mutex);

        xlist_for_each_entry_safe (tp, tmp, &purge_list, list) {
                if (tp->cleanup) {
                        tp->cleanup (tp);
                }
        }

        return 0;
}

