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
#include <libgen.h>
#include <pthread.h>
#include <semaphore.h>

#include "common.h"
#include "mem.h"
#include "logging.h"
#include "xlist.h"
#include "processor.h"
#include "hunter.h"
#include "database.h"
#include "filesystem.h"
#include "mattr.h"
#include "standard.h"

#define MH_STD "standard"

static int entry_db_apply(void *processor, struct entry_proc_op *op)
{
	processor_t *pl = (processor_t *)processor;
	journal_entry_t *entry = (journal_entry_t *)op->extra_info;
	metahunter_t *mh = pl->info;
	database_t *db = mh->db;
	worker_info_t *info = op->worker;
	void *hdl = info->priv;
	mattr_t *attr = entry->attr;
	mattr_t *pattr = entry->pattr;
	char *name = NULL;
	int ret = 0;

	if (attr == NULL) {
		xt_log(MH_STD, XT_LOG_ERROR, "attr is NULL!");
		return -1;		
	}

	switch(entry->op) {
	case op_create:
	case op_mkdir:
		name = entry->name;
		xt_log(MH_STD, XT_LOG_TRACE, "op:%d, name:%s, insert entry "
		       "%llx", entry->op, name,
		       (unsigned long long)attr->fid.inode);
		ret = database_insert(db, hdl, name, attr);
		if (ret) {
			xt_log(MH_STD, XT_LOG_ERROR, "database insert attr "
			    "failed!");
			return ret;
		}
		if (pattr == NULL) {
			xt_log(MH_STD, XT_LOG_ERROR, "parent attr is NULL!");
			return -1;
		}
		ret = database_update(db, hdl, NULL, pattr);
		if (ret) {
			xt_log(MH_STD, XT_LOG_ERROR, "database update parent "
			    "attr failed when create / mkdir!");
			return ret;
		}

		break;
	case op_unlink:
		name = entry->name;

		if (attr->nlink == 0) {
			ret = database_remove_inode(db, hdl, name, attr);
			xt_log(MH_STD, XT_LOG_TRACE, "op:%d, name:%s, remove "
			       "inode %llx", entry->op, name,
			       (unsigned long long)attr->fid.inode);
		} else {
			ret = database_remove_dentry(db, hdl, name, attr);
			xt_log(MH_STD, XT_LOG_TRACE, "op:%d, name:%s, remove "
			       "entry %llx", entry->op, name,
			       (unsigned long long)attr->fid.inode);
		}
		if (pattr == NULL) {
			xt_log(MH_STD, XT_LOG_ERROR, "parent attr is NULL!");
			return -1;
		}
		ret = database_update(db, hdl, NULL, pattr);
		if (ret) {
			xt_log(MH_STD, XT_LOG_ERROR, "database update parent "
			    "attr failed when unlink!");
			return ret;
		}

		break;

	case op_rmdir:
		name = entry->name;
		ret = database_remove_inode(db, hdl, name, attr);
		if (pattr == NULL) {
			xt_log(MH_STD, XT_LOG_ERROR, "parent attr is NULL!");
			return -1;
		}
		xt_log(MH_STD, XT_LOG_TRACE, "op:%d, name:%s, rmdir %llx",
		entry->op, name, (unsigned long long)attr->fid.inode);

		ret = database_update(db, hdl, NULL, pattr);
		if (ret) {
			xt_log(MH_STD, XT_LOG_ERROR, "database update parent "
			    "attr failed when rmdir!");
			return ret;
		}

		break;
	default:
		name = entry->name;
		xt_log(MH_STD, XT_LOG_TRACE, "op:%d, name:%s ino:%llx",
		       entry->op, name, (unsigned long long)attr->fid.inode);

		ret = database_update(db, hdl, name, attr);
		break;
	}

	return ret;
}

static int entry_reclaim_log(void *processor, struct entry_proc_op *op)
{
	int ret = -1;
	processor_t *pl = (processor_t *)processor;
	metahunter_t *mh = pl->info;
	filesystem_t *fs = mh->fs;
	journal_entry_t *entry = (journal_entry_t *)op->extra_info;

	if (op->no_release)
		return 0;

	filesystem_release_jentry(fs, entry);
	return ret;
}

/*
 * pipeline stages definition
 */
pipeline_stage_desc_t stages[] = {
	{STAGE_DB_APPLY, "STAGE_DB_APPLY", entry_valid_op,
	 entry_db_apply, entry_post_op, /* apply db */
	 0},
	{STAGE_RECLAIM_LOG, "STAGE_RECLAIM_LOG", NULL,
	 entry_reclaim_log, entry_stage_cmplt, /* reclaim log */
	 0},
};

pipeline_stage_desc_t *get_stages(int *stagecnt)
{
	*stagecnt = sizeof(stages) / sizeof(pipeline_stage_desc_t);
	return stages;
}

int init(void *processor)
{
	return 0;
}

void fini(void *processor)
{
}

int conf_parse (cJSON *seg, void **config)
{
	/*
	 * allocate standard configure structure
	 */

	/*
	 * Parse json configuration options
	 */
	return 0;
}

int worker_init(worker_info_t *info)
{
	processor_t *pl = info->pl;
	metahunter_t *mh = pl->info;
	int rc = 0;

	if (mh->db)
		rc = database_connect(mh->db, &info->priv);
	return rc;
}

void worker_fini(worker_info_t *info)
{
	processor_t *pl = info->pl;
	metahunter_t *mh = pl->info;

	if (mh->db)
		database_disconnect(mh->db, info->priv);
}
