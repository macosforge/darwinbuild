/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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

int run(CFArrayRef argv) {
  
  // check usage
  CFIndex count = CFArrayGetCount(argv);
  if (count != 3)  return -1;

  // get arguments
  CFStringRef property = CFArrayGetValueAtIndex(argv, 0);
  CFStringRef comparison = CFArrayGetValueAtIndex(argv, 1);
  CFStringRef desired_value = CFArrayGetValueAtIndex(argv, 2);

  // get current build and project list
  CFStringRef build = DBGetCurrentBuild();
  CFArrayRef projects = DBCopyProjectNames(build);
  CFIndex projcount = CFArrayGetCount(projects);

  // test each project for matching query
  for (int i=0; i < projcount; i++) {
    CFStringRef project = CFArrayGetValueAtIndex(projects, i);
    CFStringRef current_value = DBCopyPropString(build, project, property);

    if (current_value && (
	((CFStringCompare(comparison, CFSTR("="), 0) == 0) &&
	 (CFStringCompare(desired_value, current_value, kCFCompareCaseInsensitive) == kCFCompareEqualTo))
	||
	((CFStringCompare(comparison, CFSTR("!="), 0) == 0) &&
	 (CFStringCompare(desired_value, current_value, kCFCompareCaseInsensitive) != kCFCompareEqualTo))
	||
	((CFStringCompare(comparison, CFSTR(">"), 0) == 0) &&
	 (CFStringCompare(desired_value, current_value, kCFCompareCaseInsensitive) == kCFCompareGreaterThan))
	||
	((CFStringCompare(comparison, CFSTR(">="), 0) == 0) &&
	 (CFStringCompare(desired_value, current_value, kCFCompareCaseInsensitive) != kCFCompareLessThan))
	||
	((CFStringCompare(comparison, CFSTR("<="), 0) == 0) &&
	 (CFStringCompare(desired_value, current_value, kCFCompareCaseInsensitive) != kCFCompareGreaterThan))
	||
	((CFStringCompare(comparison, CFSTR("<"), 0) == 0) &&
	 (CFStringCompare(desired_value, current_value, kCFCompareCaseInsensitive) == kCFCompareLessThan))
			  )) {
      cfprintf(stdout, "%@\n", project);
    }

    if (current_value) CFRelease(current_value);
  }

  if (projects) CFRelease(projects);
  return 0;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<property> <comparison> <value>"
			      "\n\t\t... comparison is one of =, !=, >, <, <=, >="));
}

int initialize(int version) {
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("query"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}
