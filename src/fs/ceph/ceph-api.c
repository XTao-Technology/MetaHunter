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

#include "mem.h"
#include "logging.h"
#include "ceph-api.h"


#define MH_MAX_NAME 256


int ceph_connect_mds_journal(char *cluster, char *mds, char *fs, void **hdl,
			     mattr_t *root_ent)
{
	int ret = 0;
	char conf_path[256] = "";
	struct ceph_mount_info *cmount = NULL;
	uint32_t mds_num = 0;
	
	snprintf(conf_path, sizeof(conf_path), "/etc/ceph/%s.conf",
		 (cluster != NULL) ? cluster : "ceph");	

	xt_log(XT_CEPH_API, XT_LOG_DEBUG, "hunter connect %s %s %s",
	       cluster, mds, fs);
	
	ret = ceph_create(&cmount, NULL);
		if (ret) {
		xt_log(MH_CEPH, XT_LOG_ERROR, "ceph create fail");
		return ret;
	}
	
	ret = ceph_conf_read_file(cmount, conf_path);
	if (ret) {
		xt_log(MH_CEPH, XT_LOG_ERROR, "ceph read conf file fail");
		return ret;
	}
	// ret = ceph_init(cmount, NULL);
	ret = ceph_mount(cmount, NULL);
	if (ret) {
		xt_log(MH_CEPH, XT_LOG_ERROR, "ceph mount fail");
		return ret;
	}
	ret = ceph_mh_open_journal(cmount, mds_num);
	*hdl = (void *)cmount;
	
	ret = ceph_mh_get_root(cmount, (void*)root_ent, sizeof(struct mattr));
	if (ret) {
		xt_log(MH_CEPH, XT_LOG_ERROR, "ceph get root fail");
		return ret;
	}
	xt_log(MH_CEPH, XT_LOG_INFO, "ceph connect sucess");
	return 0;
}

int ceph_disconnect_mds_journal(void *hdl)
{
	xt_log(XT_CEPH_API, XT_LOG_DEBUG, "hunter disconnect %p", hdl);

	return 0;
}

int ceph_hold_journal_entry(void *hdl, journal_entry_t **ppentry)
{
	int ret = -1;
	void * raw_pjentry = NULL;
	int len = 0;
	struct ceph_mount_info *cmount = (struct ceph_mount_info *)hdl;
	
	// mh_journal_entry was defined in libcephfs.h
	//pmhentry = XT_CALLOC(1, sizeof(struct mh_journal_entry));
	raw_pjentry = XT_CALLOC(1, sizeof(struct journal_entry));
	len = sizeof(struct journal_entry);
	ret = ceph_mh_hold_journal_entry(cmount, raw_pjentry, len);
	if (ret) {
		xt_log(XT_CEPH_API, XT_LOG_ERROR, "reach ceph journal end");
		sleep(120);
		return ret;
	}
	*ppentry = (struct journal_entry *)raw_pjentry;
	/*
	*ppentry = (journal_entry_t *)XT_CALLOC(1, sizeof(journal_entry_t));
	strcpy((*ppentry)->attr.path, "/");
	(*ppentry)->attr.parentid.fs_key = 0;
	(*ppentry)->attr.parentid.validator = 0;
	(*ppentry)->attr.fid.fs_key = 0;
	(*ppentry)->attr.fid.validator = 0;
	(*ppentry)->seq = pmhentry->seq;
	(*ppentry)->op = pmhentry->op;
	(*ppentry)->attr.parentid.inode = pmhentry->attr.parentid.inode;
	(*ppentry)->attr.fid.inode = pmhentry->attr.fid.inode;
	strcpy((*ppentry)->attr.name, pmhentry->attr.name);
	(*ppentry)->attr.mode = pmhentry->attr.mode;
	(*ppentry)->attr.uid = pmhentry->attr.uid;
	(*ppentry)->attr.gid = pmhentry->attr.gid;
	(*ppentry)->attr.nlink = pmhentry->attr.nlink;
	(*ppentry)->attr.ctime = pmhentry->attr.ctime;
	(*ppentry)->attr.atime = pmhentry->attr.atime;
	(*ppentry)->attr.size = pmhentry->attr.size;
	*/


	/*
	xt_log(XT_CEPH_API, XT_LOG_INFO,
	       "hold entry op %d, seq %d, parentid %d, fid %d, name %s mode %d,\n"
	       "ctime %s, atime %s, size %d",
	       (*ppentry)->op, (*ppentry)->seq, (*ppentry)->attr.parentid.inode,
	       (*ppentry)->attr.fid.inode, (char*)((*ppentry)->attr.name),
	       (*ppentry)->attr.mode, (*ppentry)->attr.ctime, (*ppentry)->attr.atime,
		(*ppentry)->attr.size);
	*/
	xt_log(XT_CEPH_API, XT_LOG_INFO,
	       "mh hold jentry op %d, seq %d, fid %llx, name %s, size %d, "
	       " blocks %d, blksize %d, parentid %llx, parent name %s",
	       (*ppentry)->op,
	       (*ppentry)->seq,
	       (*ppentry)->attr->fid.inode,
	       (*ppentry)->name,
	       (*ppentry)->attr->size,
	       (*ppentry)->attr->blocks,
	       (*ppentry)->attr->blksize,
	       (*ppentry)->attr->parentid.inode,
	       (*ppentry)->name2);

	return 0;
}

int ceph_release_journal_entry(void *hdl, journal_entry_t *pentry)
{
	int ret = -1;
	struct ceph_mount_info *cmount = (struct ceph_mount_info *)hdl;

	ret = ceph_mh_release_journal_entry(cmount, pentry->seq);

	if (ret) {
		xt_log(XT_CEPH_API, XT_LOG_ERROR,
		       "ceph release entry seq %d fail", pentry->seq);
		/* can we free the entry here? */
		return ret;
	}

	xt_log(XT_CEPH_API, XT_LOG_INFO,
	       "ceph release entry seq %d success", pentry->seq);

	XT_FREE(pentry);
	pentry = NULL;
	return 0;
}

int ceph_terminate_journal_read(void *hdl)
{
	xt_log(XT_CEPH_API, XT_LOG_DEBUG, "hunter terminate journal read");
	return 0;
}
