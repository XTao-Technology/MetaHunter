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
#include "cJSON.h"
#include "mem.h"
#include "logging.h"
#include "database.h"
#include "rbh-db.h"
#include "global_config.h"
#include "RobinhoodConfig.h"
#include "RobinhoodLogs.h"
#include "list_mgr.h"
#include "tmp_fs_mgr_types.h"

#define MH_RBH_DB "MH_RBH_DB"

/*
 * "DataBase": {
 *	"Robinhood": {
 *		"config": "/etc/robinhood/xtaofs/xtao.conf",
 *      }
 * }
 */

robinhood_config_t rbh_config;

static int rbh_conf_parse (cJSON *seg, void **conf)
{
	cJSON *c = seg->child;
	char config_file[1024] = "";
	char err_msg[4096];

	*conf = &rbh_config;

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_conf_parse");

	for (; c; c = c->next) {
		if (!strcmp(c->string, "config")) {
			if (c->type != cJSON_String) {
				xt_log(MH_RBH_DB, XT_LOG_ERROR, "config file "
				    "type invalid");
				return -1;
			}

			if (!c->valuestring) {
				xt_log(MH_RBH_DB, XT_LOG_ERROR, "config server "
				    "is NULL");

				return -1;
			}
				
			strcpy(config_file, c->valuestring);
			xt_log(MH_RBH_DB, XT_LOG_INFO, "Configure file:%s",
			    config_file);

		} else {
			/*
			 * Invalid options
			 */
			xt_log(MH_RBH_DB, XT_LOG_TRACE, "skip invalid key.");
			continue;
		}
	}

	if (ReadRobinhoodConfig(0, config_file, err_msg, &rbh_config, FALSE)) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "error reading config :%s",
		    err_msg);
		return -1;
	}

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_conf_parse");

	return 0;
}

static int rbh_init(void *config)
{
	int ret = -1;
	robinhood_config_t *conf = config;

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_init");

	ret = InitializeLogs(MH_RBH_DB, &conf->log_config);
	if (ret) {
		goto out;
	}

	ret = ListMgr_Init(&conf->lmgr_config, FALSE);
	if (ret) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "rbh_init ListMgr failed");
		goto out;
	}
	ret = 0;
out:
	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_init");
	return ret;
}

/*
 * current thread connect to database
 */
static int rbh_connect(void *db, void **hdl)
{
	int ret = 0;

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_connect");

	*hdl = XT_CALLOC(1, sizeof (lmgr_t));
	ret = ListMgr_InitAccess(*hdl);
	if (ret) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "rbh_connect to db failed");
	}
	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_connect");

	return ret;
}

static void rbh_disconnect(void *hdl)
{
	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_disconnect");
	ListMgr_CloseAccess(hdl);
	XT_FREE(hdl);
	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_disconnect");
}

void mattr_to_rbattr(mattr_t *attr, char *name, attr_set_t *as)
{
	ATTR_MASK_INIT(as);

	ATTR_MASK_SET(as, owner);
	uid2str(attr->uid, ATTR(as, owner));
	ATTR_MASK_SET(as, gr_name);
	gid2str(attr->gid, ATTR(as, gr_name));
	ATTR_MASK_SET(as, size);
	ATTR(as, size) = attr->size;
	ATTR_MASK_SET(as, blocks);
	ATTR(as, blocks) = attr->blocks;
	ATTR_MASK_SET(as, last_access);
	ATTR(as, last_access) = MAX3(attr->ctime, attr->atime, attr->mtime);
	ATTR_MASK_SET(as, last_mod);
	ATTR(as, last_mod) = attr->mtime;
	ATTR_MASK_SET(as, creation_time);
	ATTR(as, creation_time) = attr->ctime;
	ATTR_MASK_SET(as, type);
	strcpy(ATTR(as, type), mode2type(attr->mode));
	ATTR_MASK_SET(as, nlink);
	ATTR(as, nlink) = attr->nlink;
	ATTR_MASK_SET(as, mode);
	ATTR(as, mode) = attr->mode & 07777;
	ATTR_MASK_SET(as, parent_id);
	memcpy(&ATTR(as, parent_id), &attr->parentid, sizeof (entry_id_t));

	if (name) {
		ATTR_MASK_SET(as, name);
		strcpy(ATTR(as, name), name);
	}
}

static int rbh_insert(void *hdl, char *name, mattr_t *attrs)
{
	int ret = -1;
	attr_set_t as;
	void *id = NULL;

	if (!attrs) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "attrs is NULL!");
		return ret;
	}
		
	id = &attrs->fid;

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_insert");

	mattr_to_rbattr(attrs, name, &as);
        ret = ListMgr_Insert(hdl, id, &as, FALSE);
	if (ret) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "rbh_insert failed");
	}
	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_insert");
	return ret;

}

static int rbh_update(void *hdl, char *name, mattr_t *attrs)
{
	int ret = -1;
	attr_set_t as;
	void *id = NULL;

	if (!attrs) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "attrs is NULL!");
		return ret;
	}
		
	id = &attrs->fid;

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_update");

	mattr_to_rbattr(attrs, name, &as);

        ret = ListMgr_Update(hdl, id, &as);
	if (ret) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "rbh_update failed");
	}

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_update");
	return ret;
}

static int rbh_rm_dentry(void *hdl, char *name, mattr_t *attrs)
{
	int ret = -1;
	attr_set_t as;
	void *id = NULL;

	if (!attrs) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "attrs is NULL!");
		return ret;
	}
		
	id = &attrs->fid;

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_rm_dentry");

	mattr_to_rbattr(attrs, name, &as);
        ret = ListMgr_Remove(hdl, id, &as, FALSE);
	if (ret) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "rbh_rm_dentry failed");
	}

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_rm_dentry");
	return ret;

}

static int rbh_rm_inode(void *hdl, char *name, mattr_t *attrs)
{
	int ret = -1;
	attr_set_t as;
	void *id = NULL;

	if (!attrs) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "attrs is NULL!");
		return ret;
	}
		
	id = &attrs->fid;

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "enter rbh_rm_inode");

	mattr_to_rbattr(attrs, name, &as);
        ret = ListMgr_Remove(hdl, id, &as, TRUE);
	if (ret) {
		xt_log(MH_RBH_DB, XT_LOG_ERROR, "rbh_rm_inode failed");
	}

	xt_log(MH_RBH_DB, XT_LOG_TRACE, "exit rbh_rm_inode");
	return ret;

}

struct database_ops db_ops = {
	.db_conf_parse = rbh_conf_parse,
	.db_init = rbh_init,
	.db_connect = rbh_connect,
	.db_disconnect = rbh_disconnect,
	.db_insert = rbh_insert,
	.db_update = rbh_update,
	.db_rm_dentry = rbh_rm_dentry,
	.db_rm_inode = rbh_rm_inode,
};
