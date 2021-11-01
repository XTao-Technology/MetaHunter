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
#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "logging.h"
#include "mem.h"
#include "xlist.h"

#include <syslog.h>

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

xt_log_handle_t *xtlog = NULL;

xt_loglevel_t xt_str2loglvl(char *str) {
	if (!strcasecmp(str, "INFO"))
		return XT_LOG_INFO;
	if (!strcasecmp(str, "WARNING"))
		return XT_LOG_WARNING;
	if (!strcasecmp(str, "DEBUG"))
		return XT_LOG_DEBUG;
	if (!strcasecmp(str, "ERROR"))
		return XT_LOG_ERROR;
	if (!strcasecmp(str, "TRACE"))
		return XT_LOG_TRACE;
	if (!strcasecmp(str, "ALERT"))
		return XT_LOG_ALERT;
	if (!strcasecmp(str, "NOTICE"))
		return XT_LOG_NOTICE;
	if (!strcasecmp(str, "EMERG"))
		return XT_LOG_EMERG;

	return XT_LOG_INFO;
}

void
xt_log_max_size (unsigned long max_size)
{
        xtlog->log_max_size = max_size;
}

void
xt_log_enable_syslog (void)
{
        xtlog->xt_log_syslog = 1;
}

void
xt_log_disable_syslog (void)
{
        xtlog->xt_log_syslog = 0;
}

xt_loglevel_t
xt_log_get_loglevel (void)
{
        return xtlog->loglevel;
}

void
xt_log_set_loglevel (xt_loglevel_t level)
{
        xtlog->loglevel = level;
}

int
xt_log_init (const char *file)
{
	xtlog = XT_CALLOC(1, sizeof(xt_log_handle_t));
        LOCK_INIT(&xtlog->logfile_mutex);

        /* For the 'syslog' output. one can grep 'XTAO' in syslog
           for serious logs */
        openlog ("XTAO", LOG_PID, LOG_DAEMON);

        if (!file){
                fprintf (stderr, "ERROR: no filename specified\n");
                return -1;
        }

        xtlog->logfile = fopen (file, "a");
        if (!xtlog->logfile){
                fprintf (stderr, "ERROR: failed to open logfile \"%s\" (%s)\n",
                         file, strerror (errno));
                return -1;
        }

        return 0;
}



struct _msg_queue {
        struct xlist_head msgs;
};

struct _log_msg {
        const char *msg;
        struct xlist_head queue;
};


void
xt_log_lock (void)
{
        LOCK(&xtlog->logfile_mutex);
}


void
xt_log_unlock (void)
{
        UNLOCK(&xtlog->logfile_mutex);
}


void
xt_log_cleanup (void)
{
        LOCK_DESTROY(&xtlog->logfile_mutex);
	XT_FREE(xtlog);
	xtlog = NULL;
}

int
_xt_log_nomem (const char *domain, const char *file,
               const char *function, int line, xt_loglevel_t level,
               size_t size)
{
        const char     *basename        = NULL;
        struct tm      *tm              = NULL;
        struct timeval  tv              = {0,};
        int             ret             = 0;
        xt_loglevel_t   loglevel = 0;
        char            msg[8092];
        char            timestr[256];
        char            callstr[4096];

	loglevel = xtlog->loglevel;

        if (level > loglevel)
                goto out;

       static char *level_strings[] = {"",  /* NONE */
                                        "M", /* EMERGENCY */
                                        "A", /* ALERT */
                                        "C", /* CRITICAL */
                                        "E", /* ERROR */
                                        "W", /* WARNING */
                                        "N", /* NOTICE */
                                        "I", /* INFO */
                                        "D", /* DEBUG */
                                        "T", /* TRACE */
                                        ""};

        if (!domain || !file || !function) {
                fprintf (stderr,
                         "logging: %s:%s():%d: invalid argument\n",
                         __FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -1;
        }

#if HAVE_BACKTRACE
        /* Print 'calling function' */
        do {
                void *array[5];
                char **callingfn = NULL;
                size_t bt_size = 0;

                bt_size = backtrace (array, 5);
                if (bt_size)
                        callingfn = backtrace_symbols (&array[2], bt_size-2);
                if (!callingfn)
                        break;

                if (bt_size == 5)
                        snprintf (callstr, 4096, "(-->%s (-->%s (-->%s)))",
                                  callingfn[2], callingfn[1], callingfn[0]);
                if (bt_size == 4)
                        snprintf (callstr, 4096, "(-->%s (-->%s))",
                                  callingfn[1], callingfn[0]);
                if (bt_size == 3)
                        snprintf (callstr, 4096, "(-->%s)", callingfn[0]);

                free (callingfn);
        } while (0);
#endif /* HAVE_BACKTRACE */

        ret = gettimeofday (&tv, NULL);
        if (-1 == ret)
                goto out;

        tm    = localtime (&tv.tv_sec);

        LOCK(&xtlog->logfile_mutex);
        {
                strftime (timestr, 256, "%Y-%m-%d %H:%M:%S", tm);
                snprintf (timestr + strlen (timestr), 256 - strlen (timestr),
                          ".%"XT_PRI_SUSECONDS, tv.tv_usec);

                basename = strrchr (file, '/');
                if (basename)
                        basename++;
                else
                        basename = file;

                ret = sprintf (msg, "[%s] %s [%s:%d:%s] %s %s: no memory "
                               "available for size (%"XT_PRI_SIZET")",
                               timestr, level_strings[level],
                               basename, line, function, callstr,
                               domain, size);
                if (-1 == ret) {
                        goto unlock;
                }

                if (xtlog->logfile) {
                        fprintf (xtlog->logfile, "%s\n", msg);
                        fflush (xtlog->logfile);
                } else {
                        fprintf (stderr, "%s\n", msg);
                }

                /* We want only serious log in 'syslog', not our debug
                   and trace logs */
                if (xtlog->xt_log_syslog && level && (level <= XT_LOG_ERROR))
                        syslog ((level-1), "%s\n", msg);
        }

unlock:
        UNLOCK(&xtlog->logfile_mutex);
out:
        return ret;
 }

int
_xt_log_callingfn (const char *domain, const char *file, const char *function,
                   int line, xt_loglevel_t level, const char *fmt, ...)
{
        const char     *basename        = NULL;
        struct tm      *tm              = NULL;
        char           *str1            = NULL;
        char           *str2            = NULL;
        char           *msg             = NULL;
        char            timestr[256]    = {0,};
        char            callstr[4096]   = {0,};
        struct timeval  tv              = {0,};
        size_t          len             = 0;
        int             ret             = 0;
        xt_loglevel_t   loglevel = 0;
        va_list         ap;

	loglevel = xtlog->loglevel;

        if (level > loglevel)
                goto out;

        static char *level_strings[] = {"",  /* NONE */
                                        "M", /* EMERGENCY */
                                        "A", /* ALERT */
                                        "C", /* CRITICAL */
                                        "E", /* ERROR */
                                        "W", /* WARNING */
                                        "N", /* NOTICE */
                                        "I", /* INFO */
                                        "D", /* DEBUG */
                                        "T", /* TRACE */
                                        ""};

        if (!domain || !file || !function || !fmt) {
                fprintf (stderr,
                         "logging: %s:%s():%d: invalid argument\n",
                         __FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -1;
        }

#if HAVE_BACKTRACE
        /* Print 'calling function' */
        do {
                void *array[5];
                char **callingfn = NULL;
                size_t size = 0;

                size = backtrace (array, 5);
                if (size)
                        callingfn = backtrace_symbols (&array[2], size-2);
                if (!callingfn)
                        break;

                if (size == 5)
                        snprintf (callstr, 4096, "(-->%s (-->%s (-->%s)))",
                                  callingfn[2], callingfn[1], callingfn[0]);
                if (size == 4)
                        snprintf (callstr, 4096, "(-->%s (-->%s))",
                                  callingfn[1], callingfn[0]);
                if (size == 3)
                        snprintf (callstr, 4096, "(-->%s)", callingfn[0]);

                free (callingfn);
        } while (0);
#endif /* HAVE_BACKTRACE */

        ret = gettimeofday (&tv, NULL);
        if (-1 == ret)
                goto out;

        tm    = localtime (&tv.tv_sec);

        LOCK(&xtlog->logfile_mutex);
        {
                va_start (ap, fmt);

                strftime (timestr, 256, "%Y-%m-%d %H:%M:%S", tm);
                snprintf (timestr + strlen (timestr), 256 - strlen (timestr),
                          ".%"XT_PRI_SUSECONDS, tv.tv_usec);

                basename = strrchr (file, '/');
                if (basename)
                        basename++;
                else
                        basename = file;

                ret = xt_asprintf (&str1, "[%s] %s [%s:%d:%s] %s %s: ",
                                   timestr, level_strings[level],
                                   basename, line, function, callstr,
                                   domain);
                if (-1 == ret) {
                        goto unlock;
                }

                ret = vasprintf (&str2, fmt, ap);
                if (-1 == ret) {
                        goto unlock;
                }

                va_end (ap);

                len = strlen (str1);
                msg = XT_MALLOC (len + strlen (str2) + 1);

                strcpy (msg, str1);
                strcpy (msg + len, str2);

                if (xtlog->logfile) {
                        fprintf (xtlog->logfile, "%s\n", msg);
                        fflush (xtlog->logfile);
                } else {
                        fprintf (stderr, "%s\n", msg);
                }

                /* We want only serious log in 'syslog', not our debug
                   and trace logs */
                if (xtlog->xt_log_syslog && level
		    && (level <= XT_LOG_CRITICAL))
                        syslog ((level-1), "%s\n", msg);
        }

unlock:
        UNLOCK(&xtlog->logfile_mutex);

        if (msg) {
                XT_FREE (msg);
        }

        if (str1)
                XT_FREE (str1);

        if (str2)
                FREE (str2);

out:
        return ret;
}

int
_xt_log (const char *domain, const char *file, const char *function, int line,
         xt_loglevel_t level, const char *fmt, ...)
{
        const char  *basename = NULL;
        va_list      ap;
        struct tm   *tm = NULL;
        char         timestr[256];
        struct timeval tv = {0,};
        char        *str1 = NULL;
        char        *str2 = NULL;
        char        *msg  = NULL;
        size_t       len  = 0;
        int          ret  = 0;

        if (level > xtlog->loglevel)
                goto out;

        static char *level_strings[] = {"",  /* NONE */
                                        "M", /* EMERGENCY */
                                        "A", /* ALERT */
                                        "C", /* CRITICAL */
                                        "E", /* ERROR */
                                        "W", /* WARNING */
                                        "N", /* NOTICE */
                                        "I", /* INFO */
                                        "D", /* DEBUG */
                                        "T", /* TRACE */
                                        ""};

        if (!domain || !file || !function || !fmt) {
                fprintf (stderr,
                         "logging: %s:%s():%d: invalid argument\n",
                         __FILE__, __PRETTY_FUNCTION__, __LINE__);
                return -1;
        }

        ret = gettimeofday (&tv, NULL);
        if (-1 == ret)
                goto out;

        tm    = localtime (&tv.tv_sec);

        LOCK(&xtlog->logfile_mutex);
        {
                va_start (ap, fmt);

                strftime (timestr, 256, "%Y-%m-%d %H:%M:%S", tm);
                snprintf (timestr + strlen (timestr), 256 - strlen (timestr),
                          ".%"XT_PRI_SUSECONDS, tv.tv_usec);

                basename = strrchr (file, '/');
                if (basename)
                        basename++;
                else
                        basename = file;

                ret = xt_asprintf (&str1, "[%s] %s [%s:%d:%s] %s: ",
                                   timestr, level_strings[level],
                                   basename, line, function,
                                   domain);
                if (-1 == ret) {
                        goto unlock;
                }

                ret = vasprintf (&str2, fmt, ap);
                if (-1 == ret) {
                        goto unlock;
                }

                va_end (ap);

                len = strlen (str1);
                msg = XT_MALLOC (len + strlen (str2) + 1);

                strcpy (msg, str1);
                strcpy (msg + len, str2);

                if (xtlog->logfile) {
                        fprintf (xtlog->logfile, "%s\n", msg);
                        fflush (xtlog->logfile);
                } else {
                        fprintf (stderr, "%s\n", msg);
                }

                /* We want only serious log in 'syslog', not our debug
                   and trace logs */
                if (xtlog->xt_log_syslog && level
		    && (level <= XT_LOG_CRITICAL))
                        syslog ((level-1), "%s\n", msg);
        }

unlock:
        UNLOCK(&xtlog->logfile_mutex);

        if (msg) {
                XT_FREE (msg);
        }

        if (str1)
                XT_FREE (str1);

        if (str2)
                FREE (str2);

out:
        return (0);
}
