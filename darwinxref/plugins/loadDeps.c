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
#include <sys/types.h>
#include <sys/param.h>

int loadDeps(const char* build, const char* project, const char *root);

static int run(CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count != 2)  return -1;
	char* project = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));
	char* root = strdup_cfstr(CFArrayGetValueAtIndex(argv, 1));
	
	char* build = strdup_cfstr(DBGetCurrentBuild());
	loadDeps(build, project, root);
	free(project);
	return res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<project> <buildroot>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("loadDeps"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}

static int has_suffix(const char* big, const char* little) {
	char* found = NULL;
	while (1) {
		char* next = strcasestr(found ? found+1 : big, little);
		if (next) { found = next; } else { break; }
	}
	return found ? (strcmp(found, little) == 0) : 0;
}


int loadDeps(const char* build, const char* project, const char *root) {
	size_t size;
	char* line;
	int count = 0;

	char* table = "CREATE TABLE unresolved_dependencies (build TEXT, project TEXT, type TEXT, dependency TEXT)";
	char* index = "CREATE INDEX unresolved_dependencies_index ON unresolved_dependencies (build, project, type, dependency)";

	SQL_NOERR(table);
	SQL_NOERR(index);

	if (SQL("BEGIN")) { return -1; }

	while ((line = fgetln(stdin, &size)) != NULL) {
		if (line[size-1] == '\n') line[size-1] = 0; // chomp newline
		char* tab = memchr(line, '\t', size);
		if (tab) {
		  	char fullpath[MAXPATHLEN];
			char *type, *file;
			struct stat sb;
			int typesize = (intptr_t)tab - (intptr_t)line;
			asprintf(&type, "%.*s", typesize, line);
			asprintf(&file, "%.*s", (int)size - typesize - 1, tab+1);
			if (strcmp(type, "open") == 0) {
				free(type);
				if (has_suffix(file, ".h")) {
					type = "header";
				} else if (has_suffix(file, ".a")
					   || has_suffix(file, ".o")) {
					type = "staticlib";
				} else {
					type = "build";
				}
			} else if (strcmp(type, "execve") == 0) {
				free(type);
				type = "build";
			} else if (strcmp(type, "readlink") == 0) {
				free(type);
				type = "build";
			}
			
			sprintf(fullpath, "%s/%s", root, file);
			int res = lstat(fullpath, &sb);
			// for now, skip if the path points to a directory
			if (res == 0 && !S_ISDIR(sb.st_mode)) {
				SQL("INSERT INTO unresolved_dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
					build, project, type, file);
			}
			free(file);
		} else {
			fprintf(stderr, "Error: syntax error in input.  no tab delimiter found.\n");
		}
		++count;
	}

	if (SQL("COMMIT")) { return -1; }

	fprintf(stderr, "loaded %d unresolved dependencies.\n", count);

	return 0;
}
