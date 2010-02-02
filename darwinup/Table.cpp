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


Table::Table() {
	m_column_max    = 1;
	m_column_count  = 0;
	m_columns       = (Column**)malloc(sizeof(Column*) * m_column_max);
	m_name          = strdup("unnamed_table");
	m_create_sql    = NULL;
}

Table::Table(const char* name) {
	m_column_max    = 1;
	m_column_count  = 0;
	m_columns       = (Column**)malloc(sizeof(Column*) * m_column_max);
	m_name          = strdup(name);
	m_create_sql    = NULL;
}

Table::~Table() {
	for (uint32_t i = 0; i < m_column_count; i++) {
		delete m_columns[i];
	}
	free(m_columns);
	free(m_name);
	free(m_create_sql);
}


const char* Table::name() {
	return m_name;
}

const Column** Table::columns() {
	return (const Column**)m_columns;
}


bool Table::add_column(Column* c) {
	if (m_column_count >= m_column_max) {
		m_columns = (Column**)realloc(m_columns, m_column_max * sizeof(Column*) * 4);
		if (!m_columns) {
			fprintf(stderr, "Error: unable to reallocate memory to add a column\n");
			return false;
		}
		m_column_max *= 4;
	}
	m_columns[m_column_count++] = c;
	
	return true;
}

char* Table::count(const char* where) {
	IF_DEBUG("[TABLE] entering count of %s with where: %s \n", m_name, where);
	char* buf;
	if (where) {
		IF_DEBUG("[TABLE] counting %s with where %s \n", m_name, where);
		asprintf(&buf, "SELECT count(*) FROM %s %s ;", m_name, where);
	} else {
		IF_DEBUG("[TABLE] counting %s \n", m_name);
		asprintf(&buf, "SELECT count(*) FROM %s ;", m_name);
	}
	IF_DEBUG("[TABLE] count() returning: %s \n", buf);
	return buf;
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



