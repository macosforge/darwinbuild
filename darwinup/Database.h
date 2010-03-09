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

#ifndef _DATABASE_H
#define _DATABASE_H

#include <assert.h>
#include <cache.h>
#include <cache_callbacks.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "Table.h"
#include "Digest.h"
#include "Archive.h"

// flag for generating queries with ORDER BY clauses
#define ORDER_BY_DESC 0
#define ORDER_BY_ASC  1

// initial number of rows we allocate for when querying
#define INITIAL_ROWS   8

// how much we grow by when we need more space
#define REALLOC_FACTOR 4
#define ERROR_BUF_SIZE 1024

// return code bits
#define DB_OK        0x0000
#define DB_ERROR     0x0001
#define DB_FOUND     0x0010

// test return code to see if actual results were found
#define FOUND(x)  ((x & DB_FOUND) && !(x & DB_ERROR))

// Schema creation macros
#define SCHEMA_VERSION(v) this->schema_version(v);
#define ADD_TABLE(t) assert(this->add_table(t)==0);
#define ADD_COLUMN(table, name, type, index, pk, unique) \
    assert(table->add_column(new Column(name, type, index, pk, unique), this->schema_version())==0);
#define ADD_INDEX(table, name, type, unique) \
	assert(table->add_column(new Column(name, type, true, false, unique), this->schema_version())==0);
#define ADD_PK(table, name) \
	assert(table->add_column(new Column(name, TYPE_INTEGER, \
                                        false, true, false), this->schema_version())==0);
#define ADD_TEXT(table, name) \
	assert(table->add_column(new Column(name, TYPE_TEXT), this->schema_version())==0);
#define ADD_INTEGER(table, name) \
	assert(table->add_column(new Column(name, TYPE_INTEGER), this->schema_version())==0);
#define ADD_BLOB(table, name) \
	assert(table->add_column(new Column(name, TYPE_BLOB), this->schema_version())==0);


/**
 * 
 * Generic sqlite abstraction
 *
 */
struct Database {
	Database();
	Database(const char* path);
	virtual ~Database();

	// public setter/getter of class attr
	uint32_t     schema_version();
	void         schema_version(uint32_t v);
	
	/**
	 * init_schema is called during db connection.
	 * Projects implementing a Database derived class
	 * should use Table::add_column() or the ADD_*
	 * macros in their init_schema() to define their schema
	 */
	virtual int  init_schema();
	
	const char*  path();
	const char*  error();
	int          connect();
	int          connect(const char* path);
	
	int          begin_transaction();
	int          rollback_transaction();
	int          commit_transaction();
	
	/**
	 * statement caching and execution
	 *
	 * - name is a string key that labels the query for caching purposes 
	 * - output is where we will store the value requested
	 * - count is the number of sets of parameters
	 * - va_list should have sets of 3 (integer and text) or 4 (blob) 
	 *     parameters for WHERE clause like Column*, char, value(s)
	 *      - Column* is the column to match against
	 *      - char is how to compare, one of '=', '!', '>', or '<'
	 *      - value(s) is the value to match
	 *          - text columns require a char* arg
	 *          - integer columns require a uint64_t arg
	 *          - blob columns require 2 args in the list:
	 *              - first is a uint8_t* of data
	 *              - second is a uint32_t value for size of the data
	 *
	 */
	int  count(const char* name, void** output, Table* table, uint32_t count, ...);
	int  get_value(const char* name, void** output, Table* table, Column* value_column, 
				   uint32_t count, ...);
	int  get_column(const char* name, void** output, uint32_t* result_count, 
					Table* table, Column* column, uint32_t count, ...);
	int  get_row(const char* name, uint8_t** output, Table* table, uint32_t count, ...);
	int  get_row_ordered(const char* name, uint8_t** output, Table* table, Column* order_by, 
						 int order, uint32_t count, ...);
	int  get_all_ordered(const char* name, uint8_t*** output, uint32_t* result_count,
						 Table* table, Column* order_by, int order, uint32_t count, ...);
	int  update_value(const char* name, Table* table, Column* value_column, void** value, 
					  uint32_t count, ...);
	int  del(const char* name, Table* table, uint32_t count, ...);
	
	/**
	 * update/insert whole rows
	 *
	 * Given a table and a va_list in the same order as Table::add_column() 
	 * calls, minus any primary key columns, bind and executes a sql query 
	 * for insert or update. 
	 *
	 * The Table is responsible for preparing the statement
	 *
	 * text columns require char* args
	 * integer columns require uint64_t args
	 * blob columns require 2 args in the list:
	 *    - first is a uint8_t* of data
	 *    - second is a uint32_t value for size of the data 
	 *
	 */	
	int  update(Table* table, uint64_t pkvalue, ...);
	int  insert(Table* table, ...);
	
	// delete row with primary key equal to serial
	int  del(Table* table, uint64_t serial);
	
	uint64_t last_insert_id();
	
	
protected:

	// pre- and post- connection work
	int   pre_connect();
	int   post_connect();
	
	int   upgrade_schema(uint32_t version);
	int   upgrade_internal_schema(uint32_t version);
	
	int   init_internal_schema();
	
	int   get_information_value(const char* variable, char** value);
	int   update_information_value(const char* variable, const char* value);
	
	// get and set version info in actual database
	int       set_schema_version(uint32_t version);
	uint32_t  get_schema_version(); 
	
	// execute query with printf-style format, does not cache statement
	int   sql_once(const char* fmt, ...);
	// cache statement with name, execute query with printf-style format
	int   sql(const char* name, const char* fmt, ...);
	int   execute(sqlite3_stmt* stmt);
	
	int   add_table(Table*);
	
	// test if database has had its tables created
	bool  is_empty();
	// create tables for the client
	int   create_table(Table* table);
	int   create_tables();
	// create tables for ourselves
	int   create_internal_tables();

	// bind all table columns from va_list
	int   bind_all_columns(sqlite3_stmt* stmt, Table* table, va_list args);
	// bind each set of parameters from va_list 
	int   bind_va_columns(sqlite3_stmt* stmt, uint32_t count, va_list args);
	// bind parameters from va_list, starting with the param'th parameter in stmt
	int   bind_columns(sqlite3_stmt* stmt, uint32_t count, int param, 
					   va_list args);
	
	/**
	 * step and store functions
	 */
	size_t store_column(sqlite3_stmt* stmt, int column, uint8_t* output);
	int step_once(sqlite3_stmt* stmt, uint8_t* output, uint32_t* used);
	int step_all(sqlite3_stmt* stmt, void** output, uint32_t size, uint32_t* count);
	
	// libcache
	void init_cache();
	void destroy_cache();
	
	char*            m_path;
	sqlite3*         m_db;
	
	uint32_t         m_schema_version;
	Table*           m_information_table;
	sqlite3_stmt*    m_get_information_value;
	
	Table**          m_tables;
	uint32_t         m_table_count;
	uint32_t         m_table_max;

	cache_t*         m_statement_cache;
	
	sqlite3_stmt*    m_begin_transaction;
	sqlite3_stmt*    m_rollback_transaction;
	sqlite3_stmt*    m_commit_transaction;
	
	char*            m_error;
	size_t           m_error_size;
	
	static const int TYPE_INTEGER = SQLITE_INTEGER;
	static const int TYPE_TEXT    = SQLITE3_TEXT;
	static const int TYPE_BLOB    = SQLITE_BLOB;

};

// libcache callbacks
void cache_key_retain(void* key_in, void** key_out, void* user_data);
void cache_statement_retain(void* value, void* user_data);
void cache_statement_release(void* value, void* user_data);

#endif
