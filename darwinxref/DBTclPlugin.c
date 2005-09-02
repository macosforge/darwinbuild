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
		result = CFStringCreateWithBytes(NULL, (UInt8*)buf, length, kCFStringEncodingUTF8, 0);
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

Tcl_Obj* tcl_cfdata(CFDataRef cf) {
	Tcl_Obj* tcl_result = NULL;
	if (cf) {
		CFIndex length = CFDataGetLength(cf);
		unsigned char* buffer = (unsigned char*)Tcl_Alloc(length);
		if (buffer) {
			CFDataGetBytes(cf, CFRangeMake(0, length), (UInt8*)buffer);
			tcl_result = Tcl_NewByteArrayObj(buffer, length);
			Tcl_Free((char*)buffer);
		}
	}
	return tcl_result;
}


Tcl_Obj* tcl_cfarray(CFArrayRef array) {
	Tcl_Obj** objv;
	int i, objc = CFArrayGetCount(array);
	objv = (Tcl_Obj**)malloc(sizeof(Tcl_Obj*) * objc);
	for (i = 0; i < objc; ++i) {
		CFStringRef str = CFArrayGetValueAtIndex(array, i);
		assert(CFGetTypeID(str) == CFStringGetTypeID());
		objv[i] = tcl_cfstr(str);
	}
	Tcl_Obj* list = Tcl_NewListObj(objc, objv);
	return list;
}

CFArrayRef cfarray_tcl(Tcl_Interp* interp, Tcl_Obj* list) {
	CFArrayRef result = NULL;
	Tcl_Obj** objv;
	int i, objc;
	if (Tcl_ListObjGetElements(interp, list, &objc, &objv) == TCL_OK) {
		const void** array = malloc(sizeof(CFStringRef) * objc);
		for (i = 0; i < objc; ++i) {
			array[i] = cfstr_tcl(objv[i]);
		}
		result = CFArrayCreate(NULL, array, objc, &kCFTypeArrayCallBacks);
		free(array);
	}
	return result;
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
		plugin->type = kDBPluginBasicType;
	} else if (strcmp(type, "property") == 0) {
		plugin->type = kDBPluginPropertyType;
	} else if (strcmp(type, "property.project") == 0) {
		plugin->type = kDBPluginProjectPropertyType;
	} else if (strcmp(type, "property.build") == 0) {
		plugin->type = kDBPluginBuildPropertyType;
	} else {
			Tcl_AppendResult(interp, "Unknown type: ", type, NULL);
			return TCL_ERROR;
	}
	return TCL_OK;
}

int DBPluginSetDatatypeCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "datatype");
		return TCL_ERROR;
	}
	int length;
	char* type = Tcl_GetStringFromObj(objv[1], &length);
	DBPlugin* plugin = (DBPlugin*)data;
	if (strcmp(type, "string") == 0) {
		plugin->datatype = CFStringGetTypeID();
	} else if (strcmp(type, "data") == 0) {
		plugin->datatype = CFDataGetTypeID();
	} else if (strcmp(type, "array") == 0) {
		plugin->datatype = CFArrayGetTypeID();
	} else if (strcmp(type, "dictionary") == 0) {
		plugin->datatype = CFDictionaryGetTypeID();
	} else {
			Tcl_AppendResult(interp, "Unknown type: ", type, NULL);
			return TCL_ERROR;
	}
	return TCL_OK;
}

int DBGetCurrentBuildCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, tcl_cfstr(DBGetCurrentBuild()));
	return TCL_OK;
}


int DBSetPropStringCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 5) {
		Tcl_WrongNumArgs(interp, 1, objv, "build project property value");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFStringRef project = cfstr_tcl(objv[2]);
	CFStringRef property = cfstr_tcl(objv[3]);
	CFStringRef value = cfstr_tcl(objv[4]);
	DBSetPropString(build, project, property, value);
	return TCL_OK;
}

int DBCopyPropStringCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "build project property");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFStringRef project = cfstr_tcl(objv[2]);
	CFStringRef property = cfstr_tcl(objv[3]);
	CFStringRef str = DBCopyPropString(build, project, property);
	if (str) {
		Tcl_SetObjResult(interp, tcl_cfstr(str));
		CFRelease(str);
	}
	return TCL_OK;
}

int DBCopyPropDataCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "build project property");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFStringRef project = cfstr_tcl(objv[2]);
	CFStringRef property = cfstr_tcl(objv[3]);
	CFDataRef res = DBCopyPropData(build, project, property);
	if (data) {
		Tcl_SetObjResult(interp, tcl_cfdata(res));
		CFRelease(res);
	}
	return TCL_OK;
}


int DBSetPropArrayCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 5) {
		Tcl_WrongNumArgs(interp, 1, objv, "build project property list");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFStringRef project = cfstr_tcl(objv[2]);
	CFStringRef property = cfstr_tcl(objv[3]);
	CFArrayRef  list = cfarray_tcl(interp, objv[4]);
	DBSetPropArray(build, project, property, list);
	return TCL_OK;
}

int DBCopyPropArrayCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "build project property");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFStringRef project = cfstr_tcl(objv[2]);
	CFStringRef property = cfstr_tcl(objv[3]);
	CFArrayRef array = DBCopyPropArray(build, project, property);
	if (array) {
		Tcl_SetObjResult(interp, tcl_cfarray(array));
		CFRelease(array);
	}
	return TCL_OK;
}

int DBBeginTransactionCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}
	DBBeginTransaction();
	return TCL_OK;
}

int DBCommitTransactionCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}
	DBCommitTransaction();
	return TCL_OK;
}

int DBRollbackTransactionCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "");
		return TCL_ERROR;
	}
	DBRollbackTransaction();
	return TCL_OK;
}

int DBCopyProjectNamesCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "build");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFArrayRef array = DBCopyProjectNames(build);
	if (array) {
		Tcl_SetObjResult(interp, tcl_cfarray(array));
		CFRelease(array);
	}
	return TCL_OK;
}

int DBCopyChangedProjectNamesCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "oldbuild newbuild");
		return TCL_ERROR;
	}

	CFStringRef oldbuild = cfstr_tcl(objv[1]);
	CFStringRef newbuild = cfstr_tcl(objv[2]);
	CFArrayRef array = DBCopyChangedProjectNames(oldbuild, newbuild);
	if (array) {
		Tcl_SetObjResult(interp, tcl_cfarray(array));
		CFRelease(array);
	}
	return TCL_OK;
}

int DBCopyGroupNamesCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "build");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFArrayRef array = DBCopyGroupNames(build);
	if (array) {
		Tcl_SetObjResult(interp, tcl_cfarray(array));
		CFRelease(array);
	}
	CFRelease(build);
	return TCL_OK;
}

int DBCopyGroupMembersCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "build group");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFStringRef group = cfstr_tcl(objv[2]);
	CFArrayRef array = DBCopyGroupMembers(build, group);
	if (array) {
		Tcl_SetObjResult(interp, tcl_cfarray(array));
		CFRelease(array);
	}
	CFRelease(build);
	CFRelease(group);
	return TCL_OK;
}

int DBSetGroupMembersCmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* CONST objv[]) {
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "build group list");
		return TCL_ERROR;
	}

	CFStringRef build = cfstr_tcl(objv[1]);
	CFStringRef group = cfstr_tcl(objv[2]);
	CFArrayRef  members = cfarray_tcl(interp, objv[3]);
	DBSetGroupMembers(build, group, members);
	CFRelease(build);
	CFRelease(group);
	CFRelease(members);
	return TCL_OK;
}




int load_tcl_plugin(DBPlugin* plugin, const char* filename) {
	Tcl_Interp* interp = Tcl_CreateInterp();

	plugin->usage = &_DBPluginTclUsage;
	plugin->run = &_DBPluginTclRun;
	plugin->interp = interp;

	// Register our plugin callbacks
	Tcl_CreateObjCommand(interp, "DBPluginSetName", DBPluginSetNameCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBPluginSetType", DBPluginSetTypeCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBPluginSetDatatype", DBPluginSetDatatypeCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBGetCurrentBuild", DBGetCurrentBuildCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBSetPropString", DBSetPropStringCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBCopyPropString", DBCopyPropStringCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBCopyPropData", DBCopyPropDataCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBSetPropArray", DBSetPropArrayCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBCopyPropArray", DBCopyPropArrayCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateObjCommand(interp, "DBBeginTransaction", DBBeginTransactionCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBCommitTransaction", DBCommitTransactionCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBRollbackTransaction", DBRollbackTransactionCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateObjCommand(interp, "DBCopyProjectNames", DBCopyProjectNamesCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBCopyChangedProjectNames", DBCopyChangedProjectNamesCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);

	Tcl_CreateObjCommand(interp, "DBCopyGroupNames", DBCopyGroupNamesCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBCopyGroupMembers", DBCopyGroupMembersCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);
	Tcl_CreateObjCommand(interp, "DBSetGroupMembers", DBSetGroupMembersCmd, (ClientData)plugin, (Tcl_CmdDeleteProc *)NULL);

	// Source the plugin file
	Tcl_EvalFile(interp, filename);

	return 0;
}

CFStringRef _DBPluginTclUsage() {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();

	// Test if the 'usage' proc exists, if not, use the default handler.
	if (Tcl_Eval(plugin->interp, "info commands usage") == TCL_OK) {
		const char* result = Tcl_GetStringResult(plugin->interp);
		if (result && strcmp(result, "usage") != 0) {
			return DBPluginPropertyDefaultUsage();
		}
	}

	Tcl_Eval(plugin->interp, "usage");
	Tcl_Obj* res = Tcl_GetObjResult(plugin->interp);
	return cfstr_tcl(res);
}

int _DBPluginTclRun(CFArrayRef args) {
	DBPlugin* plugin = _DBPluginGetCurrentPlugin();

	// Test if the 'run' proc exists, if not, use the default handler.
	if (Tcl_Eval(plugin->interp, "info commands run") == TCL_OK) {
		const char* result = Tcl_GetStringResult(plugin->interp);
		if (result && strcmp(result, "run") != 0) {
			return DBPluginPropertyDefaultRun(args);
		}
	}
	
	Tcl_Obj* tcl_args = tcl_cfarray(args);
	Tcl_Obj* varname = tcl_cfstr(CFSTR("__args__"));
	Tcl_ObjSetVar2(plugin->interp, varname, NULL, tcl_args, TCL_GLOBAL_ONLY);
	int exitCode = -1;
	(void)Tcl_Eval(plugin->interp, "fconfigure stdout -translation lf");
	if (Tcl_Eval(plugin->interp, "eval run ${__args__}") == TCL_OK) {
		Tcl_Obj* result = Tcl_GetObjResult(plugin->interp);
		if (Tcl_GetCharLength(result) == 0) {
			exitCode = 0;
		} else {
			Tcl_GetIntFromObj(plugin->interp, result, &exitCode);
		}
	} else {
		Tcl_Obj* result = Tcl_GetObjResult(plugin->interp);
		CFStringRef str = cfstr_tcl(result);
		cfprintf(stderr, "Tcl error in \'%@\' plugin: %@\n", plugin->name, str);
		CFRelease(str);
	}
	return exitCode;
}

#endif /* HAVE_TCL_PLUGINS */
