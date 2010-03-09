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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "Table.h"
#include "Database.h"


Table::Table(const char* name) {
	m_column_max    = 2;
	m_column_count  = 0;
	m_columns       = (Column**)malloc(sizeof(Column*) * m_column_max);
	m_columns_size  = 0; 
	m_result_max    = 1;
	m_result_count  = 0;
	m_results       = (uint8_t**)malloc(sizeof(uint8_t*) * m_result_max);
	m_name          = strdup(name);
	m_create_sql    = NULL;
	m_custom_create_sql    = NULL;
	m_insert_sql    = NULL;
	m_update_sql    = NULL;
	m_delete_sql    = NULL;
	m_prepared_insert = NULL;
	m_prepared_update = NULL;
	m_prepared_delete = NULL;
	m_version       = 0;
}

Table::~Table() {
	for (uint32_t i = 0; i < m_column_count; i++) {
		delete m_columns[i];
	}
	free(m_columns);
	

	for (uint32_t i=0; i < m_result_count; i++) {
		if (m_results[i]) {
			this->free_result(m_results[i]);
		}
	}
	free(m_results);
	
	free(m_name);

	free(m_create_sql);
	free(m_custom_create_sql);
	free(m_insert_sql);
	free(m_update_sql);
	free(m_delete_sql);
	
	sqlite3_finalize(m_prepared_insert);
	sqlite3_finalize(m_prepared_update);
	sqlite3_finalize(m_prepared_delete);
	
}

const char* Table::name() {
	return m_name;
}

uint32_t Table::version() {
	return m_version;
}

int Table::set_custom_create(const char* sql) {
	this->m_custom_create_sql = strdup(sql);
	return this->m_custom_create_sql == 0;
}

int Table::add_column(Column* c, uint32_t schema_version) {
	// accumulate offsets for columns in m_columns_size
	c->m_offset = this->m_columns_size;
	this->m_columns_size += c->size();
	
	// reallocate if needed
	if (m_column_count >= m_column_max) {
		m_columns = (Column**)realloc(m_columns, m_column_max * sizeof(Column*) * REALLOC_FACTOR);
		if (!m_columns) {
			fprintf(stderr, "Error: unable to reallocate memory to add a column\n");
			return 1;
		}
		m_column_max *= REALLOC_FACTOR;
	}
	m_columns[m_column_count++] = c;
	c->m_version = schema_version;
	return 0;
}

Column* Table::column(uint32_t index) {
	if (index < m_column_count) {
		return this->m_columns[index];
	} else {
		return NULL;
	}
}

int Table::offset(uint32_t index) {
	return this->m_columns[index]->offset();
}

uint32_t Table::row_size() {
	return m_columns_size;
}

uint8_t* Table::alloc_result() {
	if (m_result_count >= m_result_max) {
		m_results = (uint8_t**)realloc(m_results, m_result_max * sizeof(uint8_t*) * REALLOC_FACTOR);
		if (!m_results) {
			fprintf(stderr, "Error: unable to reallocate memory to add a result row\n");
			return NULL;
		}
		m_result_max *= REALLOC_FACTOR;
	}
	m_result_count++;
	m_results[m_result_count-1] = (uint8_t*)calloc(1, this->row_size());
	return m_results[m_result_count-1];
}

int Table::free_result(uint8_t* result) {
	for (uint32_t i=0; i < m_result_count; i++) {
		// look for matching result
		if (result == m_results[i]) {
			this->free_row((uint8_t*)m_results[i]);
			free(m_results[i]);
			m_results[i] = NULL;
			// if we did not free the last result,
			// move last result to the empty slot
			if (i != (m_result_count - 1)) {
				m_results[i] = m_results[m_result_count-1];
				m_results[m_result_count-1] = NULL;
			}
			m_result_count--;
		}
	}
	return 0;
}

sqlite3_stmt* Table::count(sqlite3* db) {
	sqlite3_stmt* stmt = (sqlite3_stmt*)malloc(sizeof(sqlite3_stmt*));
	char* query;
	int size = asprintf(&query, "SELECT count(*) FROM %s ;", m_name) + 1;
    int res = sqlite3_prepare_v2(db, query, size, &stmt, NULL); \
    free(query); \
    if (res != SQLITE_OK) { \
        fprintf(stderr, "Error: unable to prepare statement: %s\n", \
				sqlite3_errmsg(db)); \
        return NULL; \
    }	
	return stmt;
}

/**
 * Prepare and cache the update statement. 
 * Assumes table only has 1 primary key
 */
sqlite3_stmt* Table::update(sqlite3* db) {
	// we only need to prepare once, return if we already have it
	if (m_prepared_update) {
		return m_prepared_update;
	}
	uint32_t i = 0;
	bool comma = false;  // flag we set to start adding commas
	
	// calculate the length of the sql statement
	size_t size = 27 + 5*m_column_count;
	for (i=0; i<m_column_count; i++) {
		size += strlen(m_columns[i]->name());
	}
	
	// generate the sql query
	m_update_sql = (char*)malloc(size);
	strlcpy(m_update_sql, "UPDATE ", size);
	strlcat(m_update_sql, m_name, size);
	strlcat(m_update_sql, " SET ", size);
	for (i=0; i<m_column_count; i++) {
		// comma separate after 0th column
		if (comma) strlcat(m_update_sql, ", ", size);
		// primary keys do not get inserted
		if (!m_columns[i]->is_pk()) {
			strlcat(m_update_sql, m_columns[i]->name(), size);
			strlcat(m_update_sql, "=?", size);
			comma = true;
		}
	}
	
	// WHERE statement using primary keys
	strlcat(m_update_sql, " WHERE ", size);
	for (i=0; i<m_column_count; i++) {
		if (m_columns[i]->is_pk()) {
			strlcat(m_update_sql, m_columns[i]->name(), size);
			strlcat(m_update_sql, "=?", size);
			break;
		}
	}
	strlcat(m_update_sql, ";", size);
	
	// prepare
	int res = sqlite3_prepare_v2(db, m_update_sql, strlen(m_update_sql), &m_prepared_update, NULL);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to prepare update statement for table: %s \n", m_name);
		return NULL;
	}
	return m_prepared_update;
}


sqlite3_stmt* Table::insert(sqlite3* db) {
	// we only need to prepare once, return if we already have it
	if (m_prepared_insert) {
		return m_prepared_insert;
	}
	
	uint32_t i = 0;
	bool comma = false;  // flag we set to start adding commas
	
	// calculate the length of the sql statement
	size_t size = 27 + 5*m_column_count;
	for (i=0; i<m_column_count; i++) {
		size += strlen(m_columns[i]->name());
	}
	
	// generate the sql query
	m_insert_sql = (char*)malloc(size);
	strlcpy(m_insert_sql, "INSERT INTO ", size);
	strlcat(m_insert_sql, m_name, size);
	strlcat(m_insert_sql, " (", size);
	for (i=0; i<m_column_count; i++) {
		// comma separate after 0th column
		if (comma) strlcat(m_insert_sql, ", ", size);
		// primary keys do not get inserted
		if (!m_columns[i]->is_pk()) {
			strlcat(m_insert_sql, m_columns[i]->name(), size);
			comma = true;
		}
	}
	comma = false;
	strlcat(m_insert_sql, ") VALUES (", size);
	for (i=0; i<m_column_count; i++) {
		// comma separate after 0th column
		if (comma) strlcat(m_insert_sql, ", ", size); 
		// primary keys do not get inserted
		if (!m_columns[i]->is_pk()) {
			strlcat(m_insert_sql, "?", size);
			comma = true;
		}
	}
	strlcat(m_insert_sql, ");", size);
	
	// prepare
	int res = sqlite3_prepare_v2(db, m_insert_sql, strlen(m_insert_sql), &m_prepared_insert, NULL);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to prepare insert statement for table: %s \n", m_name);
		return NULL;
	}
	return m_prepared_insert;
}

sqlite3_stmt* Table::del(sqlite3* db) {
	// we only need to prepare once, return if we already have it
	if (m_prepared_delete) return m_prepared_delete;
	
	uint32_t i = 0;
	
	// generate the sql query
	size_t size = 22 + strlen(m_name);
	for (i=0; i<m_column_count; i++) {
		if (m_columns[i]->is_pk()) {
			size += strlen(m_columns[i]->name()) + 2;
			break;
		}
	}
	m_delete_sql = (char*)malloc(size);
	strlcpy(m_delete_sql, "DELETE FROM ", size);
	strlcat(m_delete_sql, m_name, size);
	
	// WHERE statement using primary keys
	strlcat(m_delete_sql, " WHERE ", size);
	for (i=0; i<m_column_count; i++) {
		if (m_columns[i]->is_pk()) {
			strlcat(m_delete_sql, m_columns[i]->name(), size);
			strlcat(m_delete_sql, "=?", size);
			break;
		}
	}
	strlcat(m_delete_sql, ";", size);
	
	// prepare
	int res = sqlite3_prepare_v2(db, m_delete_sql, strlen(m_delete_sql), &m_prepared_delete, NULL);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to prepare delete statement for table: %s \n", m_name);
		return NULL;
	}
	return m_prepared_delete;
	
}

#define __alloc_stmt_query \
	size_t size = 256; \
	size_t used = 0; \
	char* query = (char*)malloc(size); \
	sqlite3_stmt** pps = (sqlite3_stmt**)malloc(sizeof(sqlite3_stmt*));

#define __check_and_cat(text) \
	used = strlcat(query, text, size); \
    if (used >= size-1) { \
        size *= 4; \
        query = (char*)realloc(query, size); \
        if (!query) { \
			fprintf(stderr, "Error: ran out of memory!\n"); \
            return NULL; \
        } \
        used = strlcat(query, text, size); \
    }

#define __prepare_stmt \
    int res = sqlite3_prepare_v2(db, query, size, pps, NULL); \
    free(query); \
    if (res != SQLITE_OK) { \
        fprintf(stderr, "Error: unable to prepare statement: %s\n", \
				sqlite3_errmsg(db)); \
        return NULL; \
    }

sqlite3_stmt** Table::count(sqlite3* db, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT count(*) FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	this->where_va_columns(count, query, size, &used, args);
	strlcat(query, ";", size);
	__prepare_stmt;

	return pps;	
}

sqlite3_stmt** Table::get_column(sqlite3* db, Column* value_column, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT ", size);
	__check_and_cat(value_column->name());
	__check_and_cat(" FROM ");
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	this->where_va_columns(count, query, size, &used, args);
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return pps;
}

sqlite3_stmt** Table::get_row(sqlite3* db, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT * FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	this->where_va_columns(count, query, size, &used, args);
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return pps;	
}

sqlite3_stmt** Table::get_row_ordered(sqlite3* db, Column* order_by, int order, 
									 uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT * FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	this->where_va_columns(count, query, size, &used, args);
	__check_and_cat(" ORDER BY ");
	__check_and_cat(order_by->name());
	__check_and_cat((order == ORDER_BY_DESC ? " DESC" : " ASC"));
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return pps;	
}

sqlite3_stmt** Table::update_value(sqlite3* db, Column* value_column, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "UPDATE ", size);
	__check_and_cat(m_name);
	__check_and_cat(" SET ");
	__check_and_cat(value_column->name());
	__check_and_cat("=? WHERE 1");
	this->where_va_columns(count, query, size, &used, args);
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return pps;
}

sqlite3_stmt** Table::del(sqlite3* db, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "DELETE FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	this->where_va_columns(count, query, size, &used, args);
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return pps;
}

const char* Table::create() {
	size_t size = 0;
	if (!m_create_sql) {
		uint32_t i = 0;
		
		// size of "create table ( );" plus table name, plus 1 for each column to separate
		size = strlen(m_name) + 22 + m_column_count;
		for (i=0; i<m_column_count; i++) {		
			// size for column spec
			size += strlen(m_columns[i]->create());
			// size for create index query
			size += 26 + 2*strlen(m_columns[i]->name()) + 2*strlen(m_name);
			// custom sql
			if (m_custom_create_sql) size += strlen(m_custom_create_sql);
		}
		
		// create creation sql
		m_create_sql = (char*)malloc(size);
		strlcpy(m_create_sql, "CREATE TABLE ", size);
		strlcat(m_create_sql, m_name, size);
		strlcat(m_create_sql, " (", size);
		// get creation sql for each column
		for (i=0; i<m_column_count; i++) {
			if (i) strlcat(m_create_sql, ", ", size); // comma separate after 0th column
			strlcat(m_create_sql, m_columns[i]->create(), size);
		}
		strlcat(m_create_sql, "); ", size);
		
		for (i=0; i<m_column_count; i++) {
			if (m_columns[i]->is_index()) {
				char* buf;
				asprintf(&buf, "CREATE INDEX %s_%s ON %s (%s);", 
						 m_name, m_columns[i]->name(), m_name, m_columns[i]->name());
				strlcat(m_create_sql, buf, size);
				free(buf);
			}
		}
		if (m_custom_create_sql) strlcat(m_create_sql, m_custom_create_sql, size);
	}

	return (const char*)m_create_sql;
}

const char* Table::alter_add_column(uint32_t index) {
	if (m_columns[index]) return m_columns[index]->alter(m_name);
	return NULL;
}

int Table::where_va_columns(uint32_t count, char* query, size_t size, 
							size_t* used, va_list args) {
	char tmpstr[256];
    char tmp_op = '=';
    char op = '=';
    char not_op = ' ';
	int len;
	for (uint32_t i=0; i < count; i++) {
		Column* col = va_arg(args, Column*);
        tmp_op = va_arg(args, int);
        if (tmp_op == '!') {
            not_op = tmp_op;
        } else {
            op = tmp_op;
        }
        va_arg(args, void*);
        if (col->type() == SQLITE_BLOB) va_arg(args, uint32_t);
		len = snprintf(tmpstr, 256, " AND %s%c%c?", col->name(), not_op, op);
		if (len >= 255) {
			fprintf(stderr, "Error: column name is too big (limit: 248): %s\n", 
					col->name());
			return NULL;
		}
		*used = strlcat(query, tmpstr, size);
		if (*used >= size-1) {
			size *= 4;
			query = (char*)realloc(query, size);
			if (!query) {
				fprintf(stderr, "Error: ran out of memory!\n");
				return -1;
			}
			*used = strlcat(query, tmpstr, size);
		}
	}
	
	return 0;
}

const Column** Table::columns() {
	return (const Column**)m_columns;
}

uint32_t Table::column_count() {
	return this->m_column_count;
}

int Table::free_row(uint8_t* row) {
	uint8_t* current = row;
	void* ptr;
	for (uint32_t i=0; i < m_column_count; i++) {
		switch (m_columns[i]->type()) {
			case SQLITE_INTEGER:
				current += sizeof(uint64_t);
				// nothing to free
				break;
			default:
				memcpy(&ptr, current, sizeof(void*));
				free(ptr);
				current += sizeof(void*);
		}
	}
	return 0;
}

void Table::dump_results(FILE* f) {
	fprintf(f, "====================================================================\n");
	for (uint32_t i=0; i < m_result_count; i++) {
		fprintf(f, "%p %u:\n", m_results[i], i);
		__data_hex(f, m_results[i], 48);
	}
	fprintf(f, "====================================================================\n");
}
