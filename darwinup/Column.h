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

#ifndef _COLUMN_H
#define _COLUMN_H

#include <stdint.h>
#include <sqlite3.h>

#include "Utils.h"

/**
 * Column objects represent a column in a database table. They store
 *  type information and chunks of sql for their Table to build
 *  queries with. 
 */
struct Column {
	Column(const char* name, uint32_t type);
	Column(const char* name, uint32_t type,
		   bool is_index, bool is_pk, bool is_unique);
	virtual ~Column();
	
	const char*    name();
	uint32_t       type();
	const bool     is_index();
	const bool     is_pk();
	const bool     is_unique();

	uint32_t       version();
	
	// return size of this column when packed into a result record
	uint32_t       size();

protected:
	// return a string representation  of this columns type suitable 
	//  for sql queries
	const char*    typestr();
	
	// return the offset of this column in the Table's result record
	int            offset();
	
	// generate the sql needed to create this column
	const char*    create();
	// generate alter table sql for this column for table named table_name
	const char*    alter(const char* table_name);

	char*          m_name;
	uint32_t       m_version; // schema version this was added
	char*          m_create_sql; // sql fragment for use in CREATE TABLE
	char*          m_alter_sql; // entire ALTER TABLE ADD COLUMN sql
	uint32_t       m_type; // SQLITE_* type definition
	bool           m_is_index;
	bool           m_is_pk;
	bool           m_is_unique;
	int            m_offset;
	
	friend struct Table;
};

#endif
