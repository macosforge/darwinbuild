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

static int run(CFArrayRef argv) {

  // get a list of all builds in the database
  CFArrayRef builds = DBCopyBuilds();
  CFIndex buildCount = CFArrayGetCount(builds);

  // make a directed graph in dot format
  cfprintf(stdout, "digraph \"all_builds\" {\n");

  int j;
  for (j =0; j < buildCount; ++j) {
    // get the j-th build and the build it inherits from
    CFStringRef build = CFArrayGetValueAtIndex(builds, j);
    CFStringRef parent = DBCopyOnePropString(build, NULL, CFSTR("inherits"));    

    // if there is an inheritance, print an edgeop
    if (parent) {
      cfprintf(stdout, "\t\"%@\" -> \"%@\"\n", parent, build);
      CFRelease(parent);
    } else {
      // else just print a node in case its an orphan
      cfprintf(stdout, "\t\"%@\"\n", build);      
    }
  }

  // close the digraph stanza
  cfprintf(stdout, "}\n");

  CFRelease(builds);

}

static CFStringRef usage() {
  return CFRetain(CFSTR(""));
}

int initialize(int version) {
	
	DBPluginSetType(kDBPluginPropertyType);
	DBPluginSetName(CFSTR("dot"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	DBPluginSetDataType(CFStringGetTypeID());
	
	return 0;
}
