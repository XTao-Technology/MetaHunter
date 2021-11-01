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

#include <string.h>
#include <features.h>
#include <utime.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdbool.h>


#include "cJSON.h"
#include "mem.h"
#include "logging.h"
#include "filesystem.h"
#include "ceph-api.h"
#include "mh-ceph.h"

#define MH_CEPH "MH_CEPH"

/*
 * cephfs configuration:
 *
 * "FileSystem" {
 *      "name": "ceph",
 *	"cluster": "xtao",
 *	"mds": "xt1",
 *	"filesystem": "cephfs"
 * }
 */
static int ceph_conf_parse (cJSON *seg, void **config)
{
	cJSON *c = NULL;
	ceph_config_t *conf = NULL;
	int ret = -1;

	xt_log(MH_CEPH, XT_LOG_TRACE, "config parse enter");

	conf = XT_CALLOC(1, sizeof (ceph_config_t));
	if (!conf) {
		xt_log(MH_CEPH, XT_LOG_ERROR, "config allocation failed");
		return -1;
	}

	*config = conf;

	for (c = seg->child; c; c = c->next) {
		if (!strcmp(c->string, "cluster")) {
			if (c->type != cJSON_String) {
				xt_log(MH_CEPH, XT_LOG_ERROR, "config cluster "
				    "type invalid");
				goto err;
			}

			if (!c->valuestring) {
				xt_log(MH_CEPH, XT_LOG_ERROR, "config cluster "
				    "value is NULL.");
				goto err;
			}

			conf->cluster = xt_strdup(c->valuestring);
		} else if (!strcmp(c->string, "mds")) {
			if (c->type != cJSON_String) {
				xt_log(MH_CEPH, XT_LOG_ERROR, "config mds "
				    "type invalid.");
				goto err;
			}

			if (!c->valuestring) {
				xt_log(MH_CEPH, XT_LOG_ERROR, "config mds "
				    "value is NULL.");
				goto err;		
			}

			conf->mds = xt_strdup(c->valuestring);
		} else if (!strcmp(c->string, "filesystem")) {
			if (c->type != cJSON_String) {
				xt_log(MH_CEPH, XT_LOG_ERROR, "config fs "
				       "type invalid.");
				goto err;		
			}

			if (!c->valuestring) {
				xt_log(MH_CEPH, XT_LOG_ERROR, "config fs "
				       "value invalid.");
				goto err;		
			}

			conf->filesystem = xt_strdup(c->valuestring);
		} else {
			/*
			 * Invalid options
			 */
			xt_log(MH_CEPH, XT_LOG_DEBUG, "config skip invalid "
			    "key");
			continue;
		}
	}
	ret = 0;
	xt_log(MH_CEPH, XT_LOG_TRACE, "config parse cluster:%s, mds:%s, fs:%s",
	    conf->cluster, conf->mds, conf->filesystem);
	return ret;
err:
	XT_FREE(conf);
	return ret;
}

static int ceph_fs_init(void *conf, void **hdl, mattr_t *root)
{
	ceph_config_t *ceph_conf = conf;
	char *cluster = ceph_conf->cluster;
	char *mds = ceph_conf->mds;
	char *fs = ceph_conf->filesystem;
	int ret = 0;

	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph init enter");
	ret = ceph_connect_mds_journal(cluster, mds, fs, hdl, root);
	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph init exit");
	return ret;
}

static int ceph_fs_fini(void *hdl)
{
	int ret = -1;

	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph fini enter");
	ret = ceph_terminate_journal_read(hdl);
	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph fini enter");
	return ret;
}

static int ceph_hold_jentry(void *hdl, journal_entry_t **entry)
{
	int ret = -1;

	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph hold jentry enter");
	ret = ceph_hold_journal_entry(hdl, entry);
	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph hold jentry exit");
	return ret;
}

static int ceph_release_jentry(void *hdl, journal_entry_t *entry)
{
	int ret = -1;

	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph hold jentry enter");
	ret = ceph_release_journal_entry(hdl, entry);
	xt_log(MH_CEPH, XT_LOG_TRACE, "ceph hold jentry exit");

	return ret;
}

static int ceph_fs_mount(void *conf, void **mount)
{
        int ret = 0;
        char conf_path[256] = "";
	ceph_config_t *ceph_conf = conf;
	char *cluster = ceph_conf->cluster;

	struct ceph_mount_info *cmount = NULL;

        snprintf(conf_path, sizeof(conf_path), "/etc/ceph/%s.conf",
                 (cluster != NULL) ? cluster : "ceph");

        ret = ceph_create(&cmount, NULL);
	if (ret) {
                return ret;
        }

        ret = ceph_conf_read_file(cmount, conf_path);
        if (ret) {
                return ret;
        }

        ret = ceph_mount(cmount, NULL);
        if (ret) {
                return ret;
        }

	*mount = cmount;

	return 0;
}

static int ceph_fs_lstat(void *mount, const char *path, struct stat *stbuf)
{
	struct ceph_mount_info *cmount = mount;
	return ceph_lstat(cmount, path, stbuf);
}

static int ceph_fs_opendir(void *mount, const char *path, void **dirpp)
{
	struct ceph_mount_info *cmount = mount;
	struct ceph_dir_result *cdir = NULL;
	int ret = ceph_opendir(cmount, path, &cdir);
	if (!ret)
		*dirpp = cdir;
	return ret;
}

static int ceph_fs_closedir(void *mount, void *dirp)
{
	struct ceph_dir_result *cdir = dirp;
	struct ceph_mount_info *cmount = mount;
	return ceph_closedir(cmount, cdir);
}

static struct dirent *ceph_fs_readdir(void *mount, void *dirp)
{
	struct ceph_dir_result *cdir = dirp;
	struct ceph_mount_info *cmount = mount;
	return ceph_readdir(cmount, cdir);
}

static int ceph_fs_readdir_r(void *mount, void *dirp, struct dirent *result)
{
	struct ceph_dir_result *cdir = dirp;
	struct ceph_mount_info *cmount = mount;
	return ceph_readdir_r(cmount, cdir, result);
}

struct filesystem_ops fs_ops = {
	ceph_conf_parse,
	ceph_fs_init,
	ceph_fs_fini,
	ceph_hold_jentry,
	ceph_release_jentry,
	ceph_fs_mount,
	ceph_fs_lstat,
	ceph_fs_opendir,
	ceph_fs_closedir,
	ceph_fs_readdir,
	ceph_fs_readdir_r,
};
