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

#ifndef __MH_DATABASE_H__
#define __MH_DATABASE_H__

#include "cJSON.h"
#include "mattr.h"


/*
 * Parse configuration according database type
 */
typedef int (*database_conf_parse_t) (cJSON *db_conf, void **conf);

/*
 * Init the database
 */
typedef int (*database_init_t) (void *conf);

/*
 * connect to database
 */
typedef int (*database_connect_t) (void *db, void **hdl);

/*
 * disconnect database
 */
typedef void (*database_disconnect_t) (void *hdl);

/*
 * insert object into database
 */
typedef int (*database_insert_t) (void *hdl, char *name, mattr_t *attrs);

/*
 * update object attrs in the database
 */
typedef int (*database_update_t) (void *hdl, char *name, mattr_t *attrs);

/*
 * remove dentry records
 */
typedef int (*database_remove_dentry_t) (void *hdl, char *name, mattr_t *attr);

/*
 * remove inode associated records
 */
typedef int (*database_remove_inode_t) (void *hdl, char *name, mattr_t *attr);

struct database_ops {
	database_conf_parse_t db_conf_parse;
	database_init_t db_init;
	database_connect_t db_connect;
	database_disconnect_t db_disconnect;
	database_insert_t db_insert;
	database_update_t db_update;
	database_remove_dentry_t db_rm_dentry;
	database_remove_inode_t db_rm_inode;
};

typedef struct database_desc {
	char *name;
	void *conf;
	void *dlhandle;
	void *private;
	struct database_ops *db_ops;
} database_t;

int database_load(database_t *db);

int database_init(database_t *db);

int database_connect(database_t *db, void **hdl);

void database_disconnect(database_t *db, void *hdl);

int database_insert(database_t *db, void *hdl, char *name, mattr_t *attr);

int database_update(database_t *db, void *hdl, char *name, mattr_t *attr);

int database_remove_dentry(database_t *db, void *hdl, char *name, mattr_t *attr);

int database_remove_inode(database_t *db, void *hdl, char *name, mattr_t *attr);

#endif
