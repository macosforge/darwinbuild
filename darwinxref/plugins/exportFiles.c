/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @APPLE_BSD_LICENSE_HEADER_END@
 */

#include "DBPlugin.h"
#include <sys/stat.h>
#include <stdio.h>
#include <regex.h>

static int exportFiles(void* db, char* buildparam, char* project);

static int run(void* session, CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count > 1)  return -1;

	char* project = NULL;
	if (count == 1) {
		project = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));
	}
	
	char* build = strdup_cfstr(DBGetCurrentBuild(session));
	exportFiles(session, build, project);
	if (project) free(project);
	return res;
}

static CFStringRef usage(void* session) {
	return CFRetain(CFSTR("[<project>]"));
}

DBPlugin* initialize(int version) {
	DBPlugin* plugin = NULL;

	if (version != kDBPluginCurrentVersion) return NULL;
	
	plugin = malloc(sizeof(DBPlugin));
	if (plugin == NULL) return NULL;
	
	plugin->version = kDBPluginCurrentVersion;
	plugin->type = kDBPluginType;
	plugin->name = CFSTR("exportFiles");
	plugin->run = &run;
	plugin->usage = &usage;

	return plugin;
}

int printFiles(void* pArg, int argc, char **argv, char** columnNames) {
	char* project = ((char**)pArg)[0];
	if (strcmp(project, argv[0]) != 0) {
		strncpy(project, argv[0], BUFSIZ);
		fprintf(stdout, "%s:\n", project);
	}
	fprintf(stdout, "\t%s\n", argv[3]);
	return 0;
}

static int exportFiles(void* session, char* build, char* project) {
	int res;

	char* table = "CREATE TABLE files (build text, project text, path text)";
	char* index = "CREATE INDEX files_index ON files (build, project, path)";
	SQL_NOERR(session, table);
	SQL_NOERR(session, index);

	fprintf(stdout, "# BUILD %s\n", build);

	CFArrayRef projects = DBCopyProjectNames(session, DBGetCurrentBuild(session));
	if (projects) {
		CFIndex i, count = CFArrayGetCount(projects);
		for (i = 0; i < count; ++i) {
			CFStringRef name = CFArrayGetValueAtIndex(projects, i);
			char projbuf[BUFSIZ];
			res = SQL_CALLBACK(session, &printFiles, projbuf,
				"SELECT project,path FROM files WHERE build=%Q AND project=%Q",
				build, project);
		}
		CFRelease(projects);
	}
	
	return 0;
}
