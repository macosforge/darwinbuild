/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/types.h>
#include <uuid/uuid.h>
#include <sqlite3.h>

struct Archive;
struct File;

typedef int (*ArchiveIteratorFunc)(Archive* archive, void* context);
typedef int (*FileIteratorFunc)(File* file, void* context);

struct Depot {
	Depot();
	Depot(const char* prefix);
	
	virtual ~Depot();

	int initialize();
	int is_initialized();
	
        const char*     prefix();
	const char*	database_path();
	const char*	archives_path();

	virtual int	begin_transaction();
	virtual int	commit_transaction();
	virtual int	rollback_transaction();

	Archive*	archive(uint64_t serial);
	Archive*	archive(uuid_t uuid);
	Archive*	archive(const char* uuid);

	int dump();
	static int dump_archive(Archive* archive, void* context);
	
	int list();
	static int list_archive(Archive* archive, void* context);

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

        // test if the depot is currently locked 
        int is_locked();

	protected:

	// Serialize access to the Depot via flock(2).
	int lock(int operation);
	int unlock(void);

	// Inserts an Archive into the database.
	// This modifies the Archive's serial number.
	// If the Archive already has a serial number, it cannot be inserted.
	int insert(Archive* archive);
	
	// Inserts a File into the database, as part of the specified Archive.
	// This modifies the File's serial number.
	// This modifies the File's Archive pointer.
	// If the File already has a serial number, it cannot be inserted.
	int insert(Archive* archive, File* file);

	// Removes an Archive from the database.
	int remove(Archive* archive);
	
	// Removes a File from the database.
	int remove(File* file);

	int		analyze_stage(const char* path, Archive* archive, Archive* rollback, int* rollback_files);
	int		prune_directories();
	
	// Removes all archive entries which have no corresponding files entries.
	int		prune_archives();

	File*		file_superseded_by(File* file);
	File*		file_preceded_by(File* file);
	File*		file_star_eded_by(File* file, sqlite3_stmt* stmt);

	int		check_consistency();


	virtual int	SQL(const char* fmt, ...);

	sqlite3*	m_db;
	mode_t		m_depot_mode;
        char*           m_prefix;
	char*		m_depot_path;
	char*		m_database_path;
	char*		m_archives_path;
	int		m_lock_fd;
        int             m_is_locked;
};
