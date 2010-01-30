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
}

Database::Database(const char* path) {
	m_table_max = 1;
	m_table_count = 0;
	m_tables = (Table**)malloc(sizeof(Table*) * m_table_max);
	m_db = NULL;		
	m_path = strdup(path);
}

Database::~Database() {
	for (uint32_t i = 0; i < m_table_count; i++) {
		delete m_tables[i];
	}
	free(m_tables);
	free(m_path);
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


DarwinupDatabase::DarwinupDatabase() {
	m_path = strdup("");
}

DarwinupDatabase::DarwinupDatabase(const char* path) {
	m_path = strdup(path);
	
	Table* archives = new Table("archives");
	assert(archives->add_column(new Column("serial", SQLITE_INTEGER, false, true, false)));
	assert(archives->add_column(new Column("uuid", SQLITE_BLOB, true, false, true)));
	assert(archives->add_column(new Column("name", SQLITE3_TEXT)));
	assert(archives->add_column(new Column("date_added", SQLITE_INTEGER)));
	assert(archives->add_column(new Column("active", SQLITE_INTEGER)));
	assert(archives->add_column(new Column("info", SQLITE_INTEGER)));
	assert(add_table(archives));

	Table* files = new Table("files");
	assert(files->add_column(new Column("serial", SQLITE_INTEGER, false, true, false)));
	assert(files->add_column(new Column("archive", SQLITE_INTEGER)));
	assert(files->add_column(new Column("info", SQLITE_INTEGER)));
	assert(files->add_column(new Column("mode", SQLITE_INTEGER)));
	assert(files->add_column(new Column("uid", SQLITE_INTEGER)));
	assert(files->add_column(new Column("gid", SQLITE_INTEGER)));
	assert(files->add_column(new Column("size", SQLITE_INTEGER)));
	assert(files->add_column(new Column("digest", SQLITE_BLOB)));
	assert(files->add_column(new Column("path", SQLITE3_TEXT)));
	assert(add_table(files));
}

DarwinupDatabase::~DarwinupDatabase() {
}


