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
#ifndef __MH_HUNTER_H__
#define __MH_HUNTER_H__

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "mem.h"
#include "logging.h"
#include "xlist.h"

#include "database.h"
#include "filesystem.h"
#include "processor.h"

/* reader thread info, one per MDS */
typedef struct metahunter
{
	/** thread id */
	pthread_t thr_id;

	/** nbr of records read by this thread */
	unsigned long long nb_read;

	/** time when the last line was read */
	time_t  last_read_time;

	/** time of the last read record */
	struct timeval last_read_record_time;

	/** last read record id */
	unsigned long long last_read_record;

	/** last record id committed to database */
	unsigned long long last_committed_record;

	/** last record id cleared with changelog */
	unsigned long long last_cleared_record;

	/** last record pushed to the pipeline */
	unsigned long long last_pushed;

	/** thread was asked to stop */
	unsigned int force_stop : 1;

	/** Queue of pending changelogs to push to the pipeline. */
	struct xlist_head op_queue;
	unsigned int op_queue_count;

	/*
	 * database description
	 */
	database_t *db;

	/*
	 * FS description
	 */

	filesystem_t *fs;

	/*
	 * processor description
	 */
	processor_t *processor;

	struct mem_pool *entry_pool;
	struct mem_pool *attr_pool;


} metahunter_t;

#endif
