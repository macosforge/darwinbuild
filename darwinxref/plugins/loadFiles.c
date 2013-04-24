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
#include "DBDataStore.h"
#include <sys/stat.h>
#include <stdio.h>
#include <regex.h>

static int loadFiles(const char* buildparam, const char* path);

static int run(CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count != 1)  return -1;
	char* filename = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));
	char* build = strdup_cfstr(DBGetCurrentBuild());
	loadFiles(build, filename);
	free(filename);
	return res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<logfile>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("loadFiles"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}

static inline int min(int a, int b) {
	return (a < b) ? a : b; 
}

//
// Loads the specified index file into the sqlite database
// This way we can do intelligent sql queries on the index
// data.
//
int loadFiles(const char* buildparam, const char* path) {
	FILE* fp = fopen(path, "r");
	int loaded = 0, total = 0;
	if (fp) {
		//
		// Create the projects table if it does not already exist
		//
				
		if (SQL("BEGIN")) { return -1; }

		char project[PATH_MAX];
		char build[PATH_MAX];
		
		project[0] = 0;
		strncpy(build, buildparam, PATH_MAX);
				
		for (;;) {
			int skip = 0;
			size_t size;
			char *line, *buf = fgetln(fp, &size);
			if (buf == NULL) break;
			if (buf[size-1] == '\n') --size;
			line = malloc(size+1);
			memcpy(line, buf, size);
			line[size] = 0;
						
			// The parsing state machine works as follows:
			//
			// 1. look for project followed by colon newline
			// 2. look for \t, insert paths with last build and project name, otherwise goto 1.
			
			regex_t regex;
			regmatch_t matches[3];

			// Skip any commented lines (start with #)
			regcomp(&regex, "^[[:space:]]*#.*", REG_EXTENDED);
			if (regexec(&regex, line, 1, matches, 0) == 0) {
				skip = 1;
			}
			regfree(&regex);

			// Look for build numbers in the comments
			regcomp(&regex, "^[[:space:]]*#[[:space:]]*(BUILD[=[:space:]])?[[:space:]]*([^[:space:]]+)[[:space:]]*.*", REG_EXTENDED | REG_ICASE);
			if (regexec(&regex, line, 3, matches, 0) == 0) {
				int len = min((int)matches[2].rm_eo - (int)matches[2].rm_so, PATH_MAX);
				strncpy(build, line + matches[2].rm_so, len);
				build[len] = 0;
				skip = 1;
			}
			regfree(&regex);

			// File
			regcomp(&regex, "^\t(/.*)$", REG_EXTENDED);
			if (!skip &&
				project[0] &&
				build[0] &&
				regexec(&regex, line, 2, matches, 0) == 0) {
				char path[PATH_MAX];
				int len = min((int)matches[1].rm_eo - (int)matches[1].rm_so, PATH_MAX);
				strncpy(path, line + matches[1].rm_so, len);
				path[len] = 0;
				int res = SQL("INSERT INTO files (build,project,path) VALUES (%Q, %Q, %Q)",
					build, project, path);
				if (res != 0) { return res; }
				++loaded;
				skip = 1;
			}
			regfree(&regex);
			
			
			// New Project
			regcomp(&regex, "^[[:space:]]*([^-]+).*:[[:space:]]*$", REG_EXTENDED | REG_ICASE);
			if (!skip &&
				regexec(&regex, line, 3, matches, 0) == 0) {
				int len = (int)matches[1].rm_eo - (int)matches[1].rm_so;
				strncpy(project, line + matches[1].rm_so, len);
				project[len] = 0;
				int res = SQL("DELETE FROM files WHERE build=%Q AND project=%Q",
					build, project);
				if (res != 0) { return res; }
				++total;
				fprintf(stdout, "%s (%s)\n", project, build);
				skip = 1;
			}
			regfree(&regex);


			free(line);
		}
		fclose(fp);

		if (SQL("COMMIT")) { return -1; }

	} else {
		perror(path);
		return 1;
	}
	fprintf(stdout, "%d files for %d projects loaded.\n", loaded, total);
	return 0;
}
