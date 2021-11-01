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
#include "filesystem.h"

int filesystem_load(filesystem_t *fs)
{
	char *name = NULL;
	int ret = -1;

	xt_log("filesystem", XT_LOG_TRACE, "filesystem loading..");

	ret = xt_asprintf(&name, "%s/%s.so", MHFSDIR, fs->name);
	if (ret == -1) {
		xt_log("filesystem", XT_LOG_ERROR, "failed to compose %s "
		    "lib name", fs->name);
		goto out;
	}

	fs->dlhandle = dlopen (name, RTLD_NOW| RTLD_GLOBAL);
	if (!fs->dlhandle) {
		xt_log("filesystem", XT_LOG_ERROR, "failed to load %s, %s", name, dlerror());
		ret = -1;
		goto out;
	}

	fs->fs_ops = dlsym(fs->dlhandle, "fs_ops");
	if (!fs->fs_ops) {
		xt_log("filesystem", XT_LOG_ERROR, "failed to load fs_ops");
		ret = -1;
		goto out;
	}
	ret = 0;

	xt_log("filesystem", XT_LOG_TRACE, "filesystem load successfully.");
out:
	if (ret) {
		if (fs->dlhandle)
			dlclose(fs->dlhandle);
	}
	XT_FREE(name);
	return ret;
}

int filesystem_init(filesystem_t *fs)
{
	return fs->fs_ops->fs_init(fs->conf, &fs->private, &fs->root);
}

void filesystem_fini(filesystem_t *fs)
{
	if (!fs->private)
		return;

	fs->fs_ops->fs_fini(fs->private);
}

int filesystem_hold_jentry(filesystem_t *fs, journal_entry_t **entry)
{
	return fs->fs_ops->fs_hold_jentry(fs->private, entry);
}

void filesystem_release_jentry(filesystem_t *fs, journal_entry_t *entry)
{
	fs->fs_ops->fs_release_jentry(fs->private, entry);
}

int filesystem_mount(filesystem_t *fs)
{
	return fs->fs_ops->fs_mount(fs->conf, &fs->private);
}

int filesystem_lstat(filesystem_t *fs, const char *path, struct stat *stbuf)
{
	return fs->fs_ops->fs_lstat(fs->private, path, stbuf);
}

int filesystem_opendir(filesystem_t *fs, const char *path, void **dirpp)
{
	return fs->fs_ops->fs_opendir(fs->private, path, dirpp);
}

int filesystem_closedir(filesystem_t *fs, void *dirp)
{
	return fs->fs_ops->fs_closedir(fs->private, dirp);
}

struct dirent *filesystem_readdir(filesystem_t *fs, void *dirp)
{
	return fs->fs_ops->fs_readdir(fs->private, dirp);
}

int filesystem_readdir_r(filesystem_t *fs, void *dirp, struct dirent *ent)
{
	return fs->fs_ops->fs_readdir_r(fs->private, dirp, ent);
}
