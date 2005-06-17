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

void printDependencies(CFStringRef* types, CFStringRef* recursiveTypes, CFMutableSetRef visited, CFStringRef build, CFStringRef project);


static int run(CFArrayRef argv) {
	CFIndex count = CFArrayGetCount(argv);
	if (count != 2) return -1;

	CFStringRef project = CFArrayGetValueAtIndex(argv, 1);

	CFStringRef type = CFArrayGetValueAtIndex(argv, 0);
	if (CFEqual(type, CFSTR("-run"))) {
		CFStringRef types[] = { CFSTR("lib"), CFSTR("run"), NULL };
		printDependencies(types, types, NULL, DBGetCurrentBuild(), project);
	} else if (CFEqual(type, CFSTR("-build"))) {
		CFStringRef types[] = { CFSTR("lib"), CFSTR("run"), CFSTR("build"), NULL };
		CFStringRef recursive[] = { CFSTR("lib"), CFSTR("run"), NULL };
		printDependencies(types, recursive, NULL, DBGetCurrentBuild(), project);
	} else if (CFEqual(type, CFSTR("-header"))) {
		CFStringRef types[] = { CFSTR("header"), NULL };
		printDependencies(types, types, NULL, DBGetCurrentBuild(), project);
	} else {
		return -1;
	}
	return 0;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("-run | -build | -header <project>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginPropertyType);
	DBPluginSetName(CFSTR("dependencies"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	DBPluginSetDataType(CFDictionaryGetTypeID());
	return 0;
}


void printDependencies(CFStringRef* types, CFStringRef* recursiveTypes, CFMutableSetRef visited, CFStringRef build, CFStringRef project) {
	if (!visited) visited = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	
	CFDictionaryRef dependencies = DBCopyPropDictionary(build, project, CFSTR("dependencies"));
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
						cfprintf(stdout, "%@\n", newproject);
						CFSetAddValue(visited, newproject);
						printDependencies(recursiveTypes, recursiveTypes, visited, build, newproject);
					}
				}
				
				CFRelease(array);
			}
			++type;
		}
	}
	CFRelease(dependencies);
}
