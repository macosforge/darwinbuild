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

int resolve_dependencies(const char* build, const char* project);

static int run(CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count >= 1)  return -1;
	char* project = (count == 1) ? strdup_cfstr(CFArrayGetValueAtIndex(argv, 0)) : NULL;
	char* build = strdup_cfstr(DBGetCurrentBuild());
	resolve_dependencies(build, project);
	free(project);
	return res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("[<project>]"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("resolveDeps"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}

static int addToCStrArrays(void* pArg, int argc, char** argv, char** columnNames) {
	int i;
	for (i = 0; i < argc; ++i) {
		CFMutableArrayRef array = ((CFMutableArrayRef*)pArg)[i];
		CFArrayAppendValue(array, argv[i]);
	}
	return 0;
}

int resolve_project_dependencies( const char* build, const char* project, int* resolvedCount, int* unresolvedCount) {
	CFMutableArrayRef files = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	CFMutableArrayRef types = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	CFMutableArrayRef params[2] = { files, types };

	if (SQL("BEGIN")) { return -1; }

	SQL_CALLBACK(&addToCStrArrays, params,
		"SELECT DISTINCT dependency,type FROM unresolved_dependencies WHERE build=%Q AND project=%Q",
		build, project);

	CFIndex i, count = CFArrayGetCount(files);
	for (i = 0; i < count; ++i) {
		const char* file = CFArrayGetValueAtIndex(files, i);
		const char* type = CFArrayGetValueAtIndex(types, i);
		// XXX
		// This assumes a 1-to-1 mapping between files and projects.
		char* dep = (char*)SQL_STRING("SELECT project FROM files WHERE path=%Q", file);
		if (dep) {
			// don't add duplicates
			int exists = SQL_BOOLEAN("SELECT 1 FROM dependencies WHERE build=%Q AND project=%Q AND type=%Q AND dependency=%Q",
				build, project, type, dep);
			if (!exists) {
				SQL("INSERT INTO dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
					build, project, type, dep);
				*resolvedCount += 1;
				fprintf(stderr, "\t%s (%s)\n", dep, type);
			}
			SQL("DELETE FROM unresolved_dependencies WHERE build=%Q AND project=%Q AND type=%Q AND dependency=%Q",
				build, project, type, file);
		} else {
			*unresolvedCount += 1;
		}
	}

	if (SQL("COMMIT")) { return -1; }
	
	CFRelease(files);
	CFRelease(types);
}

int resolve_dependencies(const char* build, const char* project) {
	CFMutableArrayRef builds = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	CFMutableArrayRef projects = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	int resolvedCount = 0, unresolvedCount = 0;
	CFMutableArrayRef params[2] = { builds, projects };

	//
	// If no project, version specified, resolve everything.
	// Otherwise, resolve only that project or version.
	//
	if (build == NULL && project == NULL) {
		SQL_CALLBACK(&addToCStrArrays, params, "SELECT DISTINCT build,project FROM unresolved_dependencies");
	} else {
		SQL_CALLBACK(&addToCStrArrays, params, "SELECT DISTINCT build,project FROM unresolved_dependencies WHERE project=%Q", project);
	}
	CFIndex i, count = CFArrayGetCount(projects);
	for (i = 0; i < count; ++i) {
		const char* build = CFArrayGetValueAtIndex(builds, i);
		const char* project = CFArrayGetValueAtIndex(projects, i);
		fprintf(stderr, "%s (%s)\n", project, build);
		resolve_project_dependencies(build, project, &resolvedCount, &unresolvedCount);
	}

	fprintf(stderr, "%d dependencies resolved, %d remaining.\n", resolvedCount, unresolvedCount);

	CFRelease(builds);
	CFRelease(projects);
}
