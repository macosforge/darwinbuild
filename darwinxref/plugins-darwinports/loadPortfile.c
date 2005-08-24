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
#include "DBPluginPriv.h"
#include <sys/stat.h>
#include <libgen.h>
#include <tcl.h>
#include <unistd.h>

// XXX
extern CFStringRef cfstr_tcl(Tcl_Obj* obj);
extern Tcl_Obj* tcl_cfstr(CFStringRef str);
extern Tcl_Obj* tcl_cfarray(CFArrayRef);
static CFDataRef archivePortfile(const char* filename);

char *DefaultsTrace(ClientData clientData, Tcl_Interp *interp, const char *name1, const char *name2, int flags) {
	const char* expr = (const char*)clientData;
	// Only set the default value if the variable has not been explicitly set.
	if (Tcl_GetVar2(interp, name1, name2, TCL_GLOBAL_ONLY) == NULL) {
		Tcl_VarEval(interp, "return ", expr, NULL);
		Tcl_Obj* result = Tcl_GetObjResult(interp);
			CFStringRef str = cfstr_tcl(result);
//cfprintf(stderr, "DEFAULT: %s: %@\n", name1, str);
			CFRelease(str);
		Tcl_ObjSetVar2(interp, Tcl_NewStringObj(name1, strlen(name1)), NULL, result, TCL_GLOBAL_ONLY);
	}
	return NULL;
}

int UnknownCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	CFStringRef project = *((CFStringRef*)data);
	CFStringRef build = DBGetCurrentBuild();
	CFStringRef property = cfstr_tcl(objv[1]);

	//////
	// Special hacks
	//////
	if (CFEqual(property, CFSTR("name"))) {
		*((CFStringRef*)data) = cfstr_tcl(objv[2]);
		Tcl_ObjSetVar2(interp, objv[1], NULL, objv[2], TCL_GLOBAL_ONLY);
	}

	CFTypeID type = DBCopyPropType(property);
	if (type == CFStringGetTypeID()) {
		// concatenate all the remaining arguments into one string and store it.
		CFIndex i;
		CFMutableStringRef newstr = CFStringCreateMutable(NULL, 0);
		for (i = 2; i < objc; ++i) {
			CFStringRef str = cfstr_tcl(objv[i]);
			if (i > 2) CFStringAppend(newstr, CFSTR(" "));
			CFStringAppend(newstr, str);
			CFRelease(str);
		}
		DBSetPropString(build, project, property, newstr);
		Tcl_Obj* tcl_newstr = tcl_cfstr(newstr);
		Tcl_ObjSetVar2(interp, objv[1], NULL, tcl_newstr, TCL_GLOBAL_ONLY);
		CFRelease(newstr);
//cfprintf(stderr, "PROPERTY: %@\n", property);
	} else if (type == CFArrayGetTypeID()) {
		// add each remaining argument as a new string in an array
		CFIndex i;
		CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (i = 2; i < objc; ++i) {
			CFStringRef str = cfstr_tcl(objv[i]);
			CFArrayAppendValue(array, str);
			CFRelease(str);
		}
		DBSetPropArray(build, project, property, array);
		Tcl_Obj* tcl_array = tcl_cfarray(array);
		Tcl_ObjSetVar2(interp, objv[1], NULL, tcl_array, TCL_GLOBAL_ONLY);
		CFRelease(array);
//cfprintf(stderr, "PROPERTY: %@\n", property);
	} else if (type == CFDictionaryGetTypeID()) {
		// XXX: not yet supported
	} else {
//cfprintf(stderr, "SKIPPED: %@\n", property);
	}
	return TCL_OK;
}

#define DEFAULT(interp, name, value) Tcl_TraceVar2((interp), (name), NULL, TCL_TRACE_READS | TCL_GLOBAL_ONLY, DefaultsTrace, (ClientData)(value))

static int run(CFArrayRef argv) {
	int res = 0;
	CFStringRef project = NULL;
	CFIndex count = CFArrayGetCount(argv);
	if (count != 1)  return -1;
	char* filename = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));

	Tcl_Interp* interp = Tcl_CreateInterp();

	Tcl_CreateObjCommand(interp, "unknown", UnknownCmd, (ClientData)&project, (Tcl_CmdDeleteProc *)NULL);

	DEFAULT(interp, "portdbpath", "/opt/local/var/db/dports");
	DEFAULT(interp, "portbuildpath", "[file join $portdbpath build XXX-$name]");


	DEFAULT(interp, "portname", "$name");
	DEFAULT(interp, "portversion", "$version");
	DEFAULT(interp, "portrevision", "$revision");
	DEFAULT(interp, "portepoch", "$epoch");

	DEFAULT(interp, "revision", "0");
	DEFAULT(interp, "epoch", "0");

	DEFAULT(interp, "prefix", "/opt/local");
	DEFAULT(interp, "x11prefix", "/usr/X11R6");
	DEFAULT(interp, "workdir", "work");
	DEFAULT(interp, "workpath", "[file join $portbuildpath $workdir]");
	DEFAULT(interp, "worksrcdir", "$distname");
	DEFAULT(interp, "worksrcpath", "[file join $workpath $worksrcdir]");

	
	DEFAULT(interp, "destpath", "${workpath}/${destdir}");
	DEFAULT(interp, "destroot", "$destpath");
	DEFAULT(interp, "destdir", "destroot");
	DEFAULT(interp, "distname", "${name}-${version}");
	DEFAULT(interp, "extract.suffix", ".tar.gz");

	DBBeginTransaction();
	if (Tcl_EvalFile(interp, filename) != TCL_OK) {
		DBRollbackTransaction();
		Tcl_Obj* result = Tcl_GetObjResult(interp);
		CFStringRef str = cfstr_tcl(result);
		cfprintf(stderr, "Tcl error in %s: %@\n", filename, str);
		CFRelease(str);
	} else {
		CFDataRef data = archivePortfile(filename);
		DBSetPropData(DBGetCurrentBuild(), project, CFSTR("Portfile"), data);
		CFRelease(data);
		DBCommitTransaction();
	}
	free(filename);
	return res;
}

static CFStringRef usage() {
	return CFRetain(CFSTR("<Portfile>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("loadPortfile"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}

extern char** environ;
static CFDataRef archivePortfile(const char* filename) {
	const char* portdir = dirname(filename);
	pid_t pid;
	int status;
	int fds[2];
	CFMutableDataRef result = CFDataCreateMutable(NULL, 0);
	assert(pipe(fds) != -1);
	
	pid = fork();
	assert(pid != -1);
	if (pid == 0) {
		close(fds[0]);
		assert(dup2(fds[1], STDOUT_FILENO) != -1);
		const char* args[] = {
			"/usr/bin/tar",
			"czvf", "-",
			"--directory", portdir,
			"--exclude", "CVS",
			"--exclude", "work",
			".",
			NULL
		};
		assert(execve(args[0], (char**)args, environ) != -1);
		// NOT REACHED
	}
	close(fds[1]);
	
	for (;;) {
		char buf[BUFSIZ];
		int bytes = read(fds[0], buf, BUFSIZ);
		if (bytes == -1) {
			if (errno == EINTR) {
				// interrupted by signal (SIGCHLD), try again
				continue;
			} else {
				// error occurred
				CFRelease(result);
				result = NULL;
				break;
			}
		} else if (bytes == 0) {
			// EOF
			break;
		} else {
			// valid data
			CFDataAppendBytes(result, (UInt8*)buf, bytes);
		}
	}

	close(fds[0]);
	waitpid(pid, &status, 0);
	if (status != 0) {
		CFRelease(result);
		result = NULL;
	}
	
	return result;
}
