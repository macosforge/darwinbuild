/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 * Portions copyright (c) 2008 Michael Franz <mvfranz@gmail.com>
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

/*
 * Given a build name, an array of project names, and the index to start at...
 * Print the names in the array to stdout
 */
void _print_remaining_projects(CFStringRef build, CFArrayRef nameArray, int index) {
  CFIndex count = CFArrayGetCount(nameArray);

  while ( index < count ) {
    CFStringRef projectName = CFArrayGetValueAtIndex(nameArray, index);
    CFStringRef version = DBCopyPropString(build, projectName, CFSTR("version"));
    cfprintf(stdout, "%@-%@ only in %@\n", projectName, version, build);    
    CFRelease(version);
    index++;
  }
}

/*
 * Diff two project files
 */
int run(CFArrayRef argv) {

  // ensure we have two and only two arguments
  CFIndex count = CFArrayGetCount(argv);
  if (count != 2)  return -1;
  
  // get the build values
  CFStringRef build1 = CFArrayGetValueAtIndex(argv, 0);
  CFStringRef build2 = CFArrayGetValueAtIndex(argv, 1);
  
  // ensure that both build exist in the DB
  if (!DBHasBuild(build1)) cfprintf(stderr, "Error: no such build: %@\n", build1);
  if (!DBHasBuild(build2)) cfprintf(stderr, "Error: no such build: %@\n", build2);
  
  // get the full list of projects, including inherited projects
  CFArrayRef nameArray1 = DBCopyProjectNames(build1);
  CFArrayRef nameArray2 = DBCopyProjectNames(build2);
  CFIndex count1 = CFArrayGetCount(nameArray1);
  CFIndex count2 = CFArrayGetCount(nameArray2);
  
  // loop through each build's list of project 1 at a time compare versions of matching
  // projects.  If one build is missing a  project, advance to the next for that list

  int i=0, j=0;

  // first, go through the comparisons until 1 array runs out of items
  for ( ; i < count1 && j < count2 ; ) {

    CFStringRef projectName1 = CFArrayGetValueAtIndex(nameArray1, i);
    CFStringRef projectName2 = CFArrayGetValueAtIndex(nameArray2, j);
    
    CFComparisonResult res = CFStringCompare(projectName1, projectName2, 0);
    
    if (res == kCFCompareEqualTo) {
      // the project exist in both builds, check the versions
      CFStringRef version1 = DBCopyPropString(build1, projectName1, CFSTR("version"));
      CFStringRef version2 = DBCopyPropString(build2, projectName2, CFSTR("version"));
      
      // projectName1 and projectName2 are the same, doesn't matter which one we use
      if (!CFEqual(version1, version2)) {
	cfprintf(stdout, "%@ differs: %@ vs %@\n", projectName1, version1, version2);
      }

      CFRelease(version1);
      CFRelease(version2);
      
      // move both lists to next project
      ++i;
      ++j;

    } else if (res == kCFCompareLessThan) {
      // 1 fell behind 2, 2 is missing a project
      CFStringRef version1 = DBCopyPropString(build1, projectName1, CFSTR("version"));
      cfprintf(stdout, "%@-%@ only in %@\n", projectName1, version1, build1);
      CFRelease(version1);
      ++i;

    } else if (res == kCFCompareGreaterThan) {
      // 1 got ahead of 2, which means 1 is missing a project
      CFStringRef version2 = DBCopyPropString(build2, projectName1, CFSTR("version"));
      cfprintf(stdout, "%@-%@ only in %@\n", projectName2, version2, build2);
      CFRelease(version2);
      ++j;
    }


  }
  
  // check if one of the arrays has projects left to print
  if (i < count1) {
    _print_remaining_projects(build1, nameArray1, i);
  } else if (j < count2) {
    _print_remaining_projects(build2, nameArray2, j);
  }

  CFRelease(nameArray1);
  CFRelease(nameArray2);

  return 0;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<build1> <build2>"));
}

int initialize(int version) {
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("diff"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}
