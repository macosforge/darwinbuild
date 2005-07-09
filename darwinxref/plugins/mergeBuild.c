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

static void addValues(const void* key, const void* value, void* context);

int run(CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count != 2)  return -1;
	
	CFStringRef oldbuild = CFArrayGetValueAtIndex(argv, 0);
	CFStringRef newbuild = CFArrayGetValueAtIndex(argv, 1);

	if (!DBHasBuild(oldbuild)) cfprintf(stderr, "Error: no such build: %@\n", oldbuild);
	if (!DBHasBuild(newbuild)) cfprintf(stderr, "Error: no such build: %@\n", newbuild);

	CFPropertyListRef oldplist = DBCopyBuildPlist(oldbuild);
	CFPropertyListRef newplist = DBCopyBuildPlist(newbuild);

	CFDictionaryRef oldprojects = CFDictionaryGetValue(oldplist, CFSTR("projects"));
	CFDictionaryRef newprojects = CFDictionaryGetValue(newplist, CFSTR("projects"));
		
	CFArrayRef names = dictionaryGetSortedKeys(newprojects);

	CFIndex i;
	count = CFArrayGetCount(names);
	for (i = 0; i < count; ++i) {
		CFStringRef name = CFArrayGetValueAtIndex(names, i);
		CFDictionaryRef oldproj = CFDictionaryGetValue(oldprojects, name);
		CFDictionaryRef newproj = CFDictionaryGetValue(newprojects, name);

		if (oldproj && newproj) {
			CFDictionaryApplyFunction(oldproj, addValues, (void*)newproj);
		}
	}
	DBSetPlist(newbuild, newplist);
	CFRelease(oldplist);
	CFRelease(newplist);
	
	return res;
}

static void addValues(const void* key, const void* value, void* context) {
	CFDictionaryAddValue((CFMutableDictionaryRef)context, (CFStringRef)key, (CFTypeRef)value);
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<old build> <new build>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("mergeBuild"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}
