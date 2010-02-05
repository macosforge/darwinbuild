/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#include "Archive.h"
#include "Depot.h"
#include "File.h"
#include "SerialSet.h"
#include "Utils.h"

#include <assert.h>
#include <copyfile.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sqlite3.h>

Depot::Depot() {
	m_prefix = NULL;
	m_depot_path = NULL;
	m_database_path = NULL;
	m_archives_path = NULL;
	m_downloads_path = NULL;
	m_db = NULL;
	m_lock_fd = -1;
	m_is_locked = 0;
	m_depot_mode = 0750;
}

Depot::Depot(const char* prefix) {
	m_lock_fd = -1;
	m_is_locked = 0;
	m_depot_mode = 0750;

	asprintf(&m_prefix, "%s", prefix);
	join_path(&m_depot_path, m_prefix, "/.DarwinDepot");
	join_path(&m_database_path, m_depot_path, "/Database-V100");
	join_path(&m_archives_path, m_depot_path, "/Archives");
	join_path(&m_downloads_path, m_depot_path, "/Downloads");
}

Depot::~Depot() {
	if (m_lock_fd != -1)	this->unlock();
	if (m_db)		sqlite3_close(m_db);
	if (m_prefix)           free(m_prefix);
	if (m_depot_path)	free(m_depot_path);
	if (m_database_path)	free(m_database_path);
	if (m_archives_path)	free(m_archives_path);
	if (m_downloads_path)	free(m_downloads_path);
}

const char*	Depot::archives_path()		{ return m_archives_path; }
const char*	Depot::downloads_path()		{ return m_downloads_path; }
const char*     Depot::prefix()                 { return m_prefix; }

// Initialize the depot storage on disk
int Depot::initialize() {
	int res = 0;
	
	// initialization requires all these paths to be set
	if (!(m_prefix && m_depot_path && m_database_path && m_archives_path && m_downloads_path)) {
		return -1;
	}
	
	res = mkdir(m_depot_path, m_depot_mode);
	if (res && errno != EEXIST) {
		perror(m_depot_path);
		return res;
	}
	res = mkdir(m_archives_path, m_depot_mode);
	if (res && errno != EEXIST) {
		perror(m_archives_path);
		return res;
	}
	
	res = mkdir(m_downloads_path, m_depot_mode);
	if (res && errno != EEXIST) {
		perror(m_downloads_path);
		return res;
	}

	res = this->lock(LOCK_SH);
	if (res) return res;
	m_is_locked = 1;
	
	int exists = is_regular_file(m_database_path);
	
	res = sqlite3_open(m_database_path, &m_db);
	if (res) {
		sqlite3_close(m_db);
		m_db = NULL;
	}
	
	if (m_db && !exists) {
		this->SQL("CREATE TABLE archives (serial INTEGER PRIMARY KEY AUTOINCREMENT, uuid BLOB UNIQUE, name TEXT, date_added INTEGER, active INTEGER, info INTEGER)");
		this->SQL("CREATE TABLE files (serial INTEGER PRIMARY KEY AUTOINCREMENT, archive INTEGER, info INTEGER, mode INTEGER, uid INTEGER, gid INTEGER, size INTEGER, digest BLOB, path TEXT)");
		this->SQL("CREATE INDEX archives_uuid ON archives (uuid)");
		this->SQL("CREATE INDEX files_path ON files (path)");
	}
	
	return res;
}

int Depot::is_initialized() {
	return (m_db != NULL);
}

// Unserialize an archive from the database.
// Find the archive by UUID.
// XXX: should be memoized
Archive* Depot::archive(uuid_t uuid) {
	int res = 0;
	Archive* archive = NULL;
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT serial, name, info, date_added FROM archives WHERE uuid=?";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		res = sqlite3_bind_blob(stmt, 1, uuid, sizeof(uuid_t), SQLITE_STATIC);
		if (res == 0) res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			uint64_t serial = sqlite3_column_int64(stmt, 0);
			const unsigned char* name = sqlite3_column_text(stmt, 1);
			uint64_t info = sqlite3_column_int64(stmt, 2);
			time_t date_added = sqlite3_column_int(stmt, 3);
			archive = new Archive(serial, uuid, (const char*)name, NULL, info, date_added);
		}
		sqlite3_reset(stmt);
	}
	return archive;
}

// Unserialize an archive from the database.
// Find the archive by serial.
// XXX: should be memoized
Archive* Depot::archive(uint64_t serial) {
	int res = 0;
	Archive* archive = NULL;
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT uuid, name, info, date_added FROM archives WHERE serial=?";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		res = sqlite3_bind_int64(stmt, 1, serial);
		if (res == 0) res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			uuid_t uuid;
			const void* blob = sqlite3_column_blob(stmt, 0);
			int blobsize = sqlite3_column_bytes(stmt, 0);
			if (blobsize > 0) {
				assert(blobsize == sizeof(uuid_t));
				memcpy(uuid, blob, sizeof(uuid_t));
			} else {
				uuid_clear(uuid);
			}
			const unsigned char* name = sqlite3_column_text(stmt, 1);
			uint64_t info = sqlite3_column_int64(stmt, 2);
			time_t date_added = sqlite3_column_int(stmt, 3);
			archive = new Archive(serial, uuid, (const char*)name, NULL, info, date_added);
		}
		sqlite3_reset(stmt);
	}
	return archive;
}

// Unserialize an archive from the database.
// Find the last archive installed with this name
Archive* Depot::archive(archive_name_t name) {
	int res = 0;
	Archive* archive = NULL;
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT serial, uuid, info, date_added FROM archives WHERE name=? ORDER BY date_added DESC LIMIT 1";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		res = sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
		if (res == 0) res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			uuid_t uuid;
			const void* blob = sqlite3_column_blob(stmt, 1);
			int blobsize = sqlite3_column_bytes(stmt, 1);
			if (blobsize > 0) {
				assert(blobsize == sizeof(uuid_t));
				memcpy(uuid, blob, sizeof(uuid_t));
			} else {
				uuid_clear(uuid);
			}
			uint64_t serial = sqlite3_column_int64(stmt, 0);
			uint64_t info = sqlite3_column_int64(stmt, 2);
			time_t date_added = sqlite3_column_int(stmt, 3);
			archive = new Archive(serial, uuid, (const char*)name, NULL, info, date_added);
		}
		sqlite3_reset(stmt);
	}
	return archive;
}

Archive* Depot::archive(archive_keyword_t keyword) {
	int res = 0;
	Archive* archive = NULL;
	static sqlite3_stmt* stmt = NULL;
	const char* query = NULL;
	if (stmt == NULL && m_db) {
		if (keyword == DEPOT_ARCHIVE_NEWEST) {
			query = "SELECT serial FROM archives WHERE name != '<Rollback>' ORDER BY date_added DESC LIMIT 1";
		} else if (keyword == DEPOT_ARCHIVE_OLDEST) {
			query = "SELECT serial FROM archives WHERE name != '<Rollback>' ORDER BY date_added ASC LIMIT 1";
		} else {
			fprintf(stderr, "Error: unknown archive keyword.\n");
			res = -1;
		}
		if (res == 0) res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			uint64_t serial = sqlite3_column_int64(stmt, 0);
			archive = Depot::archive(serial);
		}
		sqlite3_reset(stmt);
	}
	return archive;	
}

// create a new Archive from a database row
Archive* Depot::archive(sqlite3_stmt* stmt) {
	uuid_t uuid;
	uint64_t serial = sqlite3_column_int64(stmt, 0);
	const void* blob = sqlite3_column_blob(stmt, 1);
	int blobsize = sqlite3_column_bytes(stmt, 1);
	const unsigned char* name = sqlite3_column_text(stmt, 2);
	uint64_t info = sqlite3_column_int64(stmt, 3);
	time_t date_added = sqlite3_column_int(stmt, 4);
	if (blobsize > 0) {
		assert(blobsize == sizeof(uuid_t));
		memcpy(uuid, blob, sizeof(uuid_t));
	} else {
		uuid_clear(uuid);
	}
	return new Archive(serial, uuid, (const char*)name, NULL, info, date_added);	
}


// Return Archive from database matching arg, which is one of:
//
//   uuid (ex: 22969F32-9C4F-4370-82C8-DD3609736D8D)
//   serial (ex: 12)
//   name  (ex root.tar.gz)
//   keyword (either "newest" for the most recent root installed
//            or     "oldest" for the oldest installed root)
//   
Archive* Depot::get_archive(const char* arg) {
	uuid_t uuid;
	uint64_t serial; 
	if (uuid_parse(arg, uuid) == 0) {
		return Depot::archive(uuid);
	}
	serial = strtoull(arg, NULL, 0);
	if (serial) {
		return Depot::archive(serial);
	}
	if (strncasecmp("oldest", arg, 6) == 0) {
		IF_DEBUG("looking for oldest\n");
		return Depot::archive(DEPOT_ARCHIVE_OLDEST);
	}
	if (strncasecmp("newest", arg, 6) == 0) {
		IF_DEBUG("looking for newest\n");
		return Depot::archive(DEPOT_ARCHIVE_NEWEST);
	}
	return Depot::archive((archive_name_t)arg);
}

Archive** Depot::get_all_archives(size_t* count) {
	extern uint32_t verbosity;
	int res = 0;
	*count = this->count_archives();
	Archive** list = (Archive**)malloc(*count * sizeof(Archive*));
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT serial, uuid, name, info, date_added FROM archives WHERE name != '<Rollback>' ORDER BY serial DESC";
		if (verbosity & VERBOSE_DEBUG) {
			query = "SELECT serial, uuid, name, info, date_added FROM archives ORDER BY serial DESC";
		}
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		size_t i = 0;
		while (res != SQLITE_DONE) {
			res = sqlite3_step(stmt);
			if (res == SQLITE_ROW) {
				list[i++] = this->archive(stmt);
			} 
		}
		sqlite3_reset(stmt);
	}
	return list;	
}

size_t Depot::count_archives() {
	extern uint32_t verbosity;
	int res = 0;
	size_t count = 0;
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT count(*) FROM archives WHERE name != '<Rollback>'";
		if (verbosity & VERBOSE_DEBUG) {
			query = "SELECT count(*) FROM archives";
		}
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			count = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_reset(stmt);
	}
	return count;
}

int Depot::iterate_archives(ArchiveIteratorFunc func, void* context) {
	int res = 0;
	size_t count = 0;
	Archive** list = this->get_all_archives(&count);
	for (size_t i = 0; i < count; i++) {
		if (list[i]) {
			res = func(list[i], context);
			delete list[i];
		}
	}
	return res;
}

int Depot::iterate_files(Archive* archive, FileIteratorFunc func, void* context) {
	int res = 0;
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT serial, info, path, mode, uid, gid, size, digest FROM files WHERE archive=? ORDER BY path";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		res = sqlite3_bind_int64(stmt, 1, archive->serial());
		while (res == 0) {
			res = sqlite3_step(stmt);
			if (res == SQLITE_ROW) {
				res = 0;
				int i = 0;
				uint64_t serial = sqlite3_column_int64(stmt, i++);
				uint32_t info = sqlite3_column_int(stmt, i++);
				const unsigned char* path = sqlite3_column_text(stmt, i++);
				mode_t mode = sqlite3_column_int(stmt, i++);
				uid_t uid = sqlite3_column_int(stmt, i++);
				gid_t gid = sqlite3_column_int(stmt, i++);
				off_t size = sqlite3_column_int64(stmt, i++);
				const void* blob = sqlite3_column_blob(stmt, i);
				int blobsize = sqlite3_column_bytes(stmt, i++);

				Digest* digest = NULL;
				if (blobsize > 0) {
					digest = new Digest();
					digest->m_size = blobsize;
					memcpy(digest->m_data, blob, ((size_t)blobsize < sizeof(digest->m_data)) ? blobsize : sizeof(digest->m_data));
				}

				File* file = FileFactory(serial, archive, info, (const char*)path, mode, uid, gid, size, digest);
				if (file) {
					res = func(file, context);
					delete file;
				} else {
					fprintf(stderr, "%s:%d: FileFactory returned NULL\n", __FILE__, __LINE__);
					res = -1;
					break;
				}
			} else if (res == SQLITE_DONE) {
				res = 0;
				break;
			}
		}
		sqlite3_reset(stmt);
	}
	return res;
}


int Depot::analyze_stage(const char* path, Archive* archive, Archive* rollback, int* rollback_files) {
	int res = 0;
	assert(archive != NULL);
	assert(rollback != NULL);
	assert(rollback_files != NULL);

	*rollback_files = 0;

	const char* path_argv[] = { path, NULL };
	
	IF_DEBUG("[analyze] analyzing path: %s\n", path);

	FTS* fts = fts_open((char**)path_argv, FTS_PHYSICAL | FTS_COMFOLLOW | FTS_XDEV, fts_compare);
	FTSENT* ent = fts_read(fts); // throw away the entry for path itself
	while (res != -1 && (ent = fts_read(fts)) != NULL) {
		File* file = FileFactory(archive, ent);
		if (file) {
			char state = '?';

			IF_DEBUG("[analyze] %s\n", file->path());

			// Perform a three-way-diff between the file to be installed (file),
			// the file we last installed in this location (preceding),
			// and the file that actually exists in this location (actual).
		
			char* actpath;
			join_path(&actpath, this->prefix(), file->path());
			File* actual = FileFactory(actpath);

			File* preceding = this->file_preceded_by(file);
			
			if (actual == NULL) {
				// No actual file exists already, so we create a placeholder.
				actual = new NoEntry(file->path());
				IF_DEBUG("[analyze]    actual == NULL\n");
			}
			
			if (preceding == NULL) {
				// Nothing is known about this file.
				// We'll insert this file into the rollback archive as a
				// base system file.  Back up its data (if not a directory).
				actual->info_set(FILE_INFO_BASE_SYSTEM);
				IF_DEBUG("[analyze]    base system\n");
				if (!S_ISDIR(actual->mode()) && !INFO_TEST(actual->info(), FILE_INFO_NO_ENTRY)) {
					IF_DEBUG("[analyze]    needs base system backup, and installation\n");
					actual->info_set(FILE_INFO_ROLLBACK_DATA);
					file->info_set(FILE_INFO_INSTALL_DATA);
				}
				preceding = actual;
			}
		
			uint32_t actual_flags = File::compare(file, actual);
			uint32_t preceding_flags = File::compare(actual, preceding);
		
			// If file == actual && actual == preceding then nothing needs to be done.
			if (actual_flags == FILE_INFO_IDENTICAL && preceding_flags == FILE_INFO_IDENTICAL) {
				state = ' ';
				IF_DEBUG("[analyze]    no changes\n");
			}
			
			// If file != actual, but actual == preceding, then install file
			//   but we don't need to save actual, since it's already saved by preceding.
			//   i.e. no user changes since last installation
			// If file != actual, and actual != preceding, then install file
			//  after saving actual in the rollback archive.
			//  i.e. user changes since last installation
			if (actual_flags != FILE_INFO_IDENTICAL) {
				if (INFO_TEST(actual->info(), FILE_INFO_NO_ENTRY)) {
					state = 'A';
				} else {
					state = 'U';
				}
				
				if (INFO_TEST(actual_flags, FILE_INFO_TYPE_DIFFERS) ||
				    INFO_TEST(actual_flags, FILE_INFO_DATA_DIFFERS)) {
					IF_DEBUG("[analyze]    needs installation\n");
					file->info_set(FILE_INFO_INSTALL_DATA);

					if ((INFO_TEST(preceding_flags, FILE_INFO_TYPE_DIFFERS) ||
					    INFO_TEST(preceding_flags, FILE_INFO_DATA_DIFFERS)) &&
					    !INFO_TEST(actual->info(), FILE_INFO_NO_ENTRY)) {
						IF_DEBUG("[analyze]    needs user data backup\n");
						actual->info_set(FILE_INFO_ROLLBACK_DATA);
					}
				}				
			}
			
			// XXX: should this be done in backup_file?
			// If we're going to need to squirrel away data, create
			// the directory hierarchy now.
			if (INFO_TEST(actual->info(), FILE_INFO_ROLLBACK_DATA)) {
				char path[PATH_MAX];
				char* backup_dirpath;

				// we need the path minus our destination prefix for moving to the archive
				strlcpy(path, actual->path() + strlen(m_prefix) - 1, sizeof(path));

				const char* dir = dirname(path);
				assert(dir != NULL);
				
				char *uuidpath;
				char uuidstr[37];
				uuid_unparse_upper(rollback->uuid(), uuidstr);
				
				asprintf(&uuidpath, "%s/%s", m_archives_path, uuidstr);
				assert(uuidpath != NULL);
				join_path(&backup_dirpath, uuidpath, dir);
				assert(backup_dirpath != NULL);
				
				res = mkdir_p(backup_dirpath);
				if (res != 0 && errno != EEXIST) {
					fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__, backup_dirpath, strerror(errno), errno);
				} else {
					res = 0;
				}
				free(backup_dirpath);
				free(uuidpath);
			}
			
			
			if ((state != ' ' && preceding_flags != FILE_INFO_IDENTICAL) ||
				INFO_TEST(actual->info(), FILE_INFO_BASE_SYSTEM | FILE_INFO_ROLLBACK_DATA)) {
				*rollback_files += 1;
				IF_DEBUG("[analyze]    insert rollback\n");
				res = this->insert(rollback, actual);
				assert(res == 0);
				// need to save parent directories as well
				char *ppath;
				char *pathbuf;
				pathbuf = strdup(actual->path());
				ppath = dirname(pathbuf);
				free(pathbuf);
				// while we have a valid path that is below the prefix
				while (ppath 
					   && strncmp(ppath, this->prefix(), strlen(this->prefix())) == 0
					   && strncmp(ppath, this->prefix(), strlen(ppath)) > 0) {
					File* parent = FileFactory(ppath);
					// if parent dir does not exist, we are
					//  generating a rollback of base system
					//  which does not have matching directories,
					//  so we can just move on.
					if (!parent) {
						IF_DEBUG("[analyze]      parent path not found, skipping parents\n");
						break;
					}
					IF_DEBUG("[analyze]      adding parent to rollback: %s \n", parent->path());
					res = this->insert(rollback, parent);
					assert(res == 0);
					ppath = dirname(ppath);
				}
			}

			fprintf(stderr, "%c %s\n", state, file->path());
			res = this->insert(archive, file);
			assert(res == 0);
			if (preceding && preceding != actual) delete preceding;
			if (actual) delete actual;
			free(actpath);
			delete file;
		}
	}
	if (fts) fts_close(fts);
	return res;
}



struct InstallContext {
	InstallContext(Depot* d, Archive* a) {
		depot = d;
		archive = a;
		files_modified = 0;
		files_added = 0;
		files_removed = 0;
		files_to_remove = new SerialSet();
	}
	
	~InstallContext() {
		delete files_to_remove;
	}
	
	Depot* depot;
	Archive* archive;
	uint64_t files_modified;
	uint64_t files_added;
	uint64_t files_removed;
	SerialSet* files_to_remove;	// for uninstall
};

int Depot::backup_file(File* file, void* ctx) {
	InstallContext* context = (InstallContext*)ctx;
	int res = 0;

	IF_DEBUG("[backup] backup_file: %s , %s \n", file->path(), context->archive->m_name);

	if (INFO_TEST(file->info(), FILE_INFO_ROLLBACK_DATA)) {
	        char *path;        // the file's path
		char *dstpath;     // the path inside the archives
		char *relpath;     // the file's path minus the destination prefix
		char *uuidpath;    // archives path plus the uuid
		char uuidstr[37];

		// we need the path minus our destination path for moving to the archive
		size_t prefixlen = strlen(context->depot->m_prefix);
		if (strncmp(context->archive->m_name, "<Rollback>", strlen("<Rollback>")) == 0) {
		  join_path(&path, context->depot->m_prefix, file->path());
		} else {
		  asprintf(&path, "%s", file->path());
		}
		relpath = path;
		if (strncmp(path, context->depot->m_prefix, prefixlen) == 0) {
		        relpath += prefixlen - 1;
		}

		uuid_unparse_upper(context->archive->uuid(), uuidstr);		
		asprintf(&uuidpath, "%s/%s", context->depot->m_archives_path, uuidstr);
		assert(uuidpath != NULL);
		join_path(&dstpath, uuidpath, relpath);
		assert(dstpath != NULL);

		IF_DEBUG("[backup] path = %s \n", path);
		IF_DEBUG("[backup] relpath = %s \n", relpath);
		IF_DEBUG("[backup] dstpath = %s \n", dstpath);
		IF_DEBUG("[backup] uuidpath = %s \n", uuidpath);

		++context->files_modified;

		// XXX: res = file->backup()
		IF_DEBUG("[backup] copyfile(%s, %s)\n", path, dstpath);
		res = copyfile(path, dstpath, NULL, COPYFILE_ALL|COPYFILE_NOFOLLOW);

		if (res != 0) fprintf(stderr, "%s:%d: backup failed: %s: %s (%d)\n", __FILE__, __LINE__, dstpath, strerror(errno), errno);
		free(path);
		free(dstpath);
		free(uuidpath);
	}
	return res;
}


int Depot::install_file(File* file, void* ctx) {
	InstallContext* context = (InstallContext*)ctx;
	int res = 0;

	if (INFO_TEST(file->info(), FILE_INFO_INSTALL_DATA)) {
		++context->files_modified;

		res = file->install(context->depot->m_archives_path, context->depot->m_prefix);
	} else {
		res = file->install_info(context->depot->m_prefix);
	}
	if (res != 0) fprintf(stderr, "%s:%d: install failed: %s: %s (%d)\n", __FILE__, __LINE__, file->path(), strerror(errno), errno);
	return res;
}


int Depot::install(Archive* archive) {
	int res = 0;
	Archive* rollback = new RollbackArchive();

	assert(rollback != NULL);
	assert(archive != NULL);

	// Check the consistency of the database before proceeding with the installation
	// If this fails, abort the installation.
	// res = this->check_consistency();
	// if (res != 0) return res;

	res = this->lock(LOCK_EX);
	if (res != 0) return res;

	//
	// The fun starts here
	//
	if (res == 0) res = this->begin_transaction();	

	//
	// Insert the rollback archive before the new archive to install, thus keeping
	// the chronology of the serial numbers correct.  We may later choose to delete
	// the rollback archive if we determine that it was not necessary.
	//
	if (res == 0) res = this->insert(rollback);
	if (res == 0) res = this->insert(archive);

	//
	// Create the stage directory and rollback backing store directories
	//
	char* archive_path = archive->create_directory(m_archives_path);
	assert(archive_path != NULL);
	char* rollback_path = rollback->create_directory(m_archives_path);
	assert(rollback_path != NULL);


	// Extract the archive into its backing store directory
	if (res == 0) res = archive->extract(archive_path);

	// Analyze the files in the archive backing store directory
	// Inserts new file records into the database for both the new archive being
	// installed and the rollback archive.
	int rollback_files = 0;
	if (res == 0) res = this->analyze_stage(archive_path, archive, rollback, &rollback_files);
	
	// If no files were added to the rollback archive, delete the rollback archive.
	if (res == 0 && rollback_files == 0) {
		res = this->remove(rollback);
	}

	// Commit the archive and its list of files to the database.
	// Note that the archive's "active" flag is still not set.
	if (res == 0) {
		res = this->commit_transaction();
	} else {
		this->rollback_transaction();
	}

	// Save a copy of the backing store directory now, we will soon
	// be moving the files into place.
	if (res == 0) res = archive->compact_directory(m_archives_path);

	//
	// Move files from the root file system to the rollback archive's backing store,
	// then move files from the archive backing directory to the root filesystem
	//
	InstallContext rollback_context(this, rollback);
	if (res == 0) res = this->iterate_files(rollback, &Depot::backup_file, &rollback_context);

	// compact the rollback archive (if we actually added any files)
	if (rollback_context.files_modified > 0) {
		if (res == 0) res = rollback->compact_directory(m_archives_path);
	}

	InstallContext install_context(this, archive);
	if (res == 0) res = this->iterate_files(archive, &Depot::install_file, &install_context);

	// Installation is complete.  Activate the archive in the database.
	if (res == 0) res = this->begin_transaction();
	if (res == 0) res = SQL("UPDATE archives SET active=1 WHERE serial=%lld;", rollback->serial());
	if (res == 0) res = SQL("UPDATE archives SET active=1 WHERE serial=%lld;", archive->serial());
	if (res == 0) res = this->commit_transaction();

	// Remove the stage and rollback directories (save disk space)
	remove_directory(archive_path);
	remove_directory(rollback_path);

	free(rollback_path);
	free(archive_path);
	
	(void)this->lock(LOCK_SH);

	return res;
}

// deletes expanded backing store directories in m_archives_path
int Depot::prune_directories() {
	int res = 0;
	
	const char* path_argv[] = { m_archives_path, NULL };
	
	FTS* fts = fts_open((char**)path_argv, FTS_PHYSICAL | FTS_COMFOLLOW | FTS_XDEV, fts_compare);
	FTSENT* ent = fts_read(fts); // get the entry for m_archives_path itself
	ent = fts_children(fts, 0);
	while (res != -1 && ent != NULL) {
		if (ent->fts_info == FTS_D) {
			char path[PATH_MAX];
			snprintf(path, PATH_MAX, "%s/%s", m_archives_path, ent->fts_name);
			IF_DEBUG("pruning: %s\n", path);
			res = remove_directory(path);
		}
		ent = ent->fts_link;
	}
	if (fts) fts_close(fts);
	return res;
}

int Depot::prune_archives() {
	int res = 0;
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "DELETE FROM archives WHERE serial IN (SELECT serial FROM archives WHERE serial NOT IN (SELECT DISTINCT archive FROM files));";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		if (res == 0) res = sqlite3_step(stmt);
		if (res == SQLITE_DONE) {
			res = 0;
		} else {
			fprintf(stderr, "%s:%d: Could not prune archives in database: %s (%d)\n", __FILE__, __LINE__, sqlite3_errmsg(m_db), res);
		}
		sqlite3_reset(stmt);
	}
	return res;
}

int Depot::uninstall_file(File* file, void* ctx) {
	InstallContext* context = (InstallContext*)ctx;
	int res = 0;
	char state = ' ';

	IF_DEBUG("[uninstall] %s\n", file->path());

	// We never uninstall a file that was part of the base system
	if (INFO_TEST(file->info(), FILE_INFO_BASE_SYSTEM)) {
		IF_DEBUG("[uninstall]    base system; skipping\n");
		return 0;
	}
	
	char* actpath;
	join_path(&actpath, context->depot->m_prefix, file->path());
	IF_DEBUG("[uninstall] actual path is %s\n", actpath);
	File* actual = FileFactory(actpath);
	uint32_t flags = File::compare(file, actual);
		
	if (actual != NULL && flags != FILE_INFO_IDENTICAL) {
		// XXX: probably not the desired behavior
		IF_DEBUG("[uninstall]    changes since install; skipping\n");
	} else {
		File* superseded = context->depot->file_superseded_by(file);
		if (superseded == NULL) {
			// no one's using this file anymore
			File* preceding = context->depot->file_preceded_by(file);
			assert(preceding != NULL);
			if (INFO_TEST(preceding->info(), FILE_INFO_NO_ENTRY)) {
				state = 'R';
				IF_DEBUG("[uninstall]    removing file\n");
				if (actual && res == 0) res = actual->remove();
			} else {
				// copy the preceding file back out to the system
				// if it's different from what's already there
				uint32_t flags = File::compare(file, preceding);
				if (INFO_TEST(flags, FILE_INFO_DATA_DIFFERS)) {
					state = 'U';
					IF_DEBUG("[uninstall]    restoring\n");
					if (res == 0) res = preceding->install(context->depot->m_archives_path, context->depot->m_prefix);
				} else if (INFO_TEST(flags, FILE_INFO_MODE_DIFFERS) ||
					   INFO_TEST(flags, FILE_INFO_GID_DIFFERS) ||
					   INFO_TEST(flags, FILE_INFO_UID_DIFFERS)) {
					if (res == 0) res = preceding->install_info(context->depot->m_prefix);
				} else {
					IF_DEBUG("[uninstall]    no changes; leaving in place\n");
				}
			}
			uint32_t info = preceding->info();
			if (INFO_TEST(info, FILE_INFO_NO_ENTRY | FILE_INFO_ROLLBACK_DATA) &&
			    !INFO_TEST(info, FILE_INFO_BASE_SYSTEM)) {
				if (res == 0) res = context->files_to_remove->add(preceding->serial());
			}
			delete preceding;
		} else {
			IF_DEBUG("[uninstall]    in use by newer installation; leaving in place\n");
			delete superseded;
		}
	}

	fprintf(stderr, "%c %s\n", state, file->path());

	if (res != 0) fprintf(stderr, "%s:%d: uninstall failed: %s\n", __FILE__, __LINE__, file->path());

	free(actpath);
	return res;
}

int Depot::uninstall(Archive* archive) {
	extern uint32_t verbosity;
	int res = 0;

	assert(archive != NULL);
	uint64_t serial = archive->serial();

	if (INFO_TEST(archive->info(), ARCHIVE_INFO_ROLLBACK)) {
		// if in debug mode, get_all_archives returns rollbacks too, so just ignore
		if (verbosity & VERBOSE_DEBUG) {
			fprintf(stderr, "[uninstall] skipping uninstall since archive is a rollback.\n");
			return 0;
		}
		fprintf(stderr, "%s:%d: cannot uninstall a rollback archive.\n", __FILE__, __LINE__);
		return -1;
	}

//	res = this->check_consistency();
//	if (res != 0) return res;

	res = this->lock(LOCK_EX);
	if (res != 0) return res;

	// XXX: this may be superfluous
	// uninstall_file should be smart enough to do a mtime check...
	if (res == 0) res = this->prune_directories();

	// We do this here to get an exclusive lock on the database.
	if (res == 0) res = this->begin_transaction();
	if (res == 0) res = SQL("UPDATE archives SET active=0 WHERE serial=%lld;", serial);
	if (res == 0) res = this->commit_transaction();

	InstallContext context(this, archive);
	if (res == 0) res = this->iterate_files(archive, &Depot::uninstall_file, &context);
	
	if (res == 0) res = this->begin_transaction();
	uint32_t i;
	for (i = 0; i < context.files_to_remove->count; ++i) {
		uint64_t serial = context.files_to_remove->values[i];
		IF_DEBUG("deleting file %lld\n", serial);
		if (res == 0) res = SQL("DELETE FROM files WHERE serial=%lld;", serial);
	}
	if (res == 0) res = this->commit_transaction();

	if (res == 0) res = this->begin_transaction();	
	if (res == 0) res = this->remove(archive);
	if (res == 0) res = this->commit_transaction();

	// delete all of the expanded archive backing stores to save disk space
	if (res == 0) res = this->prune_directories();

	if (res == 0) res = prune_archives();

	(void)this->lock(LOCK_SH);

	return res;
}

int Depot::verify_file(File* file, void* context) {
	File* actual = FileFactory(file->path());
	if (actual) {
		uint32_t flags = File::compare(file, actual);
		
		if (flags != FILE_INFO_IDENTICAL) {
			fprintf(stdout, "M ");
		} else {
			fprintf(stdout, "  ");
		}
	} else {
		fprintf(stdout, "R ");
	}
	file->print(stdout);
	return 0;
}

int Depot::verify(Archive* archive) {
	int res = 0;
	fprintf(stdout, "%-6s %-36s  %-23s  %s\n", "Serial", "UUID", "Date Installed", "Name");
	fprintf(stdout, "====== ====================================  =======================  =================\n");
	list_archive(archive, stdout);	
	fprintf(stdout, "=======================================================================================\n");
	if (res == 0) res = this->iterate_files(archive, &Depot::verify_file, NULL);
	fprintf(stdout, "=======================================================================================\n\n");
	return res;
}

int Depot::list_archive(Archive* archive, void* context) {	
	uint64_t serial = archive->serial();
	
	char uuid[37];
	uuid_unparse_upper(archive->uuid(), uuid);

	char date[100];
	struct tm local;
	time_t seconds = archive->date_installed();
	localtime_r(&seconds, &local);
	strftime(date, sizeof(date), "%F %T %Z", &local);

	fprintf((FILE*)context, "%-6llu %-36s  %-23s  %s\n", serial, uuid, date, archive->name());
	
	return 0;
}

int Depot::list() {
	int res = 0;
	fprintf(stdout, "%-6s %-36s  %-23s  %s\n", "Serial", "UUID", "Date Installed", "Name");
	fprintf(stdout, "====== ====================================  =======================  =================\n");
	if (res == 0) res = this->iterate_archives(&Depot::list_archive, stdout);
	return res;
}

int Depot::print_file(File* file, void* context) {
	extern uint32_t verbosity;
	if (verbosity & VERBOSE_DEBUG) fprintf((FILE*)context, "%04x ", file->info());
	file->print((FILE*)context);
	return 0;
}

int Depot::files(Archive* archive) {
	int res = 0;
	fprintf(stdout, "%-6s %-36s  %-23s  %s\n", "Serial", "UUID", "Date Installed", "Name");
	fprintf(stdout, "====== ====================================  =======================  =================\n");
	list_archive(archive, stdout);
	fprintf(stdout, "=======================================================================================\n");
	if (res == 0) res = this->iterate_files(archive, &Depot::print_file, stdout);
	fprintf(stdout, "=======================================================================================\n\n");
	return res;
}

int Depot::dump_archive(Archive* archive, void* context) {
	Depot* depot = (Depot*)context;
	int res = 0;
	list_archive(archive, stdout);
	fprintf(stdout, "=======================================================================================\n");
	if (res == 0) res = depot->iterate_files(archive, &Depot::print_file, stdout);
	fprintf(stdout, "=======================================================================================\n\n");
	return res;
}

int Depot::dump() {
	extern uint32_t verbosity;
	verbosity = 0xFFFFFFFF; // dump is intrinsically a debug command
	int res = 0;
	fprintf(stdout, "%-6s %-36s  %-23s  %s\n", "Serial", "UUID", "Date Installed", "Name");
	fprintf(stdout, "====== ====================================  =======================  =================\n");
	if (res == 0) res = this->iterate_archives(&Depot::dump_archive, this);
	return res;
}


File* Depot::file_star_eded_by(File* file, sqlite3_stmt* stmt) {
	assert(file != NULL);
	assert(file->archive() != NULL);
	
	File* result = NULL;
	uint64_t serial = 0;
	int res = 0;
	if (stmt && res == 0) {
		if (res == 0) res = sqlite3_bind_int64(stmt, 1, file->archive()->serial());
		if (res == 0) res = sqlite3_bind_text(stmt, 2, file->path(), -1, SQLITE_STATIC);
		if (res == 0) res = sqlite3_step(stmt);
		switch (res) {
			case SQLITE_DONE:
				serial = 0;
				break;
			case SQLITE_ROW:
				{
				int i = 0;
				uint64_t serial = sqlite3_column_int64(stmt, i++);
				uint64_t archive_serial = sqlite3_column_int64(stmt, i++);
				uint32_t info = sqlite3_column_int(stmt, i++);
				const unsigned char* path = sqlite3_column_text(stmt, i++);
				mode_t mode = sqlite3_column_int(stmt, i++);
				uid_t uid = sqlite3_column_int(stmt, i++);
				gid_t gid = sqlite3_column_int(stmt, i++);
				off_t size = sqlite3_column_int64(stmt, i++);
				const void* blob = sqlite3_column_blob(stmt, i);
				int blobsize = sqlite3_column_bytes(stmt, i++);

				Digest* digest = NULL;
				if (blobsize > 0) {
					digest = new Digest();
					digest->m_size = blobsize;
					memcpy(digest->m_data, blob, ((size_t)blobsize < sizeof(digest->m_data)) ? blobsize : sizeof(digest->m_data));
				}

				Archive* archive = this->archive(archive_serial);

				result = FileFactory(serial, archive, info, (const char*)path, mode, uid, gid, size, digest);
				}
				break;
			default:
				fprintf(stderr, "%s:%d: unexpected SQL error: %d\n", __FILE__, __LINE__, res);
				break;
		}
		sqlite3_reset(stmt);
	} else {
		fprintf(stderr, "%s:%d: unexpected SQL error: %d\n", __FILE__, __LINE__, res);
	}
	
	return result;
}

File* Depot::file_superseded_by(File* file) {
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		// archive which installed this file immediately after
		const char* query = "SELECT serial, archive, info, path, mode, uid, gid, size, digest FROM files WHERE archive>? AND path=? ORDER BY archive ASC LIMIT 1";
		int res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	return this->file_star_eded_by(file, stmt);
}

File* Depot::file_preceded_by(File* file) {
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		// archive which installed this file immediately before
		const char* query = "SELECT serial, archive, info, path, mode, uid, gid, size, digest FROM files WHERE archive<? AND path=? ORDER BY archive DESC LIMIT 1";
		int res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	return this->file_star_eded_by(file, stmt);
}

int Depot::check_consistency() {
	int res = 0;

	SerialSet* inactive = new SerialSet();
	assert(inactive != NULL);
	
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT serial FROM archives WHERE active=0 ORDER BY serial DESC";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		while (res == 0) {
			res = sqlite3_step(stmt);
			if (res == SQLITE_ROW) {
				res = 0;
				uint64_t serial = sqlite3_column_int64(stmt, 0);
				inactive->add(serial);
			} else if (res == SQLITE_DONE) {
				res = 0;
				break;
			} else {
				fprintf(stderr, "%s:%d: unexpected SQL error: %d\n", __FILE__, __LINE__, res);
			}
		}
		sqlite3_reset(stmt);
	}
	
	if (res == 0 && inactive && inactive->count > 0) {
		fprintf(stderr, "The following archive%s in an inconsistent state and must be uninstalled before proceeding:\n\n", inactive->count > 1 ? "s are" : " is");
		uint32_t i;
		fprintf(stderr, "%-36s %-23s %s\n", "UUID", "Date Installed", "Name");
		fprintf(stderr, "====================================  =======================  =================\n");
		for (i = 0; i < inactive->count; ++i) {
			Archive* archive = this->archive(inactive->values[i]);
			if (archive) {
				list_archive(archive, stderr);
				delete archive;
			}
		}
		fprintf(stderr, "\nWould you like to uninstall %s now? [y/n] ", inactive->count > 1 ? "them" : "it");
		int c = getchar();
		fprintf(stderr, "\n");
		if (c == 'y' || c == 'Y') {
			for (i = 0; i < inactive->count; ++i) {
				Archive* archive = this->archive(inactive->values[i]);
				if (archive) {
					res = this->uninstall(archive);
					delete archive;
				}
				if (res != 0) break;
			}
		}
	}
	
	return res;
}


int Depot::begin_transaction() {
	return this->SQL("BEGIN TRANSACTION");
}

int Depot::rollback_transaction() {
	return this->SQL("ROLLBACK TRANSACTION");
}

int Depot::commit_transaction() {
	return this->SQL("COMMIT TRANSACTION");
}

int Depot::is_locked() { return m_is_locked; }

int Depot::lock(int operation) {
	int res = 0;
	if (m_lock_fd == -1) {
		m_lock_fd = open(m_depot_path, O_RDONLY);
		if (m_lock_fd == -1) {
			perror(m_depot_path);
			res = m_lock_fd;
		}
	}
	if (res) return res;
	res = flock(m_lock_fd, operation);
	if (res == -1) {
		perror(m_depot_path);
	}
	return res;
}

int Depot::unlock(void) {
	int res = 0;
	res = flock(m_lock_fd, LOCK_UN);
	if (res == -1) {
		perror(m_depot_path);
	}
	close(m_lock_fd);
	m_lock_fd = -1;
	return res;
}

int Depot::insert(Archive* archive) {
	// Don't insert an archive that is already in the database
	assert(archive->serial() == 0);
	
	int res = 0;
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "INSERT INTO archives (uuid, info, name, date_added) VALUES (?, ?, ?, ?)";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		int i = 1;
		if (res == 0) res = sqlite3_bind_blob(stmt, i++, archive->uuid(), sizeof(uuid_t), SQLITE_STATIC);
		if (res == 0) res = sqlite3_bind_int(stmt, i++, archive->info());
		if (res == 0) res = sqlite3_bind_text(stmt, i++, archive->name(), -1, SQLITE_STATIC);
		if (res == 0) res = sqlite3_bind_int(stmt, i++, archive->date_installed());
		if (res == 0) res = sqlite3_step(stmt);
		if (res == SQLITE_DONE) {
			archive->m_serial = (uint64_t)sqlite3_last_insert_rowid(m_db);
			res = 0;
		} else {
			fprintf(stderr, "%s:%d: Could not add archive to database: %s (%d)\n", __FILE__, __LINE__, sqlite3_errmsg(m_db), res);
		}
		sqlite3_reset(stmt);
	}
	return res;
}

int Depot::insert(Archive* archive, File* file) {
	int res = 0;
	int do_update = 0;
	// check for the destination prefix in file's path, remove if found
	char *path, *relpath;
	size_t prefixlen = strlen(this->prefix());
	asprintf(&path, "%s", file->path());
	relpath = path;
	if (strncmp(file->path(), this->prefix(), prefixlen) == 0) {
	        relpath += prefixlen - 1;
	}

	const char* query = NULL;
	if (this->has_file(archive, file)) {
		do_update = 1;
		IF_DEBUG("archive(%llu) already has %s, updating instead \n", archive->serial(), relpath);
		query = "UPDATE files SET info=?, mode=?, uid=?, gid=?, digest=? WHERE archive=? and path=?";
	} else {
		query = "INSERT INTO files (info, mode, uid, gid, digest, archive, path) VALUES (?, ?, ?, ?, ?, ?, ?)";	
	}
	sqlite3_stmt* stmt = NULL;
	if (m_db) {
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		int i = 1;
		if (res == 0) res = sqlite3_bind_int(stmt, i++, file->info());
		if (res == 0) res = sqlite3_bind_int(stmt, i++, file->mode());
		if (res == 0) res = sqlite3_bind_int(stmt, i++, file->uid());
		if (res == 0) res = sqlite3_bind_int(stmt, i++, file->gid());
		Digest* dig = file->digest();
		if (res == 0 && dig) res = sqlite3_bind_blob(stmt, i++, dig->data(), dig->size(), SQLITE_STATIC);
		else if (res == 0) res = sqlite3_bind_blob(stmt, i++, NULL, 0, SQLITE_STATIC);
		if (res == 0) res = sqlite3_bind_int64(stmt, i++, archive->serial());
		if (res == 0) res = sqlite3_bind_text(stmt, i++, relpath, -1, SQLITE_STATIC);
		if (res == 0) res = sqlite3_step(stmt);
		if (res == SQLITE_DONE) {
			// if we did an insert, update the file serial
			if (!do_update) {
				file->m_serial = (uint64_t)sqlite3_last_insert_rowid(m_db);
			}
			res = 0;
		} else {
			fprintf(stderr, "%s:%d: Could not add file to database: %s (%d)\n", __FILE__, __LINE__, sqlite3_errmsg(m_db), res);
		}
		sqlite3_reset(stmt);
	}
	free(path);
	return res;
}

int Depot::has_file(Archive* archive, File* file) {
	int res = 0;
	size_t count = 0;
	// check for the destination prefix in file's path, remove if found
	char *path, *relpath;
	size_t prefixlen = strlen(this->prefix());
	asprintf(&path, "%s", file->path());
	relpath = path;
	if (strncmp(file->path(), this->prefix(), prefixlen) == 0) {
		relpath += prefixlen - 1;
	}
	
	static sqlite3_stmt* stmt = NULL;
	if (stmt == NULL && m_db) {
		const char* query = "SELECT count(*) FROM files WHERE archive=? and path=?";
		res = sqlite3_prepare(m_db, query, -1, &stmt, NULL);
		if (res != 0) fprintf(stderr, "%s:%d: sqlite3_prepare: %s: %s (%d)\n", __FILE__, __LINE__, query, sqlite3_errmsg(m_db), res);
	}
	if (stmt && res == 0) {
		int i = 1;
		if (res == 0) res = sqlite3_bind_int64(stmt, i++, archive->serial());
		if (res == 0) res = sqlite3_bind_text(stmt, i++, relpath, -1, SQLITE_STATIC);
		if (res == 0) res = sqlite3_step(stmt);
		if (res == SQLITE_ROW) {
			count = sqlite3_column_int64(stmt, 0);
			IF_DEBUG("has_file(%llu, %s) got count = %u \n", archive->serial(), relpath, (unsigned int)count);
		} else {
			res = -1;
			fprintf(stderr, "%s:%d: Could not query for file in archive: %s (%d)\n", __FILE__, __LINE__, sqlite3_errmsg(m_db), res);
		}
		sqlite3_reset(stmt);
	}
	free(path);
	return count > 0;
}

int Depot::remove(Archive* archive) {
	int res = 0;
	uint64_t serial = archive->serial();
	if (res == 0) res = SQL("DELETE FROM files WHERE archive=%lld", serial);
	if (res == 0) res = SQL("DELETE FROM archives WHERE serial=%lld", serial);
	return res;
}

int Depot::remove(File* file) {
	int res = 0;
	uint64_t serial = file->serial();
	if (res == 0) res = SQL("DELETE FROM files WHERE serial=%lld", serial);
	return res;
}

// helper to dispatch the actual command for process_archive()
int Depot::dispatch_command(Archive* archive, const char* command) {
	int res = 0;

	if (strncasecmp((char*)command, "files", 5) == 0) {
		res = this->files(archive);
	} else if (strncasecmp((char*)command, "uninstall", 9) == 0) {
		res = this->uninstall(archive);
	} else if (strncasecmp((char*)command, "verify", 6) == 0) {
		res = this->verify(archive);
	} else {
		fprintf(stderr, "Error: unknown command given to dispatch_command.\n");
	}
	if (res != 0) {
		fprintf(stderr, "An error occurred.\n");
	}
	return res;
}

// perform a command on an archive specification
int Depot::process_archive(const char* command, const char* arg) {
	extern uint32_t verbosity;
	int res = 0;
	size_t count = 0;
	Archive** list = NULL;
	
	if (strncasecmp(arg, "all", 3) == 0) {
		list = this->get_all_archives(&count);
	} else {
		// make a list of 1 Archive
		list = (Archive**)malloc(sizeof(Archive*));
		list[0] = this->get_archive(arg);
		count = 1;
	}
	
	for (size_t i = 0; i < count; i++) {
		if (!list[i]) {
			fprintf(stderr, "Archive not found: %s\n", arg);
			return -1;
		}
		if (verbosity & VERBOSE_DEBUG) {
			char uuid[37];
			uuid_unparse_upper(list[i]->uuid(), uuid);
			fprintf(stderr, "Found archive: %s\n", uuid);
		}
		res = this->dispatch_command(list[i], command);
		delete list[i];
	} 
	free(list);
	return res;
}


#define __SQL(callback, context, fmt) \
	va_list args; \
	char* errmsg; \
	va_start(args, fmt); \
	if (this->m_db) { \
		char *query = sqlite3_vmprintf(fmt, args); \
		res = sqlite3_exec(this->m_db, query, callback, context, &errmsg); \
		if (res != SQLITE_OK) { \
			fprintf(stderr, "Error: %s (%d)\n  SQL: %s\n", errmsg, res, query); \
		} \
		sqlite3_free(query); \
	} else { \
		fprintf(stderr, "Error: database not open.\n"); \
		res = SQLITE_ERROR; \
	} \
	va_end(args);

int Depot::SQL(const char* fmt, ...) {
	int res;
	__SQL(NULL, NULL, fmt);
	return res;
}

#undef __SQL
