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
#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#include <unistd.h>

static int run(CFArrayRef argv) {
	ssize_t res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count > 2)  return -1;
	int xml = 0, i;
	CFMutableDictionaryRef dict, preplist;
	CFDictionaryRef project = NULL;
	CFStringRef projname;
	CFArrayRef builds;
	const void *ssites, *bsites;
	CFStringRef build = DBGetCurrentBuild();

	if (count == 2) {
		CFStringRef arg = CFArrayGetValueAtIndex(argv, 0);
		xml = CFEqual(arg, CFSTR("-xml"));
		// -xml is the only supported option
		if (!xml) return -1;
		projname = CFArrayGetValueAtIndex(argv, 1);
	} else if (count == 1 ) {
		projname = CFArrayGetValueAtIndex(argv, 0);
	} else
		return -1;

	builds = DBCopyBuildInheritance(build);
	dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);

	preplist = (CFMutableDictionaryRef)DBCopyProjectPlist(build, NULL);
	ssites = CFDictionaryGetValue(preplist, CFSTR("source_sites"));
	bsites = CFDictionaryGetValue(preplist, CFSTR("binary_sites"));

	preplist = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
	for( i = 0; i < CFArrayGetCount(builds); i++ ) {
		build = CFArrayGetValueAtIndex(builds, i);
		project = DBCopyProjectPlist(build, projname);
		if( CFDictionaryGetCount(project) > 0 )
			break;
	}
	if( CFDictionaryGetCount(project) < 1 )
		return -1;
	CFDictionaryAddValue(dict, projname, project);
	CFRelease(project);
	CFDictionarySetValue(preplist, CFSTR("build"), build);
	CFDictionarySetValue(preplist, CFSTR("projects"), dict);
	if(ssites) CFDictionarySetValue(preplist, CFSTR("source_sites"), ssites);
	if(bsites) CFDictionarySetValue(preplist, CFSTR("binary_sites"), bsites);

	CFPropertyListRef plist = preplist;
	if (xml) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6
	    CFDataRef data = CFPropertyListCreateData(kCFAllocatorDefault, 
			  				  plist, 
							  kCFPropertyListXMLFormat_v1_0,
							  0,
							  NULL);
#else
        CFDataRef data = CFPropertyListCreateData(NULL, kCFAllocatorDefault, kCFPropertyListXMLFormat_v1_0, 0, 0);
#endif
		res = write(STDOUT_FILENO, CFDataGetBytePtr(data), (ssize_t)CFDataGetLength(data));
	} else {
		res = writePlist(stdout, plist, 0);
	}
	return (int)res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("[-xml] project"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("exportProject"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}

