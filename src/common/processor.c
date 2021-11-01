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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <semaphore.h>
#include <dlfcn.h>
#include <netdb.h>
#include <fnmatch.h>

#include "common.h"
#include "mem.h"
#include "logging.h"
#include "xlist.h"
#include "hashfn.h"
#include "rbthash.h"
#include "filesystem.h"
#include "database.h"
#include "processor.h"
#include "mattr.h"

#define MHPROC "MH_PROCESSOR"

int processor_load(processor_t *pl)
{
	char *name = NULL;
	int ret = -1;
	get_stages_t func;
	int stgcnt = 0;

	xt_log(MHPROC, XT_LOG_TRACE, "processor loading..");

	ret = xt_asprintf(&name, "%s/%s.so", MHPROCDIR, pl->name);
	if (ret == -1) {
		xt_log(MHPROC, XT_LOG_ERROR, "failed to compose lib name %s",
		    pl->name);
		return ret;
	}

	pl->dlhandle = dlopen (name, RTLD_NOW| RTLD_GLOBAL);
	if (!pl->dlhandle) {
		ret = -1;
		xt_log(MHPROC, XT_LOG_ERROR, "failed to load %s: %s", name,
		       dlerror());
		goto out;
	}

	xt_log(MHPROC, XT_LOG_TRACE, "loading get_stages");
	func = dlsym(pl->dlhandle, "get_stages");
	if (!func) {
		ret = -1;
		xt_log(MHPROC, XT_LOG_ERROR, "failed to load get_stages func");		
	}

	pl->stages_desc = func(&stgcnt);
	if (!pl->stages_desc) {
		ret = -1;
		xt_log(MHPROC, XT_LOG_ERROR, "failed to load stages");
		goto out;
	}

	pl->stage_count = stgcnt;
	/*
	 * processor can specify extra configuration
	 */
	xt_log(MHPROC, XT_LOG_TRACE, "loading conf_parse");
	pl->conf_parse = dlsym(pl->dlhandle, "conf_parse");

	xt_log(MHPROC, XT_LOG_TRACE, "loading init");
	pl->init = dlsym(pl->dlhandle, "init");

	xt_log(MHPROC, XT_LOG_TRACE, "loading fini");
	pl->fini = dlsym(pl->dlhandle, "fini");

	xt_log(MHPROC, XT_LOG_TRACE, "loading worker init");
	pl->worker_init = dlsym(pl->dlhandle, "worker_init");

	xt_log(MHPROC, XT_LOG_TRACE, "loading worker fini");
	pl->worker_fini = dlsym(pl->dlhandle, "worker_fini");

	ret = 0;
out:
	if (ret && pl->dlhandle)
		dlclose(pl->dlhandle);
	XT_FREE(name);
	return ret;
}

static void entry_push(processor_t *pl, entry_proc_op_t *op)
{
	op_obj_t *obj = NULL;
 	pipeline_stage_t *stage = &pl->stages[op->stage];
	ino_t parentid = op->pid;
	op_obj_t *parent = NULL;

	LOCK(&stage->mutex);
	xlist_add_tail(&op->list, &stage->entries);
	/*
	 * search wether the object exists in rbtree at the current stage
	 * if the object exists which means there are outstanding ops
	 * about the object at current stage.
	 */
	obj = rbthash_get(stage->obj_tbl, &op->id, sizeof(op->id));
	if (obj == NULL) {
		obj = mem_get0(pl->obj_pool);
		INIT_XLIST_HEAD(&obj->pending);
		rbthash_insert(stage->obj_tbl, obj, &op->id, sizeof(op->id));
		obj->obj = op->id;
		xt_log(MHPROC, XT_LOG_TRACE, "object: %lu does not exist at "
		       "stage:%d create rbentry :%p.", op->id, op->stage, obj);
	} else {
		xt_log(MHPROC, XT_LOG_TRACE, "object: %lu has outstanding, "
		    "op at stage: %d already.", op->id, op->stage);		
	}

	obj->ref++;
	stage->outstanding++;
	op->obj = obj;

	xt_log(MHPROC, XT_LOG_TRACE, "stage:%d object %lu ref %u, stage "
	    "outstanding %d.", op->stage, op->id, obj->ref,
	    stage->outstanding);

	/*
	 * set mark for obj or parent according the op
	 */
	switch (op->op) {
	case op_create:
	case op_mkdir:
		obj->being_created = 1;
		break;
	case op_unlink:
	case op_rmdir:
		obj->being_removed = 1;
		/*
		 * any deletion ops, should mark the parent
		 * there is something under the parent is
		 * deleting. increase the deleting children
		 * counter.
		 */
		parent = rbthash_get(stage->obj_tbl, &parentid, sizeof(ino_t));
		if (parent == NULL) {
			/*
			 * If parent is NULL, create a parent obj
			 * and reference the parent for unlink/rmdir
			 */
			parent = mem_get0(pl->obj_pool);
			INIT_XLIST_HEAD(&parent->pending);
			rbthash_insert(stage->obj_tbl, parent, &parentid, sizeof(ino_t));
			parent->obj = parentid;
			
		}
		parent->deleting_children++;
		parent->ref++;
		break;
	default:
		break;
	}
	UNLOCK(&stage->mutex);	
}

/*
 * select appropriate pipeline and stage
 * put the op into the queue
 */
void entry_proc_push(processor_t *pl, entry_proc_op_t *op)
{
	/*
	 * semphore is to limit the outstanding ops count
	 */
	sem_wait(&pl->credit);

	xt_log(MHPROC, XT_LOG_TRACE, "push the op:%p to processor", op);

	entry_push(pl, op);

	LOCK(&pl->lock);
	if (pl->waiting_workers > 0)
		COND_SIGNAL(&pl->cond);
	UNLOCK(&pl->lock);
}

/*
 * check if the op can be merged with pending ops of the
 * object, if merged, mark the op as can_skip, then list
 * into the pending.
 */
static void entry_proc_pending(entry_proc_op_t *op)
{
	op_obj_t *obj = op->obj;
	entry_proc_op_t *o = NULL;
	entry_proc_op_t *t = NULL;
	struct xlist_head *tail = NULL;

	if (!op->wait_obj_proc && !op->wait_chld_del)
		return;

	if (xlist_empty(&obj->pending))
		goto out;

	switch (op->op) {
	case op_create:
	case op_mkdir:
		break;
	case op_unlink:
	case op_rmdir:
		/*
		 * if pending ops are wait_parent_add
		 * which means the op_create hasn't be
		 * executed. the create and the unlink
		 * can be canncel.
		 *
		 * if pending ops are wait_obj_proc,
		 * unlink means outstanding setattrs
		 * can be skipped.
		 */
		xlist_for_each_entry_safe(o, t, &obj->pending,
		    waitq) {
			if (o->wait_parent_add) {
				o->can_skip = 1;
				op->can_skip = 1;
			}
			if (o->wait_obj_proc) {
				o->can_skip = 1;
			}
		}
		break;
	default:
		tail = xlist_get_tail(&obj->pending);
		/*
		 * check previous action, if the previous pending
		 * action also setattr, the last can be skipped.
		 */
		o = xlist_entry(tail, entry_proc_op_t, waitq);
		if (o->wait_obj_proc) {
			o->can_skip = 1;
		}
			
		break;
	}

out:

	/*
	 * regardless the op can be skipped or not, dequeue
	 * the pending op from post_op since there would
	 * deal with dependencies of ops. CANNOT dequeue
	 * can_skip op here in case some ops depend on the
	 * can_skip op.
	 */

	xlist_add_tail(&op->waitq, &obj->pending);
}

/*
 * validate whether the op can be taken by worker to proceed
 * return 0 means the op should be suspend
 * return 1 means the op can be taken by worker
 * return -1 means something error happens.
 */
int entry_valid_op(pipeline_stage_t *stage, entry_proc_op_t *op)
{
	int ret = 0;
	op_obj_t *obj = op->obj;
	ino_t parentid = op->pid;
	op_obj_t *parent = rbthash_get(stage->obj_tbl, &parentid,
	    sizeof(ino_t));

	switch (op->op) {
	case op_create:
	case op_mkdir:
		if (parent) {
			if (parent == obj) {
				/*
				 * should not happen
				 */
				break;
			}
			if (parent->being_created) {
				/*
				 * If parent creation is pending
				 * wait until parent creation complete.
				 */
				op->wait_parent_add = 1;
				xlist_add_tail(&op->waitq, &parent->pending);
			}
		}

		break;

	case op_unlink:
		if (obj->being_created || !xlist_empty(&obj->pending)
		    || obj->being_proceed) {
			/*
			 * If the object is pending on create, or other
			 * pending operations or being proceed. wait
			 * until the previous operation complete.
			 */
			op->wait_obj_proc = 1;
			entry_proc_pending(op);
		}

		break;

	case op_rmdir:
		if (obj->being_created || !xlist_empty(&obj->pending) ||
		    obj->being_proceed) {
			op->wait_obj_proc = 1;
			entry_proc_pending(op);
			break;
		}

		if (obj->deleting_children) {
			/*
			 * If children deletion outstanding
			 */
			op->wait_chld_del = 1;
			entry_proc_pending(op);
		}
		break;
	case op_init:
		op->no_release = 1;
	case op_setattr:
		if (obj->being_created || !xlist_empty(&obj->pending) ||
		    obj->being_proceed) {
			op->wait_obj_proc = 1;
			entry_proc_pending(op);
		}

		break;

	default:
		/*
		 * invalid op is also proceed to post_op destroy it.
		 */
		xt_log(MHPROC, XT_LOG_ERROR, "invalid op %d!", op->op);
		op->invalid = 1;
		break;
	}

	if (!op->can_skip && !op->wait_obj_proc && !op->wait_chld_del
	    && !op->wait_parent_add) {
		obj->being_proceed = 1;
		ret = 1;
	}

	return ret;
}

static void _entry_stage_cmplt(void *processor, struct entry_proc_op *op)
{
	processor_t *pl = (processor_t *)processor;

	op->stage++;
	if (op->invalid || op->stage == pl->stage_count) {
		/*
		 * last stage, no need to move to next stage
		 */
		sem_post(&pl->credit);

		xt_log(MHPROC, XT_LOG_TRACE, "release op:%p", op);

		/*
		 * release op
		 */
		entry_proc_put(pl, op);
		return;
	} else {
		/*
		 * push to next stage of pipeline
		 */
		xt_log(MHPROC, XT_LOG_TRACE, "push to next stage:%d op:%p", op->stage, op);
		entry_push(pl, op);
		/*
		 * don't need wakeup /signal other threads for
		 * the next journey of the op entry at the next
		 * stage since the current thread would loop
		 * back to the begining check available entries.
		 */
	}
}

/*
 * the jentry might be released during invoke the function
 * please be cauious to use jentry in the function.
 */
void entry_stage_cmplt(void *processor, struct entry_proc_op *op)
{
	processor_t *pl = (processor_t *)processor;
	pipeline_stage_t *stage = &pl->stages[op->stage];
	op_obj_t *obj = op->obj;
	ino_t parentid = op->pid;
	op_obj_t *parent = NULL;

	LOCK(&stage->mutex);

	obj->ref--;
	if (!obj->ref) {
		/*
		 * all outstanding op about the object has already been proceed,
		 * release the rbthash entry at the current stage.
		 */
		rbthash_remove(stage->obj_tbl, &op->id, sizeof(op->id));
		mem_put(pl->obj_pool, obj);
	}

	if (op->op == op_unlink || op->op == op_rmdir) {
		/*
		 * unlink/rmdir reference parent obj, should
		 * cleanup is reference back to 0.
		 */
		parent = rbthash_get(stage->obj_tbl, &parentid,
		    sizeof(ino_t));
		if (parent) {
			parent->ref--;
			if (!parent->ref) {
				rbthash_remove(stage->obj_tbl, &parentid,
				    sizeof(ino_t));
				mem_put(pl->obj_pool, parent);
			}
		}
	}

	UNLOCK(&stage->mutex);

	_entry_stage_cmplt(processor, op);
}

/*
 * Post op process handler
 * the journal_entry might be released during invoke the function
 * please be cauious to use jentry in the function.
 */
void entry_post_op(void *processor, struct entry_proc_op *op)
{
	processor_t *pl = (processor_t *)processor;
	pipeline_stage_t *stage = &pl->stages[op->stage];
	op_obj_t *obj = op->obj;
	entry_proc_op_t *o = NULL;
	entry_proc_op_t *t = NULL;
	ino_t parentid = op->pid;
	int wakeup = 0;

	LOCK(&stage->mutex);

	op_obj_t *parent = rbthash_get(stage->obj_tbl, &parentid,
	    sizeof(ino_t));

	/*
	 * reset can_skip whatever it is
	 */
	op->can_skip = 0;

	switch (op->op) {
	case op_mkdir:
		obj->being_created = 0;
		/*
		 * wakeup op include different children creation
		 */
		xlist_for_each_entry_safe(o, t, &obj->pending,
		    waitq) {
			if (o->wait_parent_add) {
				o->wait_parent_add = 0;
				wakeup++;
				xt_log(MHPROC, XT_LOG_TRACE, "mkdir complete "
				    "wakeup op: %p", o);
				o->woke_up = 1;
				xlist_del(&o->waitq);
			}
		}
		break;

	case op_unlink:

	case op_rmdir:
		obj->being_removed = 0;
		if (parent == NULL) {
			xt_log(MHPROC, XT_LOG_ERROR, "impossible "
			    "parent is NULL during deleting. ");
			break;
		}
			
		parent->deleting_children--;
		parent->ref--;
		if (!parent->deleting_children && parent->being_removed) {
			/*
			 * wakeup parent deletion
			 */
			xlist_for_each_entry_safe(o, t, &parent->pending,
			    waitq) {
				if (o->wait_chld_del) {
					o->wait_chld_del = 0;
					xlist_del(&o->waitq);
					wakeup = 1;
					o->woke_up = 1;
					xt_log(MHPROC, XT_LOG_TRACE, "chld "
					    "deleting complete, wakeup del "
					    "parent op :%p", o);
					break;
				}
			}
		}

		/*
		 * unlink/rmdir reference parent obj, so post_op should
		 * check if the ref is 0, if so, cleanup the parent
		 */
		if (!parent->ref) {
			rbthash_remove(stage->obj_tbl, &parentid, sizeof(ino_t));
			mem_put(pl->obj_pool, parent);
		}
		break;

	case op_create:
		obj->being_created = 0;
		/*
		 * fall through
		 */
	case op_setattr:
	case op_init:
		if (!xlist_empty(&obj->pending)) {
			/*
			 * wake up a following op
			 */
			xlist_for_each_entry_safe(o, t, &obj->pending, waitq) {
				if (o->wait_obj_proc) {
					o->wait_obj_proc = 0;
					xlist_del(&o->waitq);
					o->woke_up = 1;
					xt_log(MHPROC, XT_LOG_TRACE, "creat/"
					    "setattr/init complete, wakeup "
					    "op :%p", o);
					wakeup = 1;
					break;
				}
			}
		}
		break;
	default:
		/*
		 * invalid op
		 */
		break;
	}

	/*
	 * clean up being_proceed flag
	 */
	obj->being_proceed = 0;

	obj->ref--;
	if (!obj->ref) {
		/*
		 * all outstanding op about the object has already been prceed,
		 * release the rbthash entry.
		 */
		rbthash_remove(stage->obj_tbl, &op->id, sizeof(op->id));
		mem_put(pl->obj_pool, obj);
	}

	UNLOCK(&stage->mutex);

	_entry_stage_cmplt(pl, op);

	LOCK(&pl->lock);
	if (pl->waiting_workers > 0 && (wakeup == 1))
		COND_SIGNAL(&pl->cond);
	if (pl->waiting_workers > 1 && (wakeup > 1))
		COND_BROADCAST(&pl->cond);
	
	UNLOCK(&pl->lock);
	xt_log(MHPROC, XT_LOG_TRACE, "post op:%p, stage:%d", op, op->stage);
}

static entry_proc_op_t *entry_next_op(processor_t *pl)
{
	int i = 0;
	pipeline_stage_t *stage = NULL;
	entry_proc_op_t *processing_op = NULL;
	step_valid_op_function_t validate = NULL;

	for (i = pl->stage_count-1; i >= 0; i--) {
		entry_proc_op_t *op = NULL;
		entry_proc_op_t *tmp = NULL;

		stage = &pl->stages[i];
		validate = pl->stages_desc[i].valid_op;

		LOCK(&stage->mutex);
		if (stage->outstanding) {
			xlist_for_each_entry_safe(op, tmp, &stage->entries,
			    list) {
				if (!validate) {
					processing_op = op;
					break;
				}

				if (op->can_skip) {
					processing_op = op;
					break;
				}

				if (op->wait_parent_add || op->wait_chld_del ||
				    op->wait_obj_proc)
					continue;

				if (op->woke_up) {
					processing_op = op;
					break;
				}

				if (validate(stage, op)) {
					processing_op = op;
					break;
				}
			}
		}

		if (processing_op) {
			xlist_del_init(&processing_op->list);
			stage->outstanding--;
		}

		UNLOCK(&stage->mutex);

		if (processing_op)
			break;
	}
	return processing_op;
}

static void *entry_proc_worker(void *arg)
{
	worker_info_t *worker = (worker_info_t *)arg;
	processor_t *pl = (processor_t *)(worker->pl);
	entry_proc_op_t *op;
	step_function_t func;
	step_post_function_t post_func;
	int wait = 0;
	int ret = -1;
	
	ret = pl->worker_init(worker);
	if (ret)
		return NULL;

	xt_log(MHPROC, XT_LOG_TRACE, "worker %d start...", worker->index);

	while (1) {
		LOCK(&pl->lock);
		while(!(op = entry_next_op(pl))) {
			pl->waiting_workers++;
			wait = 1;
			if (pl->exiting) {
				/*
				 * thread exiting
				 */
				xt_log(MHPROC, XT_LOG_TRACE, "worker %d "
				    "exiting...", worker->index);
				UNLOCK(&pl->lock);
				goto out;
			}
			COND_WAIT(&pl->cond, &pl->lock);
		}
		if (wait) {
			pl->waiting_workers--;
			wait = 0;
		}

		UNLOCK(&pl->lock);

		xt_log(MHPROC, XT_LOG_TRACE, "worker %d take op:%p",
		       worker->index, op);

		func = pl->stages_desc[op->stage].function;
		
		/*
		 * if can_skip is set, skip function
		 */
		if (func && !op->can_skip &&!op->invalid) {
			xt_log(MHPROC, XT_LOG_TRACE, "worker %d proceed:%p",
			    worker->index, op);
			op->worker = worker;
			func((void *)pl, op);
		}
		
		post_func = pl->stages_desc[op->stage].post_function;
		if (post_func) {
			xt_log(MHPROC, XT_LOG_TRACE, "worker %d post handle:%p",
			    worker->index, op);
			post_func((void *)pl, op);
		}
	}

out:
	pl->worker_fini(worker);
	xt_log(MHPROC, XT_LOG_INFO, "worker %d exit...", worker->index);
	return NULL;
}

/*
 * pipeline initialization
 */
int processor_init(processor_t *pl)
{
	int i = 0;
	int ret = 0;
	worker_info_t *workers = NULL;

	/*
	 * obj_tbl_pool is pipeline-wise, but tbl is stage wise
	 */
	pl->obj_tbl_pool = mem_pool_new(sizeof(rbthash_entry_t),
	    pl->outstanding_ops);

	if (pl->obj_tbl_pool == NULL) {
		ret = ENOMEM;
		goto err;
	}

	/*
	 * intialize op memory pool
	 */
	pl->op_pool = mem_pool_new(sizeof(entry_proc_op_t),
	    pl->outstanding_ops);
	if (pl->op_pool == NULL) {
		ret = ENOMEM;
		goto err;
	}

	/*
	 * intialize object pool associated to the outstanding ops
	 */
	pl->obj_pool = mem_pool_new(sizeof(op_obj_t),
	    pl->outstanding_ops);
	if (pl->obj_pool == NULL) {
		ret = ENOMEM;
		goto err;		
	}

	/*
	 * initialize pipeline outstanding op count limit
	 */
        sem_init(&pl->credit, 0, pl->outstanding_ops);

	LOCK_INIT(&pl->lock);
	COND_INIT(&pl->cond);

	/*
	 * initialize pipeline stages
	 */
	pl->stages = XT_CALLOC(pl->stage_count,
	    sizeof(pipeline_stage_t));

	/*
	 * initialize stage of pipeline
	 */
	for (i = 0; i < pl->stage_count; i++) {
		/*
		 * object rbtree is per stage. object is unqiue
		 * only in single stage. But different could have
		 * object with the same id in different rbtree of
		 * stages.
		 */
		pipeline_stage_t *stage = &pl->stages[i];

		stage->obj_tbl = rbthash_table_init(1,
		    (rbt_hasher_t)SuperFastHash, NULL, 
	            0, pl->obj_tbl_pool);

		if (stage->obj_tbl == NULL) {
			ret = ENOMEM;
			goto err;
		}
		INIT_XLIST_HEAD(&stage->entries);
		LOCK_INIT(&stage->mutex);
	}

	if (pl->init) {
		/*
		 * initialize the pipeline private structure
		 * for specific pipeline if needed.
		 */
		ret = pl->init(pl);
		if (ret) {
			goto err;
		}
	}

	/*
	 * initialize worker threads
	 */
	workers = XT_CALLOC(pl->workercnt,
	    sizeof(worker_info_t));
	if (workers == NULL) {
		ret = ENOMEM;
		goto err;
	}

	memset(workers, 0, sizeof (worker_info_t) * pl->workercnt);
	
	pl->workers = workers;
	pl->waiting_workers = pl->workercnt;

	for (i = 0; i < pl->workercnt; i++) {
		worker_info_t *info = &workers[i];
		info->index = i;
		info->pl = (void *)pl;
		ret = pthread_create(&info->tid, NULL,
		    entry_proc_worker, info);
		if (ret) {
			ret = errno;
			goto err;
		}
	}

	return 0;
err:
	if (workers) {
		XT_FREE(workers);
	}

	if (pl->obj_tbl_pool)
		mem_pool_destroy(pl->obj_tbl_pool);

	if (pl->op_pool)
		mem_pool_destroy(pl->op_pool);

	if (pl->obj_pool)
		mem_pool_destroy(pl->obj_pool);

	if (pl->stages) {
		for (i = 0; i < pl->stage_count; i++) {
			pipeline_stage_t *stage = &pl->stages[i];
			if (stage->obj_tbl) {
				LOCK_DESTROY(&stage->mutex);
				rbthash_table_destroy(stage->obj_tbl);
			}
		}
		XT_FREE(pl->stages);
	}
	
	return ret;
}

void processor_cleanup(processor_t *pl)
{
	int i;
	void *ret;

	if (!pl || !pl->stages)
		return;

	LOCK(&pl->lock);
	pl->exiting = 1;
	COND_BROADCAST(&pl->cond);
	UNLOCK(&pl->lock);

	for (i = 0; i < pl->workercnt; i++) {
		pthread_join(pl->workers[i].tid, &ret);
	}

	if (pl->workers) {
		XT_FREE(pl->workers);
	}

	if (pl->fini) {
		/*
		 * deconstruct pl->private
		 */
		pl->fini(pl);
	}

	if (pl->stages) {
		for (i = 0; i < pl->stage_count; i++) {
			pipeline_stage_t *stage = &pl->stages[i];
			if (stage->obj_tbl) {
				LOCK_DESTROY(&stage->mutex);
				rbthash_table_destroy(stage->obj_tbl);
			}
		}
		XT_FREE(pl->stages);
		pl->stages = NULL;
	}
	if (pl->obj_tbl_pool)
		mem_pool_destroy(pl->obj_tbl_pool);

	if (pl->op_pool)
		mem_pool_destroy(pl->op_pool);

	if (pl->obj_pool)
		mem_pool_destroy(pl->obj_pool);
}

/*
 * get the op
 */
entry_proc_op_t *entry_proc_get(processor_t *pl)
{
	entry_proc_op_t *op;
	op = mem_get0(pl->op_pool);
	if (op == NULL)
		return NULL;

	INIT_XLIST_HEAD(&op->list);
	INIT_XLIST_HEAD(&op->waitq);

	op->stage = 0;
	return op;
}

/*
 * release the op
 */
void entry_proc_put(processor_t *pl, entry_proc_op_t *op)
{
	mem_put(pl->op_pool, op);
}
