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

#ifndef __MH_ATTR_H__
#define __MH_ATTR_H__

#include <limits.h>
#include <unistd.h>
#include <sys/types.h>

typedef struct obj_id
{
	uint64_t fs_key;
	ino_t inode;
	int validator;
} obj_id_t;

typedef struct mattr {
	obj_id_t parentid;
	obj_id_t fid;
	int depth;
	int dircount;
	mode_t    mode;    /* protection */
        nlink_t   nlink;   /* number of hard links */
        uid_t     uid;     /* user ID of owner */
        gid_t     gid;     /* group ID of owner */
        off_t     size;    /* total size, in bytes */
        blksize_t blksize; /* blocksize for file system I/O */
        blkcnt_t  blocks;  /* number of 512B blocks allocated */
        time_t    atime;   /* time of last access */
        time_t    mtime;   /* time of last modification */
        time_t    ctime;   /* time of last status change */
} mattr_t;
#endif
