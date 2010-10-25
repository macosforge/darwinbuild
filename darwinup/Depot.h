/*
 * Copyright (c) 2005-2010 Apple Computer, Inc. All rights reserved.
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

#ifndef _DEPOT_H
#define _DEPOT_H

#include <Availability.h>
#include <sys/types.h>
#include <uuid/uuid.h>
#include "DB.h"
#include "Archive.h"

#define DEPOT_OK              0
#define DEPOT_ERROR          -1
#define DEPOT_NOT_EXIST      -2
#define DEPOT_PERM_DENIED    -3
#define DEPOT_OBJ_CHANGE     -4
#define DEPOT_BUILD_MISMATCH -5
#define DEPOT_USAGE_ERROR    -6
#define DEPOT_PREINSTALL_ERR -7


struct Archive;
struct File;
struct DarwinupDatabase;

typedef int (*ArchiveIteratorFunc)(Archive* archive, void* context);
typedef int (*FileIteratorFunc)(File* file, void* context);

struct Depot {
	Depot();
	Depot(const char* prefix);
	
	virtual ~Depot();

	// establish database connection
	int connect();

	// create directories we need for storage
	int create_storage();
	
	// use initialize() to connect to database 
	//  and (optionally) create the storage directories
	int initialize(bool writable);
	int is_initialized();
	
	const char* prefix();
	const char*	database_path();
	const char*	archives_path();
	const char*	downloads_path();

	virtual int	begin_transaction();
	virtual int	commit_transaction();
	virtual int	rollback_transaction();

	Archive* archive(uint64_t serial);
	Archive* archive(uuid_t uuid);
	Archive* archive(archive_name_t name);
	Archive* archive(archive_keyword_t keyword);
	Archive* get_archive(const char* arg);

	// returns a list of Archive*. Caller must free the list. 
	Archive** get_all_archives(uint32_t *count);
	Archive** get_superseded_archives(uint32_t *count);
	uint64_t count_archives();
	
	int dump();
	static int dump_archive(Archive* archive, void* context);
	
	int list();
	int list(int count, char** args);
	static int list_archive(Archive* archive, void* context);

	int install(const char* path);
	int install(Archive* archive);
	static int install_file(File* file, void* context);
	static int backup_file(File* file, void* context);

	int uninstall(Archive* archive);
	static int uninstall_file(File* file, void* context);

	int verify(Archive* archive);
	static int verify_file(File* file, void* context);

	int files(Archive* archive);
	static int print_file(File* file, void* context);

	int iterate_files(Archive* archive, FileIteratorFunc func, void* context);
	int iterate_archives(ArchiveIteratorFunc func, void* context);

	// processes an archive according to command
	//  arg is an archive identifier, such as serial or uuid
	int dispatch_command(Archive* archive, const char* command);
	int process_archive(const char* command, const char* archspec);
	
	int rename_archive(const char* archspec, const char* name);
	
	// test if the depot is currently locked 
	int is_locked();

	bool is_superseded(Archive* archive);

	void    archive_header();
	
	bool    is_dirty();
	bool    has_modified_extensions();
	
protected:

	// Serialize access to the Depot via flock(2).
	int     lock(int operation);
	int     unlock(void);

	// Inserts an Archive into the database.
	// This modifies the Archive's serial number.
	// If the Archive already has a serial number, it cannot be inserted.
	int     insert(Archive* archive);
	
	// Inserts a File into the database, as part of the specified Archive.
	// This modifies the File's serial number.
	// This modifies the File's Archive pointer.
	// If the File already has a serial number, it cannot be inserted.
	int     insert(Archive* archive, File* file);
	
	int     has_file(Archive* archive, File* file);
	
	// Removes an Archive from the database.
	int     remove(Archive* archive);
	
	// Removes a File from the database.
	int     remove(File* file);

	int		analyze_stage(const char* path, Archive* archive, Archive* rollback, int* rollback_files);

	// removes expand and unexpanded files from archives path
	int		prune_directories();
	int		prune_archive(Archive* archive);
	
	File*	file_superseded_by(File* file);
	File*	file_preceded_by(File* file);

	int		check_consistency();
	
	DarwinupDatabase* m_db;
	
	mode_t		m_depot_mode;
	char*       m_prefix;
	char*		m_depot_path;
	char*		m_database_path;
	char*		m_archives_path;
	char*		m_downloads_path;
	char*       m_build;
	int		    m_lock_fd;
	int         m_is_locked;
	bool        m_is_dirty; // track if we need to update dyld cache
	bool        m_modified_extensions; // track if we need to touch /S/L/E

};

#endif
