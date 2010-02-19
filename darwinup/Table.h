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
	Table();
	Table(const char* name);
	virtual ~Table();
	
	const char*    name();
	const Column** columns();
	Column*        column(uint32_t index);
	uint32_t       column_count();
	int            offset(int column);

	uint8_t*       alloc_result();
	int            free_result(uint8_t* result);

	int            add_column(Column*);
	uint32_t       row_size();	
	
	// return SQL statements for this table
	char*          create();  

	sqlite3_stmt*    count(sqlite3* db);
	sqlite3_stmt*    count(sqlite3* db, uint32_t count, va_list args);
	sqlite3_stmt*    get_column(sqlite3* db, Column* value_column, uint32_t count, 
								va_list args);
	sqlite3_stmt*    get_row(sqlite3* db, uint32_t count, va_list args);
	sqlite3_stmt*    update_value(sqlite3* db, Column* value_column, uint32_t count,
								  va_list args);
	sqlite3_stmt*    insert(sqlite3* db);
	sqlite3_stmt*    update(sqlite3* db);
	sqlite3_stmt*    del(sqlite3* db);
	sqlite3_stmt*    del(sqlite3* db, uint32_t count, va_list args);
	
protected:
		
	int            free_row(uint8_t* row);
	
	char*          m_name;
	
	char*          m_create_sql;
	char*          m_insert_sql;
	char*          m_update_sql;
	char*          m_delete_sql;
	
	Column**       m_columns;
	uint32_t       m_column_count;
	uint32_t       m_column_max;
	
	sqlite3_stmt*  m_prepared_insert;
	sqlite3_stmt*  m_prepared_update;
	sqlite3_stmt*  m_prepared_delete;
	
	uint8_t**      m_results;
	uint32_t       m_result_count;
	uint32_t       m_result_max;

	friend struct Database;
};

#endif
