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


#include <dlfcn.h>
#include <netdb.h>
#include <fnmatch.h>
#include <stdlib.h>
#include "logging.h"
#include "mem.h"
#include "database.h"

int database_load(database_t *db)
{
	char *name = NULL;
	int ret = -1;

	xt_log("database", XT_LOG_TRACE, "database loading..");

	if (!db)
		return ret;

	ret = xt_asprintf(&name, "%s/%s.so", MHDBDIR, db->name);
	if (ret == -1) {
		xt_log("database", XT_LOG_ERROR, "failed to compose %s "
		    "lib name", db->name);
		goto out;
	}
		
	db->dlhandle = dlopen (name, RTLD_NOW| RTLD_GLOBAL);
	if (!db->dlhandle) {
		xt_log("database", XT_LOG_ERROR, "%s", dlerror());
		ret = -1;
		goto out;
	}

	db->db_ops = dlsym(db->dlhandle, "db_ops");
	if (!db->db_ops) {
		xt_log("database", XT_LOG_ERROR, "%s", dlerror());
		ret = -1;
		goto out;
	}

	xt_log("database", XT_LOG_TRACE, "database load successfully");
	ret = 0;
out:
	if (ret) {
		dlclose(db->dlhandle);
	}
	XT_FREE(name);
	return ret;
}

int database_init(database_t *db)
{
	if (!db)
		return -1;

	return db->db_ops->db_init(db->conf);
}

int database_connect(database_t *db, void **hdl)
{
	if (!db)
		return -1;

	return db->db_ops->db_connect(db, hdl);
}

void database_disconnect(database_t *db, void *hdl)
{
	if (!hdl || !db)
		return;

	return db->db_ops->db_disconnect(hdl);
}

int database_insert(database_t *db, void *hdl, char *name, mattr_t *attr)
{
	if (!db)
		return -1;

	return db->db_ops->db_insert(hdl, name, attr);
}

int database_update(database_t *db, void *hdl, char *name, mattr_t *attr)
{
	if (!db)
		return -1;
	return db->db_ops->db_update(hdl, name, attr);
}

int database_remove_dentry(database_t *db, void *hdl, char *name, mattr_t *attr)
{
	if (!db)
		return -1;
	return db->db_ops->db_rm_dentry(hdl, name, attr) ;
}

int database_remove_inode(database_t *db, void *hdl, char *name, mattr_t *attr)
{
	if (!db)
		return -1;
	return db->db_ops->db_rm_inode(hdl, name, attr);
}
