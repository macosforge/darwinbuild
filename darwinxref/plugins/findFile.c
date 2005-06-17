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

static int findFile(char* file, char* build);

static int run(CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count != 1)  return -1;

	char* file = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));	
	char* build = strdup_cfstr(DBGetCurrentBuild());
	
	findFile(file, build);

	if (file) free(file);
	return res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<file>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("findFile"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}

int printFiles(void* pArg, int argc, char **argv, char** columnNames) {
	char* project = (char*)pArg;
	if (strcmp(project, argv[0]) != 0) {
		strncpy(project, argv[0], BUFSIZ);
		fprintf(stdout, "%s:\n", project);
	}
	fprintf(stdout, "\t%s\n", argv[1]);
	return 0;
}

static int findFile(char* file, char* build) {
	char* errmsg;
	char project[BUFSIZ];
	project[0] = 0;
	asprintf(&file, "%%%s", file);
	int res = SQL_CALLBACK(&printFiles, project,
		"SELECT project,path FROM files WHERE build=%Q AND path LIKE %Q ORDER BY project, path",
		build, file);
	return 0;
}
