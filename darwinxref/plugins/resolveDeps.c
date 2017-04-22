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

int resolve_dependencies(const char* build, const char* project, int commit);

static int run(CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count > 2)  return -1;
	char* project = NULL;
	int commit = 0;
	
	if (count > 0) {	  
	  if (CFEqual(CFSTR("-commit"), CFArrayGetValueAtIndex(argv, 0))) {
	    commit = 1;
	  } else {
	    project = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));
	  }
	}

	if (count > 1) {
	  project = strdup_cfstr(CFArrayGetValueAtIndex(argv, 1));	  
	}

	char* build = strdup_cfstr(DBGetCurrentBuild());
	resolve_dependencies(build, project, commit);
	free(project);
	return res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("[-commit] [<project>]"));
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

int resolve_project_dependencies( const char* build, const char* project, int* resolvedCount, int* unresolvedCount, int commit) {
	CFMutableArrayRef files = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	CFMutableArrayRef types = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	CFMutableArrayRef params[2] = { files, types };

        char* table = "CREATE TABLE dependencies (build TEXT, project TEXT, type TEXT, dependency TEXT)";
        char* index = "CREATE INDEX dependencies_index ON unresolved_dependencies (build, project, type, dependency)";

        SQL_NOERR(table);
        SQL_NOERR(index);

	if (SQL("BEGIN")) { return -1; }

	// Convert from unresolved_dependencies (i.e. path names) to resolved dependencies (i.e. project names)
	// Deletes unresolved_dependencies after they are processed.
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

	CFRelease(files);
	CFRelease(types);

	// If committing, merge resolved dependencies to the dependencies property dictionary.
	// Deletes resolved dependencies after they are processed.
	if (commit) {
		CFMutableArrayRef types = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
		CFMutableArrayRef projs = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
		CFMutableArrayRef params[2] = { projs, types };
		CFMutableDictionaryRef dependencies = (CFMutableDictionaryRef)DBCopyPropDictionary(cfstr(build), cfstr(project), CFSTR("dependencies"));
		if (dependencies == NULL) {
			dependencies = CFDictionaryCreateMutable(NULL, 0,
								     &kCFCopyStringDictionaryKeyCallBacks,
								     &kCFTypeDictionaryValueCallBacks);
		}
		
		SQL_CALLBACK(&addToCStrArrays, params,
			"SELECT DISTINCT dependency,type FROM dependencies WHERE build=%Q AND project=%Q",
			build, project);
		
		CFIndex i, count = CFArrayGetCount(projs);
		for (i = 0; i < count; ++i) {
			CFStringRef proj = cfstr(CFArrayGetValueAtIndex(projs, i));
			CFStringRef type = cfstr(CFArrayGetValueAtIndex(types, i));
			
			CFMutableArrayRef deparray = (CFMutableArrayRef)CFDictionaryGetValue(dependencies, type);
			if (deparray == NULL) {
				deparray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				CFDictionarySetValue(dependencies, type, deparray);
				CFRelease(deparray); // still retained by dict
			}
			if (!CFArrayContainsValue(deparray,
							CFRangeMake(0, CFArrayGetCount(deparray)),
							proj)) {
				CFArrayAppendValue(deparray, proj);
			}
			CFRelease(proj);
			CFRelease(type);
		}
		
		DBSetProp(cfstr(build), cfstr(project), CFSTR("dependencies"), dependencies);
		CFRelease(dependencies);
		CFRelease(types);
		CFRelease(projs);

		SQL("DELETE FROM dependencies WHERE build=%Q AND project=%Q", build, project);
	}

	if (SQL("COMMIT")) { return -1; }

	return 0;
}

int resolve_dependencies(const char* build, const char* project, int commit) {
	CFMutableArrayRef builds = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	CFMutableArrayRef projects = CFArrayCreateMutable(NULL, 0, &cfArrayCStringCallBacks);
	int resolvedCount = 0, unresolvedCount = 0;
	CFMutableArrayRef params[2] = { builds, projects };

	// if committing, use all projects, otherwise use unresolved projects
	char *table = ( commit ? "properties" : "unresolved_dependencies" );

	//
	// If no project, version specified, resolve everything.
	// Otherwise, resolve only that project or version.
	//
	if (project == NULL) {
		SQL_CALLBACK(&addToCStrArrays, params, "SELECT DISTINCT build,project FROM %Q WHERE project IS NOT NULL", table);
	} else {
		SQL_CALLBACK(&addToCStrArrays, params, "SELECT DISTINCT build,project FROM %Q WHERE project=%Q", table, project);
	}
	CFIndex i, count = CFArrayGetCount(projects);
	for (i = 0; i < count; ++i) {
		const char* build = CFArrayGetValueAtIndex(builds, i);
		const char* project = CFArrayGetValueAtIndex(projects, i);
		fprintf(stderr, "%s (%s)\n", project, build);
		resolve_project_dependencies(build, project, &resolvedCount, &unresolvedCount, commit);
	}

	fprintf(stderr, "%d dependencies resolved, %d remaining.\n", resolvedCount, unresolvedCount);

	CFRelease(builds);
	CFRelease(projects);
	
	return 0;
}
