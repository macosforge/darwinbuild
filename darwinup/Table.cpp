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
}

Table::Table(const char* name) {
	m_column_max    = 1;
	m_column_count  = 0;
	m_columns       = (Column**)malloc(sizeof(Column*) * m_column_max);
	m_name          = strdup(name);
}

Table::~Table() {
	for (uint32_t i = 0; i < m_column_count; i++) {
		delete m_columns[i];
	}
	free(m_columns);
	free(m_name);
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

