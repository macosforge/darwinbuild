/*
 * Copyright (c) 2004-2005, Apple Computer, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
	usage: upgrade_plist <plist>
	simple utility to upgrade from darwinbuild 0.5 plists to the new format.
*/

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cfutils.h"
#include <CoreFoundation/CoreFoundation.h>

void usage(char* progname) {
	progname = basename(progname);
	fprintf(stderr, "usage:\t%s <plist>\n", progname);
	exit(1);
}

int convert_dependencies(CFMutableDictionaryRef newdict, CFStringRef oldkey, CFStringRef newkey) {
		CFArrayRef depends = CFDictionaryGetValue(newdict, oldkey);
		if (depends) {
			CFRetain(depends);
			CFDictionaryRemoveValue(newdict, oldkey);
			CFMutableDictionaryRef dependencies = (CFMutableDictionaryRef)CFDictionaryGetValue(newdict, CFSTR("dependencies"));
			if (!dependencies) {
				dependencies = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				CFDictionarySetValue(newdict, CFSTR("dependencies"), dependencies);
				CFRelease(dependencies);
			}
			CFDictionarySetValue(dependencies, newkey, depends);
			CFRelease(depends);
		}
		return 0;
}


int main(int argc, char* argv[]) {
	int res = 0;
	char* progname = argv[0];
	char* errmsg;
	char* command = "";
	char* build = NULL;

	char* path = (argc-- > 0) ? *++argv : NULL;

	if (!path) {
		usage(progname);
	}

	CFPropertyListRef plist = read_plist(path);

	if (!plist) {
		fprintf(stderr, "Error: cannot read plist: %s\n", path);
		exit(1);
	}
	
	CFArrayRef oldprojs = CFDictionaryGetValue(plist, CFSTR("projects"));
	CFMutableDictionaryRef newprojs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	if (!oldprojs) {
		fprintf(stderr, "Error: bad plist (no projects): %s\n", path);
		exit(1);
	}
	
	CFIndex i,j;
	CFIndex oldcount = CFArrayGetCount(oldprojs);
	for(i = 0; i < oldcount; ++i) {
		CFMutableDictionaryRef newdict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(oldprojs, i);
		// XXX: verify dict && type(dict) == dictionary
		
		CFStringRef newname = CFDictionaryGetValue(newdict, CFSTR("name"));
		CFDictionaryAddValue(newprojs, newname, newdict);
		CFDictionaryRemoveValue(newdict, CFSTR("name"));
		
		// Convert from old style to new style dependencies
		convert_dependencies(newdict, CFSTR("depends.build"), CFSTR("build"));
		convert_dependencies(newdict, CFSTR("depends.lib"), CFSTR("lib"));
		convert_dependencies(newdict, CFSTR("depends.run"), CFSTR("run"));
		convert_dependencies(newdict, CFSTR("depends.header"), CFSTR("header"));
	}
	
	CFDictionarySetValue((CFMutableDictionaryRef)plist, CFSTR("projects"), newprojs);

	CFDataRef data = CFPropertyListCreateXMLData(NULL, plist);
	fprintf(stdout, "%.*s", CFDataGetLength(data), CFDataGetBytePtr(data));
}
