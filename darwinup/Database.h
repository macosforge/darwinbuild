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

// libcache callbacks
void cache_key_retain(void* key_in, void** key_out, void* user_data);
void cache_key_release(void* key, void* user_data);
void cache_value_retain(void* value, void* user_data);
void cache_value_release(void* value, void* user_data);


/**
 * 
 * Generic sqlite abstraction
 *
 */
struct Database {
	Database();
	Database(const char* path);
	virtual ~Database();

	static const int TYPE_INTEGER = SQLITE_INTEGER;
	static const int TYPE_TEXT    = SQLITE3_TEXT;
	static const int TYPE_BLOB    = SQLITE_BLOB;
	
	virtual void init_schema();
	const char* path();
	const char* error();
	bool connect();
	bool connect(const char* path);
	
	
	bool begin_transaction();
	bool rollback_transaction();
	bool commit_transaction();
	

	const char* get_row(Table* table, const char* where);
	const char* get_column(Table* table, Column* column, const char* where);
	const char* get_all(Table* table, const char* where);
		
	bool del(Table* table, const char* where, uint32_t &count);
	
	
	/**
	 * SELECT statement caching and execution
	 *
	 *  name is a string key that labels the query for caching purposes (63 char max)
	 *  output is where we will store the value requested
	 *  table and value_column are what value we'll give back
	 *
	 *  everything else are Column*,value pairs for making a WHERE clause
	 *
	 */
	bool count(const char* name, void** output, Table* table, uint32_t count, ...);
	bool get_value(const char* name, void** output, Table* table, Column* value_column, uint32_t count, ...);

	
	bool update(Table* table, uint64_t pkvalue, ...);
	bool insert(Table* table, ...);

	bool add_table(Table*);
	uint64_t last_insert_id();
	

protected:
	
	bool empty();
	bool create_tables();
	int sql(const char* fmt, ...);
	
	int execute(sqlite3_stmt* stmt);
	
	// libcache
	void init_cache();
	void destroy_cache();
	
	char*            m_path;
	sqlite3*         m_db;

	Table**          m_tables;
	uint32_t         m_table_count;
	uint32_t         m_table_max;

	cache_t*         m_statement_cache;
	
	char*            m_error;
	size_t           m_error_size;

};

#endif
