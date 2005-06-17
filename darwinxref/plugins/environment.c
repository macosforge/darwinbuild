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

static int run(CFArrayRef argv) {
	if (CFArrayGetCount(argv) > 1)  return -1;
	CFStringRef build = DBGetCurrentBuild();
	CFStringRef project = NULL;
	CFDictionaryRef projectEnv = NULL;
	CFDictionaryRef globalEnv = DBCopyPropDictionary(build, NULL, CFSTR("environment"));
	if (CFArrayGetCount(argv) == 1) {
		project = CFArrayGetValueAtIndex(argv, 0);
		projectEnv = DBCopyPropDictionary(build, project, CFSTR("environment"));
	}
	
	CFMutableDictionaryRef env = NULL;
	
	if (globalEnv && projectEnv) {
		env = (CFMutableDictionaryRef)mergeDictionaries(projectEnv, globalEnv);
	} else if (globalEnv) {
		env = (CFMutableDictionaryRef)globalEnv;
	} else if (projectEnv) {
		env = (CFMutableDictionaryRef)projectEnv;
	} else {
		return 0;
	}

	// Auto-generate some variables based on RC_ARCHS and RC_NONARCH_CFLAGS
	// RC_CFLAGS=$RC_NONARCH_CFLAGS -arch ${arch}
	// RC_${arch}=YES
	CFStringRef str = CFDictionaryGetValue(env, CFSTR("RC_NONARCH_CFLAGS"));
	if (!str) str = CFSTR("");
	CFMutableStringRef cflags = CFStringCreateMutableCopy(NULL, 0, str);
	str = CFDictionaryGetValue(env, CFSTR("RC_ARCHS"));
	if (str) {
		CFMutableStringRef trimmed = CFStringCreateMutableCopy(NULL, 0, str);
		CFStringTrimWhitespace(trimmed);
		CFArrayRef archs = tokenizeString(trimmed);
		CFIndex i, count = CFArrayGetCount(archs);
		for (i = 0; i < count; ++i) {
			CFStringRef arch = CFArrayGetValueAtIndex(archs, i);
			// -arch ${arch}
			CFStringAppendFormat(cflags, NULL, CFSTR(" -arch %@"), arch);
			
			// RC_${arch}=YES
			CFStringRef name = CFStringCreateWithFormat(NULL, NULL, CFSTR("RC_%@"), arch);
			CFDictionarySetValue(env, name, CFSTR("YES"));
			CFRelease(name);
		}
		CFRelease(trimmed);
	}
	CFDictionarySetValue(env, CFSTR("RC_CFLAGS"), cflags);
	
	// print variables to stdout
	CFArrayRef keys = dictionaryGetSortedKeys(env);
	CFIndex i, count = CFArrayGetCount(keys);
	for (i = 0; i < count; ++i) {
		CFStringRef name = CFArrayGetValueAtIndex(keys, i);
		CFStringRef value = CFDictionaryGetValue(env, name);
		cfprintf(stdout, "%@=%@\n", name, value);
	}
	return 0;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("[<project>]"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginPropertyType);
	DBPluginSetName(CFSTR("environment"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	DBPluginSetDataType(CFDictionaryGetTypeID());
	return 0;
}
