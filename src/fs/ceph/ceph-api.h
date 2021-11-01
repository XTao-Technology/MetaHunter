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
#ifndef __CEPH_API__
#define __CEPH_API__

#define XT_CEPH_API "ceph-api"
#define MH_CEPH "MH_CEPH"

#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64 1
#endif

#include <cephfs/libcephfs.h>
#include "filesystem.h"
/*
 * build connection with MDS and get the journal operating handler
 */
int ceph_connect_mds_journal(char *cluster, char *mds, char *fs, void **hdl,
	mattr_t *root_ent);
/*
 * drop connection with MDS
 */
int ceph_disconnect_mds_journal(void *hdl);
/*
 * read jounral next entry from MDS
 */
int ceph_hold_journal_entry(void *hdl, journal_entry_t **entry);
/*
 * tell MDS release journal entry
 */
int ceph_release_journal_entry(void *hdl, journal_entry_t *entry);
/*
 * tell MDS not pending on wait new journal and fail all follwing journal read
 */
int ceph_terminate_journal_read(void *hdl);

#endif
