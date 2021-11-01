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
#include <sys/stat.h>
#include <sys/types.h>
#include "common.h"
#include "mem.h"
#include "logging.h"
#include "xlist.h"
#include "processor.h"
#include "hunter.h"
#include "database.h"
#include "filesystem.h"
#include "mattr.h"
#include "mh_irods.h"

#include "rods.h"
#include "rodsErrorTable.h"
#include "rodsType.h"
#include "rodsClient.h"
#include "miscUtil.h"
#include "rodsPath.h"
#include "rcConnect.h"
#include "dataObjCreate.h"
#include "dataObjUnlink.h"
#include "collCreate.h"
#include "rmColl.h"
#include "modDataObjMeta.h"

#define MH_IRODS "irods"

static int entry_db_apply(void *processor, struct entry_proc_op *op)
{
	journal_entry_t *entry = (journal_entry_t *)op->extra_info;
	worker_info_t *info = op->worker;
	mattr_t *attr = entry->attr;
	mh_irods_priv_t *priv = info->priv;
	rcComm_t *conn = NULL;
	rodsEnv *env = priv->env;
	char *name = NULL;
	int ret = 0;
	int irods_err = 0;
	int unix_err = 0;
	int backoff = 0;

	dataObjInp_t dataObjInp;
	collInp_t collInp;
	modDataObjMeta_t modDataObjMetaInp;
	unregDataObj_t unregDataObjInp;
	keyValPair_t regParam;
	dataObjInfo_t dataObjInfo;
	char dataMode[SHORT_STR_LEN];
	char tmpStr[MAX_NAME_LEN];

	if (attr == NULL) {
		xt_log(MH_IRODS, XT_LOG_ERROR, "attr is NULL!");
		return -1;
	}

retry:
	conn = priv->conn;
	switch(entry->op) {
	case op_create:
		bzero(&dataObjInp, sizeof (dataObjInp_t));
		dataObjInp.parentId = attr->parentid.inode;
		dataObjInp.dataId = attr->fid.inode;
		rstrcpy(dataObjInp.objPath, entry->name, MAX_NAME_LEN);
		addKeyVal(&dataObjInp.condInput, NO_CREATE_FLAG_KW, "");
		addKeyVal(&dataObjInp.condInput, DATA_TYPE_KW, "generic");
		dataObjInp.createMode = attr->mode;
		dataObjInp.openFlags = O_RDWR;
		dataObjInp.dataSize = -1;

		xt_log(MH_IRODS, XT_LOG_TRACE, "create, name:%s parent:%lld",
		    entry->name, (unsigned long long)attr->parentid.inode);

		ret = rcDataObjCreate(conn, &dataObjInp);

		if (ret < 0) {
			xt_log(MH_IRODS, XT_LOG_ERROR, "create, name:%s "
			    "parent:%llx, rc:%d", entry->name,
			    (unsigned long long)attr->parentid.inode, ret);
		}

		clearKeyVal(&dataObjInp.condInput);

		break;
	case op_mkdir:
		name = entry->name;
		bzero(&collInp, sizeof (collInp_t));
		collInp.parentId = attr->parentid.inode;
		collInp.collId = attr->fid.inode;
		rstrcpy(collInp.collName, name, MAX_NAME_LEN);

		xt_log(MH_IRODS, XT_LOG_TRACE, "mkdir, name:%s parent:%lld",
		    name, (unsigned long long)attr->parentid.inode);

		ret = rcCollCreate(conn, &collInp);
		if (ret < 0) {
			xt_log(MH_IRODS, XT_LOG_ERROR, "rmdir, name:%s "
			    "parent:%llx, rc:%d", name,
			    (unsigned long long)attr->parentid.inode, ret);
		}

		break;
	case op_unlink:
		name = entry->name;

		bzero(&dataObjInfo, sizeof (dataObjInfo_t));
		dataObjInfo.dataId = attr->fid.inode;
		dataObjInfo.collId = attr->parentid.inode;
		rstrcpy(dataObjInfo.objPath, name, MAX_NAME_LEN);
		bzero(&unregDataObjInp, sizeof (unregDataObj_t));
		addKeyVal(&regParam, FORCE_FLAG_KW, "");;
		addKeyVal(&regParam, UNREG_KW, "");;
		unregDataObjInp.dataObjInfo = &dataObjInfo;
		unregDataObjInp.condInput = &regParam;

		xt_log(MH_IRODS, XT_LOG_TRACE, "unlink, name:%s parent:%llx",
		    name, (unsigned long long)attr->parentid.inode);

		ret = rcUnregDataObj(conn, &unregDataObjInp);
		if (ret < 0) {
			xt_log(MH_IRODS, XT_LOG_ERROR, "unlink, name:%s "
			    "parent:%llx, rc:%d", name,
			    (unsigned long long)attr->parentid.inode, ret);
		}
		xt_log(MH_IRODS, XT_LOG_TRACE, "unlink, name:%s ret:%d",
		    name, ret);

		clearKeyVal(&regParam);
		break;

	case op_rmdir:
		name = entry->name;

		bzero(&collInp, sizeof (collInp_t));
		collInp.parentId = attr->parentid.inode;
		collInp.collId = attr->fid.inode;
		addKeyVal(&collInp.condInput, UNREG_COLL_KW, "");
		addKeyVal(&collInp.condInput, FORCE_FLAG_KW, "");

		ret = rcRmColl(conn, &collInp, 0);

		xt_log(MH_IRODS, XT_LOG_TRACE, "rmdir, name:%s parent:%llx",
		    name, (unsigned long long)attr->parentid.inode);

		if (ret < 0) {
			xt_log(MH_IRODS, XT_LOG_ERROR, "rmdir, name:%s "
			    "parent:%llx, rc:%d", name,
			    (unsigned long long)attr->parentid.inode, ret);
		}
		clearKeyVal(&collInp.condInput);
		break;
	case op_setattr:
		name = entry->name;
		if (S_ISDIR(attr->mode))
			break;
		bzero(&regParam, sizeof (keyValPair_t));
		snprintf(dataMode, SHORT_STR_LEN, "%d", (int) (attr->mode));
		addKeyVal(&regParam, DATA_MODE_KW, dataMode);
		bzero(tmpStr, sizeof(tmpStr));
		snprintf(tmpStr, MAX_NAME_LEN, "%lld", (long long)attr->size);
		addKeyVal(&regParam, DATA_SIZE_KW, tmpStr);
		bzero(tmpStr, sizeof(tmpStr));
		snprintf(tmpStr, MAX_NAME_LEN, "%d", (int) (attr->ctime));
		addKeyVal(&regParam, DATA_CREATE_KW, tmpStr);
		bzero(tmpStr, sizeof(tmpStr));
		snprintf(tmpStr, MAX_NAME_LEN, "%d", (int) (attr->mtime));
		addKeyVal(&regParam, DATA_MODIFY_KW, tmpStr);
		bzero(tmpStr, sizeof(tmpStr));
		snprintf(tmpStr, MAX_NAME_LEN, "%d", (int) (attr->atime));
		addKeyVal(&regParam, DATA_ACCESS_KW, tmpStr);

		bzero(&dataObjInfo, sizeof (dataObjInfo_t));
		dataObjInfo.dataId = attr->fid.inode;
		dataObjInfo.collId = attr->parentid.inode;

		bzero(&modDataObjMetaInp, sizeof (modDataObjMeta_t));
		modDataObjMetaInp.regParam = &regParam;
		modDataObjMetaInp.dataObjInfo = &dataObjInfo;

		xt_log(MH_IRODS, XT_LOG_TRACE, "setattr, name:%s ino:%lld",
		       name, (unsigned long long)attr->fid.inode);

		ret = rcModDataObjMeta(conn, &modDataObjMetaInp);

		if (ret < 0) {
			xt_log(MH_IRODS, XT_LOG_ERROR, "setattr, fid:%llx "
			    "parent:%llx, rc:%d",
			    (unsigned long long)attr->fid.inode,
			    (unsigned long long)attr->parentid.inode, ret);
		}
		clearKeyVal(&regParam);
		break;
	op_init:
		/*
		 * do nothing so far
		 */
		break;
	default:
		name = entry->name;
		xt_log(MH_IRODS, XT_LOG_ERROR, "invalid op:%d, name:%s ",
		       entry->op, name);

		break;
	}

	if (ret < 0) {
		irods_err = getIrodsErrno(ret);
		unix_err = getErrno(ret);

		if ((unix_err == EPIPE) || (unix_err == EIO)) {
			while (1) {
				ret = rcReconnect(&priv->conn, env->rodsHost, env, 1);
				if ((irods_err != USER_SOCK_CONNECT_ERR) &&
				    (irods_err != USER_SOCK_OPEN_ERR)) {
					xt_log(MH_IRODS, XT_LOG_TRACE, "reconnect "
					       "return code:%d.", ret);

					break;					
				} else {
					xt_log(MH_IRODS, XT_LOG_TRACE, "sleep "
					    "%d sec for reconnect...", backoff);
					rodsSleep(backoff, 0);
					if (backoff < 10) {
						backoff += 2;
					}
				}
			}
			
			if (ret == 0) {
				xt_log(MH_IRODS, XT_LOG_TRACE, "reconnect "
				       "to iRodsAgent successfully.");
				goto retry;				
			}
		} else {
			switch (entry->op) {
			case op_unlink:
				if (irods_err == CAT_UNKNOWN_FILE) {
					ret = 0;
				}
				break;
			case op_rmdir:
				if (irods_err == CAT_UNKNOWN_COLLECTION) {
					ret = 0;
				}
				break;
			case op_create:
				if (irods_err == CAT_NAME_EXISTS_AS_DATAOBJ) {
					ret = 0;
				}

				if (irods_err ==
				    CATALOG_ALREADY_HAS_ITEM_BY_THAT_NAME) {
					ret = 0;
				}

				break;
			case op_mkdir:
				if (irods_err == CAT_NAME_EXISTS_AS_COLLECTION) {
					ret = 0;
				}

				if (irods_err ==
				    CATALOG_ALREADY_HAS_ITEM_BY_THAT_NAME) {
					ret = 0;
				}
				break;
			default:
				break;
			}
			if (ret < 0)
				goto retry;
		}
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
	xt_log(MH_IRODS, XT_LOG_TRACE, "release entry %llx",
	    (unsigned long long)entry->seq);
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
	int status;
	processor_t *pl = (processor_t *)processor;
	rodsEnv *env = NULL;

	env = XT_CALLOC(1, sizeof (rodsEnv));
	if (env == NULL) {
		xt_log(MH_IRODS, XT_LOG_ERROR, "alloc env failed!");
		return -1;
	}

	status = getRodsEnv(env);
	if (status != 0) {
		XT_FREE(env);
		xt_log(MH_IRODS, XT_LOG_ERROR, "getRodsEnv failed!");
		return -1;
	}
	pl->private = env;

	return 0;
}

void fini(void *processor)
{
	processor_t *pl = (processor_t *)processor;
	rodsEnv *env = pl->private;
	XT_FREE(env);
	pl->private = NULL;
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
	processor_t *pl = (processor_t *)(info->pl);
	rodsEnv *env = pl->private;
	mh_irods_priv_t *priv = NULL;
	int status;
	rcComm_t *conn;
	rErrMsg_t errMsg;

	conn = rcConnect(env->rodsHost, env->rodsPort, env->rodsUserName,
	    env->rodsZone, 1, &errMsg);
	if (conn == NULL) {
		xt_log(MH_IRODS, XT_LOG_ERROR, "Failed to connect to rods "
		    "server!");
		return -1;
	}

	priv = XT_CALLOC(1, sizeof (mh_irods_priv_t));
	if (priv == NULL) {
		xt_log(MH_IRODS, XT_LOG_ERROR, "Failed to alloc irods priv!");
		rcDisconnect(conn);
		return -1;
	}
	priv->conn = conn;
	priv->env = env;
	info->priv = priv;

	status = clientLogin(conn, 0, 0);
	if (status != 0) {
		xt_log(MH_IRODS, XT_LOG_ERROR, "Failed to login the rods!");
		rcDisconnect(conn);
		return -1;
	}

	return 0;
}

void worker_fini(worker_info_t *info)
{
	mh_irods_priv_t *priv = info->priv;
	rcComm_t *conn = priv->conn;
	rcDisconnect(conn);
	XT_FREE(priv);
}
