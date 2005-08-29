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

#include <CoreFoundation/CoreFoundation.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <fts.h>
#include <libgen.h>

#include "cfutils.h"
#include "DBPlugin.h"
#include "DBPluginPriv.h"

//////
// Public interfaces for plugins
// For more implementation, also see DBDataStore.c
//////
void DBPluginSetType(UInt32 type) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	switch (type) {
		case kDBPluginBasicType:
		case kDBPluginPropertyType:
		case kDBPluginBuildPropertyType:
		case kDBPluginProjectPropertyType:
			plugin->type = type;
			break;
		default:
			// XXX: error
			break;
	}
}

void DBPluginSetName(CFStringRef name) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	plugin->name = CFStringCreateCopy(NULL, name);
}

void DBPluginSetRunFunc(DBPluginRunFunc func) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	plugin->run = func;
}

void DBPluginSetUsageFunc(DBPluginUsageFunc func) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	plugin->usage = func;
}

void DBPluginSetDataType(CFTypeID type) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	plugin->datatype = type;
}

void DBPluginSetSubDictDataType(CFTypeID type) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	plugin->subdictdatatype = type;
}



//////
// Private interfaces to DBPlugin for use by darwinxref internals
//////

//////
// NOT THREAD SAFE
// We currently operate under the assumption that there is only
// one thread, with no plugin re-entrancy.
//////
const DBPlugin* __DBPluginCurrentPlugin;
void _DBPluginSetCurrentPlugin(const DBPlugin* plugin) {
	__DBPluginCurrentPlugin = plugin;
}
DBPlugin* _DBPluginGetCurrentPlugin() {
	return (DBPlugin*)__DBPluginCurrentPlugin;
}

CFDictionaryValueCallBacks cfDictionaryPluginValueCallBacks = {
	0, NULL, NULL, NULL, NULL
};

static CFMutableDictionaryRef plugins;

DBPlugin* _DBPluginInitialize() {
	DBPlugin* plugin = malloc(sizeof(DBPlugin));
	assert(plugin != NULL);
	memset(plugin, 0, sizeof(DBPlugin));
	plugin->type = kDBPluginNullType;
	return plugin;
}

int DBPluginLoadPlugins(const char* plugin_path) {
	if (plugins == NULL) {
		plugins = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &cfDictionaryPluginValueCallBacks);
	}
	if (plugins == NULL) return -1;
	
	//
	// If the path contains colons, split the path and
	// search each path component.  If there are no
	// colons, CFStringCreateArrayBySeparatingStrings()
	// will return an array with one element being the
	// entire path.
	//
	CFStringRef str = cfstr(plugin_path);
	CFArrayRef array = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR(":"));
	CFRelease(str);
	CFIndex i, path_argc = CFArrayGetCount(array);
	char** path_argv = malloc(sizeof(char*)*(path_argc+1));
	for (i = 0; i < path_argc; ++i) {
		path_argv[i] = strdup_cfstr(CFArrayGetValueAtIndex(array, i));
	}
	path_argv[i] = NULL;
	CFRelease(array);
	
	//
	// Search the directories for plugins
	//
	FTSENT* ent;
	FTS* dir = fts_open((char * const *)path_argv, FTS_LOGICAL, NULL);
	while ((ent = fts_read(dir)) != NULL) {
		DBPlugin* plugin = NULL;
		if (strstr(ent->fts_name, ".so")) {
	//		fprintf(stderr, "plugin: loading %s\n", ent->fts_accpath);
			void* handle = dlopen(ent->fts_accpath, RTLD_LAZY | RTLD_LOCAL);
			if (handle) {
				DBPluginInitializeFunc func = dlsym(handle, "initialize");
				plugin = _DBPluginInitialize();
				_DBPluginSetCurrentPlugin(plugin);
				(*func)(kDBPluginCurrentVersion);	// Call out to C plugin
				// XXX: check for error?
			} else {
				fprintf(stderr, "Could not dlopen plugin: %s\n", ent->fts_name);
			}
#if HAVE_TCL_PLUGINS
		} else if (strstr(ent->fts_name, ".tcl")) {
			plugin = _DBPluginInitialize();
			_DBPluginSetCurrentPlugin(plugin);
			load_tcl_plugin(plugin, ent->fts_accpath);	// Calls out to Tcl plugin
#endif
		}
		if (plugin) {
			if (plugin->name == NULL) {
				fprintf(stderr, "warning: plugin has no name (skipping): %s\n", ent->fts_name);
			} else if (plugin->type == kDBPluginNullType) {
				fprintf(stderr, "warning: plugin has no type (skipping): %s\n", ent->fts_name);
			} else {
				CFDictionarySetValue(plugins, plugin->name, plugin);
			}
		}
		ent = ent->fts_link;
	}
	fts_close(dir);
	
	//
	// Release the path array
	//
	for (i = 0; i < path_argc; ++i) {
		free(path_argv[i]);
	}
	free(path_argv);
}

void print_usage(char* progname, int argc, char* argv[]) {
	progname = basename(progname);
	if (!plugins) return;

	if (argc >= 1) {
		CFStringRef name = cfstr(argv[0]);
		const DBPlugin* plugin = DBGetPluginWithName(name);
		if (plugin) {
			_DBPluginSetCurrentPlugin(plugin);
			CFStringRef usage = plugin->usage();
			cfprintf(stderr, "usage: %s [-f db] [-b build] %@ %@\n", progname, name, usage);
			CFRelease(usage);
			return;
		} else {
			cfprintf(stderr, "%s: no such command: %@\n", progname, name);
		}
	}

	cfprintf(stderr, "usage: %s [-f db] [-b build] <command> ...\n", progname);
	cfprintf(stderr, "commands:\n");

	CFArrayRef pluginNames = dictionaryGetSortedKeys(plugins);
	CFIndex i, count = CFArrayGetCount(pluginNames);
	for (i = 0; i < count; ++i) {
		CFStringRef name = CFArrayGetValueAtIndex(pluginNames, i);
		const DBPlugin* plugin = DBGetPluginWithName(name);
		_DBPluginSetCurrentPlugin(plugin);
		CFStringRef usage = plugin->usage();
		cfprintf(stderr, "\t%@ %@\n", name, usage);
		CFRelease(usage);
	}
}

const DBPlugin* DBGetPluginWithName(CFStringRef name) {
	const void* plugin = CFDictionaryGetValue(plugins, name);
	return (DBPlugin*)plugin;
}

int run_plugin(int argc, char* argv[]) {
	int res = -1;
	int i;
	if (argc < 1) return -1;
	CFStringRef name = cfstr(argv[0]);
	CFMutableArrayRef args = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (i = 1; i < argc; ++i) {
		CFArrayAppendValue(args, cfstr(argv[i]));
	}
	const DBPlugin* plugin = DBGetPluginWithName(name);
	if (plugin) {
		_DBPluginSetCurrentPlugin(plugin);
		res = plugin->run(args);
	}
	CFRelease(name);
	return res;
}

static CFStringRef currentBuild = NULL;

void DBSetCurrentBuild(char* build) {
	if (currentBuild) CFRelease(currentBuild);
	currentBuild = cfstr(build);
}

CFStringRef DBGetCurrentBuild() {
	return currentBuild;
}


int DBPluginPropertyDefaultRun(CFArrayRef argv) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	assert(plugin != NULL);
	assert(plugin->name != NULL);

	CFStringRef build = DBGetCurrentBuild();
	CFIndex argc = CFArrayGetCount(argv);
	CFStringRef project = (argc > 0) ? CFArrayGetValueAtIndex(argv, 0) : NULL;
	
	// kDBPluginProjectPropertyType must have project argument,
	// kDBPluginBuildPropertyType must not have project argument,
	// kDBPluginPropertyType may have project argument.
	if (plugin->type == kDBPluginProjectPropertyType && argc != 1) return -1;
	if (plugin->type == kDBPluginBuildPropertyType && argc != 0) return -1;
	if (plugin->type == kDBPluginPropertyType && argc != 0 && argc != 1) return -1;

	if (plugin->datatype == CFStringGetTypeID()) {
		CFStringRef value = DBCopyPropString(build, project, plugin->name);
		// kDBPluginPropertyType: if no value in project, look in build.
		if (!value && project) value = DBCopyPropString(build, NULL, plugin->name);
		if (value) cfprintf(stdout, "%@\n", value);

	} else if (plugin->datatype == CFArrayGetTypeID()) {
		CFArrayRef value = DBCopyPropArray(build, project, plugin->name);
		CFIndex i, count = value ? CFArrayGetCount(value) : 0;
		// kDBPluginPropertyType: if no value in project, look in build.
		if ((!value || !count) && project) {
			value = DBCopyPropArray(build, NULL, plugin->name);
			count = value ? CFArrayGetCount(value) : 0;
		}
		for (i = 0; i < count; ++i) {
			cfprintf(stdout, "%@\n", CFArrayGetValueAtIndex(value, i));
		}

	} else {
		fprintf(stderr, "internal error: no default handler for CFDictionary type\n");
		return -1;
	}
	return 0;
}

CFStringRef DBPluginPropertyDefaultUsage() {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();
	assert(plugin != NULL);
	// kDBPluginProjectPropertyType must have project argument,
	// kDBPluginBuildPropertyType must not have project argument,
	// kDBPluginPropertyType may have project argument.
	if (plugin->type == kDBPluginProjectPropertyType) return CFRetain(CFSTR("<project>"));
	if (plugin->type == kDBPluginBuildPropertyType) return CFRetain(CFSTR(""));
	if (plugin->type == kDBPluginPropertyType) return CFRetain(CFSTR("[<project>]"));
}
