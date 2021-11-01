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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "mem.h"
#include "logging.h"
#include "cJSON.h"
#include "hunter.h"
#include "database.h"
#include "filesystem.h"
#include "processor.h"
#include "cfg-parser.h"

#define MH_PARSER "MH_PARSER"

static int parse_db(cJSON *seg, metahunter_t *mh) {
	database_t *db = NULL;
	cJSON *c = NULL;
	int ret = -1;

	xt_log(MH_PARSER, XT_LOG_TRACE, "enter parse db");

	db = XT_CALLOC(1, sizeof (database_t));
	if (!db) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "database allocation failure.");
		return -1;
	}
		
	c = cJSON_GetObjectItem(seg, "name");
	if (!c || c->type != cJSON_String) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "DB configure name invalid!");
		goto err;
	}

	db->name = xt_strdup(c->valuestring);

	ret = database_load(db);
	if (ret) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "DB load module failure!");
		goto err;
	}
		
	ret = db->db_ops->db_conf_parse(seg, &db->conf);
	if (ret) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "DB config parse failed!");
		goto err;
	}

	mh->db = db;

	ret = 0;

	xt_log(MH_PARSER, XT_LOG_TRACE, "exit parse db");
	return ret;
err:
	XT_FREE(db);
	return ret;
}

static int parse_fs(cJSON *seg, metahunter_t *mh) {
	filesystem_t *fs = NULL;
	cJSON *c = NULL;
	int ret = -1;

	xt_log(MH_PARSER, XT_LOG_TRACE, "enter parse fs");

	fs = XT_CALLOC(1, sizeof (filesystem_t));
	if (!fs) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "filesystem allocation "
		    "failed.");
		return -1;
	}
		
	c = cJSON_GetObjectItem(seg, "name");
	if (!c || c->type != cJSON_String) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "fs config name invalid.");
		goto err;		
	}

	fs->name = xt_strdup(c->valuestring);

	ret = filesystem_load(fs);
	if (ret) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "load fs module failed.");
		goto err;
	}
		
	ret = fs->fs_ops->fs_conf_parse(seg, &fs->conf);
	if (ret) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "parse fs config failed.");
		goto err;
	}
		

	ret = 0;
	mh->fs = fs;

	xt_log(MH_PARSER, XT_LOG_TRACE, "exit parse fs");

	return ret;
err:
	XT_FREE(fs);
	return ret;
}

static int parse_processor(cJSON *seg, metahunter_t *mh)
{
	processor_t *processor = NULL;
	cJSON *c = NULL;
	int ret = -1;

	xt_log(MH_PARSER, XT_LOG_TRACE, "enter parse processor");

	processor = XT_CALLOC(1, sizeof (processor_t));
	if (!processor) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "processor allocation failed.");
		return -1;
	}
		
	c = cJSON_GetObjectItem(seg, "name");
	if (!c || c->type != cJSON_String) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "processor conf name invalid.");
		goto err;
	}

	processor->name = xt_strdup(c->valuestring);

	c = cJSON_GetObjectItem(seg, "workercnt");
	if (!c || c->type != cJSON_Number) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "processor workercnt invalid.");
		goto err;
	}

	processor->workercnt = c->valueint;

	c = cJSON_GetObjectItem(seg, "outstanding_limit");
	if (!c || c->type != cJSON_Number) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "processor outstanding invalid.");
		goto err;
	}

	processor->outstanding_ops = c->valueint;

	ret = processor_load(processor);
	if (ret) {
		xt_log(MH_PARSER, XT_LOG_ERROR, "load processor %s failed.",
		    processor->name);
		goto err;
	}

	if (processor->conf_parse) {
		ret = processor->conf_parse(seg, &processor->conf);
		if (ret) {
			xt_log(MH_PARSER, XT_LOG_ERROR, "parse processor conf "
			    "failed.");
			goto err;
		}
	}

	ret = 0;
	mh->processor = processor;

	xt_log(MH_PARSER, XT_LOG_TRACE, "exit parse processor");

	return ret;
err:
	XT_FREE(processor);
	return ret;
}


static int parse_segments(cJSON *json, metahunter_t *mh)
{
	int ret = -1;
	cJSON *seg = NULL;

	if (json == NULL)
		return ret;

	for (seg = json->child; seg; seg = seg->next) {
		if (seg->type != cJSON_Object) {
			xt_log(MH_PARSER, XT_LOG_ERROR, "invalid segment type");
			return -1;
		}

		if (!strcmp(seg->string, "FileSystem")) {
			ret = parse_fs(seg, mh);
		} else if (!strcmp(seg->string, "Processor")) {
			ret = parse_processor(seg, mh);
		} else if (!strcmp(seg->string, "DataBase")) {
			ret = parse_db(seg, mh);
		} else {
			xt_log(MH_PARSER, XT_LOG_ERROR, "invalid segment");
			return -1;
		}
		if (ret)
			return -1;
	}
	return 0;
}

int xt_parse_config(char *conffile, metahunter_t *mh)
{
	FILE *f = NULL;
	long len = 0;
	char *data;
	int ret = 0;
	char *out;
	cJSON *json = NULL;

	f = fopen(conffile, "rb");
	if (f == NULL) {
		xt_log("parser", XT_LOG_ERROR, "failed to open conf file:%s",
		    conffile);
		return -1;
	}

	ret = fseek(f, 0, SEEK_END);
	if (ret == -1) {
		xt_log("parser", XT_LOG_ERROR, "failed to seek conf file to "
		    "end: %s with %s!", conffile, strerror(errno));
		return -1;
	}

	len = ftell(f);
	if (len == -1) {
		xt_log("parser", XT_LOG_ERROR, "failed to tell conf file length"
		    ": %s, %s!", conffile, strerror(errno));
		return -1;
	}

	ret = fseek(f, 0, SEEK_SET);
	if (ret == -1) {
		xt_log("parser", XT_LOG_ERROR, "failed to seek conf file to "
		    "head: %s with %s!", conffile, strerror(errno));
		return -1;
	}
	data = XT_MALLOC(len);
	if (data == NULL) {
		xt_log("parser", XT_LOG_ERROR, "Failed allocate buffer for %s."
		    , conffile);
			return -1;
	}

	fread(data, 1, len, f);
	fclose(f);

	json = cJSON_Parse(data);

	out = cJSON_Print(json);

	xt_log("parser", XT_LOG_INFO, "Configuration:\n %s \n", out);
	free(out);

	/*
	 * Parse configuration json file
	 */
	ret = parse_segments(json, mh);

	XT_FREE(data);

	cJSON_Delete(json);

	return ret;
}

