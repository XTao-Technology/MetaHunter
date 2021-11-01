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
#ifndef __MH_FILESYSTEM_H__
#define __MH_FILESYSTEM_H__

#include <unistd.h>
#include <sys/types.h>
#include "cJSON.h"
#include "mattr.h"

typedef enum {
	op_create = 0,
	op_unlink,
	op_rename,
	op_mkdir,
	op_rmdir,
	op_setattr,
	op_init
} op_type_t;

typedef struct journal_entry {
	uint64_t seq;
	uint32_t len;
	op_type_t op;
	char *name;
	char *name2; /* reserved for rename */
	mattr_t *attr;
	mattr_t *pattr;
	mattr_t *pattr2; /* reserved for rename */
	void *xattr; /* nv list */
} journal_entry_t;

/*
 * Parse configuration according file system type
 */
typedef int (*filesystem_conf_parse_t) (cJSON *seg, void **conf);

/*
 * Filesystem journal init
 */
typedef int (*filesystem_init_t) (void *conf, void **hdl,
    mattr_t *root);

/*
 * Filesystem journal fini
 */
typedef int (*filesystem_fini_t) (void *hdl);

/*
 * Read journal entry
 */
typedef int (*filesystem_hold_journal_entry_t) (void *hdl,
    journal_entry_t **entry);

/*
 * Release journal entry
 */
typedef int (*filesystem_release_journal_entry_t) (void *hdl,
    journal_entry_t *entry);

typedef int (*filesystem_mount_t) (void *conf, void **mount);

typedef int (*filesystem_lstat_t) (void *mount, const char *path, struct stat *stbuf);

typedef int (*filesystem_opendir_t) (void *mount, const char *path, void **dirpp);

typedef int (*filesystem_closedir_t) (void *mount, void *dirp);

typedef struct dirent* (*filesystem_readdir_t) (void *mount, void *dirp);

typedef int (*filesystem_readdir_r_t) (void *mount, void *dirp, struct dirent* ent);

struct filesystem_ops {
	filesystem_conf_parse_t fs_conf_parse;
	filesystem_init_t fs_init;
	filesystem_fini_t fs_fini;
	filesystem_hold_journal_entry_t fs_hold_jentry;
	filesystem_release_journal_entry_t fs_release_jentry;	
	filesystem_mount_t fs_mount;
	filesystem_lstat_t fs_lstat;
	filesystem_opendir_t fs_opendir;
	filesystem_closedir_t fs_closedir;
	filesystem_readdir_t fs_readdir;
	filesystem_readdir_r_t fs_readdir_r;
};

typedef struct filesystem_desc {
	char *name;
	void *conf;
	void *dlhandle;
	void *private;
	mattr_t root;
	struct filesystem_ops *fs_ops;
} filesystem_t;

int filesystem_load(filesystem_t *fs);

int filesystem_init(filesystem_t *fs);

void filesystem_fini(filesystem_t *fs);

int filesystem_hold_jentry(filesystem_t *fs, journal_entry_t **entry);

void filesystem_release_jentry(filesystem_t *fs, journal_entry_t *entry);

int filesystem_mount(filesystem_t *fs);

int filesystem_lstat(filesystem_t *fs, const char *path, struct stat *stbuf);

int filesystem_opendir(filesystem_t *fs, const char *path, void **dirpp);

int filesystem_closedir(filesystem_t *fs, void *dirp);

struct dirent *filesystem_readdir(filesystem_t *fs, void *dirp);

int filesystem_readdir_r(filesystem_t *fs, void *dirp, struct dirent* ent);

#endif
