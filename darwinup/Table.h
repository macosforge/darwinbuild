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

#ifndef _TABLE_H
#define _TABLE_H

#include <stdint.h>
#include <sqlite3.h>

#include "Column.h"


struct Table {	
	Table(const char* name);
	virtual ~Table();
	
	const char*    name();
	uint32_t       version();

	// Add custom SQL to table initialization
	int            set_custom_create(const char* sql);
	
	// Column handling
	int            add_column(Column*, uint32_t schema_version);
	Column*        column(uint32_t index);
	// get the result record offset for column at index
	int            offset(uint32_t index);
	// get total size of result record
	uint32_t       row_size();	

	// Result record handling
	uint8_t*       alloc_result();
	int            free_result(uint8_t* result);

	/**
	 * sql statement generators (cached on Table)
	 */
	sqlite3_stmt*    count(sqlite3* db);
	sqlite3_stmt*    update(sqlite3* db);
	sqlite3_stmt*    insert(sqlite3* db);
	sqlite3_stmt*    del(sqlite3* db);
	
	/**
	 * sql statement generators (cached by Database & libcache)
	 *
	 * - order is either ORDER_BY_ASC or ORDER_BY_DESC
	 * - count parameters should be the number of items in the va_list
	 * - args should have sets of 3 (integer and text) or 4 (blob) 
	 *     parameters for WHERE clause like Column*, char, value
	 *      - Column* is the column to match against
	 *      - char is how to compare, one of '=', '!', '>', or '<'
	 *      - value is the value to match (1 for integer and text, 2 for blobs)
	 *          which is ignored by these API since they leave placeholders 
	 *          instead
	 *
	 */
	sqlite3_stmt**    count(sqlite3* db, uint32_t count, va_list args);
	sqlite3_stmt**    get_column(sqlite3* db, Column* value_column, 
								uint32_t count, va_list args);
	sqlite3_stmt**    get_row(sqlite3* db, uint32_t count, va_list args);
	sqlite3_stmt**    get_row_ordered(sqlite3* db, Column* order_by, int order, 
							 uint32_t count, va_list args);
	sqlite3_stmt**    update_value(sqlite3* db, Column* value_column, 
								  uint32_t count, va_list args);
	sqlite3_stmt**    del(sqlite3* db, uint32_t count, va_list args);
	
protected:

	const char*    create();  
	const char*    alter_add_column(uint32_t index);
	
	int            where_va_columns(uint32_t count, char* query, size_t size, 
									size_t* used, va_list args);
	const Column** columns();
	uint32_t       column_count();

	// free the out-of-band columns (text, blob) from a result record
	int            free_row(uint8_t* row);
	void           dump_results(FILE* f);	
	
	char*          m_name;
	uint32_t       m_version; // schema version this was added

	char*          m_create_sql;
	char*          m_custom_create_sql;
	char*          m_insert_sql;
	char*          m_update_sql;
	char*          m_delete_sql;
	
	Column**       m_columns;
	uint32_t       m_column_count;
	uint32_t       m_column_max;
	int            m_columns_size;
	
	sqlite3_stmt*  m_prepared_insert;
	sqlite3_stmt*  m_prepared_update;
	sqlite3_stmt*  m_prepared_delete;
	
	uint8_t**      m_results;
	uint32_t       m_result_count;
	uint32_t       m_result_max;

	friend struct Database;
};

#endif
