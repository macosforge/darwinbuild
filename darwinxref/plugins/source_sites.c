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

static int run(void* session, CFArrayRef argv) {
	CFIndex i, count = CFArrayGetCount(argv);
	CFStringRef build = DBGetCurrentBuild(session);
	CFStringRef project = NULL;
	if (count > 1)  return -1;
	if (count == 1) {
		project = CFArrayGetValueAtIndex(argv, 0);
	}
	CFArrayRef source_sites = DBCopyPropArray(session, build, project, CFSTR("source_sites"));
	count = CFArrayGetCount(source_sites);
	if (count == 0) {
		source_sites = DBCopyPropArray(session, build, NULL, CFSTR("source_sites"));
		count = CFArrayGetCount(source_sites);
	}
	for (i = 0; i < count; ++i) {
		cfprintf(stdout, "%@\n", CFArrayGetValueAtIndex(source_sites, i));
	}
	return 0;
}

static CFStringRef usage(void* session) {
	return CFRetain(CFSTR("[<project>]"));
}

DBPropertyPlugin* initialize(int version) {
	DBPropertyPlugin* plugin = NULL;

	if (version != kDBPluginCurrentVersion) return NULL;
	
	plugin = malloc(sizeof(DBPropertyPlugin));
	if (plugin == NULL) return NULL;
	
	plugin->base.version = kDBPluginCurrentVersion;
	plugin->base.type = kDBPluginPropertyType;
	plugin->base.name = CFSTR("source_sites");
	plugin->base.run = &run;
	plugin->base.usage = &usage;
	plugin->datatype = CFArrayGetTypeID();

	return plugin;
}
