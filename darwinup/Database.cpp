/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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

#include "Database.h"

/**
 * sqlite3_trace callback for debugging
 */
void dbtrace(void* context, const char* sql) {
	IF_DEBUG("[TRACE] %s \n", sql);
}

Database::Database() {
	// XXX: make the initial allocation for 2 to tailor to darwinup usage
	m_table_max = 1;
	m_table_count = 0;
	m_tables = (Table**)malloc(sizeof(Table*) * m_table_max);
	this->init_cache();
	m_db = NULL;	
	m_path = NULL;
	m_error_size = 1024;
	m_error = (char*)malloc(m_error_size);
}

Database::Database(const char* path) {
	m_table_max = 1;
	m_table_count = 0;
	m_tables = (Table**)malloc(sizeof(Table*) * m_table_max);
	this->init_cache();
	m_db = NULL;		
	m_path = strdup(path);
	if (!m_path) {
		fprintf(stderr, "Error: ran out of memory when constructing database object.\n");
	}
	m_error_size = 1024;
	m_error = (char*)malloc(m_error_size);
}

Database::~Database() {
	for (uint32_t i = 0; i < m_table_count; i++) {
		delete m_tables[i];
	}
	this->destroy_cache();
	free(m_tables);
	free(m_path);
	free(m_error);
}


void Database::init_schema() {
	// do nothing... children should implement this
}


void Database::init_cache() {
	fprintf(stderr, "CACHE: init_cache \n");
	cache_attributes_t attrs;
	attrs.version = CACHE_ATTRIBUTES_VERSION_2;
	attrs.key_hash_cb = cache_key_hash_cb_cstring;
	attrs.key_is_equal_cb = cache_key_is_equal;
	attrs.key_retain_cb = cache_key_retain;
	attrs.key_release_cb = cache_key_release;
	attrs.value_release_cb = cache_value_release;
	attrs.value_retain_cb = cache_value_retain;
	cache_create("org.macosforge.darwinbuild.darwinup.statements", &attrs, &m_statement_cache);
}

void Database::destroy_cache() {
	fprintf(stderr, "CACHE: destroy_cache \n");
	cache_destroy(m_statement_cache);
}

bool cache_key_is_equal(void* key1, void* key2, void* user) {
	fprintf(stderr, "CACHE: key1: %s key2: %s \n", (char*)key1, (char*)key2);
	bool res = (strcmp((char*)key1, (char*)key2) == 0);
	fprintf(stderr, "CACHE: key_is_equal returning %d \n", res);
	return res;
}

void cache_key_retain(void* key_in, void** key_out, void* user_data) {
	fprintf(stderr, "CACHE: key_retain %s\n", (char*)key_in);
	*key_out = strdup((char*)key_in);
}

void cache_key_release(void* key, void* user_data) {
	fprintf(stderr, "CACHE: key_release %s\n", (char*)key);
	free(key);
}

void cache_value_retain(void* value, void* user_data) {
	fprintf(stderr, "CACHE: value_retain %p\n", value);
	// do nothing
}

void cache_value_release(void* value, void* user_data) {
	fprintf(stderr, "CACHE: value_release %p\n", value);
	sqlite3_finalize((sqlite3_stmt*)value);
}


const char* Database::path() {
	return m_path;
}

const char* Database::error() {
	return m_error;
}

bool Database::connect() {
	int res = 0;
	this->init_schema();
	res = sqlite3_open(m_path, &m_db);
	if (res) {
		sqlite3_close(m_db);
		m_db = NULL;
		fprintf(stderr, "Error: unable to connect to database at: %s \n", m_path);
		return false;
	}	
	sqlite3_trace(m_db, dbtrace, NULL);
	if (this->empty()) {
		assert(this->create_tables());
	}	
	return true;	
}

bool Database::connect(const char* path) {
	this->m_path = strdup(path);
	if (!m_path) fprintf(stderr, "Error: ran out of memory when trying to connect to database.\n");
	return m_path && this->connect();
}


bool Database::add_table(Table* t) {
	if (m_table_count >= m_table_max) {
		m_tables = (Table**)realloc(m_tables, m_table_max * sizeof(Table*) * 4);
		if (!m_tables) {
			fprintf(stderr, "Error: unable to reallocate memory to add a table\n");
			return false;
		}
		m_table_max *= 4;
	}
	m_tables[m_table_count++] = t;
	
	return true;
}


bool Database::empty() {
	if (!m_tables[0]) {
		fprintf(stderr, "Error: Database has not had a schema initialized.\n");
		return false;
	}
	sqlite3_stmt* stmt = m_tables[0]->count(m_db);
	int res = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return res!=SQLITE_ROW;
}

bool Database::create_tables() {
	int res = SQLITE_OK;
	for (uint32_t i=0; i<m_table_count; i++) {
		IF_DEBUG("[DATABASE] creating table #%u \n", i);
		res = this->sql(m_tables[i]->create());
		if (res!=SQLITE_OK) {
			fprintf(stderr, "Error: sql error trying to create table: %s: %s\n", 
					m_tables[i]->name(), m_error);
			return false;
		}
	}
	return res==SQLITE_OK;
}

#define __SQL(callback, context, fmt) \
    va_list args; \
    va_start(args, fmt); \
    char* error; \
    if (this->m_db) { \
        char *query = sqlite3_vmprintf(fmt, args); \
        IF_DEBUG("[DATABASE] SQL: %s \n", query); \
        res = sqlite3_exec(this->m_db, query, callback, context, &error); \
        sqlite3_free(query); \
    } else { \
        fprintf(stderr, "Error: database not open.\n"); \
        res = SQLITE_ERROR; \
    } \
    va_end(args);

int Database::sql(const char* fmt, ...) {
	int res = 0;
	__SQL(NULL, NULL, fmt);
	if (error) {
		strlcpy(m_error, error, m_error_size);
		sqlite3_free(error);
	}
	IF_DEBUG("[DATABASE] __SQL set res = %d \n", res);
	return res;
}

#undef __SQL


#define __bind_all_columns(_lastarg) \
    va_list args; \
    va_start(args, _lastarg); \
	for (uint32_t i=0; i<table->column_count(); i++) { \
		Column* col = table->column(i); \
		if (col->is_pk()) continue; \
		uint8_t* bdata = NULL; \
		uint32_t bsize = 0; \
		switch(col->type()) { \
			case TYPE_INTEGER: \
				res = sqlite3_bind_int64(stmt, param++, va_arg(args, uint64_t)); \
				break; \
			case TYPE_TEXT: \
				res = sqlite3_bind_text(stmt, param++, va_arg(args, char*), -1, SQLITE_STATIC); \
				break; \
			case TYPE_BLOB: \
				bdata = va_arg(args, uint8_t*); \
				bsize = va_arg(args, uint32_t); \
				res = sqlite3_bind_blob(stmt, param++, \
										bdata, \
										bsize, \
										SQLITE_STATIC); \
				break; \
		} \
		if (res != SQLITE_OK) { \
			fprintf(stderr, "Error: failed to bind parameter #%d with column #%d of type %d " \
					"table %s \n", \
					param, i, col->type(), table->name()); \
			return false; \
		} \
	} \
    va_end(args);

#define __bind_va_columns(_lastarg) \
    va_list args; \
    va_start(args, _lastarg); \
    for (uint32_t i=0; i<count; i++) { \
        Column* col = va_arg(args, Column*); \
        fprintf(stderr, "DEBUG: got a column from va_arg: %p \n", col); \
        uint8_t* bdata = NULL; \
        uint32_t bsize = 0; \
        switch(col->type()) { \
            case TYPE_INTEGER: \
                fprintf(stderr, "DEBUG: param %d is integer\n", param); \
                res = sqlite3_bind_int64(stmt, param++, va_arg(args, uint64_t)); \
                break; \
            case TYPE_TEXT: \
                fprintf(stderr, "DEBUG: param %d is text\n", param); \
                res = sqlite3_bind_text(stmt, param++, va_arg(args, char*), -1, SQLITE_STATIC); \
                break; \
            case TYPE_BLOB: \
                fprintf(stderr, "DEBUG: param %d is blob\n", param); \
                bdata = va_arg(args, uint8_t*); \
                bsize = va_arg(args, uint32_t); \
                res = sqlite3_bind_blob(stmt, param++, \
                                        bdata, \
										bsize, \
                                        SQLITE_STATIC); \
                break; \
		} \
        if (res != SQLITE_OK) { \
            fprintf(stderr, "Error: failed to bind parameter #%d with column #%d of type %d " \
                            "table %s \n", \
							param, i, col->type(), table->name()); \
            return false; \
        } \
    } \
    va_end(args);

int Database::execute(sqlite3_stmt* stmt) {
	int res = sqlite3_step(stmt);
	if (res == SQLITE_DONE) res = SQLITE_OK;
	sqlite3_reset(stmt);
	return res;
}

#define __get_stmt(expr) \
	sqlite3_stmt* stmt; \
	char* key = strdup(name); \
    fprintf(stderr, "CACHE: statement cache at %p \n", m_statement_cache); \
	cache_get_and_retain(m_statement_cache, key, (void**)&stmt); \
	if (!stmt) { \
		fprintf(stderr, "DEBUG: generating query for %s \n", key); \
		va_list args; \
		va_start(args, count); \
		stmt = expr; \
		va_end(args); \
		cache_set_and_retain(m_statement_cache, key, stmt, sizeof(stmt)); \
	} else { \
		fprintf(stderr, "DEBUG: found query for %s in cache: %p \n", key, stmt); \
		fprintf(stderr, "DEBUG: query is: %s \n", sqlite3_sql(stmt)); \
	} \
	free(key);

#define __step_and_store(_stmt, _type, _output) \
    fprintf(stderr, "DEBUG: _stmt = %p \n", _stmt); \
    fprintf(stderr, "DEBUG: _output = %p \n", _output); \
    fprintf(stderr, "DEBUG: query = -%s- \n", sqlite3_sql(_stmt)); \
    fprintf(stderr, "DEBUG: col count: %d \n", sqlite3_column_count(_stmt)); \
    res = sqlite3_step(_stmt); \
    if (res == SQLITE_ROW) { \
	    switch(_type) { \
		    case TYPE_INTEGER: \
				fprintf(stderr, "DEBUG: step and store : integer\n"); \
			    *(uint64_t*)_output = (uint64_t)sqlite3_column_int64(_stmt, 0); \
				fprintf(stderr, "DEBUG: step and store : %p %llu\n", (uint64_t*)_output, *(uint64_t*)_output); \
			    break; \
		    case TYPE_TEXT: \
				fprintf(stderr, "DEBUG: step and store : text\n"); \
			    *(const unsigned char**)_output = sqlite3_column_text(_stmt, 0); \
				fprintf(stderr, "DEBUG: step and store : %s\n", *(char**)_output); \
			    break; \
    		case TYPE_BLOB: \
				fprintf(stderr, "DEBUG: step and store : blob\n"); \
	    		*(const void**)_output = sqlite3_column_blob(_stmt, 0); \
		    	break; \
    	} \
    } else { \
        fprintf(stderr, "ERROR: %d \n", res); \
    } \
    sqlite3_reset(_stmt); \
    cache_release_value(m_statement_cache, &_stmt);

bool Database::count(const char* name, void** output, Table* table, uint32_t count, ...) {
	__get_stmt(table->count(m_db, count, args));
	int res = SQLITE_OK;
	uint32_t param = 1;
	__bind_va_columns(count);
	__step_and_store(stmt, TYPE_INTEGER, output);
	return output != NULL;
}

bool Database::get_value(const char* name, void** output, Table* table, Column* value_column, 
						 uint32_t count, ...) {
	__get_stmt(table->get_value(m_db, value_column, count, args));
	int res = SQLITE_OK;
	uint32_t param = 1;
	__bind_va_columns(count);
	__step_and_store(stmt, value_column->type(), output);
	return output != NULL;
}

/**
 * Given a table and an arg list in the same order as Table::add_column() calls,
 * binds and executes a sql update. The Table is responsible for preparing the
 * statement in Table::update()
 *
 * All integer args must be cast to uint64_t
 * All blob columns must provide 2 args in the list. The first arg is a uint8_t* of data
 * and then the uint32_t value for size of the data. 
 *
 */
bool Database::update(Table* table, uint64_t pkvalue, ...) {
	int res = SQLITE_OK;
	
	// get the prepared statement
	sqlite3_stmt* stmt = table->update(m_db);
	if (!stmt) {
		fprintf(stderr, "Error: %s table gave a NULL statement when trying to update.\n", table->name());
		return false;
	}
	
	uint32_t param = 1; // counter to track placeholders in sql statement
	__bind_all_columns(pkvalue);
	
	// bind the primary key in the WHERE clause
	res = sqlite3_bind_int64(stmt, param++, pkvalue);
	res = this->execute(stmt);
	return res == SQLITE_OK;
}


/**
 * Given a table and an arg list in the same order as Table::add_column() calls,
 * binds and executes a sql insertion. The Table is responsible for preparing the
 * statement in Table::insert()
 *
 * All integer args must be cast to uint64_t
 * All blob columns must provide 2 args in the list. The first arg is a uint8_t* of data
 * and then the uint32_t value for size of the data. 
 *
 */
bool Database::insert(Table* table, ...) {
	int res = SQLITE_OK;

	// get the prepared statement
	sqlite3_stmt* stmt = table->insert(m_db);
	if (!stmt) {
		fprintf(stderr, "Error: %s table gave a NULL statement when trying to insert.\n", table->name());
		return false;
	}
	
	uint32_t param = 1; // counter to track placeholders in sql statement
	__bind_all_columns(table);
	res = this->execute(stmt);
	return res == SQLITE_OK;
}


bool Database::del(Table* table, uint64_t serial) {
	int res = SQLITE_OK;
	sqlite3_stmt* stmt = table->del(m_db);
	if (!stmt) {
		fprintf(stderr, "Error: %s table gave a NULL statement when trying to delete.\n", table->name());
		return false;
	}
	res = sqlite3_bind_int64(stmt, 1, serial);
	res = this->execute(stmt);
	return res == SQLITE_OK;
}

bool Database::del(const char* name, Table* table, uint32_t count, ...) {
	__get_stmt(table->del(m_db, count, args));
	int res = SQLITE_OK;
	uint32_t param = 1;
	__bind_va_columns(count);
	res = this->execute(stmt);
	return res == SQLITE_OK;
	
}

#undef __bind_all_columns
#undef __get_stmt
#undef __step_and_store

uint64_t Database::last_insert_id() {
	return (uint64_t)sqlite3_last_insert_rowid(m_db);
}


