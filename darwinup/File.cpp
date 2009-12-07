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

#include "Archive.h"
#include "File.h"
#include "Utils.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

File::File() {
	m_serial = 0;
	m_archive = NULL;
	m_info = FILE_INFO_NONE;
	m_path = NULL;
	m_mode = 0;
	m_uid = 0;
	m_gid = 0;
	m_size = 0;
	m_digest = NULL;
}

File::File(const char* path) {
	m_serial = 0;
	m_archive = NULL;
	m_info = FILE_INFO_NONE;
	m_mode = 0;
	m_uid = 0;
	m_gid = 0;
	m_size = 0;
	m_digest = NULL;
	if (path) m_path = strdup(path);
}

File::File(Archive* archive, FTSENT* ent) {	
	char path[PATH_MAX];
	path[0] = 0;
	ftsent_filename(ent, path, PATH_MAX);
	m_path = strdup(path);
	m_archive = archive;
	m_info = FILE_INFO_NONE;
	m_mode = ent->fts_statp->st_mode;
	m_uid = ent->fts_statp->st_uid;
	m_gid = ent->fts_statp->st_gid;
	m_size = ent->fts_statp->st_size;
	
	m_digest = NULL;
}

File::File(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest) {
	m_serial = serial;
	m_archive = archive;
	m_info = info;
	m_path = strdup(path);
	m_mode = mode;
	m_uid = uid;
	m_gid = gid;
	m_size = size;
	m_digest = digest;
}


File::~File() {
	if (m_path) free(m_path);
	if (m_digest) delete m_digest;
}

uint64_t	File::serial()	{ return m_serial; }
Archive*	File::archive()	{ return m_archive; }
uint32_t	File::info()	{ return m_info; }
const char*	File::path()	{ return m_path; }
mode_t		File::mode()	{ return m_mode; }
uid_t		File::uid()	{ return m_uid; }
gid_t		File::gid()	{ return m_gid; }
off_t		File::size()	{ return m_size; }
Digest*		File::digest()	{ return m_digest; }

void		File::info_set(uint32_t flag)	{ m_info = INFO_SET(m_info, flag); }
void		File::info_clr(uint32_t flag)	{ m_info = INFO_CLR(m_info, flag); }
void		File::archive(Archive* archive) { m_archive = archive; }

uint32_t File::compare(File* a, File* b) {
	if (a == b) return FILE_INFO_IDENTICAL; // identity
	if (a == NULL) return 0xFFFFFFFF; // existent and nonexistent file are infinitely different
	if (b == NULL) return 0xFFFFFFFF; // existent and nonexistent file are infinitely different
	
	uint32_t result = FILE_INFO_IDENTICAL;
	if (a->m_uid != b->m_uid) result |= FILE_INFO_UID_DIFFERS;
	if (a->m_gid != b->m_gid) result |= FILE_INFO_GID_DIFFERS;
	if (a->m_mode != b->m_mode) result |= FILE_INFO_MODE_DIFFERS;
	if ((a->m_mode & S_IFMT) != (b->m_mode & S_IFMT)) result |= FILE_INFO_TYPE_DIFFERS;
	if ((a->m_mode & ALLPERMS) != (b->m_mode & ALLPERMS)) result |= FILE_INFO_PERM_DIFFERS;
	//if (a->m_size != b->m_size) result |= FILE_INFO_SIZE_DIFFERS;
	if (Digest::equal(a->m_digest, b->m_digest) == 0) result |= FILE_INFO_DATA_DIFFERS;
	return result;
}


void File::print(FILE* stream) {
	char* dig = m_digest ? m_digest->string() :
		strdup("                                        ");
	
	char mode_str[12];
	strmode(m_mode, mode_str);
		
	fprintf(stream, "%s % 4d % 4d %s %s\n", mode_str, m_uid, m_gid, dig, m_path);
	free(dig);
}

int File::install(const char* prefix, const char* dest) {
	extern uint32_t force;
	int res = 0;
	Archive* archive = this->archive();
	assert(archive != NULL);
	char* dirpath = archive->directory_name(prefix);
	IF_DEBUG("[install] dirpath is %s\n", dirpath);

	char srcpath[PATH_MAX];
	const char* path = this->path();
	char* dstpath;
	join_path(&dstpath, dest, path);

	if (dirpath) {
	        ssize_t len = snprintf(srcpath, sizeof(srcpath), "%s/%s", dirpath, path);
		if ((size_t)len > sizeof(srcpath)) {
			fprintf(stderr, "ERROR: [install] path too long: %s/%s\n", dirpath, path);
			return -1;
		}
		IF_DEBUG("[install] about to rename %s to %s\n", srcpath, dstpath);
		res = rename(srcpath, dstpath);
		if (res == -1) {
			if (errno == ENOENT) {
				// the file wasn't found, try to do on-demand
				// expansion of the archive that contains it.
				if (is_directory(dirpath) == 0) {
					IF_DEBUG("[install] File::install on-demand archive expansion");
					res = archive->expand_directory(prefix);
					if (res == 0) res = this->install(prefix, dest);
				} else {
					// archive was already expanded, so
					// the file is truly missing (worry).
					IF_DEBUG("[install] File::install missing file in archive");
					fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__, srcpath, strerror(errno), errno);
				}
			} else if (force && errno == ENOTDIR) {
				// a) some part of destination path does not exist
				// b) from is a directory, but to is not
				IF_DEBUG("[install] File::install ENOTDIR\n");
			} else if (force && errno == EISDIR) {
				// to is a directory, but from is not
				IF_DEBUG("[install] File::install EISDIR\n");
			} else if (force && errno == ENOTEMPTY) {
				// to is a directory and is not empty
				IF_DEBUG("[install] File::install ENOTEMPTY\n");
			} else {
				if (!force) {
					fprintf(stderr, "ERROR: darwinup cannot handle what you are trying to do. You can use the force option to allow more potentially-unsafe operations.\n");
				}
				fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__, dstpath, strerror(errno), errno);
				fprintf(stderr, "ERROR: fatal error during File::install. Cannot continue.\n");
			}
		} else {
			IF_DEBUG("[install] rename(%s, %s)\n", srcpath, dstpath);
		}
		free(dirpath);
	} else {
		res = -1;
	}
	free(dstpath);
	return res;
}

int File::remove() {
	// not implemented
	fprintf(stderr, "%s:%d: call to abstract function File::remove\n", __FILE__, __LINE__);
	return -1;
}

int File::install_info(const char* dest) {
	int res = 0;
	char* path;
	join_path(&path, dest, this->path());
	uid_t uid = this->uid();
	gid_t gid = this->gid();
	mode_t mode = this->mode() & ALLPERMS;

	IF_DEBUG("[install] chown(%s, %d, %d)\n", path, uid, gid);
	if (res == 0) res = chown(path, uid, gid);
	IF_DEBUG("[install] chmod(%s, %04o)\n", path, mode);
	if (res == 0) res = chmod(path, mode);

	free(path);
	return res;
}

NoEntry::NoEntry(const char* path) : File(path) {
	m_info = INFO_SET(m_info, FILE_INFO_NO_ENTRY);
}

NoEntry::NoEntry(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest) : File(serial, archive, info, path, mode, uid, gid, size, digest) {}

Regular::Regular(Archive* archive, FTSENT* ent) : File(archive, ent) {
	m_digest = new SHA1DigestMachO(ent->fts_accpath);
}

Regular::Regular(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest) : File(serial, archive, info, path, mode, uid, gid, size, digest) {
	if (digest == NULL || serial == 0) {
		m_digest = new SHA1DigestMachO(path);
	}
}

int Regular::remove() {
	int res = 0;
	const char* path = this->path();
	res = unlink(path);
	IF_DEBUG("[remove] unlink %s\n", path);
	if (res == -1 && errno == ENOENT) {
		// We can safely ignore this because we were going to
		// remove the file anyway
		res = 0;
	} else if (res != 0) {
		fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__, m_path, strerror(errno), errno);
	}
	return res;
}

Symlink::Symlink(Archive* archive, FTSENT* ent) : File(archive, ent) {
	m_digest = new SHA1DigestSymlink(ent->fts_accpath);
}

Symlink::Symlink(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest) : File(serial, archive, info, path, mode, uid, gid, size, digest) {
	if (digest == NULL || serial == 0) {
		m_digest = new SHA1DigestSymlink(path);
	}
}

int Symlink::remove() {
	int res = 0;
	const char* path = this->path();
	res = unlink(path);
	IF_DEBUG("[remove] unlink %s", path);
	if (res == -1 && errno == ENOENT) {
		// We can safely ignore this because we were going to
		// remove the file anyway
		res = 0;
	} else if (res == -1) {
		fprintf(stderr, "%s:%d: %s (%d)\n", __FILE__, __LINE__, strerror(errno), errno);
	}
	return res;
}

int Symlink::install_info(const char* dest) {
	int res = 0;
	const char* path = this->path();
	//mode_t mode = this->mode() & ALLPERMS;
	uid_t uid = this->uid();
	gid_t gid = this->gid();
	IF_DEBUG("[install] lchown(%d, %d)\n", uid, gid);
	if (res == 0) res = lchown(path, uid, gid);
	//IF_DEBUG("[install] lchmod(%o)\n", mode);
	//if (res == 0) res = lchmod(path, mode);
	return res;
}

Directory::Directory(Archive* archive, FTSENT* ent) : File(archive, ent) {}

Directory::Directory(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest) : File(serial, archive, info, path, mode, uid, gid, size, digest) {};

int Directory::install(const char* prefix, const char* dest) {
	// We create a new directory instead of renaming the
	// existing one, since that would move the entire
	// sub-tree, and lead to a lot of ENOENT errors.
	int res = 0;
	extern uint32_t force;
	char* dstpath;
	join_path(&dstpath, dest, this->path());
	
	mode_t mode = this->mode() & ALLPERMS;
	uid_t uid = this->uid();
	gid_t gid = this->gid();
	
	IF_DEBUG("[install] mkdir(%s, %04o)\n", dstpath, mode);
	if (res == 0) res = mkdir(dstpath, mode);
	if (res && errno == EEXIST) res = 0;

	if (force && res == -1 && errno == ENOTDIR) {
		// some part of destination path is not a directory
		IF_DEBUG("[install] Directory::install ENOTDIR1\n");
	} else if (res == -1) {
		fprintf(stderr, "ERROR: %s:%d: %s: %s (%d)\n", __FILE__, __LINE__, dstpath, strerror(errno), errno);
		fprintf(stderr, "ERROR: unable to create %s \n", dstpath);
	}
	
	if (res == 0) {
		res = chown(dstpath, uid, gid);
		if (res != 0) {
			fprintf(stderr, "ERROR: %s:%d: %s: %s (%d)\n", __FILE__, __LINE__, dstpath, strerror(errno), errno);
			fprintf(stderr, "ERROR: unable to change ownership of %s \n", dstpath);
		}
	}

	free(dstpath);
	return res;
}

int Directory::remove() {
	int res = 0;
	const char* path = this->path();
	res = rmdir(path);
	IF_DEBUG("[remove] rmdir %s\n", path);
	if (res == -1 && errno == ENOENT) {
		// We can safely ignore this because we were going to
		// remove the directory anyway
		res = 0;
	} else if (res == -1 && errno == ENOTEMPTY) {
	        res = remove_directory(path);
	} else if (res == -1) {
		fprintf(stderr, "%s:%d: %s (%d)\n", __FILE__, __LINE__, strerror(errno), errno);
	}
	return res;
}


File* FileFactory(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest) {
	File* file = NULL;
	switch (mode & S_IFMT) {
		case S_IFDIR:
			file = new Directory(serial, archive, info, path, mode, uid, gid, size, digest);
			break;
		case S_IFREG:
			file = new Regular(serial, archive, info, path, mode, uid, gid, size, digest);
			break;
		case S_IFLNK:
			file = new Symlink(serial, archive, info, path, mode, uid, gid, size, digest);
			break;
		case 0:
			if (INFO_TEST(info, FILE_INFO_NO_ENTRY)) {
				file = new NoEntry(serial, archive, info, path, mode, uid, gid, size, digest);
				break;
			}
		default:
			fprintf(stderr, "%s:%d: unexpected file type %o\n", __FILE__, __LINE__, mode & S_IFMT);
			break;
	}
	return file;
}

File* FileFactory(Archive* archive, FTSENT* ent) {
	File* file = NULL;
	switch (ent->fts_info) {
		case FTS_D:
			file = new Directory(archive, ent);
			break;
		case FTS_F:
			file = new Regular(archive, ent);
			break;
		case FTS_SL:
		case FTS_SLNONE:
			file = new Symlink(archive, ent);
			break;
		case FTS_DP:
			break;
		case FTS_DEFAULT:
		case FTS_DNR:
			fprintf(stderr, "%s:%d: could not read directory.  Run as root.\n", __FILE__, __LINE__);
			break;
		default:
			fprintf(stderr, "%s:%d: unexpected fts_info type %d\n", __FILE__, __LINE__, ent->fts_info);
			break;
	}
	return file;
}

File* FileFactory(const char* path) {
	File* file = NULL;
	struct stat sb;
	int res = 0;
	extern uint32_t force;
	
	res = lstat(path, &sb);
	if (res == -1 && errno == ENOENT) {
		// destination does not have a matching node
		return NULL;
	} else if (force && res == -1 && errno == ENOTDIR) {
		// some part of destination path does not exist
		IF_DEBUG("[       ] FileFactory ENOTDIR\n");
		return NULL;
	}	
	if (res == -1) {
		fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__, path, strerror(errno), errno);
		fprintf(stderr, "ERROR: unable to stat %s \n", path);
		return NULL;
	}
	
	file = FileFactory(0, NULL, FILE_INFO_NONE, path, sb.st_mode, sb.st_uid, sb.st_gid, sb.st_size, NULL);
	return file;
}
