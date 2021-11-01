/*
 * Copyright (c) 2016 XTAO technology <www.xtaotech.com>
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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <pthread.h>
#include <fcntl.h>              /* for open flags */
#include <signal.h>
#include <errno.h>

#include "mem.h"
#include "defaults.h"
#include "logging.h"
#include "cfg-parser.h"
#include "database.h"
#include "filesystem.h"
#include "processor.h"
#include "thread-pool.h"

typedef struct scan_dir
{
	struct xlist_head list;
	void *dir_desc;
	char path[PATH_MAX];
	obj_id_t id;
} scan_dir_t;

static pthread_t sigwaiter;

/*
 * allocated a op and then push the op into pipeline
 */
static int queue_log_entry(metahunter_t *info, journal_entry_t *entry)
{
	entry_proc_op_t *op;
	processor_t *pl = info->processor;


	op = entry_proc_get(pl);
	if (!op) {
		xt_log("scanner", XT_LOG_ERROR, "processor failed "
		       "to allocate a new op" );
		return 1;
	}

	op->stage = 0;
	op->log_inserted = time(NULL);
	op->extra_info = entry;
	op->id = entry->attr->fid.inode;

	/*
	 * push into to the pipeline
	 */
	entry_proc_push(pl, op);

	return 0;
}

static void stat2id (obj_id_t *id, struct stat *st)
{
	id->fs_key = st->st_dev;
	id->inode = st->st_ino;
	id->validator = st->st_ctime;
}

static mattr_t * mattr_new(struct mem_pool *pool, obj_id_t *pid, struct stat *st)
{
	mattr_t *attr = NULL;

	attr = mem_get0(pool);
	memcpy(&attr->parentid, pid, sizeof(obj_id_t));
	stat2id(&attr->fid, st);
	attr->mode = st->st_mode;
	attr->nlink = st->st_nlink;
	attr->uid = st->st_uid;
	attr->gid = st->st_gid;
	attr->size = st->st_size;
	attr->blksize = st->st_blksize;
	attr->blocks = st->st_blocks;
	attr->atime = st->st_atime;
	attr->mtime = st->st_mtime;
	attr->ctime = st->st_ctime;
	return attr;
}

static scan_dir_t *xt_new_scan(filesystem_t *fs, const char *path, struct stat *st)
{
	void *dirp = NULL;
	int ret = -1;
	scan_dir_t *new = NULL;

	if (!strlen(path))
		ret = filesystem_opendir(fs, "/", &dirp);
	else
		ret = filesystem_opendir(fs, path, &dirp);
	if (ret)
		return new;

	new = XT_CALLOC(1, sizeof (scan_dir_t));
	new->dir_desc = dirp;
	strncpy(new->path, path, PATH_MAX);
	INIT_XLIST_HEAD(&new->list);
	stat2id(&new->id, st);
	return new;
}

static void xt_del_scan(filesystem_t *fs, scan_dir_t *scan)
{
	filesystem_closedir(fs, scan->dir_desc);
	xlist_del(&scan->list);
	XT_FREE(scan);	
}

static void xt_traverse_tree(metahunter_t *info)
{
	struct xlist_head scan_list;
	scan_dir_t *scan = NULL;
	scan_dir_t *new = NULL;
	struct dirent *dent = NULL;
	struct stat stbuf;
	struct mem_pool *entry_pool = info->entry_pool;
	struct mem_pool *attr_pool = info->attr_pool;
	journal_entry_t *jentry = NULL;
	filesystem_t *fs = info->fs;
	char path[PATH_MAX];
	uint64_t i = 0;

	INIT_XLIST_HEAD(&scan_list);

	filesystem_lstat(fs, "/", &stbuf);

	scan = xt_new_scan(fs, "", &stbuf);
	xlist_add(&scan->list, &scan_list);

#if 0
	/*
	 * build root jentry and push to pipeline
	 */
	jentry = mem_get0(entry_pool);

	jentry->seq = i;
	jentry->op = op_setattr;
	jentry->name = strdup("/");
	jentry->attr = mattr_new(attr_pool, &scan->id, &stbuf);

	queue_log_entry(info, jentry);
#endif
	/*
	 * deep first traverse filesystem tree
	 */
	while (scan) {
		dent = filesystem_readdir(fs, scan->dir_desc);
		if (dent == NULL) {
			xt_del_scan(fs, scan);
			goto next;
		}

		if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
			/*
			 * skip . and ..
			 */
			continue;
		}

		snprintf(path, PATH_MAX, "%s/%s", scan->path, dent->d_name);
		xt_log("scanner", XT_LOG_TRACE, "scan: %s", path);
		filesystem_lstat(fs, path, &stbuf);
		
		jentry = mem_get0(entry_pool);

		jentry->seq = i++;
		jentry->name = strdup(dent->d_name);
		jentry->attr = mattr_new(attr_pool, &scan->id, &stbuf);
		/*
		 * If directory, add the directory into scan list head
		 */
		jentry->op = op_setattr;

		if (S_ISDIR(stbuf.st_mode)) {
			/*
			 * directory
			 */
			new = xt_new_scan(fs, path, &stbuf);
			xlist_add(&new->list, &scan_list);
			/*
			 * build op_mkdir jentry
			 */

		}

		/*
		 * push to pipeline
		 */
		queue_log_entry(info, jentry);

	next:		
		if (xlist_empty(&scan_list))
			break;
		scan = xlist_entry(scan_list.next, scan_dir_t, list);
	}
}
/*
 * start filesystem scan
 */
static int xt_start_scanner(metahunter_t *info)
{
	int ret = -1;
	filesystem_t *fs = info->fs;
	database_t *db = info->db;
	processor_t *processor = info->processor;
	
	if (!fs|| !db || !processor) {
		return ret;
	}

	INIT_XLIST_HEAD(&info->op_queue);

	/*
	 * init filesystem
	 */
	ret = filesystem_mount(fs);
	if (ret) {
		ret = -1;
		xt_log("scanner", XT_LOG_ERROR, "Failed to init filesystem ...");
		return ret;
	}
	/*
	 * init database
	 */
	if (db != NULL) {
        	ret = database_init(db);
        	if (ret) {
        		ret = -1;
        		xt_log("scanner", XT_LOG_ERROR, "Failed to init "
			    "database ...");
        		goto err;
        	}
	}

	/*
	 * associate the fs and db to processor
	 */
	processor->info = info;

	/*
	 * init processor
	 */
	ret = processor_init(processor);
	if (ret) {
		ret = -1;
		xt_log("scanner", XT_LOG_ERROR, "Failed to init processor ...");
		goto err;
	}

	info->entry_pool = mem_pool_new(sizeof(journal_entry_t),
	    processor->outstanding_ops);
	info->attr_pool = mem_pool_new(sizeof(mattr_t),
	    processor->outstanding_ops);

	xt_traverse_tree(info);
	ret = 0;
err:
	processor_cleanup(processor);

	if (info->entry_pool) {
		mem_pool_destroy(info->entry_pool);
	}

	if (info->attr_pool) {
		mem_pool_destroy(info->attr_pool);
	}

	return ret;
}

static void *xt_sigwaiter (void *arg)
{
        sigset_t  set;
        int       ret = 0;
        int       sig = 0;

        sigemptyset (&set);
        sigaddset (&set, SIGTERM);  /* xt_terminate */
        sigaddset (&set, SIGINT);  /* xt_terminate */
        sigaddset (&set, SIGUSR1);  /* xt_reload */

        for (;;) {
                ret = sigwait (&set, &sig);
                if (ret)
                        continue;

                switch (sig) {
                case SIGINT:
                case SIGTERM:
			xt_log("hunter", XT_LOG_ERROR, "handle signal TERM");
                        break;
                case SIGUSR1:
			xt_log("hunter", XT_LOG_ERROR, "handle signal USR1");
                        break;
                default:

                        break;
                }
        }
	exit(1);
        return NULL;
}

static int xt_signals_setup(void)
{
        sigset_t  set;
        int       ret = 0;

        sigemptyset (&set);

        /* block these signals from non-sigwaiter threads */
        sigaddset (&set, SIGTERM);  /* xt_terminate */
        sigaddset (&set, SIGUSR1);  /* xt_reload */

        ret = pthread_sigmask (SIG_BLOCK, &set, NULL);
        if (ret) {
                xt_log ("", XT_LOG_WARNING,
                        "failed to execute pthread_signmask  %s",
                        strerror (errno));
                return ret;
        }

        ret = pthread_create (&sigwaiter, NULL, xt_sigwaiter,
                              (void *) &set);
        if (ret) {
                xt_log ("", XT_LOG_WARNING,
                        "failed to create pthread  %s for signal waiter",
                        strerror (errno));
                return ret;
        }
	return 0;
}

static struct option option_tab[] = {
    {"config-file", required_argument, NULL, 'f'},

    /* override config file options */
    {"log-file", required_argument, NULL, 'L'},
    {"log-level", required_argument, NULL, 'l'},

    /* miscellaneous options */
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
};

static void display_version(void)
{
    printf("\n");
    printf("Product:         " PACKAGE_NAME "\n");
    printf("Version:         " PACKAGE_VERSION "\n");
    printf("\n");
}

static void display_help(void)
{
	
}

#define SHORT_OPT_STRING    "c:p:L:l:vnh"

int main(int argc, char **argv)
{
	int ret = -1;
	char *log_file = MH_DEFAULT_LOG_FILE;
	char *conf_file = MH_DEFAULT_CONF_FILE;
	int option_index = 0;
	metahunter_t *info;
	xt_loglevel_t log_lvl = XT_LOG_TRACE;
	int c;

	while ((c = getopt_long(argc, argv, SHORT_OPT_STRING,
	    option_tab, &option_index)) != -1) {
		switch (c) {
		case 'c':
			conf_file = xt_strdup(optarg);
			break;
		case 'L':
			log_file = xt_strdup(optarg);
			break;
		case 'l':
			log_lvl = xt_str2loglvl(optarg);
			break;
		case 'v':
			display_version();
			exit(0);
		case 'h':
		default:
			display_help();
			exit(1);
		}
	}

	info = XT_CALLOC(1, sizeof (metahunter_t));
	if (info == NULL) {
		printf("No memory to initialize scanner!\n");
		exit(1);
	}

	ret = xt_log_init(log_file);
	if (ret) {
		printf("Logging initialization failed!\n");
		exit(1);
	}

	xt_log_set_loglevel(log_lvl);

	ret = xt_parse_config(conf_file, info);
	if (ret) {
		xt_log("scanner", XT_LOG_ERROR, "Failed to parse configuration"
		    "file!");
		exit(1);
	}

	if (info->processor) {
		if (strcmp(info->processor->name, "scanner")) {
			XT_FREE(info->processor->name);
			info->processor->name = xt_strdup("scanner");
		}
	}

	/*
	 * initialize signal handlers
	 */
	ret = xt_signals_setup();
	if (ret) {
		xt_log("hunter", XT_LOG_ERROR, "failed to setup signals.");
		exit(1);
	}
	
	/*
	 * start filesystem scan
	 */
	ret = xt_start_scanner(info);
	if (ret) {
		xt_log("hunter", XT_LOG_ERROR, "failed to start reader thread.");
		exit(1);
	}

	return 0;
}
