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
	@field datatype The datatype of this property
	(i.e. one of CFStringGetTypeID(), CFArrayGetTypeID(), CFDictionaryGetTypeID())
	@field subdictdatatype For dictionary data types, force values to be
	this type (i.e. CFArrayGetTypeID())
	@field getprop The property get accessor, NULL for default behavior
	@field setprop The property set accessor, NULL for default behavior
*/
struct DBPlugin {
	// for all plugins
	UInt32		type;
	CFStringRef	name;
	// for compiled plugins
	DBPluginRunFunc		run;
	DBPluginUsageFunc	usage;
#if HAVE_TCL_PLUGINS
	// for tcl plugins
	void*		interp;
#endif
	// for property plugins
	CFTypeID	datatype;
	CFTypeID	subdictdatatype;
//	DBPluginGetPropFunc getprop;
//	DBPluginSetPropFunc setprop;
};

void* _DBPluginGetDataStorePtr();
DBPlugin* _DBPluginGetCurrentPlugin();

const DBPlugin* DBGetPluginWithName(CFStringRef name);

int DBBeginTransaction();
int DBRollbackTransaction();
int DBCommitTransaction();

#if HAVE_TCL_PLUGINS
int load_tcl_plugin(DBPlugin* plugin, const char* filename);
CFStringRef call_tcl_usage(DBPlugin* plugin);
int call_tcl_run(DBPlugin* plugin, CFArrayRef args);
#endif

int DBPluginLoadPlugins(const char* path);
int run_plugin(int argc, char* argv[]);
int DBDataStoreInitialize(const char* datafile);
void DBSetCurrentBuild(char* build);
