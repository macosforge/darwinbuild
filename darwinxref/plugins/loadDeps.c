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
#include <sys/types.h>

int loadDeps(const char* build, const char* project);

static int run(CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count != 1)  return -1;
	char* project = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));
	char* build = strdup_cfstr(DBGetCurrentBuild());
	loadDeps(build, project);
	free(project);
	return res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<project>"));
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

static char* canonicalize_path(const char* path) {
#if 1 // USE_REALPATH
	char* result = malloc(PATH_MAX);
	realpath(path, result);
	return result;
#else
	char* result = malloc(strlen(path) + 1);
	const char* src = path;
	char* dst = result;
	
	while (*src != 0) {
		// Backtrack to previous /
		if (strncmp(src, "/../", 4) == 0) {
			src += 3; // skip to last /, so we can evaluate that below
			if (dst == result) {
				*dst = '/';
				dst += 1;
			} else {
				do {
					dst -= 1;
				} while (dst != result && *dst != '/');
			}
		// Trim /..
		} else if (strcmp(src, "/..") == 0) {
			src += 3;
			if (dst == result) {
				*dst = '/';
				dst += 1;
			} else {
				do {
					dst -= 1;
				} while (dst != result && *dst != '/');
			}
		// Compress /./ to /
		} else if (strncmp(src, "/./", 3) == 0) {
			src += 2;	// skip to last /, so we can evaluate that below
		// Trim /.
		} else if (strcmp(src, "/.") == 0) {
			*dst = 0;
			break;
		// compress slashes, trim trailing slash
		} else if (*src == '/') {
			if ((dst == result || dst[-1] != '/') && src[1] != 0) {
				*dst = '/';
				dst += 1;
				src += 1;
			} else {
				src += 1;
			}
		// default to copying over a single char
		} else {
			*dst = *src;
			dst += 1;
			src += 1;
		}
	}
	
	*dst = 0;  // NUL terminate
	
	return result;
#endif
}

int loadDeps(const char* build, const char* project) {
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
			char *type, *file;
			int typesize = (intptr_t)tab - (intptr_t)line;
			asprintf(&type, "%.*s", typesize, line);
			asprintf(&file, "%.*s", size - typesize - 1, tab+1);
			if (strcmp(type, "open") == 0) {
				free(type);
				if (has_suffix(file, ".h")) {
					type = "header";
				} else if (has_suffix(file, ".a")) {
					type = "staticlib";
				} else {
					type = "build";
				}
			} else if (strcmp(type, "execve") == 0) {
				free(type);
				type = "build";
			}
			char* canonicalized = canonicalize_path(file);
			
			struct stat sb;
			int res = lstat(canonicalized, &sb);
			// for now, skip if the path points to a directory
			if (!(res == 0 && (sb.st_mode & S_IFDIR) == S_IFDIR)) {
				SQL("INSERT INTO unresolved_dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
					build, project, type, canonicalized);
			}
			free(canonicalized);
			free(file);
		} else {
			fprintf(stderr, "Error: syntax error in input.  no tab delimiter found.\n");
		}
		++count;
	}

	if (SQL("COMMIT")) { return -1; }

	fprintf(stderr, "loaded %d unresolved dependencies.\n", count);
}
