/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 * Portions copyright (c) 2003 Kevin Van Vechten <kevin@opendarwin.org>
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


#if HAVE_TCL_PLUGINS

#include "DBPluginPriv.h"

#include <tcl.h>

CFStringRef cfstr_tcl(Tcl_Obj* obj) {
	CFStringRef result = NULL;
	int length;
	char* buf = Tcl_GetStringFromObj(obj, &length);
	if (buf) {
		result = CFStringCreateWithBytes(NULL, buf, length, kCFStringEncodingUTF8, 0);
	}
	return result;
}

Tcl_Obj* tcl_cfstr(CFStringRef cf) {
	Tcl_Obj* tcl_result = NULL;
	if (cf) {
		CFIndex length = CFStringGetLength(cf);
		CFIndex size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUnicode);
		UniChar* buffer = (UniChar*)Tcl_Alloc(size);
		if (buffer) {
			CFStringGetCharacters(cf, CFRangeMake(0, length), buffer);
			tcl_result = Tcl_NewUnicodeObj(buffer, length);
			Tcl_Free((char*)buffer);
		}
	}
	return tcl_result;
}


int DBPluginSetNameCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "name");
		return TCL_ERROR;
	}

	CFStringRef str = cfstr_tcl(objv[1]);
	DBPlugin* plugin = (DBPlugin*)data;
	plugin->name = str;
	return TCL_OK;
}

int DBPluginSetTypeCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "type");
		return TCL_ERROR;
	}
	int length;
	char* type = Tcl_GetStringFromObj(objv[1], &length);
	DBPlugin* plugin = (DBPlugin*)data;
	if (strcmp(type, "basic") == 0) {
		plugin->type = kDBPluginBasicType | kDBPluginTclType;
	} else if (strcmp(type, "property") == 0) {
		plugin->type = kDBPluginPropertyType | kDBPluginTclType;
	} else {
			Tcl_AppendResult(interp, "Unknown type: ", type, NULL);
			return TCL_ERROR;
	}
	return TCL_OK;
}


int load_tcl_plugin(DBPlugin* plugin, const char* filename) {
	// Create a plugin object
	Tcl_Interp* interp = Tcl_CreateInterp();

	plugin->interp = (DBPluginRunFunc)interp;

	// Register our plugin callback
	Tcl_CreateObjCommand(interp, "DBPluginSetName", DBPluginSetNameCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBPluginSetType", DBPluginSetTypeCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);

	// Source the plugin file
	Tcl_EvalFile(interp, filename);

	return 0;
}

CFStringRef call_tcl_usage(DBPlugin* plugin) {
	Tcl_Eval(plugin->interp, "usage");
	Tcl_Obj* res = Tcl_GetObjResult(plugin->interp);
	return cfstr_tcl(res);
}

int call_tcl_run(DBPlugin* plugin, CFArrayRef args) {
	// XXX: need to pass args
	Tcl_Eval(plugin->interp, "run");
	Tcl_Obj* res = Tcl_GetObjResult(plugin->interp);
	return 0;
}

#endif /* HAVE_TCL_PLUGINS */