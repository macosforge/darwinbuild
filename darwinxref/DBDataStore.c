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
#include "cfutils.h"
#include "sqlite3.h"

extern const void*  DBGetPluginWithName(CFStringRef);

int DBBeginTransaction(void* session);
int DBRollbackTransaction(void* session);
int DBCommitTransaction(void* session);


//////
//
// sqlite3 utility functions
//
//////

#define __SQL(db, callback, context, fmt) \
	va_list args; \
	char* errmsg; \
	va_start(args, fmt); \
	char *query = sqlite3_vmprintf(fmt, args); \
	res = sqlite3_exec(db, query, callback, context, &errmsg); \
	if (res != SQLITE_OK) { \
		fprintf(stderr, "Error: %s (%d)\n  SQL: %s\n", errmsg, res, query); \
	} \
	sqlite3_free(query);

int SQL(sqlite3* db, const char* fmt, ...) {
	int res;
	__SQL(db, NULL, NULL, fmt);
	return res;
}

int SQL_CALLBACK(sqlite3* db, sqlite3_callback callback, void* context, const char* fmt, ...) {
	int res;
	__SQL(db, callback, context, fmt);
	return res;
}

static int isTrue(void* pArg, int argc, char **argv, char** columnNames) {
	*(int*)pArg = (strcmp(argv[0], "1") == 0);
	return 0;
}

int SQL_BOOLEAN(sqlite3* db, const char* fmt, ...) {
	int res;
	int val = 0;
	__SQL(db, isTrue, &val, fmt);
	return val;
}

static int getString(void* pArg, int argc, char **argv, char** columnNames) {
	if (*(char**)pArg == NULL) {
		*(char**)pArg = strdup(argv[0]);
	}
	return 0;
}

char* SQL_STRING(sqlite3* db, const char* fmt, ...) {
	int res;
	char* str = 0;
	__SQL(db, getString, &str, fmt);
	return str;
}

CFStringRef SQL_CFSTRING(sqlite3* db, const char* fmt, ...) {
	int res;
	CFStringRef str = NULL;
	char* cstr = NULL;
	__SQL(db, getString, &cstr, fmt);
	str = cfstr(cstr);
	if (cstr) free(cstr);
	return str;
}

static int sqlAddStringToArray(void* pArg, int argc, char **argv, char** columnNames) {
	CFMutableArrayRef array = pArg;
	CFStringRef str = cfstr(argv[0]);
	if (str) {
		CFArrayAppendValue(array, str);
		CFRelease(str);
	}
	return 0;
}

CFArrayRef SQL_CFARRAY(sqlite3* db, const char* fmt, ...) {
	int res;
	CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	__SQL(db, sqlAddStringToArray, array, fmt);
	return array;
}

static int sqlAddValueToDictionary(void* pArg, int argc, char **argv, char** columnNames) {
	CFMutableDictionaryRef dict = pArg;
	CFStringRef name = cfstr(argv[0]);
	CFStringRef value = cfstr(argv[1]);
	if (!value) value = CFRetain(CFSTR(""));
	if (name) {
		CFTypeRef cf = CFDictionaryGetValue(dict, name);
		if (cf == NULL) {
			CFDictionarySetValue(dict, name, value);
		} else if (CFGetTypeID(cf) == CFStringGetTypeID()) {
			CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(array, cf);
			CFArrayAppendValue(array, value);
			CFDictionarySetValue(dict, name, array);
		} else if (CFGetTypeID(cf) == CFArrayGetTypeID()) {
			CFArrayAppendValue((CFMutableArrayRef)cf, value);
		}
		CFRelease(name);
	}
	CFRelease(value);
	return 0;
}

CFDictionaryRef SQL_CFDICTIONARY(sqlite3* db, const char* fmt, ...) {
	int res;
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	__SQL(db, sqlAddValueToDictionary, dict, fmt);
	return dict;
}

void SQL_NOERR(sqlite3* db, char* sql) {
	char* errmsg;
	int res = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if (res != SQLITE_OK && res != SQLITE_ERROR) {
		fprintf(stderr, "Error: %s (%d)\n", errmsg, res);
	} else {
		res = 0;
	}
}

void* DBDataStoreInitialize(const char* datafile) {
	sqlite3* db;
	sqlite3_open(datafile, &db);
	if (db == NULL) return NULL;

	char* table = "CREATE TABLE properties (build TEXT, project TEXT, property TEXT, key TEXT, value TEXT)";
	char* index = "CREATE INDEX properties_index ON properties (build, project, property, key, value)";
	SQL_NOERR(db, table);
	SQL_NOERR(db, index);

	return db; 
}

CFArrayRef DBCopyPropNames(void* session, CFStringRef build, CFStringRef project) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* sql;
	if (project) {
		sql = "SELECT DISTINCT property FROM properties WHERE build=%Q AND project=%Q ORDER BY property";
	} else {
		sql = "SELECT DISTINCT property FROM properties WHERE build=%Q AND project IS NULL ORDER BY property";
	}
	CFArrayRef res = SQL_CFARRAY(session, sql, cbuild, cproj);
	free(cbuild);
	free(cproj);
	return res;
}

CFArrayRef DBCopyProjectNames(void* session, CFStringRef build) {
	char* cbuild = strdup_cfstr(build);
	CFArrayRef res = SQL_CFARRAY(session, "SELECT DISTINCT project FROM properties WHERE build=%Q ORDER BY project", cbuild);
	free(cbuild);
	return res;
}

CFTypeID  DBCopyPropType(void* session, CFStringRef property) {
	CFTypeID type = -1;
	const DBPropertyPlugin* plugin = DBGetPluginWithName(property);
	if (plugin && plugin->base.type == kDBPluginPropertyType) {
		type = plugin->datatype;
	}
	return type;
}

CFTypeRef DBCopyProp(void* session, CFStringRef build, CFStringRef project, CFStringRef property) {
	CFTypeRef res = NULL;
	CFTypeID type = DBCopyPropType(session, property);
	if (type == -1) return NULL;

	if (type == CFStringGetTypeID()) {
		res = DBCopyPropString(session, build, project, property);
	} else if (type == CFArrayGetTypeID()) {
		res = DBCopyPropArray(session, build, project, property);
	} else if (type == CFDictionaryGetTypeID()) {
		res = DBCopyPropDictionary(session, build, project, property);
	}
	return res;
}

CFStringRef DBCopyPropString(void* session, CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql;
	if (project)
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project=%Q";
	else
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL";
	CFStringRef res = SQL_CFSTRING(session, sql, cprop, cbuild, cproj);
	free(cproj);
	free(cprop);
	free(cbuild);
	return res;
}

CFArrayRef DBCopyPropArray(void* session, CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql;
	if (project)
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project=%Q ORDER BY key";
	else
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL ORDER BY key";
	CFArrayRef res = SQL_CFARRAY(session, sql, cprop, cbuild, cproj);
	free(cproj);
	free(cprop);
	free(cbuild);
	return res;
}



CFDictionaryRef DBCopyPropDictionary(void* session, CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql;
	if (project)
		sql = "SELECT DISTINCT key,value FROM properties WHERE property=%Q AND build=%Q AND project=%Q ORDER BY key";
	else
		sql = "SELECT DISTINCT key,value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL ORDER BY key";
	CFDictionaryRef res = SQL_CFDICTIONARY(session, sql, cprop, cbuild, cproj);
	free(cbuild);
	free(cproj);
	free(cprop);
	return res;
}

int DBSetProp(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFTypeRef value) {
	int res = 0;
	CFTypeID type = DBCopyPropType(session, property);
	if (type == -1) {
		cfprintf(stderr, "Error: unknown property in project \"%@\": %@\n", project, property);
		return -1;
	}
	if (type != CFGetTypeID(value)) {
		CFStringRef expected = CFCopyTypeIDDescription(type);
		CFStringRef actual = CFCopyTypeIDDescription(CFGetTypeID(value));
		cfprintf(stderr, "Error: incorrect type for \"%@\" in project \"%@\": expected %@ but got %@\n", property, project, expected, actual);
		CFRelease(expected);
		CFRelease(actual);
		return -1;
	}

	if (type == CFStringGetTypeID()) {
		res = DBSetPropString(session, build, project, property, value);
	} else if (type == CFArrayGetTypeID()) {
		res = DBSetPropArray(session, build, project, property, value);
	} else if (type == CFDictionaryGetTypeID()) {
		res = DBSetPropDictionary(session, build, project, property, value);
	}
	return res;
}

int DBSetPropString(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFStringRef value) {
        char* cbuild = strdup_cfstr(build);
        char* cproj = strdup_cfstr(project);
        char* cprop = strdup_cfstr(property);
        char* cvalu = strdup_cfstr(value);
	if (project) {
		SQL(session, "DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
		SQL(session, "INSERT INTO properties (build,project,property,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cproj, cprop, cvalu);
	} else {
		SQL(session, "DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
		SQL(session, "INSERT INTO properties (build,property,value) VALUES (%Q, %Q, %Q)", cbuild, cprop, cvalu);
	}
	free(cbuild);
        free(cproj);
        free(cprop);
	free(cvalu);
	return 0;
}

int DBSetPropArray(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFArrayRef value) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	if (project) {
		SQL(session, "DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
	} else {
		SQL(session, "DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
	}
	CFIndex i, count = CFArrayGetCount(value);
	for (i = 0; i < count; ++i) {
		char* cvalu = strdup_cfstr(CFArrayGetValueAtIndex(value, i));
		if (project) {
			SQL(session, "INSERT INTO properties (build,project,property,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cproj, cprop, cvalu);
		} else {
			SQL(session, "INSERT INTO properties (build,property,value) VALUES (%Q, %Q, %Q)", cbuild, cprop, cvalu);
		}
		free(cvalu);
	}
	free(cbuild);
	free(cproj);
	free(cprop);
	return 0;
}

int DBSetPropDictionary(void* session, CFStringRef build, CFStringRef project, CFStringRef property, CFDictionaryRef value) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);

	CFArrayRef keys = dictionaryGetSortedKeys(value);
	CFIndex i, count = CFArrayGetCount(keys);
	for (i = 0; i < count; ++i) {
		CFStringRef key = CFArrayGetValueAtIndex(keys, i);
		char* ckey = strdup_cfstr(key);
		CFTypeRef cf = CFDictionaryGetValue(value, key);
		if (CFGetTypeID(cf) == CFStringGetTypeID()) {
			char* cvalu = strdup_cfstr(cf);
			if (project) {
				SQL(session, "INSERT INTO properties (build,project,property,key,value) VALUES (%Q, %Q, %Q, %Q, %Q)", cbuild, cproj, cprop, ckey, cvalu);
			} else {
				SQL(session, "INSERT INTO properties (build,property,key,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cprop, ckey, cvalu);
			}
			free(cvalu);
		} else if (CFGetTypeID(cf) == CFArrayGetTypeID()) {
			CFIndex j, count = CFArrayGetCount(cf);
			for (j = 0; j < count; ++j) {
				char* cvalu = strdup_cfstr(CFArrayGetValueAtIndex(cf, j));
				if (project) {
					SQL(session, "INSERT INTO properties (build,project,property,key,value) VALUES (%Q, %Q, %Q, %Q, %Q)", cbuild, cproj, cprop, ckey, cvalu);
				} else {
					SQL(session, "INSERT INTO properties (build,property,key,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cprop, ckey, cvalu);
				}
				free(cvalu);
			}
		}
		free(ckey);
	}
	CFRelease(keys);
	free(cbuild);
	free(cproj);
	free(cprop);
	return 0;
}


CFDictionaryRef DBCopyProjectPlist(void* session, CFStringRef build, CFStringRef project) {
	CFMutableDictionaryRef res = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef props = DBCopyPropNames(session, build, project);
	CFIndex i, count = CFArrayGetCount(props);
	for (i = 0; i < count; ++i) {
		CFStringRef prop = CFArrayGetValueAtIndex(props, i);
		CFTypeRef value = DBCopyProp(session, build, project, prop);
		if (value) {
			CFDictionaryAddValue(res, prop, value);
			CFRelease(value);
		}
	}
	return res;
}


CFDictionaryRef DBCopyBuildPlist(void* session, CFStringRef build) {
	CFMutableDictionaryRef plist = (CFMutableDictionaryRef)DBCopyProjectPlist(session, build, NULL);
	CFMutableDictionaryRef projects = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef names = DBCopyProjectNames(session, build);
	CFIndex i, count = CFArrayGetCount(names);
	for (i = 0; i < count; ++i) {
		CFStringRef name = CFArrayGetValueAtIndex(names, i);
		CFDictionaryRef proj = DBCopyProjectPlist(session, build, name);
		CFDictionaryAddValue(projects, name, proj);
		CFRelease(proj);
	}
	CFDictionarySetValue(plist, CFSTR("build"), build);
	CFDictionarySetValue(plist, CFSTR("projects"), projects);
	return plist;
}


int _DBSetPlist(void* session, CFStringRef buildParam, CFStringRef projectParam, CFPropertyListRef plist) {
	int res = 0;
	CFIndex i, count;
	
	if (!plist) return -1;
	if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) return -1;

	CFStringRef build = CFDictionaryGetValue(plist, CFSTR("build"));
	if (!build) build = buildParam;
	CFStringRef project = CFDictionaryGetValue(plist, CFSTR("name"));
	if (!project) project = projectParam;
	CFDictionaryRef projects = CFDictionaryGetValue(plist, CFSTR("projects"));
		
	if (projects && CFGetTypeID(projects) != CFDictionaryGetTypeID()) {
		fprintf(stderr, "Error: projects must be a dictionary.\n");
		return -1;
	}

	CFArrayRef props = dictionaryGetSortedKeys(plist);

	//
	// Delete any properties which may have been removed
	//
	CFArrayRef existingProps = DBCopyPropNames(session, build, project);
	count = CFArrayGetCount(props);
	CFRange range = CFRangeMake(0, count);
	CFIndex existingCount = CFArrayGetCount(existingProps);
	for (i = 0; i < existingCount; ++i) {
		CFStringRef prop = CFArrayGetValueAtIndex(existingProps, i);
		if (!CFArrayContainsValue(props, range, prop)) {
			char* cbuild = strdup_cfstr(build);
			char* cproj = strdup_cfstr(project);
			char* cprop = strdup_cfstr(prop);
			if (project) {
				char* cproj = strdup_cfstr(project);
				SQL(session, "DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
				free(cproj);
			} else {
				SQL(session, "DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
			}
			free(cbuild);
			free(cprop);
		}
	}
	CFRelease(existingProps);
	
	count = CFArrayGetCount(props);
	for (i = 0; i < count; ++i) {
		CFStringRef prop = CFArrayGetValueAtIndex(props, i);
		
		// These are more like pseudo-properties
		if (CFEqual(prop, CFSTR("build")) || CFEqual(prop, CFSTR("projects"))) continue;
		
		CFTypeRef value = CFDictionaryGetValue(plist, prop);
		res = DBSetProp(session, build, project, prop, value);
		if (res != 0) break;
	}
	
	if (res == 0 && projects) {
		CFArrayRef projectNames = dictionaryGetSortedKeys(projects);
		count = CFArrayGetCount(projectNames);
		for (i = 0; i < count; ++i ) {
			CFStringRef project = CFArrayGetValueAtIndex(projectNames, i);
			CFDictionaryRef subplist = CFDictionaryGetValue(projects, project);
			res = _DBSetPlist(session, build, project, subplist);
			if (res != 0) break;
		}
		CFRelease(projectNames);
	}
	if (props) CFRelease(props);
	return res;
}

int DBSetPlist(void* session, CFStringRef buildParam, CFPropertyListRef plist) {
	int res;
	res = DBBeginTransaction(session);
	if (res != 0) return res;
	res = _DBSetPlist(session, buildParam, NULL, plist);
	if (res != 0) {
		DBRollbackTransaction(session);
		return res;
	}
	DBCommitTransaction(session);
	return res;
}


int DBBeginTransaction(void* session) {
	return SQL(session, "BEGIN");
}
int DBRollbackTransaction(void* session) {
	return SQL(session, "ROLLBACK");
}
int DBCommitTransaction(void* session) {
	return SQL(session, "COMMIT");
}


