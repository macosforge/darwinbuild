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


// how much we grow by when we need more space
#define REALLOC_FACTOR 4


// XXX
void __hex_str(const char* s) {
	int len = strlen(s);
	IF_DEBUG("HEXSTR: %d \n", len);
	for (int i=0; i <= len; i++) {
		IF_DEBUG("%02x", (unsigned int)s[i]);
	}
	IF_DEBUG("\n");
}


Table::Table() {
	m_column_max    = 1;
	m_column_count  = 0;
	m_columns       = (Column**)malloc(sizeof(Column*) * m_column_max);
	m_columns_size  = 0; 
	m_result_max    = 1;
	m_result_count  = 0;
	m_results       = (uint8_t**)malloc(sizeof(uint8_t*) * m_result_max);
	IF_DEBUG("[ALLOC] constructor %p \n", m_results);
	m_name          = strdup("unnamed_table");
	m_create_sql    = NULL;
	m_insert_sql    = NULL;
	m_update_sql    = NULL;
	m_delete_sql    = NULL;
	m_prepared_insert = NULL;
	m_prepared_update = NULL;
	m_prepared_delete = NULL;

}

Table::Table(const char* name) {
	m_column_max    = 1;
	m_column_count  = 0;
	m_columns       = (Column**)malloc(sizeof(Column*) * m_column_max);
	m_columns_size  = 0; 
	m_result_max    = 1;
	m_result_count  = 0;
	m_results       = (uint8_t**)malloc(sizeof(uint8_t*) * m_result_max);
	IF_DEBUG("[ALLOC] constructor %p \n", m_results);
	m_name          = strdup(name);
	m_create_sql    = NULL;
	m_insert_sql    = NULL;
	m_update_sql    = NULL;
	m_delete_sql    = NULL;
	m_prepared_insert = NULL;
	m_prepared_update = NULL;
	m_prepared_delete = NULL;	
}

Table::~Table() {
	for (uint32_t i = 0; i < m_column_count; i++) {
		delete m_columns[i];
	}
	free(m_columns);
	

	for (uint32_t i=0; i < m_result_count; i++) {
		if (m_results[i]) {
			IF_DEBUG("[FREE] destructor loop %d %p \n", i, m_results[i]);
			this->free_result(m_results[i]);
		}
	}
	IF_DEBUG("[FREE] destructor free %p \n", m_results);
	free(m_results);
	
	free(m_name);

	free(m_create_sql);
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

uint32_t Table::row_size() {
	return m_columns_size;
}

const Column** Table::columns() {
	return (const Column**)m_columns;
}


int Table::add_column(Column* c) {
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
	
	return 0;
}

int Table::offset(int column) {
	return this->m_columns[column]->offset();
}

uint8_t* Table::alloc_result() {
	if (m_result_count >= m_result_max) {
		m_results = (uint8_t**)realloc(m_results, m_result_max * sizeof(uint8_t*) * REALLOC_FACTOR);
		IF_DEBUG("[ALLOC] realloc m_results %p \n", m_results);
		if (!m_results) {
			fprintf(stderr, "Error: unable to reallocate memory to add a result row\n");
			return NULL;
		}
		m_result_max *= REALLOC_FACTOR;
	}
	m_result_count++;
	m_results[m_result_count-1] = (uint8_t*)calloc(1, this->row_size());
	IF_DEBUG("[ALLOC] calloc(%d) %d %p \n", this->row_size(), m_result_count-1, 
			 m_results[m_result_count-1]);
	return m_results[m_result_count-1];
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
				IF_DEBUG("[FREE] %s: %p (type of %d: %d) \n", m_name, ptr, i, m_columns[i]->type());
				free(ptr);
				current += sizeof(void*);
		}
	}
	return 0;
}

int Table::free_result(uint8_t* result) {
	for (uint32_t i=0; i < m_result_count; i++) {
		// look for matching result
		if (result == m_results[i]) {
			this->free_row((uint8_t*)m_results[i]);
			IF_DEBUG("[FREE] %s result %d %p \n", m_name, i, m_results[i]);
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

void Table::dump_results() {
	fprintf(stderr, "====================================================================\n");
	for (uint32_t i=0; i < m_result_count; i++) {
		fprintf(stderr, "%p %u:\n", m_results[i], i);
		__data_hex(m_results[i], 48);
	}
	fprintf(stderr, "====================================================================\n");
}

char* Table::create() {
	if (!m_create_sql) {
		uint32_t i = 0;

		// size of "create table ( );" plus table name, plus 1 for each column to separate
		size_t size = strlen(m_name) + 22 + m_column_count;
		for (i=0; i<m_column_count; i++) {		
			// size for column spec
			size += strlen(m_columns[i]->create());
			// size for create index query
			size += 26 + 2*strlen(m_columns[i]->name()) + 2*strlen(m_name);
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
	}
	
	IF_DEBUG("[TABLE] create(): %s \n", m_create_sql);
		
	return m_create_sql;
}

#define __alloc_stmt_query \
	size_t size = 256; \
	size_t used = 0; \
	char* query = (char*)malloc(size); \
	sqlite3_stmt* stmt = (sqlite3_stmt*)malloc(sizeof(sqlite3_stmt*));


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

#define __where_va_columns \
	char tmpstr[256]; \
    char tmp_op = '='; \
    char op = '='; \
    char not_op = ' '; \
	int len; \
	for (uint32_t i=0; i < count; i++) { \
		Column* col = va_arg(args, Column*); \
        tmp_op = va_arg(args, int); \
        if (tmp_op == '!') { \
            not_op = tmp_op; \
        } else { \
            op = tmp_op; \
        } \
        va_arg(args, void*); \
		len = snprintf(tmpstr, 256, " AND %s%c%c?", col->name(), not_op, op); \
		if (len >= 255) { \
			fprintf(stderr, "Error: column name is too big (limit: 248): %s\n", col->name()); \
			return NULL; \
		} \
		used = strlcat(query, tmpstr, size); \
		if (used >= size-1) { \
			size *= 4; \
			query = (char*)realloc(query, size); \
			if (!query) { \
				fprintf(stderr, "Error: ran out of memory!\n"); \
				return NULL; \
			} \
			used = strlcat(query, tmpstr, size); \
		} \
	}

#define __prepare_stmt \
	int res = sqlite3_prepare_v2(db, query, size, &stmt, NULL); \
	free(query); \
	if (res != SQLITE_OK) { \
		fprintf(stderr, "Error: unable to prepare statement: %s\n", sqlite3_errmsg(db)); \
		return NULL; \
	}


sqlite3_stmt* Table::count(sqlite3* db) {
	sqlite3_stmt* stmt = (sqlite3_stmt*)malloc(sizeof(sqlite3_stmt*));
	char* query;
	int size = asprintf(&query, "SELECT count(*) FROM %s ;", m_name) + 1;
	__prepare_stmt;
	return stmt;
}

sqlite3_stmt* Table::count(sqlite3* db, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT count(*) FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	__where_va_columns;
	strlcat(query, ";", size);
	__prepare_stmt;

	return stmt;	
}

sqlite3_stmt* Table::get_column(sqlite3* db, Column* value_column, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT ", size);
	__check_and_cat(value_column->name());
	__check_and_cat(" FROM ");
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	__where_va_columns;
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return stmt;
}

sqlite3_stmt* Table::get_row(sqlite3* db, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT * FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	__where_va_columns;
	strlcat(query, ";", size);
	IF_DEBUG("[TABLE] get_row query: %s \n", query);
	__prepare_stmt;
	
	return stmt;	
}

sqlite3_stmt* Table::get_row_ordered(sqlite3* db, Column* order_by, int order, 
									 uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "SELECT * FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	__where_va_columns;
	__check_and_cat(" ORDER BY ");
	__check_and_cat(order_by->name());
	__check_and_cat((order == ORDER_BY_DESC ? " DESC" : " ASC"));
	strlcat(query, ";", size);
	IF_DEBUG("[TABLE] get_row_ordered query: %s \n", query);
	__prepare_stmt;
	
	return stmt;	
}


sqlite3_stmt* Table::update_value(sqlite3* db, Column* value_column, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "UPDATE ", size);
	__check_and_cat(m_name);
	__check_and_cat(" SET ");
	__check_and_cat(value_column->name());
	__check_and_cat("=? WHERE 1");
	__where_va_columns;
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return stmt;
}

/**
 * Prepare and cache the update statement. 
 * Assumes table only has 1 primary key
 */
sqlite3_stmt* Table::update(sqlite3* db) {
	// we only need to prepare once, return if we already have it
	if (m_prepared_update) {
		IF_DEBUG("[TABLE] %s table found cached update statement at %p \n", m_name, m_prepared_update);
		return m_prepared_update;
	}
	
	IF_DEBUG("[TABLE] %s is generating an update statement \n", m_name);
	
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
	
	IF_DEBUG("[TABLE] prepared update: %s \n", m_update_sql);
	
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
		IF_DEBUG("[TABLE] %s table found cached insert statement at %p \n",
				 m_name, m_prepared_insert);
		return m_prepared_insert;
	}
	
	IF_DEBUG("[TABLE] %s is generating an insert statement \n", m_name);
	
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
	
	IF_DEBUG("[TABLE] prepared insert: %s \n", m_insert_sql);
	
	// prepare
	int res = sqlite3_prepare_v2(db, m_insert_sql, strlen(m_insert_sql), &m_prepared_insert, NULL);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to prepare insert statement for table: %s \n", m_name);
		return NULL;
	}
	return m_prepared_insert;
}


Column* Table::column(uint32_t index) {
	if (index < m_column_count) {
		return this->m_columns[index];
	} else {
		return NULL;
	}
}

uint32_t Table::column_count() {
	return this->m_column_count;
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
	
	IF_DEBUG("[TABLE] prepared delete: %s \n", m_delete_sql);
	
	// prepare
	int res = sqlite3_prepare_v2(db, m_delete_sql, strlen(m_delete_sql), &m_prepared_delete, NULL);
	if (res != SQLITE_OK) {
		fprintf(stderr, "Error: unable to prepare delete statement for table: %s \n", m_name);
		return NULL;
	}
	return m_prepared_delete;
	
}

sqlite3_stmt* Table::del(sqlite3* db, uint32_t count, va_list args) {
	__alloc_stmt_query;
	strlcpy(query, "DELETE FROM ", size);
	__check_and_cat(m_name);
	__check_and_cat(" WHERE 1");
	__where_va_columns;
	strlcat(query, ";", size);
	__prepare_stmt;
	
	return stmt;
}

