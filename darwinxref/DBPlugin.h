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
	@result 0 for success, -1 for error.
*/
typedef int (*DBPluginInitializeFunc)(int version);

/*!
	@typedef DBPluginRunFunc
	Performs an action when the plugin is invoked from the command line.
	@param session An opaque session pointer that should be passed to
	any callbacks from the plugin.
	@param argv The command line arguments.
	@result Returns an exit status for darwinxref.
*/
typedef int (*DBPluginRunFunc)(CFArrayRef argv);

/*!
	@typedef DBPluginUsageFunc
	Returns the command line usage string for this plugin.
	@param session An opaque session pointer that should be passed to
	any callbacks from the plugin.
	@result The command line usage string.
*/
typedef CFStringRef (*DBPluginUsageFunc)();


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
	@constant kDBPluginProjectPropertyType
	Plugin which adds a per-project property to the plist file.
	@constant kDBPluginBuildPropertyType
	Plugin which adds a per-build property to the plist file.
*/
enum {
	kDBPluginNullType = 0,
	kDBPluginBasicType = 1,
	kDBPluginPropertyType = 2,
	kDBPluginProjectPropertyType = 3,
	kDBPluginBuildPropertyType = 4,
};

/*!
	@typedef DBPropPluginGetFunc
	Accessor for returning the property implemented by this plugin.
	This gives the plugin the opportunity to perform integrity checks
	on the data other than the basic type checking performed by darwinxref.
	@param session An opaque session pointer that should be passed to
	any callbacks from the plugin.
	@param project The project whose property should be returned, or NULL to get
	a build property.
	@result The value of the property.
*/
//typedef CFPropertyListRef (*DBPluginGetPropFunc)(void* session, CFStringRef project);

/*!
	@typedef DBPropPluginSetFunc
	Accessor for setting the property implemented by this plugin.
	This gives the plugin the opportunity to perform integrity checks
	on the data other than the basic type checking performed by darwinxref.
	@param session An opaque session pointer that should be passed to
	any callbacks from the plugin.
	@param project The project whose property should be set, or NULL to set
	a build property.
	@param value The new value of the property.
	@result The status, 0 for success.
*/
//typedef int (*DBPluginSetPropFunc)(void* session, CFStringRef project, CFPropertyListRef value);

// plugin API

// initialization routines, only call during initialize.
void DBPluginSetType(UInt32 type);
void DBPluginSetName(CFStringRef name);
void DBPluginSetRunFunc(DBPluginRunFunc func);
void DBPluginSetUsageFunc(DBPluginUsageFunc func);
void DBPluginSetDataType(CFTypeID type);
void DBPluginSetSubDictDataType(CFTypeID type);

// default handlers
int DBPluginPropertyDefaultRun(CFArrayRef argv);
CFStringRef DBPluginPropertyDefaultUsage();

// generally available routines

CFStringRef DBGetCurrentBuild();
int DBHasBuild(CFStringRef build);
CFArrayRef DBCopyBuilds();

CFTypeID  DBCopyPropType(CFStringRef property);
CFTypeID  DBCopyPropSubDictType(CFStringRef property);
CFArrayRef DBCopyPropNames(CFStringRef build, CFStringRef project);
CFArrayRef DBCopyProjectNames(CFStringRef build);

CFArrayRef DBCopyChangedProjectNames(CFStringRef oldbuild, CFStringRef newbuild);

CFTypeRef DBCopyProp(CFStringRef build, CFStringRef project, CFStringRef property);
CFStringRef DBCopyPropString(CFStringRef build, CFStringRef project, CFStringRef property);
CFDataRef DBCopyPropData(CFStringRef build, CFStringRef project, CFStringRef property);
CFArrayRef DBCopyPropArray(CFStringRef build, CFStringRef project, CFStringRef property);
CFDictionaryRef DBCopyPropDictionary(CFStringRef build, CFStringRef project, CFStringRef property);

int DBSetProp(CFStringRef build, CFStringRef project, CFStringRef property, CFTypeRef value);
int DBSetPropString(CFStringRef build, CFStringRef project, CFStringRef property, CFStringRef value);
int DBSetPropData(CFStringRef build, CFStringRef project, CFStringRef property, CFDataRef value);
int DBSetPropArray(CFStringRef build, CFStringRef project, CFStringRef property, CFArrayRef value);
int DBSetPropDictionary(CFStringRef build, CFStringRef project, CFStringRef property, CFDictionaryRef value);

CFDictionaryRef DBCopyProjectPlist(CFStringRef build, CFStringRef project);
CFDictionaryRef DBCopyBuildPlist(CFStringRef build);

/*!
	@function DBSetPlist
	Sets properties in the database according to the specified plist.
	@param build The build number whose properties to set.
	@param project The project whose properties to set, or if NULL, the entire build.
	@param plist The plist containing the properties to set.
	@result The status, 0 for success.
*/
int DBSetPlist(CFStringRef build, CFStringRef project, CFPropertyListRef plist);

int DBBeginTransaction();
int DBCommitTransaction();
int DBRollbackTransaction();

#include "cfutils.h"

#endif
