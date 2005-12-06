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
#include "cfutils.h"
#include "sqlite3.h"

//////
// NOT THREAD SAFE
// We currently operate under the assumption that there is only
// one thread, with no plugin re-entrancy.
//////
int __nestedTransactions = 0;
void* __DBDataStore;
void* _DBPluginGetDataStorePtr() {
	return __DBDataStore;
}


//////
//
// sqlite3 utility functions
//
//////

#define __SQL(callback, context, fmt) \
	va_list args; \
	char* errmsg; \
	va_start(args, fmt); \
	sqlite3* db = _DBPluginGetDataStorePtr(); \
	if (db) { \
		char *query = sqlite3_vmprintf(fmt, args); \
		res = sqlite3_exec(db, query, callback, context, &errmsg); \
		if (res != SQLITE_OK) { \
			fprintf(stderr, "Error: %s (%d)\n  SQL: %s\n", errmsg, res, query); \
		} \
		sqlite3_free(query); \
	} else { \
		fprintf(stderr, "Error: database not open.\n"); \
		res = SQLITE_ERROR; \
	} \
	va_end(args);

int SQL(const char* fmt, ...) {
	int res;
	__SQL(NULL, NULL, fmt);
	return res;
}

int SQL_CALLBACK(sqlite3_callback callback, void* context, const char* fmt, ...) {
	int res;
	__SQL(callback, context, fmt);
	return res;
}

static int isTrue(void* pArg, int argc, char **argv, char** columnNames) {
	*(int*)pArg = (strcmp(argv[0], "1") == 0);
	return 0;
}

int SQL_BOOLEAN(const char* fmt, ...) {
	int res;
	int val = 0;
	__SQL(isTrue, &val, fmt);
	return val;
}

static int getString(void* pArg, int argc, char **argv, char** columnNames) {
	if (*(char**)pArg == NULL) {
		*(char**)pArg = strdup(argv[0]);
	}
	return 0;
}

char* SQL_STRING(const char* fmt, ...) {
	int res;
	char* str = 0;
	__SQL(getString, &str, fmt);
	return str;
}

CFStringRef SQL_CFSTRING(const char* fmt, ...) {
	int res;
	CFStringRef str = NULL;
	char* cstr = NULL;
	__SQL(getString, &cstr, fmt);
	str = cfstr(cstr);
	if (cstr) free(cstr);
	return str;
}

CFDataRef SQL_CFDATA(const char* fmt, ...) {
	int res;
	CFDataRef data = NULL;
	sqlite3_stmt* stmt = NULL;
	va_list args;
	char* errmsg;
	va_start(args, fmt);
	sqlite3* db = _DBPluginGetDataStorePtr();
	if (db) {
		char *query = sqlite3_vmprintf(fmt, args);
		sqlite3_prepare(db, query, -1, &stmt, NULL);
		res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			const void* buf = sqlite3_column_blob(stmt, 0);
			data = CFDataCreate(NULL, buf, sqlite3_column_bytes(stmt, 0));
		}
		sqlite3_finalize(stmt);
	}
	return data;
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

CFArrayRef SQL_CFARRAY(const char* fmt, ...) {
	int res;
	CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	__SQL(sqlAddStringToArray, array, fmt);
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
		  CFDictionaryAddValue(dict, name, value);
		}
		CFRelease(name);
	}
	CFRelease(value);
	return 0;
}

static int sqlAddArrayValueToDictionary(void* pArg, int argc, char **argv, char** columnNames) {
	CFMutableDictionaryRef dict = pArg;
	CFStringRef name = cfstr(argv[0]);
	CFStringRef value = cfstr(argv[1]);
	if (!value) value = CFRetain(CFSTR(""));
	if (name) {
		CFTypeRef cf = CFDictionaryGetValue(dict, name);
		if (cf == NULL) {
			CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(array, value);
			CFDictionarySetValue(dict, name, array);
			CFRelease(array);
		} else if (CFGetTypeID(cf) == CFArrayGetTypeID()) {
			CFArrayAppendValue((CFMutableArrayRef)cf, value);
		}
		CFRelease(name);
	}
	CFRelease(value);
	return 0;
}

CFDictionaryRef SQL_CFDICTIONARY(const char* fmt, ...) {
	int res;
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	__SQL(sqlAddValueToDictionary, dict, fmt);
	return dict;
}

CFDictionaryRef SQL_CFDICTIONARY_OFCFARRAYS(const char* fmt, ...) {
	int res;
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	__SQL(sqlAddArrayValueToDictionary, dict, fmt);
	return dict;
}

void SQL_NOERR(char* sql) {
	char* errmsg;
	sqlite3* db = _DBPluginGetDataStorePtr();
	if (db) {
		int res = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
		if (res != SQLITE_OK && res != SQLITE_ERROR) {
			fprintf(stderr, "Error: %s (%d)\n", errmsg, res);
		}
	}
}

int DBDataStoreInitialize(const char* datafile) {
	int res = sqlite3_open(datafile, (sqlite3**)&__DBDataStore);
	if (res != SQLITE_OK) {
		sqlite3_close(__DBDataStore);
		__DBDataStore = NULL;
	}

	SQL_NOERR("CREATE TABLE properties (build TEXT, project TEXT, property TEXT, key TEXT, value TEXT)");
	SQL_NOERR("CREATE INDEX properties_index ON properties (build, project, property, key, value)");

	SQL_NOERR("CREATE TABLE groups (build TEXT, name TEXT, member TEXT)");
	SQL_NOERR("CREATE INDEX groups_index ON groups (build, name, member)");

	return 0;
}

int DBHasBuild(CFStringRef build) {
	char* cbuild = strdup_cfstr(build);
	const char* sql = "SELECT 1 FROM properties WHERE build=%Q LIMIT 1";
	return SQL_BOOLEAN(sql, cbuild);
}

CFArrayRef DBCopyBuilds() {
	const char* sql = "SELECT DISTINCT build FROM properties";
	return SQL_CFARRAY(sql);
}

CFArrayRef DBCopyBuildInheritance(CFStringRef param) {
	CFMutableArrayRef builds = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFStringRef build = param;
	do {
		CFArrayInsertValueAtIndex(builds, 0, build);
		if (build != param) CFRelease(build);
		build = DBCopyOnePropString(build, NULL, CFSTR("inherits"));
	} while (build != NULL);
	return builds;
}

CFArrayRef DBCopyPropNames(CFStringRef build, CFStringRef project) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* sql;
	if (project) {
		sql = "SELECT DISTINCT property FROM properties WHERE build=%Q AND project=%Q ORDER BY property";
	} else {
		sql = "SELECT DISTINCT property FROM properties WHERE build=%Q AND project IS NULL ORDER BY property";
	}
	CFArrayRef res = SQL_CFARRAY(sql, cbuild, cproj);
	free(cbuild);
	free(cproj);
	return res;
}

CFArrayRef DBCopyOneProjectNames(CFStringRef build) {
	char* origbuild = strdup_cfstr(build);

	CFMutableArrayRef projects = (CFMutableArrayRef)SQL_CFARRAY("SELECT DISTINCT project FROM properties WHERE build=%Q", origbuild);
	
	char* cbuild = strdup(origbuild);
	
	// also include any build aliases for these projects
	do {		
		CFArrayRef res = SQL_CFARRAY("SELECT DISTINCT project FROM properties WHERE build=%Q AND property='original' AND value IN (SELECT DISTINCT project FROM properties WHERE build=%Q)", cbuild, origbuild);
		free(cbuild);

		arrayAppendArrayDistinct(projects, res);
		CFRelease(res);

		build = DBCopyOnePropString(build, NULL, CFSTR("inherits"));
		cbuild = strdup_cfstr(build);
	} while (build != NULL);

	CFArraySortValues(projects, CFRangeMake(0, CFArrayGetCount(projects)), (CFComparatorFunction)CFStringCompare, 0);
	
	free(origbuild);
	return projects;
}

CFArrayRef DBCopyProjectNames(CFStringRef build) {
	CFMutableArrayRef projects = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	do {		
		char* cbuild = strdup_cfstr(build);
		CFArrayRef res = SQL_CFARRAY("SELECT DISTINCT project FROM properties WHERE build=%Q", cbuild);
		free(cbuild);

		arrayAppendArrayDistinct(projects, res);
		CFRelease(res);

		build = DBCopyOnePropString(build, NULL, CFSTR("inherits"));
	} while (build != NULL);

	CFArraySortValues(projects, CFRangeMake(0, CFArrayGetCount(projects)), (CFComparatorFunction)CFStringCompare, 0);

	return projects;
}


CFArrayRef DBCopyChangedProjectNames(CFStringRef oldbuild, CFStringRef newbuild) {
	char* coldbuild = strdup_cfstr(oldbuild);
	char* cnewbuild = strdup_cfstr(newbuild);
	CFArrayRef res = SQL_CFARRAY(
		"SELECT DISTINCT new.project AS project FROM properties AS new LEFT JOIN properties AS old "
			"ON (new.project=old.project AND new.property=old.property AND new.property='version') "
			"WHERE new.build=%Q AND old.build=%Q "
			"AND new.value<>old.value "
		"UNION "
		"SELECT DISTINCT project FROM properties "
			"WHERE build=%Q "
			"AND project NOT IN (SELECT project FROM properties WHERE build=%Q) "
		"ORDER BY project", cnewbuild, coldbuild, cnewbuild, coldbuild);
	free(coldbuild);
	free(cnewbuild);
	return res;
}


CFTypeID  DBCopyPropType(CFStringRef property) {
	CFTypeID type = -1;
	const DBPlugin* plugin = DBGetPluginWithName(property);
	if (plugin &&
		(plugin->type == kDBPluginPropertyType ||
		 plugin->type == kDBPluginBuildPropertyType ||
		 plugin->type == kDBPluginProjectPropertyType)) {
		type = plugin->datatype;
	}
	return type;
}

CFTypeID  DBCopyPropSubDictType(CFStringRef property) {
	CFTypeID type = -1;
	const DBPlugin* plugin = DBGetPluginWithName(property);
	if (plugin &&
		(plugin->type == kDBPluginPropertyType ||
		 plugin->type == kDBPluginBuildPropertyType ||
		 plugin->type == kDBPluginProjectPropertyType)) {
		type = plugin->subdictdatatype;
	}
	return type;
}

CFTypeRef DBCopyProp(CFStringRef build, CFStringRef project, CFStringRef property) {
	CFTypeRef res = NULL;
	CFTypeID type = DBCopyPropType(property);
	if (type == -1) return NULL;

	if (type == CFStringGetTypeID()) {
		res = DBCopyPropString(build, project, property);
	} else if (type == CFArrayGetTypeID()) {
		res = DBCopyPropArray(build, project, property);
	} else if (type == CFDictionaryGetTypeID()) {
		res = DBCopyPropDictionary(build, project, property);
	} else if (type == CFDataGetTypeID()) {
		res = DBCopyPropData(build, project, property);
	}
	return res;
}

CFTypeRef DBCopyOneProp(CFStringRef build, CFStringRef project, CFStringRef property) {
	CFTypeRef res = NULL;
	CFTypeID type = DBCopyPropType(property);
	if (type == -1) return NULL;

	if (type == CFStringGetTypeID()) {
		res = DBCopyOnePropString(build, project, property);
	} else if (type == CFArrayGetTypeID()) {
		res = DBCopyOnePropArray(build, project, property);
	} else if (type == CFDictionaryGetTypeID()) {
		res = DBCopyOnePropDictionary(build, project, property);
	} else if (type == CFDataGetTypeID()) {
		res = DBCopyOnePropData(build, project, property);
	}
	return res;
}


CFStringRef DBCopyOnePropString(CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql;
	if (cproj && *cproj != 0)
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project=%Q";
	else
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL";
	CFStringRef res = SQL_CFSTRING(sql, cprop, cbuild, cproj);
	free(cproj);
	free(cprop);
	free(cbuild);
	return res;
}

CFDataRef DBCopyOnePropData(CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql;
	if (cproj && *cproj != 0)
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project=%Q";
	else
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL";
	CFDataRef res = SQL_CFDATA(sql, cprop, cbuild, cproj);
	free(cproj);
	free(cprop);
	free(cbuild);
	return res;
}

CFArrayRef DBCopyOnePropArray(CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql;
	if (cproj && *cproj != 0)
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project=%Q ORDER BY key";
	else
		sql = "SELECT value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL ORDER BY key";
	CFArrayRef res = SQL_CFARRAY(sql, cprop, cbuild, cproj);
	if (res && CFArrayGetCount(res) == 0) {
		CFRelease(res);
		res = NULL;
	}
	free(cproj);
	free(cprop);
	free(cbuild);
	return res;
}

CFDictionaryRef DBCopyOnePropDictionary(CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	CFTypeID subtype = DBCopyPropSubDictType(property);
	char* sql;
	if (cproj && *cproj != 0)
		sql = "SELECT DISTINCT key,value FROM properties WHERE property=%Q AND build=%Q AND project=%Q ORDER BY key";
	else
		sql = "SELECT DISTINCT key,value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL ORDER BY key";
	CFDictionaryRef res;

	if(subtype == CFArrayGetTypeID())
	  res = SQL_CFDICTIONARY_OFCFARRAYS(sql, cprop, cbuild, cproj);
	else
	  res = SQL_CFDICTIONARY(sql, cprop, cbuild, cproj);

	if (res && CFDictionaryGetCount(res) == 0) {
		CFRelease(res);
		res = NULL;
	}
	free(cbuild);
	free(cproj);
	free(cprop);
	return res;
}

//
// Kluge to globally support build aliases ("original") and inheritance ("inherits") properties.
//
// ok, this is how this routine should work, even if it doesn't currently. -ssen
/*
  Let's say you have build 8A1, 8A2, and 8A3, and each inherits from the previous.
  Let's say you have "foo" and "foo_prime", where "foo_prime" is a build alias
  for "foo". If you're searching for "foo" in 8A3, look there first, and if you fail,
  walk up the build inheritance tree.
  
  8A1   foo   
  8A2   foo   foo_prime->foo
  8A3   foo   

  If you're looking for "foo_prime" in 8A3, we need to look up the entire
  build inheritance for an overriding property. Along the way, if we find
  that foo_prime was a build alias, and no property was found, restart the
  search with "foo"

  Once an alias is detected (like "foo_prime" in 8A2), "foo_prime" in
  8A1 must be an alias if it is present at all. A non-alias (without "original")
  is semantically a different project, and must abort the inheritance search
  for "foo_prime", although it can continue for "foo" in 8A3.

*/
CFTypeRef _DBCopyPropWithInheritance(CFStringRef build, CFStringRef project, CFStringRef property,
	CFTypeRef (*func)(CFStringRef, CFStringRef, CFStringRef)) {

	CFTypeRef res = NULL;

	CFMutableArrayRef builds = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(builds, build);

	CFStringRef original = NULL;

	// Look for the property in the current build and all inheritied builds
	// if a build alias ("original") is found, break out of the search path
	// and restart using the original name instead.
	do {
		res = func(build, project, property);
		if (res) break;
		
		original = DBCopyOnePropString(build, project, CFSTR("original"));
		if (original) break;

		CFStringRef inherits = DBCopyOnePropString(build, NULL, CFSTR("inherits"));
		if (inherits) CFArrayAppendValue(builds, inherits);
		build = inherits;
	} while (build != NULL);

	if (res == NULL) {
		CFIndex i, count = CFArrayGetCount(builds);
		for (i = 0; i < count; ++i) {
			build = CFArrayGetValueAtIndex(builds, i);
			res = func(build, original ? original : project, property);
			if (res) break;
		}
	}
	if (builds) CFRelease(builds);
	if (original) CFRelease(original);
	return res;
}


CFStringRef DBCopyPropString(CFStringRef build, CFStringRef project, CFStringRef property) {
	return (CFStringRef)_DBCopyPropWithInheritance(build, project, property, (void*)DBCopyOnePropString);
}

CFDataRef DBCopyPropData(CFStringRef build, CFStringRef project, CFStringRef property) {
	return (CFDataRef)_DBCopyPropWithInheritance(build, project, property, (void*)DBCopyOnePropData);
}

CFArrayRef DBCopyPropArray(CFStringRef build, CFStringRef project, CFStringRef property) {
	return (CFArrayRef)_DBCopyPropWithInheritance(build, project, property, (void*)DBCopyOnePropArray);
}

CFDictionaryRef DBCopyPropDictionary(CFStringRef build, CFStringRef project, CFStringRef property) {
	return (CFDictionaryRef)_DBCopyPropWithInheritance(build, project, property, (void*)DBCopyOnePropDictionary);
}


int DBSetProp(CFStringRef build, CFStringRef project, CFStringRef property, CFTypeRef value) {
	int res = 0;
	CFTypeID type = DBCopyPropType(property);
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
		res = DBSetPropString(build, project, property, value);
	} else if (type == CFArrayGetTypeID()) {
		res = DBSetPropArray(build, project, property, value);
	} else if (type == CFDictionaryGetTypeID()) {
		res = DBSetPropDictionary(build, project, property, value);
	} else if (type == CFDataGetTypeID()) {
		res = DBSetPropData(build, project, property, value);
	}
	return res;
}

int DBSetPropString(CFStringRef build, CFStringRef project, CFStringRef property, CFStringRef value) {
        char* cbuild = strdup_cfstr(build);
        char* cproj = strdup_cfstr(project);
        char* cprop = strdup_cfstr(property);
        char* cvalu = strdup_cfstr(value);
	if (project) {
		SQL("DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
		SQL("INSERT INTO properties (build,project,property,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cproj, cprop, cvalu);
	} else {
		SQL("DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
		SQL("INSERT INTO properties (build,property,value) VALUES (%Q, %Q, %Q)", cbuild, cprop, cvalu);
	}
	free(cbuild);
        free(cproj);
        free(cprop);
	free(cvalu);
	return 0;
}

int DBSetPropData(CFStringRef build, CFStringRef project, CFStringRef property, CFDataRef value) {
	sqlite3* db = _DBPluginGetDataStorePtr();
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql = NULL;
	sqlite3_stmt* stmt = NULL;
	int i = 1;
	int res;
	if (project) {
		SQL("DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
		sql = "INSERT INTO properties (build,project,property,value) VALUES (?, ?, ?, ?)";
	} else {
		SQL("DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
		sql = "INSERT INTO properties (build,property,value) VALUES (?, ?, ?)";
	}

	sqlite3_prepare(db, sql, -1, &stmt, NULL);
	sqlite3_bind_text(stmt, i++, cbuild, -1, NULL);
	if (project) sqlite3_bind_text(stmt, i++, cproj, -1, NULL);
	sqlite3_bind_text(stmt, i++, cprop, -1, NULL);
	sqlite3_bind_blob(stmt, i++, CFDataGetBytePtr(value), CFDataGetLength(value), NULL);
	res = sqlite3_step(stmt);
	if (res != SQLITE_DONE) fprintf(stderr, "%s:%d result = %d\n", __FILE__, __LINE__, res);
	sqlite3_finalize(stmt);

	free(cbuild);
	if (project) free(cproj);
	free(cprop);
	return 0;
}


int DBSetPropArray(CFStringRef build, CFStringRef project, CFStringRef property, CFArrayRef value) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	if (project) {
		SQL("DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
	} else {
		SQL("DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
	}
	CFIndex i, count = CFArrayGetCount(value);
	for (i = 0; i < count; ++i) {
		char* cvalu = strdup_cfstr(CFArrayGetValueAtIndex(value, i));
		if (project) {
			SQL("INSERT INTO properties (build,project,property,key,value) VALUES (%Q, %Q, %Q, %d, %Q)", cbuild, cproj, cprop, i, cvalu);
		} else {
			SQL("INSERT INTO properties (build,property,key,value) VALUES (%Q, %Q, %d, %Q)", cbuild, cprop, i, cvalu);
		}
		free(cvalu);
	}
	free(cbuild);
	free(cproj);
	free(cprop);
	return 0;
}

int DBSetPropDictionary(CFStringRef build, CFStringRef project, CFStringRef property, CFDictionaryRef value) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);

	// Delete all keys from the dictionary prior to insertion.
	if (project) {
		SQL("DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
	} else {
		SQL("DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
	}

	CFArrayRef keys = dictionaryGetSortedKeys(value);
	CFIndex i, count = CFArrayGetCount(keys);
	for (i = 0; i < count; ++i) {
		CFStringRef key = CFArrayGetValueAtIndex(keys, i);
		char* ckey = strdup_cfstr(key);
		CFTypeRef cf = CFDictionaryGetValue(value, key);

		if (CFGetTypeID(cf) == CFStringGetTypeID()) {
			char* cvalu = strdup_cfstr(cf);
			if (project) {
				SQL("INSERT INTO properties (build,project,property,key,value) VALUES (%Q, %Q, %Q, %Q, %Q)", cbuild, cproj, cprop, ckey, cvalu);
			} else {
				SQL("INSERT INTO properties (build,property,key,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cprop, ckey, cvalu);
			}
			free(cvalu);
		} else if (CFGetTypeID(cf) == CFArrayGetTypeID()) {
			CFIndex j, count = CFArrayGetCount(cf);
			for (j = 0; j < count; ++j) {
				char* cvalu = strdup_cfstr(CFArrayGetValueAtIndex(cf, j));
				if (project) {
					SQL("INSERT INTO properties (build,project,property,key,value) VALUES (%Q, %Q, %Q, %Q, %Q)", cbuild, cproj, cprop, ckey, cvalu);
				} else {
					SQL("INSERT INTO properties (build,property,key,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cprop, ckey, cvalu);
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


CFDictionaryRef DBCopyProjectPlist(CFStringRef build, CFStringRef project) {
	CFMutableDictionaryRef res = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef props = DBCopyPropNames(build, project);
	CFIndex i, count = CFArrayGetCount(props);
	for (i = 0; i < count; ++i) {
		CFStringRef prop = CFArrayGetValueAtIndex(props, i);
		CFTypeRef value = DBCopyProp(build, project, prop);
		if (value) {
			CFDictionaryAddValue(res, prop, value);
			CFRelease(value);
		}
	}
	return res;
}


CFDictionaryRef DBCopyBuildPlist(CFStringRef build) {
	CFMutableDictionaryRef plist = (CFMutableDictionaryRef)DBCopyProjectPlist(build, NULL);

	CFDictionarySetValue(plist, CFSTR("build"), build);

	// Generate projects dictionary
	CFMutableDictionaryRef projects = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef names = DBCopyOneProjectNames(build);
	CFIndex i, count = CFArrayGetCount(names);
	for (i = 0; i < count; ++i) {
		CFStringRef name = CFArrayGetValueAtIndex(names, i);
		CFDictionaryRef proj = DBCopyProjectPlist(build, name);
		CFDictionaryAddValue(projects, name, proj);
		CFRelease(proj);
	}
	CFDictionarySetValue(plist, CFSTR("projects"), projects);
	CFRelease(projects);
	CFRelease(names);
	
	// Generate groups dictionary
	CFMutableDictionaryRef groups = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	names = DBCopyGroupNames(build);
	count = CFArrayGetCount(names);
	for (i = 0; i < count; ++i) {
		CFStringRef name = CFArrayGetValueAtIndex(names, i);
		CFArrayRef members = DBCopyGroupMembers(build, name);
		CFDictionaryAddValue(groups, name, members);
		CFRelease(members);
	}
	if (CFDictionaryGetCount(groups) > 0) {
		CFDictionarySetValue(plist, CFSTR("groups"), groups);
	}
	CFRelease(groups);
	CFRelease(names);
	
	return plist;
}


int DBSetPlist(CFStringRef buildParam, CFStringRef projectParam, CFPropertyListRef plist) {
	int res = 0;
	CFIndex i, count;

	if (!plist) return -1;
	if (CFGetTypeID(plist) != CFDictionaryGetTypeID()) return -1;

	CFStringRef build = CFDictionaryGetValue(plist, CFSTR("build"));
	if (!build) build = buildParam;
	CFStringRef project = CFDictionaryGetValue(plist, CFSTR("name"));
	if (!project) project = projectParam;
	CFDictionaryRef projects = CFDictionaryGetValue(plist, CFSTR("projects"));
	CFDictionaryRef groups = CFDictionaryGetValue(plist, CFSTR("groups"));
		
	if (projects && CFGetTypeID(projects) != CFDictionaryGetTypeID()) {
		fprintf(stderr, "Error: projects must be a dictionary.\n");
		return -1;
	}
	if (groups && CFGetTypeID(groups) != CFDictionaryGetTypeID()) {
		fprintf(stderr, "Error: groups must be a dictionary.\n");
		return -1;
	}

	CFArrayRef props = dictionaryGetSortedKeys(plist);

	res = DBBeginTransaction();
	if (res != 0) return res;

	//
	// Delete any properties which may have been removed
	//
	CFArrayRef existingProps = DBCopyPropNames(build, project);
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
				SQL("DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q", cbuild, cproj, cprop);
				free(cproj);
			} else {
				SQL("DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q", cbuild, cprop);
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
		if (CFEqual(prop, CFSTR("build")) || CFEqual(prop, CFSTR("projects")) || CFEqual(prop, CFSTR("groups"))) continue;
		
		CFTypeRef value = CFDictionaryGetValue(plist, prop);
		res = DBSetProp(build, project, prop, value);
		if (res != 0) break;
	}
	
	if (res == 0 && projects) {
		CFArrayRef projectNames = dictionaryGetSortedKeys(projects);
		count = CFArrayGetCount(projectNames);
		for (i = 0; i < count; ++i ) {
			CFStringRef project = CFArrayGetValueAtIndex(projectNames, i);
			CFDictionaryRef subplist = CFDictionaryGetValue(projects, project);
			res = DBSetPlist(build, project, subplist);
			if (res != 0) break;
		}
		CFRelease(projectNames);
	}
	if (props) CFRelease(props);
	
	//
	// Load the groups dictionary if present
	//
	if (groups) {
		// delete old groups so we don't leave any stale entries
		char* cbuild = strdup_cfstr(build);
		SQL("DELETE FROM groups WHERE build=%Q", cbuild);
		free(cbuild);

		CFArrayRef groupNames = dictionaryGetSortedKeys(groups);
		CFIndex i, count = CFArrayGetCount(groupNames);
		for (i = 0; i < count; ++i) {
			CFStringRef name = CFArrayGetValueAtIndex(groupNames, i);
			CFArrayRef members = CFDictionaryGetValue(groups, name);
			DBSetGroupMembers(build, name, members);
		}
		CFRelease(groupNames);
	}

	DBCommitTransaction();

	return res;
}


CFArrayRef DBCopyGroupNames(CFStringRef build) {
	char* cbuild = strdup_cfstr(build);
	CFArrayRef res = SQL_CFARRAY("SELECT DISTINCT name FROM groups WHERE build=%Q ORDER BY name", cbuild);
	free(cbuild);
	return res;
}

// copy group members for a single build (no inheritance)
static CFArrayRef _DBCopyGroupMembers(CFStringRef build, CFStringRef group) {
	char* cbuild = strdup_cfstr(build);
	char* cgroup = strdup_cfstr(group);
	CFArrayRef res = SQL_CFARRAY("SELECT DISTINCT member FROM groups WHERE build=%Q AND name=%Q ORDER BY member", cbuild, cgroup);
	free(cbuild);
	free(cgroup);
	return res;
}

CFArrayRef DBCopyGroupMembers(CFStringRef build, CFStringRef group) {
	CFArrayRef res = NULL;
	do {
		res = _DBCopyGroupMembers(build, group);
		if (res) break;
		
		build = DBCopyOnePropString(build, NULL, CFSTR("inherits"));
	} while (build != NULL);
	return res;
}

int DBSetGroupMembers(CFStringRef build, CFStringRef group, CFArrayRef members) {
	char* cbuild = strdup_cfstr(build);
	char* cgroup = strdup_cfstr(group);
	SQL("DELETE FROM groups WHERE build=%Q AND name=%Q", cbuild, cgroup);
	CFIndex i, count = CFArrayGetCount(members);
	for (i = 0; i < count; ++i) {
		char* cmember = strdup_cfstr(CFArrayGetValueAtIndex(members, i));
		SQL("INSERT INTO groups (build,name,member) VALUES (%Q, %Q, %Q)", cbuild, cgroup, cmember);
		free(cmember);
	}
	free(cbuild);
	free(cgroup);
	return 0;
}

// NOT THREAD SAFE
int DBBeginTransaction() {
	++__nestedTransactions;
	if (__nestedTransactions == 1) {
		return SQL("BEGIN");
	} else {
		return SQLITE_OK;
	}
}
// NOT THREAD SAFE
int DBRollbackTransaction() {
	__nestedTransactions = 0;
	return SQL("ROLLBACK");
}
// NOT THREAD SAFE
int DBCommitTransaction() {
	--__nestedTransactions;
	if (__nestedTransactions == 0) {
		return SQL("COMMIT");
	} else {
		return SQLITE_OK;
	}
}


