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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "Database.h"

/**
 * sqlite3_trace callback for debugging
 */
void dbtrace(void* context, const char* sql) {
	fprintf(stderr, "[TRACE] %s \n", sql);
}

Database::Database() {
	// XXX: make the initial allocation for 2 to tailor to darwinup usage
	m_table_max = 1;
	m_table_count = 0;
	m_tables = (Table**)malloc(sizeof(Table*) * m_table_max);
	m_db = NULL;	
	m_path = NULL;
	m_error_size = 1024;
	m_error = (char*)malloc(m_error_size);
}

Database::Database(const char* path) {
	m_table_max = 1;
	m_table_count = 0;
	m_tables = (Table**)malloc(sizeof(Table*) * m_table_max);
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
	free(m_tables);
	free(m_path);
	free(m_error);
}

void Database::init_schema() {
	// do nothing... children should implement this
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
	char* sqlstr = m_tables[0]->count(NULL);
	int res = this->sql(sqlstr);
	free(sqlstr);
	return res!=SQLITE_OK;
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


bool Database::update(Table* table, Column* column, const char* value, const char* where, 
					  uint32_t &count) {
	// not implemented
	assert(false);
	return false;
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


/**
 * Given a table and an arg list in the same order as table->add_column() calls,
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
	va_list args;
	va_start(args, table);

	// get the prepared statement
	sqlite3_stmt* stmt = table->insert(m_db);
	if (!stmt) {
		fprintf(stderr, "Error: %s table gave a NULL statement when trying to insert.\n", table->name());
		return false;
	}
	
	uint32_t param = 1; // counter to track placeholders in sql statement
	
	for (uint32_t i=0; i<table->column_count(); i++) {
		Column* col = table->column(i);
		
		// primary keys do not get inserted
		if (col->is_pk()) continue;

		// temp variable for blob columns
		uint8_t* bdata = NULL;
		uint32_t bsize = 0;
		
		switch(col->type()) {
			case SQLITE_INTEGER:
				res = sqlite3_bind_int64(stmt, param++, va_arg(args, uint64_t));
				break;
			case SQLITE_TEXT:
				res = sqlite3_bind_text(stmt, param++, va_arg(args, char*), -1, SQLITE_STATIC);
				break;
			case SQLITE_BLOB:
				bdata = va_arg(args, uint8_t*);
				bsize = va_arg(args, uint32_t);
				res = sqlite3_bind_blob(stmt, param++, 
										bdata, 
										bsize, 
										SQLITE_STATIC);
				break;
				
		}
		if (res != SQLITE_OK) {
			fprintf(stderr, "Error: failed to bind parameter #%d with column #%d of type %d when inserting "
					        "to table %s \n",
					param, i, col->type(), table->name());
			return false;
		}
	}

	sqlite3_trace(m_db, dbtrace, NULL);
	
	res = sqlite3_step(stmt);
	if (res == SQLITE_DONE) res = SQLITE_OK;
	sqlite3_reset(stmt);
	
	va_end(args);
	return res == SQLITE_OK;
}

uint64_t Database::last_insert_id() {
	return (uint64_t)sqlite3_last_insert_rowid(m_db);
}


