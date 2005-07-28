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
			CFDictionarySetValue(dict, name, value);
		} else if (CFGetTypeID(cf) == CFStringGetTypeID()) {
			CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(array, cf);
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

	char* table = "CREATE TABLE properties (build TEXT, project TEXT, property TEXT, key TEXT, value TEXT)";
	char* index = "CREATE INDEX properties_index ON properties (build, project, property, key, value)";
	SQL_NOERR(table);
	SQL_NOERR(index);
	
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

CFArrayRef DBCopyProjectNames(CFStringRef build) {
	char* cbuild = strdup_cfstr(build);
	CFArrayRef res = SQL_CFARRAY("SELECT DISTINCT project FROM properties WHERE build=%Q ORDER BY project", cbuild);
	free(cbuild);
	return res;
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
	if (plugin && plugin->type == kDBPluginPropertyType) {
		type = plugin->datatype;
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
	}
	return res;
}

CFStringRef _DBCopyPropString(CFStringRef build, CFStringRef project, CFStringRef property) {
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

CFArrayRef _DBCopyPropArray(CFStringRef build, CFStringRef project, CFStringRef property) {
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

CFDictionaryRef _DBCopyPropDictionary(CFStringRef build, CFStringRef project, CFStringRef property) {
	char* cbuild = strdup_cfstr(build);
	char* cproj = strdup_cfstr(project);
	char* cprop = strdup_cfstr(property);
	char* sql;
	if (cproj && *cproj != 0)
		sql = "SELECT DISTINCT key,value FROM properties WHERE property=%Q AND build=%Q AND project=%Q ORDER BY key";
	else
		sql = "SELECT DISTINCT key,value FROM properties WHERE property=%Q AND build=%Q AND project IS NULL ORDER BY key";
	CFDictionaryRef res = SQL_CFDICTIONARY(sql, cprop, cbuild, cproj);
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
		
		original = _DBCopyPropString(build, project, CFSTR("original"));
		if (original) break;

		CFStringRef inherits = _DBCopyPropString(build, NULL, CFSTR("inherits"));
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
	return (CFStringRef)_DBCopyPropWithInheritance(build, project, property, (void*)_DBCopyPropString);
}

CFArrayRef DBCopyPropArray(CFStringRef build, CFStringRef project, CFStringRef property) {
	return (CFArrayRef)_DBCopyPropWithInheritance(build, project, property, (void*)_DBCopyPropArray);
}

CFDictionaryRef DBCopyPropDictionary(CFStringRef build, CFStringRef project, CFStringRef property) {
	return (CFDictionaryRef)_DBCopyPropWithInheritance(build, project, property, (void*)_DBCopyPropDictionary);
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
			SQL("INSERT INTO properties (build,project,property,value) VALUES (%Q, %Q, %Q, %Q)", cbuild, cproj, cprop, cvalu);
		} else {
			SQL("INSERT INTO properties (build,property,value) VALUES (%Q, %Q, %Q)", cbuild, cprop, cvalu);
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

	CFArrayRef keys = dictionaryGetSortedKeys(value);
	CFIndex i, count = CFArrayGetCount(keys);
	for (i = 0; i < count; ++i) {
		CFStringRef key = CFArrayGetValueAtIndex(keys, i);
		char* ckey = strdup_cfstr(key);
		CFTypeRef cf = CFDictionaryGetValue(value, key);

		if (project) {
			SQL("DELETE FROM properties WHERE build=%Q AND project=%Q AND property=%Q AND key=%Q", cbuild, cproj, cprop, ckey);
		} else {
			SQL("DELETE FROM properties WHERE build=%Q AND project IS NULL AND property=%Q AND key=%Q", cbuild, cprop, ckey);
		}

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
	CFMutableDictionaryRef projects = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFArrayRef names = DBCopyProjectNames(build);
	CFIndex i, count = CFArrayGetCount(names);
	for (i = 0; i < count; ++i) {
		CFStringRef name = CFArrayGetValueAtIndex(names, i);
		CFDictionaryRef proj = DBCopyProjectPlist(build, name);
		CFDictionaryAddValue(projects, name, proj);
		CFRelease(proj);
	}
	CFDictionarySetValue(plist, CFSTR("build"), build);
	CFDictionarySetValue(plist, CFSTR("projects"), projects);
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
		
	if (projects && CFGetTypeID(projects) != CFDictionaryGetTypeID()) {
		fprintf(stderr, "Error: projects must be a dictionary.\n");
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
		if (CFEqual(prop, CFSTR("build")) || CFEqual(prop, CFSTR("projects"))) continue;
		
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

	DBCommitTransaction();

	return res;
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


