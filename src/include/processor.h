#ifndef __MH_PROCESSOR_H__
#define __MH_PROCESSOR_H__

#include <pthread.h>
#include <semaphore.h>

#include "mem.h"
#include "xlist.h"
#include "rbthash.h"

#include "filesystem.h"
#include "database.h"

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
typedef struct entry_proc_op
{
	op_type_t op;
	ino_t id;
	ino_t pid;

	/* current stage in pipeline */
	unsigned int stage;

	/* internal flag for pipeline management */
	unsigned int wait_parent_add:1;
	unsigned int wait_chld_del:1;
	unsigned int wait_obj_proc:1;
	unsigned int woke_up:1;
	unsigned int can_skip:1;
	unsigned int invalid:1;
	unsigned int no_release:1;

	time_t log_inserted; /* used by changelog reader */

	/* double chained list for pipeline */
	struct xlist_head list;

	struct xlist_head waitq;

	void *extra_info;

	void * obj; /* pointer to op_object in the rbthash */

	void *worker; /* Current worker handler thread */
} entry_proc_op_t;

typedef struct op_object
{
	ino_t obj;
	xt_lock_t lock;
	unsigned int ref;
	unsigned int being_proceed:1;
	unsigned int being_created:1;
	unsigned int being_removed:1;

	unsigned int deleting_children;
	struct xlist_head pending;
} op_obj_t;

/*
 * each stage of the pipeline consist of the following information:
 */
typedef struct stage_stat
{
    unsigned int   nb_threads;
    unsigned int   nb_unprocessed_entries;
    unsigned int   nb_current_entries;
    unsigned int   nb_processed_entries;
    unsigned long long total_processed;
    unsigned long long nb_batches;
    unsigned long long total_batched_entries;
    struct timeval total_processing_time;
} stage_stat_t;


typedef struct pipeline_stage {
	xt_lock_t mutex;
	struct xlist_head entries;
	int outstanding;
	stage_stat_t stage_stat;
	rbthash_table_t *obj_tbl;
} pipeline_stage_t;

/*
 * Definition of pipeline stage functions
 */
typedef int (*step_function_t) (void *pl, struct entry_proc_op *op);
typedef int (*step_valid_op_function_t) (pipeline_stage_t *stage,
    struct entry_proc_op *op);
typedef void (*step_post_function_t) (void *pl,
    struct entry_proc_op *op);

/*
 * Definition of a pipeline stage
 */
typedef struct pipeline_stage_desc {
	unsigned int stage_index; 
	const char *stage_name;  
	step_valid_op_function_t valid_op;
	step_function_t function; 
	step_post_function_t post_function;
	unsigned int max_thread_count; /*< 0 = UNLIMITED */
} pipeline_stage_desc_t;

typedef int (*pl_init_t) (void *pl);
typedef void (*pl_fini_t) (void *pl);

typedef int (*pl_conf_parse_t) (cJSON *seg, void **config);
typedef pipeline_stage_desc_t * (*get_stages_t) (int *stagecnt);

typedef struct worker_info {
	unsigned int index;
	pthread_t tid;
	void *priv;
	void *pl; /* point back to the pipeline_desc */
} worker_info_t;

typedef int (*pl_worker_init_t) (worker_info_t *worker);
typedef void (*pl_worker_fini_t) (worker_info_t *worker);
/*
 * This structure indicates pipeline steps index and limits
 */
struct metahunter;
typedef struct processor
{
	char *name;
	int workercnt;
	int outstanding_ops;
	void *conf;
	void *dlhandle;
	void *private;

	/*
	 * back reference
	 */
	void *info;

	/*
	 * loadable pipeline module stages description
	 */
	int stage_count;
	pipeline_stage_desc_t *stages_desc;
	pl_conf_parse_t conf_parse;
	pl_init_t init;
	pl_fini_t fini;
	pl_worker_init_t worker_init;
	pl_worker_fini_t worker_fini;

	/*
	 * stage array.
	 */
	pipeline_stage_t *stages;
	int waiting_workers;
	worker_info_t *workers;
	xt_lock_t lock;
	xt_cond_t cond;
	sem_t credit;
	struct mem_pool *op_pool;
	struct mem_pool *obj_pool;
	struct mem_pool *obj_tbl_pool;
	int exiting;
} processor_t;

int processor_load(processor_t *pl);

int processor_init(processor_t *pl);

void processor_cleanup(processor_t *pl);

/*
 * get a new entry
 */
entry_proc_op_t *entry_proc_get(processor_t *pl);

/*
 * release a entry
 */
void entry_proc_put(processor_t *pl, entry_proc_op_t *op);

/*
 * according the pipeline stage and selected appropriate pipeline
 * start processing.
 */
void entry_proc_push(processor_t *pl, entry_proc_op_t *op);

int entry_valid_op(pipeline_stage_t *s, entry_proc_op_t *op);

/*
 * the entry is ready to push to next stage
 */
void entry_stage_cmplt(void *pipeline, struct entry_proc_op *op);

void entry_post_op(void *pipeline, struct entry_proc_op *op);


#endif
