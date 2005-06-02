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

#ifndef __DARWINBUILD_PLUGIN_H__
#define __DARWINBUILD_PLUGIN_H__

#include <CoreFoundation/CoreFoundation.h>

typedef struct DBPlugin DBPlugin;

/*!
	@typedef DBPluginInitializeFunc
	Initialization function present in every plugin.
	@param version The latest plugin version that darwinxref is aware of
	(kDBPluginCurrentVersion).
	@result A pointer to an initialized DBPlugin structure describing this
	plugin.  The structure should be allocated with malloc(3).
*/
typedef DBPlugin* (*DBPluginInitializeFunc)(int version);

/*!
	@typedef DBPluginRunFunc
	Performs an action when the plugin is invoked from the command line.
	@param session An opaque session pointer that should be passed to
	any callbacks from the plugin.
	@param argv The command line arguments.
	@result Returns an exit status for darwinxref.
*/
typedef int (*DBPluginRunFunc)(void* session, CFArrayRef argv);

/*!
	@typedef DBPluginUsageFunc
	Returns the command line usage string for this plugin.
	@param session An opaque session pointer that should be passed to
	any callbacks from the plugin.
	@result The command line usage string.
*/
typedef CFStringRef (*DBPluginUsageFunc)(void* session);


/*!
	@constant kDBPluginCurrentVersion
	Current version of the plugin data structure.
*/
enum {
	kDBPluginCurrentVersion = 0,
};

/*!
	@constant kDBPluginType
	Basic plugin type that extends darwinxref functionality from the
	command line.
	@constant kDBPluginPropertyType
	Plugin which adds a build or project property to the plist file.
*/
enum {
	kDBPluginType = FOUR_CHAR_CODE('plug'),
	kDBPluginPropertyType = FOUR_CHAR_CODE('prop'),
};


/*!
	@typedef DBPlugin
	Basic plugin data structure returned by plugin's initialize function.
	@field version kDBPluginCurrentVersion (this structure is version 0).
	@field type kDBPluginType, or kDBPluginPropertyType.
	@field name The name of the plugin (visible from the command line).
	@field run The function to call when the plugin is invoked from the
	command line.
	@field usage The function that returns the command line usage string
	for this plugin.
*/
struct DBPlugin {
	UInt32		version;
	UInt32		type;
	CFStringRef	name;
	DBPluginRunFunc		run;
	DBPluginUsageFunc	usage;
};


/*!
	@typedef DBPropertyPlugin
	Extended plugin data structure for the kDBPluginPropertyType.
	@field base The basic plugin structure
	@field datatype The datatype of this property
	(i.e. one of CFStringGetTypeID(), CFArrayGetTypeID(), CFDictionaryGetTypeID())
*/
typedef struct {
	DBPlugin	base;
	CFTypeID	datatype;
} DBPropertyPlugin;

// plugin API

CFStringRef DBGetCurrentBuild(void* session);

CFArrayRef DBCopyPropNames(void* session, CFStringRef build, CFStringRef project);
CFArrayRef DBCopyProjectNames(void* session, CFStringRef build);

CFTypeRef DBCopyProp(void* session, CFStringRef build, CFStringRef project, CFStringRef property);
CFStringRef DBCopyPropString(void* session, CFStringRef build, CFStringRef project, CFStringRef property);
CFArrayRef DBCopyPropArray(void* session, CFStringRef build, CFStringRef project, CFStringRef property);
CFDictionaryRef DBCopyPropDictionary(void* session, CFStringRef build, CFStringRef project, CFStringRef property);

int DBSetProp(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFTypeRef value);
int DBSetPropString(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFStringRef value);
int DBSetPropArray(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFArrayRef value);
int DBSetPropDictionary(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFDictionaryRef value);

CFDictionaryRef DBCopyProjectPlist(void* session, CFStringRef build, CFStringRef project);
CFDictionaryRef DBCopyBuildPlist(void* session, CFStringRef build);

int DBSetPlist(void* session, CFStringRef build, CFPropertyListRef plist);


#include "cfutils.h"

#endif
