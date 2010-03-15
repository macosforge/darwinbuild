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

#ifndef _DB_H
#define _DB_H

#include <stdint.h>
#include <assert.h>
#include <uuid/uuid.h>
#include <time.h>

#include "Database.h"
#include "Table.h"
#include "Archive.h"
#include "Digest.h"
#include "File.h"


/**
 *
 * Darwinup database abstraction. This class is responsible
 *  for generating the Table and Column objects that make
 *  up the darwinup database schema, but the parent handles
 *  deallocation. 
 *
 */
struct DarwinupDatabase : Database {
	DarwinupDatabase(const char* path);
	virtual ~DarwinupDatabase();
	int init_schema();
	
	uint64_t count_files(Archive* archive, const char* path);
	uint64_t count_archives(bool include_rollbacks);
	
	// Archives
	Archive* make_archive(uint8_t* data);
	int      get_archives(uint8_t*** data, uint32_t* count, bool include_rollbacks);
	int      get_archive(uint8_t** data, uuid_t uuid);
	int      get_archive(uint8_t** data, uint64_t serial);
	int      get_archive(uint8_t** data, const char* name);
	int      get_archive(uint8_t** data, archive_keyword_t keyword);
	int      get_inactive_archive_serials(uint64_t** serials, uint32_t* count);
	int      archive_offset(int column);
	int      activate_archive(uint64_t serial);
	int      deactivate_archive(uint64_t serial);
	int      update_archive(uint64_t serial, uuid_t uuid, const char* name,
							time_t date_added, uint32_t active, uint32_t info);
	uint64_t insert_archive(uuid_t uuid, uint32_t info, const char* name, 
							time_t date, const char* build);
	int      delete_empty_archives();
	int      delete_archive(Archive* archive);
	int      delete_archive(uint64_t serial);
	int      free_archive(uint8_t* data);

	// Files
	File*    make_file(uint8_t* data);
	int      get_next_file(uint8_t** data, File* file, file_starseded_t star);
	int      get_file_serials(uint64_t** serials, uint32_t* count);
	int      get_file_serial_from_archive(Archive* archive, const char* path, 
										  uint64_t** serial);
	int      get_files(uint8_t*** data, uint32_t* count, Archive* archive, bool reverse);
	int      file_offset(int column);
	int      update_file(uint64_t serial, Archive* archive, uint32_t info, mode_t mode, 
						 uid_t uid, gid_t gid, Digest* digest, const char* path);
	uint64_t insert_file(uint32_t info, mode_t mode, uid_t uid, gid_t gid, 
						 Digest* digest, Archive* archive, const char* path);
	int      delete_file(uint64_t serial);
	int      delete_file(File* file);
	int      delete_files(Archive* archive);
	int      free_file(uint8_t* data);
	
	// memoization
	Archive* get_last_archive(uint64_t serial);
	int      clear_last_archive();
	int      set_last_archive(uint8_t* data);
	

protected:
	
	int      set_archive_active(uint64_t serial, uint64_t* active);
	
	Table*        m_archives_table;
	Table*        m_files_table;
	
	// memoize some get_archive calls
	Archive*      last_archive;
	
};

#endif

