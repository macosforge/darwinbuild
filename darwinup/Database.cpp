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
#include <stdlib.h>
#include "Database.h"


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
 *
 * Darwinup database abstraction. This class is responsible
 *  for generating the Table and Column objects that make
 *  up the darwinup database schema, but the parent handles
 *  deallocation.
 *
 */
DarwinupDatabase::DarwinupDatabase() {
	this->connect();
}

DarwinupDatabase::DarwinupDatabase(const char* path) : Database(path) {
	this->connect();
}

DarwinupDatabase::~DarwinupDatabase() {
	// parent automatically deallocates schema objects
}

void DarwinupDatabase::init_schema() {
	Table* archives = new Table("archives");
	//                                                                   index  pk     unique
	assert(archives->add_column(new Column("serial",     SQLITE_INTEGER, false, true,  false)));
	assert(archives->add_column(new Column("uuid",       SQLITE_BLOB,    true,  false, true)));
	assert(archives->add_column(new Column("name",       SQLITE3_TEXT)));
	assert(archives->add_column(new Column("date_added", SQLITE_INTEGER)));
	assert(archives->add_column(new Column("active",     SQLITE_INTEGER)));
	assert(archives->add_column(new Column("info",       SQLITE_INTEGER)));
	assert(this->add_table(archives));
	
	Table* files = new Table("files");
	//                                                                   index  pk     unique
	assert(files->add_column(new Column("serial",  SQLITE_INTEGER,       false, true,  false)));
	assert(files->add_column(new Column("archive", SQLITE_INTEGER)));
	assert(files->add_column(new Column("info",    SQLITE_INTEGER)));
	assert(files->add_column(new Column("mode",    SQLITE_INTEGER)));
	assert(files->add_column(new Column("uid",     SQLITE_INTEGER)));
	assert(files->add_column(new Column("gid",     SQLITE_INTEGER)));
	assert(files->add_column(new Column("size",    SQLITE_INTEGER)));
	assert(files->add_column(new Column("digest",  SQLITE_BLOB)));
	assert(files->add_column(new Column("path",    SQLITE3_TEXT,         true,  false, false)));
	assert(this->add_table(files));	
}
