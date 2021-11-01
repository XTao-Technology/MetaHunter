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

#ifndef __LOGGING_H__
#define __LOGGING_H__

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <libgen.h>

#include "locking.h"


#define XT_PRI_FSBLK       PRIu64
#define XT_PRI_DEV         PRIu64
#define XT_PRI_NLINK       PRIu32
#define XT_PRI_SUSECONDS   "ld"
#define XT_PRI_BLKSIZE     PRId32
#define XT_PRI_SIZET       "zu"

typedef enum {
        XT_LOG_NONE,
        XT_LOG_EMERG,
        XT_LOG_ALERT,
        XT_LOG_CRITICAL,   /* fatal errors */
        XT_LOG_ERROR,      /* major failures (not necessarily fatal) */
        XT_LOG_WARNING,    /* info about normal operation */
        XT_LOG_NOTICE,
        XT_LOG_INFO,       /* Normal information */
        XT_LOG_DEBUG,      /* internal errors */
        XT_LOG_TRACE,      /* full trace of operation */
} xt_loglevel_t;

typedef struct xt_log_handle_ {
        xt_lock_t  logfile_mutex;
        xt_loglevel_t    loglevel;
        int              xt_log_syslog;
        xt_loglevel_t    sys_log_level;
        FILE            *logfile;
        size_t          log_max_size; 
} xt_log_handle_t;

#define FMT_WARN(fmt...) do { if (0) printf (fmt); } while (0)

#define xt_log(dom, levl, fmt...) do {                                  \
                FMT_WARN (fmt);                                         \
                                                                        \
                _xt_log (dom, __FILE__, __FUNCTION__, __LINE__,         \
                         levl, ##fmt);                                  \
        } while (0)

#define xt_log_callingfn(dom, levl, fmt...) do {                        \
                FMT_WARN (fmt);                                         \
                                                                        \
                _xt_log_callingfn (dom, __FILE__, __FUNCTION__, __LINE__, \
                                   levl, ##fmt);                        \
        } while (0)


/* No malloc or calloc should be called in this function */
#define xt_log_nomem(dom, levl, size) do {                              \
                _xt_log_nomem (dom, __FILE__, __FUNCTION__, __LINE__,   \
                               levl, size);                             \
        } while (0)


/* Log once in XT_UNIVERSAL_ANSWER times */
#define XT_LOG_OCCASIONALLY(var, args...) if (!(var++%XT_UNIVERSAL_ANSWER)) { \
                xt_log (args);                                          \
        }


void xt_log_logrotate (int signum);
void xt_log_max_size (unsigned long max_size);

int xt_log_init (const char *filename);

void xt_log_cleanup (void);

int _xt_log (const char *domain, const char *file, const char *function,
             int32_t line, xt_loglevel_t level, const char *fmt, ...);
int _xt_log_callingfn (const char *domain, const char *file, const char *function,
                       int32_t line, xt_loglevel_t level, const char *fmt, ...);

int _xt_log_nomem (const char *domain, const char *file,
                   const char *function, int line, xt_loglevel_t level,
                   size_t size);

void xt_log_lock (void);
void xt_log_unlock (void);

void xt_log_disable_syslog (void);
void xt_log_enable_syslog (void);
xt_loglevel_t xt_log_get_loglevel (void);
void xt_log_set_loglevel (xt_loglevel_t level);

xt_loglevel_t xt_str2loglvl(char *str);

#define XT_DEBUG(xl, format, args...)                           \
        xt_log ((xl)->name, XT_LOG_DEBUG, format, ##args)
#define XT_INFO(xl, format, args...)                            \
        xt_log ((xl)->name, XT_LOG_INFO, format, ##args)
#define XT_WARNING(xl, format, args...)                         \
        xt_log ((xl)->name, XT_LOG_WARNING, format, ##args)
#define XT_ERROR(xl, format, args...)                           \
        xt_log ((xl)->name, XT_LOG_ERROR, format, ##args)

#endif /* __LOGGING_H__ */
