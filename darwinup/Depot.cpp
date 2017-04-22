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

#include "Archive.h"
#include "Depot.h"
#include "File.h"
#include "SerialSet.h"
#include "Utils.h"
#include <assert.h>
#include <copyfile.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


Depot::Depot() {
	m_prefix = NULL;
	m_depot_path = NULL;
	m_database_path = NULL;
	m_archives_path = NULL;
	m_downloads_path = NULL;
	m_build = NULL;
	m_db = NULL;
	m_lock_fd = -1;
	m_is_locked = 0;
	m_depot_mode = 0750;
	m_is_dirty = false;
	m_modified_extensions = false;
	m_modified_xpc_services = false;
}

Depot::Depot(const char* prefix) {
	m_lock_fd = -1;
	m_is_locked = 0;
	m_depot_mode = 0750;
	m_build = NULL;
	m_is_dirty = false;
	m_modified_extensions = false;
	m_modified_xpc_services = false;
	
	asprintf(&m_prefix, "%s", prefix);
	join_path(&m_depot_path, m_prefix, "/.DarwinDepot");
	join_path(&m_database_path, m_depot_path, "/Database-V100");
	join_path(&m_archives_path, m_depot_path, "/Archives");
	join_path(&m_downloads_path, m_depot_path, "/Downloads");
}

Depot::~Depot() {
	
	// XXX: this is expensive, but is it necessary?
	//this->check_consistency();

	if (m_lock_fd != -1)	this->unlock();
	delete m_db;
	if (m_prefix)           free(m_prefix);
	if (m_depot_path)	free(m_depot_path);
	if (m_database_path)	free(m_database_path);
	if (m_archives_path)	free(m_archives_path);
	if (m_downloads_path)	free(m_downloads_path);
}

const char*	Depot::archives_path()		      { return m_archives_path; }
const char*	Depot::downloads_path()		      { return m_downloads_path; }
const char* Depot::prefix()                   { return m_prefix; }
bool        Depot::is_dirty()                 { return m_is_dirty; }
bool        Depot::has_modified_extensions()  { return m_modified_extensions; }
bool        Depot::has_modified_xpc_services(){ return m_modified_xpc_services; }

int Depot::connect() {
	m_db = new DarwinupDatabase(m_database_path);
	if (!m_db || !m_db->is_connected()) {
		fprintf(stderr, "Error: unable to connect to database.\n");
		return DB_ERROR;
	}
	return DB_OK;
}

int Depot::create_storage() {
	uid_t uid = getuid();
	gid_t gid = 0;
	struct group *gs = getgrnam("admin");
	if (gs) {
		gid = gs->gr_gid;
	}
	
	int res = mkdir(m_depot_path, m_depot_mode);
	if (res && errno != EEXIST) {
		perror(m_depot_path);
		return res;
	}

	res = chmod(m_depot_path, m_depot_mode);
	res = chown(m_depot_path, uid, gid);
	if (res && errno != EEXIST) {
		perror(m_depot_path);
		return res;
	}
	res = mkdir(m_archives_path, m_depot_mode);
	res = chmod(m_archives_path, m_depot_mode);
	res = chown(m_archives_path, uid, gid);
	if (res && errno != EEXIST) {
		perror(m_archives_path);
		return res;
	}
	
	res = mkdir(m_downloads_path, m_depot_mode);
	res = chmod(m_downloads_path, m_depot_mode);
	res = chown(m_downloads_path, uid, gid);
	if (res && errno != EEXIST) {
		perror(m_downloads_path);
		return res;
	}
	return DEPOT_OK;
}

// Initialize the depot
int Depot::initialize(bool writable) {
	int res = 0;
	
	// initialization requires all these paths to be set
	if (!(m_prefix && m_depot_path && m_database_path && 
		  m_archives_path && m_downloads_path)) {
		return DEPOT_ERROR;
	}
	
	if (writable) {
		uid_t uid = getuid();
		if (uid) {
			fprintf(stdout, "You must be root to perform that operation.\n");
			exit(3);
		}			
		
		res = this->create_storage();
		if (res) return res;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1060				
		build_number_for_path(&m_build, m_prefix);
#else 
		m_build = (char*)calloc(1, 2);
		snprintf(m_build, 2, " ");
#endif
	}
	
	struct stat sb;
	res = stat(m_database_path, &sb);
	if (!writable && res == -1 && (errno == ENOENT || errno == ENOTDIR)) {
		// depot does not exist
		return DEPOT_NOT_EXIST; 
	}
	if (!writable && res == -1 && errno == EACCES) {
		// permission denied
		return DEPOT_PERM_DENIED;
	}

	// take an exclusive lock
	res = this->lock(LOCK_EX);
	if (res) return res;
	m_is_locked = 1;			
		
	res = this->connect();

	return res;
}

int Depot::is_initialized() {
	return (m_db != NULL);
}

// Unserialize an archive from the database.
// Find the archive by UUID.
Archive* Depot::archive(uuid_t uuid) {
	int res = 0;
	Archive* archive = NULL;
	uint8_t* data;
	
	res = this->m_db->get_archive(&data, uuid);
	if (FOUND(res)) archive = this->m_db->make_archive(data);
	return archive;
}

// Unserialize an archive from the database.
// Find the archive by serial.
Archive* Depot::archive(uint64_t serial) {
	int res = 0;
	Archive* archive = NULL;
	uint8_t* data;
	
	res = this->m_db->get_archive(&data, serial);
	if (FOUND(res)) archive = this->m_db->make_archive(data);

	return archive;
}

// Unserialize an archive from the database.
// Find the last archive installed with this name
Archive* Depot::archive(archive_name_t name) {
	int res = 0;
	Archive* archive = NULL;
	uint8_t* data;
	
	res = this->m_db->get_archive(&data, name);
	if (FOUND(res)) archive = this->m_db->make_archive(data);
	return archive;
}

Archive* Depot::archive(archive_keyword_t keyword) {
	int res = 0;
	Archive* archive = NULL;
	uint8_t* data;
	
	res = this->m_db->get_archive(&data, keyword);
	if (FOUND(res)) archive = this->m_db->make_archive(data);	
	return archive;	
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

	// test for arg being a uuid
	uuid_t uuid;
	if (uuid_parse(arg, uuid) == 0) {
		return Depot::archive(uuid);
	}

	// test for arg being a serial number
	uint64_t serial; 
	char* endptr = NULL;
	serial = strtoull(arg, &endptr, 0);
	if (serial && (*arg != '\0') && (*endptr == '\0')) {
		return Depot::archive(serial);
	}
	
	// test for keywords
	if (strncasecmp("oldest", arg, 6) == 0) {
		return Depot::archive(DEPOT_ARCHIVE_OLDEST);
	}
	if (strncasecmp("newest", arg, 6) == 0) {
		return Depot::archive(DEPOT_ARCHIVE_NEWEST);
	}
	
	// if nothing else, must be an archive name
	return Depot::archive((archive_name_t)arg);
}

Archive** Depot::get_all_archives(uint32_t* count) {
	extern uint32_t verbosity;
	int res = DB_OK;
	uint8_t** archlist;
	res = this->m_db->get_archives(&archlist, count, verbosity & VERBOSE_DEBUG);
	
	Archive** list = (Archive**)malloc(sizeof(Archive*) * (*count));
	if (!list) {
		fprintf(stderr, "Error: ran out of memory in Depot::get_all_archives\n");
		return NULL;
	}
	if (FOUND(res)) {
		for (uint32_t i=0; i < *count; i++) {
			Archive* archive = this->m_db->make_archive(archlist[i]);
			if (archive) {
				list[i] = archive;
			} else {
				fprintf(stderr, "%s:%d: DB::make_archive returned NULL\n",
						__FILE__, __LINE__);
				res = -1;
				break;
			}
		}
	}

	return list;	
}

Archive** Depot::get_superseded_archives(uint32_t* count) {
	int res = DB_OK;
	uint8_t** archlist;
	res = this->m_db->get_archives(&archlist, count, false); // rollbacks cannot be superseded
	
	Archive** list = (Archive**)malloc(sizeof(Archive*) * (*count));
	if (!list) {
		fprintf(stderr, "Error: ran out of memory in Depot::get_superseded_archives\n");
		return NULL;
	}

	uint32_t i = 0;
	uint32_t cur = i;
	if (FOUND(res)) {
		while (i < *count) {
			Archive* archive = this->m_db->make_archive(archlist[i++]);
			if (archive && this->is_superseded(archive)) {
				list[cur++] = archive;
			} else if (!archive) {
				fprintf(stderr, "%s:%d: DB::make_archive returned NULL\n",
						__FILE__, __LINE__);
				res = -1;
				break;
			}
		}
	}
	// adjust count based on our is_superseded filtering
	*count = cur;
	return list;	
}

uint64_t Depot::count_archives() {
	extern uint32_t verbosity;
	uint64_t c = this->m_db->count_archives((bool)(verbosity & VERBOSE_DEBUG));
	return c;
}

struct InstallContext {
	InstallContext(Depot* d, Archive* a) {
		depot = d;
		archive = a;
		files_modified = 0;
		files_added = 0;
		files_removed = 0;
		files_to_remove = new SerialSet();
		reverse_files = false;
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
	bool reverse_files; // for uninstall
};

int Depot::iterate_archives(ArchiveIteratorFunc func, void* context) {
	int res = 0;
	uint32_t count = 0;
	Archive** list = this->get_all_archives(&count);
	for (uint32_t i = 0; i < count; i++) {
		if (list[i]) {
			res = func(list[i], context);
			delete list[i];
		}
	}
	return res;
}

int Depot::iterate_files(Archive* archive, FileIteratorFunc func, void* context) {
	int res = DB_OK;
	uint8_t** filelist;
	uint32_t count;
	bool reverse = false;
	if (context) reverse = ((InstallContext*)context)->reverse_files;
	res = this->m_db->get_files(&filelist, &count, archive, reverse);
	if (FOUND(res)) {
		for (uint32_t i=0; i < count; i++) {
			File* file = this->m_db->make_file(filelist[i]);
			if (file) {
				res = func(file, context);
				delete file;
			} else {
				fprintf(stderr, "%s:%d: DB::make_file returned NULL\n", __FILE__, __LINE__);
				res = -1;
				break;
			}
		}
	}

	return res;
}

int Depot::analyze_stage(const char* path, Archive* archive, Archive* rollback,
						 int* rollback_files) {
	extern uint32_t force;
	extern uint32_t dryrun;
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

			if (strcasestr(file->path(), ".DarwinDepot")) {
				fprintf(stderr, "Error: Root contains a .DarwinDepot, "
						"aborting to avoid damaging darwinup metadata.\n");
				return DEPOT_ERROR;
			}

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
				// if actual is a dir and file is not, recurse to save its children
				if (S_ISDIR(actual->mode()) && !S_ISDIR(file->mode())) {
					IF_DEBUG("[analyze]    directory being replaced by file, save children\n");
					const char* sub_argv[] = { actual->path(), NULL };
					FTS* subfts = fts_open((char**)sub_argv, 
										   FTS_PHYSICAL | FTS_COMFOLLOW | FTS_XDEV, 
										   fts_compare);
					FTSENT* subent = fts_read(subfts); // throw away actual
					while ((subent = fts_read(subfts)) != NULL) {
						IF_DEBUG("saving child: %s\n", subent->fts_path);
						// skip post-order visits
						if (subent->fts_info == FTS_DP) {
							continue;
						}
						File* subact = FileFactory(subent->fts_path);
						subact->info_set(FILE_INFO_BASE_SYSTEM);
						if (subent->fts_info != FTS_D) {
							IF_DEBUG("saving file data\n");
							subact->info_set(FILE_INFO_ROLLBACK_DATA);
						}
						if (!dryrun) {
							res = this->insert(rollback, subact);
						}
						*rollback_files += 1;
					}
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
				this->m_is_dirty = true;
				if (INFO_TEST(actual->info(), FILE_INFO_NO_ENTRY)) {
					state = 'A';
				} else {
					if (INFO_TEST(actual_flags, FILE_INFO_TYPE_DIFFERS) && !force) {
						// the existing file on disk is a different type than what
						// we are trying to install, so require the force option,
						// otherwise print an error and bail
						mode_t file_type = file->mode() & S_IFMT;
						mode_t actual_type = actual->mode() & S_IFMT;
						fprintf(stderr, FILE_OBJ_CHANGE_ERROR, actual->path(), 
								FILE_TYPE_STRING(file_type),
								FILE_TYPE_STRING(actual_type));
						return DEPOT_OBJ_CHANGE;
					}
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
				
				if (!this->m_modified_extensions && 
					(strncmp(file->path(), "/System/Library/Extensions", 26) == 0)) {
					IF_DEBUG("[analyze]    kernel extension detected\n");
					this->m_modified_extensions = true;
				}

				if (!this->m_modified_xpc_services) {
					if ((strstr(file->path(), ".xpc/") != NULL) && has_suffix(file->path(), "Info.plist")) {
						IF_DEBUG("[analyze]    xpc service detected\n");
						this->m_modified_xpc_services = true;
					}

					if ((strncmp(file->path(), "/System/Library/Sandbox/Profiles", 32) == 0) ||
						(has_suffix(file->path(), "framework.sb"))) {
						IF_DEBUG("[analyze]    profile modification detected\n");
						this->m_modified_xpc_services = true;
					}
				}
			}

			// if file == actual, but actual != preceding, then an external
			// process changed actual to be the same as what we are installing
			// now (OS upgrade?). We do not need to save actual, but make
			// a special state so the user knows what happened and does not
			// get a ?.
			if (actual_flags == FILE_INFO_IDENTICAL && preceding_flags != FILE_INFO_IDENTICAL) {
				IF_DEBUG("[analyze]    external changes but file same as actual\n");
				state = 'E';
			}
						
			if ((state != ' ' && preceding_flags != FILE_INFO_IDENTICAL) ||
				INFO_TEST(actual->info(), FILE_INFO_BASE_SYSTEM | FILE_INFO_ROLLBACK_DATA)) {
				*rollback_files += 1;
				if (!this->has_file(rollback, actual)) {
					IF_DEBUG("[analyze]    insert rollback\n");
					if (!dryrun) res = this->insert(rollback, actual);
				}
				assert(res == 0);

				if (!INFO_TEST(actual->info(), FILE_INFO_NO_ENTRY)) {
					// need to save parent directories as well
					FTSENT *pent = ent->fts_parent;
					
					// while we have a valid path that is below the prefix
					while (pent && pent->fts_level > 0) {
						File* parent = FileFactory(rollback, pent);
						
						// if parent dir does not exist, we are
						//  generating a rollback of base system
						//  which does not have matching directories,
						//  so we can just move on.
						if (!parent) {
							IF_DEBUG("[analyze]      parent path not found, skipping parents\n");
							break;
						}
						
						if (!this->has_file(rollback, parent)) {
							IF_DEBUG("[analyze]      adding parent to rollback: %s \n", 
									 parent->path());
							if (!dryrun) res = this->insert(rollback, parent);
						}
						assert(res == 0);
						pent = pent->fts_parent;
					}
				}
			}

			fprintf(stdout, "%c %s\n", state, file->path());
			if (!dryrun) res = this->insert(archive, file);
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

		// If we're going to need to squirrel away data, create
		// the directory hierarchy now.
		char backup_path[PATH_MAX];
		char* backup_dirpath;

		// we need the path minus our destination prefix for moving to the archive
		IF_DEBUG("[backup] file->path() = %s \n", file->path());
		strlcpy(backup_path, file->path(), sizeof(backup_path));
		IF_DEBUG("[backup] backup_path = %s \n", backup_path);
			
		const char* dir = dirname(backup_path);
		assert(dir != NULL);
		IF_DEBUG("[backup] dir = %s \n", dir);

		uuid_unparse_upper(context->archive->uuid(), uuidstr);
		asprintf(&uuidpath, "%s/%s", context->depot->m_archives_path, uuidstr);
		assert(uuidpath != NULL);
		IF_DEBUG("[backup] uuidpath = %s \n", uuidpath);
		join_path(&backup_dirpath, uuidpath, dir);
		assert(backup_dirpath != NULL);
		
		IF_DEBUG("mkdir_p: %s\n", backup_dirpath);
		res = mkdir_p(backup_dirpath);
		if (res != 0 && errno != EEXIST) {
			fprintf(stderr, "%s:%d: %s: %s (%d)\n", 
					__FILE__, __LINE__, backup_dirpath, strerror(errno), errno);
		} else {
			res = 0;
		}
		
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

		join_path(&dstpath, uuidpath, relpath);
		assert(dstpath != NULL);

		IF_DEBUG("[backup] path = %s \n", path);
		IF_DEBUG("[backup] relpath = %s \n", relpath);
		IF_DEBUG("[backup] dstpath = %s \n", dstpath);


		++context->files_modified;

		// XXX: res = file->backup()
		IF_DEBUG("[backup] copyfile(%s, %s)\n", path, dstpath);
		res = copyfile(path, dstpath, NULL, COPYFILE_ALL|COPYFILE_NOFOLLOW);

		if (res != 0) fprintf(stderr, "%s:%d: backup failed: %s: %s (%d)\n", 
							  __FILE__, __LINE__, dstpath, strerror(errno), errno);

		// XXX: we cant propagate error from callback, but its safe to die here
		assert(res == 0);
		
		free(path);
		free(dstpath);
		free(uuidpath);
		free(backup_dirpath);
	}
	return res;
}


int Depot::install_file(File* file, void* ctx) {
	InstallContext* context = (InstallContext*)ctx;
	int res = 0;

	// Strip the quarantine xattr off all files to avoid them being rendered useless.
	if (file->unquarantine(context->depot->m_archives_path) != 0) {
		fprintf(stderr, "Error: unable to unquarantine file in staging area.\n");
		return DEPOT_ERROR;
	}

	if (INFO_TEST(file->info(), FILE_INFO_INSTALL_DATA)) {
		++context->files_modified;

		res = file->install(context->depot->m_archives_path,
                        context->depot->m_prefix,
                        context->reverse_files);
	} else {
		res = file->install_info(context->depot->m_prefix);
	}
	if (res != 0) fprintf(stderr, "%s:%d: install failed: %s: %s (%d)\n", 
						  __FILE__, __LINE__, file->path(), strerror(errno), errno);
	return res;
}


int Depot::install(const char* path) {
	int res = 0;
	char uuid[37];
	Archive* archive = ArchiveFactory(path, this->downloads_path());
	if (archive) {
		res = this->install(archive);
		if (res == 0) {
			fprintf(stdout, "Installed archive: %llu %s \n", 
					archive->serial(), archive->name());
			uuid_unparse_upper(archive->uuid(), uuid);
			fprintf(stdout, "%s\n", uuid);
		} else {
			fprintf(stderr, "Error: Install failed.\n");				
			if (res != DEPOT_OBJ_CHANGE && res != DEPOT_PREINSTALL_ERR) {
				// object change errors come from analyze stage,
				// and pre-install errors happen early,
				// so there is no installation to roll back
				fprintf(stderr, "Rolling back installation.\n");
				res = this->uninstall(archive);
				if (res) {
					fprintf(stderr, "Error: Unable to rollback installation. "
							"Your system is in an inconsistent state! File a bug!\n");
				} else {
					fprintf(stdout, "Rollback successful.\n");
				}
			}
			res = DEPOT_ERROR;
		}
	} else {
		fprintf(stdout, "Error: unable to load \"%s\". Either the path is missing, invalid or"
				         " the file is in an unknown format.\n", path);
		return DEPOT_ERROR;
	}

	return res;
}


int Depot::install(Archive* archive) {
	extern uint32_t dryrun;
	int res = 0;
	Archive* rollback = new RollbackArchive();

	if (this->m_build) {
		rollback->m_build = strdup(this->m_build);
		archive->m_build = strdup(this->m_build);
	}
	
	assert(rollback != NULL);
	assert(archive != NULL);

	if (res != 0) return res;

	//
	// The fun starts here
	//
	if (!dryrun && res == 0) res = this->begin_transaction();	

	//
	// Insert the rollback archive before the new archive to install, thus keeping
	// the chronology of the serial numbers correct.  We may later choose to delete
	// the rollback archive if we determine that it was not necessary.
	//
	if (!dryrun && res == 0) res = this->insert(rollback);
	if (!dryrun && res == 0) res = this->insert(archive);

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
	
	// we can stop now if analyze failed or this is a dry run
	if (res || dryrun) {
		remove_directory(archive_path);
		remove_directory(rollback_path);
		free(rollback_path);
		free(archive_path);
		if (!dryrun && res) {
			this->rollback_transaction();
			return DEPOT_PREINSTALL_ERR;
		}
		return res;
	}
	
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
	if (res == 0) {
		res = this->m_db->activate_archive(rollback->serial());
		if (res) this->rollback_transaction();
	}
	if (res == 0) {
		res = this->m_db->activate_archive(archive->serial());
		if (res) this->rollback_transaction();
	}
	if (res == 0) res = this->commit_transaction();

	// Remove the stage and rollback directories (save disk space)
	remove_directory(archive_path);
	remove_directory(rollback_path);
	free(rollback_path);
	free(archive_path);

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
			res = remove_directory(path);
		}
		ent = ent->fts_link;
	}
	if (fts) fts_close(fts);
	return res;
}

// delete the unexpanded tarball from archives storage
int Depot::prune_archive(Archive* archive) {
	int res = 0;
	
	// clean up database
	res = this->m_db->delete_empty_archives();
	if (res) {
		fprintf(stderr, "Error: unable to prune archives from database.\n");
		return res;
	}
	
	// clean up disk
	res = archive->prune_compacted_archive(m_archives_path);
	return res;
}

int Depot::uninstall_file(File* file, void* ctx) {
	extern uint32_t dryrun;
	InstallContext* context = (InstallContext*)ctx;
	int res = 0;
	char state = ' ';

	IF_DEBUG("[uninstall] %s\n", file->path());

	// We never uninstall a file that was part of the base system
	if (INFO_TEST(file->info(), FILE_INFO_BASE_SYSTEM)) {
		IF_DEBUG("[uninstall]    base system; skipping\n");
		return DEPOT_OK;
	}
	
	char* actpath;
	join_path(&actpath, context->depot->m_prefix, file->path());
	IF_DEBUG("[uninstall] actual path is %s\n", actpath);
	File* actual = FileFactory(actpath);
	uint32_t flags = File::compare(file, actual);
	
	if (actual == NULL) {
		IF_DEBUG("[uninstall]    actual file missing, "
				 "possibly due to parent being removed already\n");
		state = '!';
	} else if (flags != FILE_INFO_IDENTICAL) {
		IF_DEBUG("[uninstall]    changes since install; skipping\n");
	} else {
		File* superseded = context->depot->file_superseded_by(file);
		if (superseded == NULL) {
			// no one's using this file anymore
			File* preceding = context->depot->file_preceded_by(file);
			assert(preceding != NULL);
			if (INFO_TEST(preceding->info(), FILE_INFO_NO_ENTRY)) {
				context->depot->m_is_dirty = true;
				state = 'R';
				IF_DEBUG("[uninstall]    removing file\n");
				if (!dryrun && actual && res == 0) res = actual->remove();
			} else {
				// copy the preceding file back out to the system
				// if it's different from what's already there
				uint32_t flags = File::compare(file, preceding);
				if (INFO_TEST(flags, FILE_INFO_DATA_DIFFERS)) {
					context->depot->m_is_dirty = true;
					state = 'U';
					IF_DEBUG("[uninstall]    restoring\n");
					if (!dryrun && res == 0) {
						if (INFO_TEST(flags, FILE_INFO_TYPE_DIFFERS) &&
							S_ISDIR(preceding->mode())) {
							// use rename instead of mkdir so children are restored
							res = preceding->dirrename(context->depot->m_archives_path, 
													               context->depot->m_prefix,
                                         context->reverse_files);							

						} else {
							res = preceding->install(context->depot->m_archives_path, 
													             context->depot->m_prefix,
                                       context->reverse_files);
						}
					}
				} else if (INFO_TEST(flags, FILE_INFO_MODE_DIFFERS) ||
					   INFO_TEST(flags, FILE_INFO_GID_DIFFERS) ||
					   INFO_TEST(flags, FILE_INFO_UID_DIFFERS)) {
					context->depot->m_is_dirty = true;
					state = 'M';
					if (!dryrun && res == 0) {
						res = preceding->install_info(context->depot->m_prefix);
					}
				} else {
					IF_DEBUG("[uninstall]    no changes; leaving in place\n");
				}
				if (!context->depot->m_modified_extensions &&
					(strncmp(file->path(), "/System/Library/Extensions", 26) == 0)) {
					IF_DEBUG("[uninstall]    kernel extension detected\n");
					context->depot->m_modified_extensions = true;
				}
			}
			uint64_t info = preceding->info();
			if (INFO_TEST(info, FILE_INFO_NO_ENTRY | FILE_INFO_ROLLBACK_DATA) &&
			    !INFO_TEST(info, FILE_INFO_BASE_SYSTEM)) {
				if (!dryrun && res == 0) {
					res = context->files_to_remove->add(preceding->serial());
				}
			}
			delete preceding;
		} else {
			IF_DEBUG("[uninstall]    in use by newer installation; leaving in place\n");
			delete superseded;
		}
	}

	fprintf(stdout, "%c %s\n", state, file->path());

	if (res != 0) fprintf(stderr, "%s:%d: uninstall failed: %s\n", 
						  __FILE__, __LINE__, file->path());

	free(actpath);
	return res;
}

int Depot::uninstall(Archive* archive) {
	extern uint32_t verbosity;
	extern uint32_t force;
	extern uint32_t dryrun;
	int res = 0;

	assert(archive != NULL);
	uint64_t serial = archive->serial();

	if (INFO_TEST(archive->info(), ARCHIVE_INFO_ROLLBACK)) {
		// if in debug mode, get_all_archives returns rollbacks too, so just ignore
		if (verbosity & VERBOSE_DEBUG) {
			fprintf(stderr, "[uninstall] skipping uninstall since archive is a rollback.\n");
			return DEPOT_OK;
		}
		fprintf(stderr, "%s:%d: cannot uninstall a rollback archive.\n", __FILE__, __LINE__);
		return DEPOT_ERROR;
	}

	/** 
	 * require -f to force uninstalling an archive installed on top of an older
	 * base system since the rollback archive we'll use will potentially damage
	 * the base system.
	 */
	if (!force && 
		this->m_build &&
		archive->build() &&
		(strcmp(this->m_build, archive->build()) != 0) &&
		!this->is_superseded(archive)
		) {
		fprintf(stderr, 
				"-------------------------------------------------------------------------------\n"
				"The %s root was installed on a different base OS build (%s). The current    \n"
				"OS build is %s. Uninstalling a root that was installed on a different OS     \n"
				"build has the potential to damage your OS install due to the fact that the   \n"
				"rollback data is from the wrong OS version.\n\n"
				" You must use the force (-f) option to make this potentially unsafe operation  \n"
				"happen.\n"
				"-------------------------------------------------------------------------------\n",
				archive->name(), archive->build(), m_build);
		return DEPOT_BUILD_MISMATCH;
	}

	if (res != 0) return res;

	if (!dryrun) {
		// XXX: this may be superfluous
		// uninstall_file should be smart enough to do a mtime check...
		if (res == 0) res = this->prune_directories();

		// We do this here to get an exclusive lock on the database.
		if (res == 0) res = this->begin_transaction();
		if (res == 0) res = m_db->deactivate_archive(serial);
		if (res == 0) res = this->commit_transaction();
	}
	
	InstallContext context(this, archive);
	context.reverse_files = true; // uninstall children before parents
	if (res == 0) res = this->iterate_files(archive, &Depot::uninstall_file, &context);
	
	if (!dryrun) {
		if (res == 0) res = this->begin_transaction();
		uint32_t i;
		for (i = 0; i < context.files_to_remove->count; ++i) {
			uint64_t serial = context.files_to_remove->values[i];
			if (res == 0) res = m_db->delete_file(serial);
		}
		if (res == 0) res = this->commit_transaction();

		if (res == 0) res = this->begin_transaction();	
		if (res == 0) res = this->remove(archive);
		if (res == 0) res = this->commit_transaction();

		// delete all of the expanded archive backing stores to save disk space
		if (res == 0) res = this->prune_directories();

		if (res == 0) res = this->prune_archive(archive);
	}
	
	if (res == 0) fprintf(stdout, "Uninstalled archive: %llu %s \n",
						  archive->serial(), archive->name());

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
	return DEPOT_OK;
}

void Depot::archive_header() {
	fprintf(stdout, "%-6s %-36s  %-12s  %-7s  %s\n", 
			"Serial", "UUID", "Date", "Build", "Name");
	fprintf(stdout, "====== ====================================  "
			"============  =======  =================\n");	
}

int Depot::verify(Archive* archive) {
	int res = 0;
	this->archive_header();
	list_archive(archive, stdout);	
	hr();
	if (res == 0) res = this->iterate_files(archive, &Depot::verify_file, NULL);
	hr();
	fprintf(stdout, "\n");
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
	strftime(date, sizeof(date), "%b %e %H:%M", &local);

	fprintf((FILE*)context, "%-6llu %-36s  %-12s  %-7s  %s\n", 
			serial, uuid, date, (archive->build()?archive->build():""), archive->name());
	
	return DEPOT_OK;
}

int Depot::list() {
	return this->list(0, NULL);
}

int Depot::list(int count, char** args) {
	int res = 0;

	this->archive_header();
	
	// handle the default case of "all"
	if (count == 0) return this->iterate_archives(&Depot::list_archive, stdout);

	Archive** list;
	Archive* archive;
	uint32_t archcnt;
	for (int i = 0; res == 0 && i < count; i++) {
		list = NULL;
		archive = NULL;
		archcnt = 0;
		// check for special keywords
		if (strncasecmp(args[i], "all", 3) == 0 && strlen(args[i]) == 3) {
			list = this->get_all_archives(&archcnt);
		} else if (strncasecmp(args[i], "superseded", 10) == 0 && strlen(args[i]) == 10) {
			list = this->get_superseded_archives(&archcnt);
		} 
		if (archcnt) {
			// loop over special keyword results
			for (uint32_t j = 0; res == 0 && j < archcnt; j++) {
				res = this->list_archive(list[j], stdout);
			}
		} else {
			// arg is a single-archive specifier
			archive = this->get_archive(args[i]);
			if (archive) res = this->list_archive(archive, stdout);
		}
	}

	return res;
}

int Depot::print_file(File* file, void* context) {
	extern uint32_t verbosity;
	if (verbosity & VERBOSE_DEBUG) fprintf((FILE*)context, "%04llx ", file->info());
	file->print((FILE*)context);
	return DEPOT_OK;
}

int Depot::files(Archive* archive) {
	int res = 0;
	this->archive_header();
	list_archive(archive, stdout);
	hr();
	if (res == 0) res = this->iterate_files(archive, &Depot::print_file, stdout);
	hr();
	fprintf(stdout, "\n");
	return res;
}

int Depot::dump_archive(Archive* archive, void* context) {
	Depot* depot = (Depot*)context;
	int res = 0;
	list_archive(archive, stdout);
	hr();
	if (res == 0) res = depot->iterate_files(archive, &Depot::print_file, stdout);
	hr();
	fprintf(stdout, "\n");
	return res;
}

int Depot::dump() {
	extern uint32_t verbosity;
	verbosity = 0xFFFFFFFF; // dump is intrinsically a debug command
	int res = 0;
	this->archive_header();
	if (res == 0) res = this->iterate_archives(&Depot::dump_archive, this);
	return res;
}


File* Depot::file_superseded_by(File* file) {
	uint8_t* data;
	int res = this->m_db->get_next_file(&data, file, FILE_SUPERSEDED);
	if (FOUND(res)) return this->m_db->make_file(data);
	return NULL;
}

File* Depot::file_preceded_by(File* file) {
	uint8_t* data;
	int res = this->m_db->get_next_file(&data, file, FILE_PRECEDED);
	if (FOUND(res)) return this->m_db->make_file(data);
	return NULL;
}

int Depot::check_consistency() {
	int res = 0;

	SerialSet* inactive = new SerialSet();
	assert(inactive != NULL);
	
	// get inactive archives serials from the database
	uint64_t* serials;
	uint32_t  count;
	this->m_db->get_inactive_archive_serials(&serials, &count);
	for (uint32_t i=0; i < count; i++) {
		inactive->add(serials[i]);
	}
	free(serials);
	
	// print a list of inactive archives
	if (res == 0 && inactive && inactive->count > 0) {
		fprintf(stderr, "The following archive%s in an inconsistent state and must be uninstalled "
						"before proceeding:\n\n", inactive->count > 1 ? "s are" : " is");
		uint32_t i;
		this->archive_header();
		for (i = 0; i < inactive->count; ++i) {
			Archive* archive = this->archive(inactive->values[i]);
			if (archive) {
				list_archive(archive, stdout);
				delete archive;
			}
		}
		fprintf(stderr, "\nWould you like to uninstall %s now? [y/n] ", 
				inactive->count > 1 ? "them" : "it");
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
	return this->m_db->begin_transaction();
}

int Depot::rollback_transaction() {
	return this->m_db->rollback_transaction();
}

int Depot::commit_transaction() {
	return this->m_db->commit_transaction();
}

int Depot::is_locked() { return m_is_locked; }

bool Depot::is_superseded(Archive* archive) {
	// return early if already known
	if (archive->m_is_superseded != -1) { 
		return (archive->m_is_superseded == 1);
	}
	
	// need to find out if superseded
	int res = DB_OK;
	uint8_t** filelist;
	uint8_t* data;
	uint32_t count;	
	res = this->m_db->get_files(&filelist, &count, archive, false);
	if (FOUND(res)) {
		for (uint32_t i=0; i < count; i++) {
			File* file = this->m_db->make_file(filelist[i]);
			
			// check for being superseded by a root
			res = this->m_db->get_next_file(&data, file, FILE_SUPERSEDED);
			this->m_db->free_file(data);
			if (FOUND(res)) continue;
			
			// check for being superseded by external changes
			char* actpath;
			join_path(&actpath, this->prefix(), file->path());
			File* actual = FileFactory(actpath);
			free(actpath);
			uint32_t flags = File::compare(file, actual);

			// not found in database and no changes on disk, 
			// so file is the current version of actual
			if (flags == FILE_INFO_IDENTICAL) {
				archive->m_is_superseded = 0;
				return false;
			}
			 
			// something external changed contents of actual,
			// so we consider this file superseded (by OS upgrade?)
		}
	}
	archive->m_is_superseded = 1;
	return true;			
}

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
	archive->m_serial = m_db->insert_archive(archive->uuid(),
											 archive->info(),
											 archive->name(),
											 archive->date_installed(),
											 archive->build());
	return archive->m_serial == 0;
}

int Depot::insert(Archive* archive, File* file) {
	// check for the destination prefix in file's path, remove if found
	char *path, *relpath;
	size_t prefixlen = strlen(this->prefix());
	asprintf(&path, "%s", file->path());
	relpath = path;
	if (strncmp(file->path(), this->prefix(), prefixlen) == 0) {
	        relpath += prefixlen - 1;
	}

	file->m_serial = m_db->insert_file(file->info(), file->mode(), file->uid(), file->gid(), 
									   file->digest(), archive, relpath);
	if (!file->m_serial) {
		fprintf(stderr, "Error: unable to insert file at path %s for archive %s \n", 
				relpath, archive->name());
		return DB_ERROR;
	}

	free(path);
	return DEPOT_OK;
}

int Depot::has_file(Archive* archive, File* file) {
	// check for the destination prefix in file's path, remove if found
	char *path, *relpath;
	size_t prefixlen = strlen(this->prefix());
	asprintf(&path, "%s", file->path());
	relpath = path;
	if (strncmp(file->path(), this->prefix(), prefixlen) == 0) {
		relpath += prefixlen - 1;
	}

	uint64_t count = m_db->count_files(archive, relpath);

	free(path);
	return count > 0;
}


int Depot::remove(Archive* archive) {
	int res = 0;
	res = m_db->delete_files(archive);
	if (res) {
		fprintf(stderr, "Error: unable to delete files for archive %llu \n", archive->serial());
		return res;
	}
	res = m_db->delete_archive(archive);
	if (res) {
		fprintf(stderr, "Error: unable to delete archive %llu \n", archive->serial());
		return res;
	}
	return res;
}

int Depot::remove(File* file) {
	return m_db->delete_file(file);
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
		fprintf(stdout, "An error occurred.\n");
	}
	return res;
}

// perform a command on an archive specification
int Depot::process_archive(const char* command, const char* archspec) {
	extern uint32_t verbosity;
	int res = 0;
	uint32_t count = 0;
	Archive** list = NULL;
	
	if (strncasecmp(archspec, "all", 3) == 0 && strlen(archspec) == 3) {
		list = this->get_all_archives(&count);
	} else if (strncasecmp(archspec, "superseded", 10) == 0 && strlen(archspec) == 10) {
		list = this->get_superseded_archives(&count);
	} else {
		// make a list of 1 Archive
		list = (Archive**)malloc(sizeof(Archive*));
		list[0] = this->get_archive(archspec);
		count = 1;
	}
	
	for (size_t i = 0; i < count; i++) {
		if (!list[i]) {
			fprintf(stdout, "Archive not found: %s\n", archspec);
			return DEPOT_ERROR;
		}
		if (verbosity & VERBOSE_DEBUG) {
			char uuid[37];
			uuid_unparse_upper(list[i]->uuid(), uuid);
			fprintf(stdout, "Found archive: %s\n", uuid);
		}
		res = this->dispatch_command(list[i], command);
		delete list[i];
	} 
	free(list);
	return res;
}

int Depot::rename_archive(const char* archspec, const char* name) {
	extern uint32_t verbosity;
	int res = 0;
	
	if ((strncasecmp(archspec, "all", 3) == 0 && strlen(archspec) == 3) ||
		(strncasecmp(archspec, "superseded", 10) == 0 && strlen(archspec) == 10)) {
		fprintf(stderr, "Error: keywords 'all' and 'superseded' cannot be used with the"
				" rename command.\n");
		return DEPOT_USAGE_ERROR;
	}
	
	Archive* archive = this->get_archive(archspec);
	if (!archive) {
		fprintf(stdout, "Archive not found: %s\n", archspec);
		return DEPOT_NOT_EXIST;
	}
	
	char uuid[37];
	uuid_unparse_upper(archive->uuid(), uuid);
	if (verbosity & VERBOSE_DEBUG) {
		fprintf(stdout, "Found archive: %s\n", uuid);
	}

	if (!name || strlen(name) == 0) {
		fprintf(stderr, "Error: invalid name: '%s'\n", name);
		return DEPOT_ERROR;
	}
	
	free(archive->m_name);
	archive->m_name = strdup(name);
	
	res = m_db->update_archive(archive->serial(),
							   archive->uuid(),
							   archive->name(),
							   archive->date_installed(),
							   1,
							   archive->info(),
							   archive->build());

	if (res == 0) fprintf(stdout, "Renamed archive %s to '%s'.\n", 
						  uuid, archive->name());
	
	delete archive;
	return res;
}
