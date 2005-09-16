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

void printDependencies(CFStringRef* types, CFStringRef* recursiveTypes, CFMutableSetRef visited, CFStringRef build, CFStringRef project, int indentLevel);



static int run(CFArrayRef argv) {
	CFIndex count = CFArrayGetCount(argv);
	if (count != 2) return -1;

	CFStringRef project = CFArrayGetValueAtIndex(argv, 1);

	CFStringRef type = CFArrayGetValueAtIndex(argv, 0);
	if (CFEqual(type, CFSTR("-run"))) {
		CFStringRef types[] = { CFSTR("lib"), CFSTR("run"), NULL };
		printDependencies(types, types, NULL, DBGetCurrentBuild(), project, 0);
	} else if (CFEqual(type, CFSTR("-build"))) {
		CFStringRef types[] = { CFSTR("staticlib"), CFSTR("lib"), CFSTR("run"), CFSTR("build"), NULL };
		CFStringRef recursive[] = { CFSTR("lib"), CFSTR("run"), NULL };
		printDependencies(types, recursive, NULL, DBGetCurrentBuild(), project, 0);
	} else if (CFEqual(type, CFSTR("-header"))) {
		CFStringRef types[] = { CFSTR("header"), NULL };
		CFStringRef recursive[] = { NULL };
		printDependencies(types, recursive, NULL, DBGetCurrentBuild(), project, 0);
	} else if (CFEqual(type, CFSTR("-staticlib"))) {
		CFStringRef types[] = { CFSTR("staticlib"), NULL };
		CFStringRef recursive[] = { NULL };
		printDependencies(types, recursive, NULL, DBGetCurrentBuild(), project, 0);
	} else if (CFEqual(type, CFSTR("-lib"))) {
		CFStringRef types[] = { CFSTR("lib"), NULL };
		CFStringRef recursive[] = { NULL };
		printDependencies(types, recursive, NULL, DBGetCurrentBuild(), project, 0);
	} else {
		return -1;
	}
	return 0;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("-run | -build | -header | -staticlib | -lib <project>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginProjectPropertyType);
	DBPluginSetName(CFSTR("dependencies"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	DBPluginSetDataType(CFDictionaryGetTypeID());
	DBPluginSetSubDictDataType(CFArrayGetTypeID());
	return 0;
}

// Dependencies are special, so we can't use a simple DBCopyPropDictionary() call.
//
// We support additive and subtractive dependencies like so:
//
// 8A428:
// foo = {
//     ...
//     dependencies = {
//         build = {
//             bash,
//             gcc_os,
//             gcc_select
//             gnumake,
//         };
//     };
// };
//
// 8B15 (inherits 8A428):
// foo = {
//     ...
//     dependencies = {
//         "+build" = {
//             gcc,
//         };
//         "-build" = {
//             gcc_os,
//         };
//     };
// };
//
// Would result in the following:
// $ darwinxref -b 8B15 dependencies build foo
// bash
// gcc
// gcc_select
// gnumake

static CFDictionaryRef copyDependenciesDictionary(CFStringRef build, CFStringRef project) {
	CFArrayRef builds = DBCopyBuildInheritance(build);
	CFMutableDictionaryRef result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFIndex i, count = CFArrayGetCount(builds);
	
	for (i = 0; i < count; ++i) {
		build = CFArrayGetValueAtIndex(builds, i);
		CFDictionaryRef deps = DBCopyOnePropDictionary(build, project, CFSTR("dependencies"));
		if (deps == NULL) continue;
		CFArrayRef keys = dictionaryGetSortedKeys(deps);
		CFIndex k, kcount = CFArrayGetCount(keys);
		// iterate through the array backwards, since we want to process these in the order:
		// "foo", "-foo", "+foo".
		for (k = kcount - 1; k >= 0; k--) {
			CFStringRef key = CFArrayGetValueAtIndex(keys, k);
			CFArrayRef newdeps = CFDictionaryGetValue(deps, key);
			
			UniChar first = CFStringGetCharacterAtIndex(key, 0);
			if (first == '+') {
				// add in these dependencies (if they don't already exist)
				CFStringRef base = CFStringCreateWithSubstring(NULL, key, CFRangeMake(1, CFStringGetLength(key)-1));
				CFMutableArrayRef olddeps = (CFMutableArrayRef)CFDictionaryGetValue(result, base);
				if (olddeps != NULL) arrayAppendArrayDistinct(olddeps, newdeps);
				CFRelease(base);
			} else if (first == '-') {
				// subtract these dependencies (if they exist)
				CFStringRef base = CFStringCreateWithSubstring(NULL, key, CFRangeMake(1, CFStringGetLength(key)-1));
				CFMutableArrayRef olddeps = (CFMutableArrayRef)CFDictionaryGetValue(result, base);
				if (olddeps != NULL) {
					CFIndex i, count = CFArrayGetCount(newdeps);
					CFRange range = CFRangeMake(0, CFArrayGetCount(olddeps));
					for (i = 0; i < count; ++i) {
						CFStringRef item = CFArrayGetValueAtIndex(newdeps, i);
						// XXX: assumes there's only one occurrance of the value
						CFIndex idx = CFArrayGetFirstIndexOfValue(olddeps, range, item);
						if (idx != kCFNotFound) {
							CFArrayRemoveValueAtIndex(olddeps, idx);
							--range.length;
						}
					}
				}
				CFRelease(base);
			} else {
				// replace the entire list of dependencies
				CFDictionarySetValue(result, key, newdeps);
			}
		}
		CFRelease(keys);
	}
	CFRelease(builds);
	return result;
}

void printDependencies(CFStringRef* types, CFStringRef* recursiveTypes, CFMutableSetRef visited, CFStringRef build, CFStringRef project, int indentLevel) {
	if (!visited) visited = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	
	CFDictionaryRef dependencies = copyDependenciesDictionary(build, project);
	if (dependencies) {
		CFStringRef* type = types;
		while (*type != NULL) {
			CFArrayRef array = CFDictionaryGetValue(dependencies, *type);
			if (array) {
				// if it's a single string, make it an array
				if (CFGetTypeID(array) == CFStringGetTypeID()) {
					array = CFArrayCreate(NULL, (const void**)&array, 1, &kCFTypeArrayCallBacks);
				} else {
					CFRetain(array);
				}

				CFIndex i, count = CFArrayGetCount(array);
				for (i = 0; i < count; ++i) {
					CFStringRef newproject = CFArrayGetValueAtIndex(array, i);
					if (!CFSetContainsValue(visited, newproject)) {
					  // use the indent level as a minimum
					  // precision for the string ""
						cfprintf(stdout, "%*s%@\n",
							 indentLevel, "",
							 newproject);
						CFSetAddValue(visited, newproject);
						printDependencies(recursiveTypes, recursiveTypes, visited, build, newproject, indentLevel+1);
					}
				}
				
				CFRelease(array);
			}
			++type;
		}
		CFRelease(dependencies);
	}
}
