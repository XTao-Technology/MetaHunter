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
#include "hunter.h"
#include "database.h"
#include "filesystem.h"
#include "processor.h"

static pthread_t sigwaiter;

static metahunter_t reader_info;

/*
 * allocated a op and then push the op into pipeline
 */
static int queue_log_entry(metahunter_t *info, journal_entry_t *entry)
{
	entry_proc_op_t *op;
	processor_t *pl = info->processor;

	op = entry_proc_get(pl);
	if (!op) {
		xt_log("reader", XT_LOG_ERROR, "processor failed "
		       "to allocate a new op" );
		return 1;
	}

	op->stage = 0;
	op->log_inserted = time(NULL);
	op->extra_info = entry;
	op->id = entry->attr->fid.inode;
	op->pid = entry->attr->parentid.inode;
	op->op = entry->op;
	/*
	 * push into to the pipeline
	 */
	entry_proc_push(pl, op);

	return 0;
}

/*
 * Wait for log reader terminated
 */
static void xt_log_reader_wait(metahunter_t *info)
{
	void *ret;	

	pthread_join(info->thr_id, &ret);

	return;
}

/*
 * single thread read journal/log and then push to multiple pipeline
 */
static void *log_reader_thr(void *arg)
{
	metahunter_t *info = (metahunter_t *)arg;
	journal_entry_t *entry = NULL;
	filesystem_t *fs = info->fs;
	mattr_t *rattr = &fs->root;
	journal_entry_t roent;
	int ret = 0;

	xt_log("reader", XT_LOG_INFO, "start log reader thread ...");

	/*
	 * build a journal entry for root according attr
	 */
	memset(&roent, 0 ,sizeof (journal_entry_t));
	roent.seq = 0;
	roent.op = op_init;
	roent.name = strdup("/");
	roent.attr = rattr;

	/*
	 * queue root entry
	 */
	queue_log_entry(info, &roent);

	while (!info->force_stop) {
		ret = filesystem_hold_jentry(fs, &entry);
		if (ret == 0) {
			/*
			 * got new entry, then queue it
			 */
			xt_log("reader", XT_LOG_TRACE, "hold entry entry:%llx",
			    (unsigned long long)entry->seq);
			queue_log_entry(info, entry);
		} else if (ret == -1) {
			/*
			 * MDS stops
			 */
			xt_log("reader", XT_LOG_WARNING, "log reader stop!");
			break;
		} else {
			xt_log("reader", XT_LOG_WARNING, "hold entry "
			       "failed %d!", ret);
			break;			
		}
	}

	return NULL;
}

/*
 * start Log Reader thread
 */
static int xt_log_reader_start(metahunter_t *info)
{
	int ret = -1;
	filesystem_t *fs = info->fs;
	database_t *db = info->db;
	processor_t *processor = info->processor;

	if (!fs || !processor) {
		return ret;
	}

	INIT_XLIST_HEAD(&info->op_queue);

	/*
	 * init filesystem
	 */
	ret = filesystem_init(fs);
	if (ret) {
		ret = -1;
		xt_log("reader", XT_LOG_ERROR, "Failed to init filesystem ...");
		return ret;
	}

	/*
	 * init database
	 */
	if (db != NULL) {
		ret = database_init(db);
		if (ret) {
			ret = -1;
			xt_log("reader", XT_LOG_ERROR, "Failed to init "
		            "database ...");
			goto err;
		}
	}

	/*
	 * associate the fs and db to processor
	 */
	processor->info = info;

#if 0	
	info->entry_pool = mem_pool_new(sizeof(journal_entry_t),
	    processor->outstanding_ops);
	info->attr_pool = mem_pool_new(sizeof(mattr_t),
	    processor->outstanding_ops);
#endif

	/*
	 * init processor
	 */
	ret = processor_init(processor);
	if (ret) {
		ret = -1;
		xt_log("reader", XT_LOG_ERROR, "Failed to init processor ...");
		goto err;
	}

	/*
	 * start the reader main thread
	 */
	if (pthread_create(&info->thr_id, NULL, log_reader_thr, info)) {
		ret = errno;
		xt_log("reader", XT_LOG_ERROR,
		       "creating main log reader thread: %s",
		       strerror(ret) );
	       
		goto err;
	}

	/*
	 * wait for reader thread join
	 */
	xt_log_reader_wait(info);

	ret = 0;
err:
	if (processor->info)
		filesystem_fini(fs);

	if (info->entry_pool) {
		mem_pool_destroy(info->entry_pool);
	}

	if (info->attr_pool) {
		mem_pool_destroy(info->attr_pool);
	}

	processor_cleanup(processor);

	return ret;
}

/*
 * Terminate log readers
 */
static void xt_log_reader_terminate(metahunter_t *info)
{
	/*
	 * inform the MDS terminate the journal read
	 */
	filesystem_fini(info->fs);
	/* ask threads to stop */
	info->force_stop = 1;
	xt_log_reader_wait(info);
	processor_cleanup(info->processor);
}

static int xt_create_pid_file(const char *pid_file)
{
	int ret = 0;
	int fd = open(pid_file, O_CREAT | O_TRUNC | O_WRONLY, 0644);

	if (fd < 0) {
		xt_log("log reader", XT_LOG_ERROR, "fail to open pid file %s",
			pid_file);
		ret = -1;
	} else {
		char pid_str[128];
		ssize_t iolen;

		snprintf(pid_str, 128, "%lu\n", (unsigned long) getpid());
		iolen = write(fd, pid_str, strlen(pid_str) + 1);

		if (iolen == -1) {
			xt_log("log reader", XT_LOG_ERROR,
			       "writing pid file %s: %s", pid_file,
			       strerror(errno));
			ret = -1;
		}
		close(fd);
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
			xt_log_reader_terminate(&reader_info);
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
    {"pid-file", required_argument, NULL, 'p'},
    {"no-daemon", no_argument, NULL, 'n'},
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
	metahunter_t *info = &reader_info;
	char *pid_file = MH_DEFAULT_PID_FILE;
	char *log_file = MH_DEFAULT_LOG_FILE;
	char *conf_file = MH_DEFAULT_CONF_FILE;
	int option_index = 0;
	int nodaemon = 0;
	xt_loglevel_t log_lvl = XT_LOG_INFO;
	int c;

	while ((c = getopt_long(argc, argv, SHORT_OPT_STRING,
	    option_tab, &option_index)) != -1) {
		switch (c) {
		case 'c':
			conf_file = xt_strdup(optarg);
			break;
		case 'p':
			pid_file = xt_strdup(optarg);
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
		case 'n':
			nodaemon = 1;
			break;
		case 'h':
		default:
			display_help();
			exit(1);
		}
	}

	ret = xt_log_init(log_file);
	if (ret) {
		printf("Logging initialization failed!\n");
		exit(1);
	}

	xt_log_set_loglevel(log_lvl);

	ret = xt_create_pid_file(pid_file);
	if (ret) {
		xt_log("hunter", XT_LOG_ERROR, "Failed to create pid file.");
		exit(1);
	}

	if (!nodaemon) {
		ret = daemon(0, 0);
		if (ret) {
			xt_log("hunter", XT_LOG_ERROR, "Failed to detach "
			    "process from parent!");
			printf("Failed to detach process from parent!\n");
			exit(1);
		}
	}

	ret = xt_parse_config(conf_file, info);
	if (ret) {
		xt_log("hunter", XT_LOG_ERROR, "Failed to parse configuration"
		    "file!");
		exit(1);
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
	 * start log reader thread
	 */
	ret = xt_log_reader_start(info);
	if (ret) {
		xt_log("hunter", XT_LOG_ERROR, "failed to start reader thread.");
		exit(1);
	}

	return 0;
}
