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

#ifndef _FILE_H
#define _FILE_H

#include "Digest.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

#define FILE_OBJ_CHANGE_ERROR \
"-----------------------------------------------------------------------------\n" \
"Potentially unsafe mismatch between the root and destination:  \n\n" \
"%s\n\n" \
"You seem to be trying to install a %s over a %s.                             \n" \
"Darwinup will not install this root by default since it could cause damage   \n" \
"to your system. You can use the force (-f) option to allow darwinup to       \n" \
"attempt the install anyway.                                                  \n" \
"-----------------------------------------------------------------------------\n"

#define FILE_TYPE_STRING(type) \
(type == S_IFIFO ? "named pipe" : \
(type == S_IFCHR ? "character special" : \
(type == S_IFDIR ? "directory" : \
(type == S_IFBLK ? "block special" : \
(type == S_IFREG ? "file" : \
(type == S_IFLNK ? "symbolic link" : \
(type == S_IFSOCK ? "socket" : \
(type == S_IFWHT ? "whiteout" : \
"unknown"))))))))


enum file_starseded_t {
	FILE_SUPERSEDED,
	FILE_PRECEDED
};

//
// FILE_INFO flags stored in the database
//
const uint32_t FILE_INFO_NONE			= 0x0000;
const uint32_t FILE_INFO_BASE_SYSTEM		= 0x0001;	// file was part of base system, cannot uninstall
const uint32_t FILE_INFO_NO_ENTRY		= 0x0002;	// placeholder in the database for non-existent file
const uint32_t FILE_INFO_INSTALL_DATA		= 0x0010;	// actually install the file
const uint32_t FILE_INFO_ROLLBACK_DATA		= 0x0020;	// file exists in rollback archive

//
// FILE_INFO flags returned by File::compare()
//
const uint32_t FILE_INFO_IDENTICAL		= 0x00000000;

const uint32_t FILE_INFO_GID_DIFFERS		= 0x00100000;
const uint32_t FILE_INFO_UID_DIFFERS		= 0x00200000;

const uint32_t FILE_INFO_MODE_DIFFERS		= 0x01000000;	// mode differs overall
const uint32_t FILE_INFO_TYPE_DIFFERS		= 0x02000000;	//   S_IFMT differs
const uint32_t FILE_INFO_PERM_DIFFERS		= 0x04000000;	//   ALLPERMS differs

const uint32_t FILE_INFO_SIZE_DIFFERS		= 0x10000000;
const uint32_t FILE_INFO_DATA_DIFFERS		= 0x20000000;


struct Archive;
struct File;

////
//  File
//
//  File is the root class for all filesystem objects.
//  Conceptually it's an abstract class, although that
//  hasn't been formalized.
//
//  Concrete subclasses exist for specific file types:
//  Regular, Symlink, Directory, and NoEntry, indicating
//  that the given path does not exist.
//
//  FileFactory functions exist to return the correct
//  concrete subclass for a given filesystem object.
////

File* FileFactory(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest);
File* FileFactory(const char* path);
File* FileFactory(Archive* archive, FTSENT* ent);


struct File {
	File();
	File(File*);
	File(const char* path);
	File(Archive* archive, FTSENT* ent);
	File(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest);
	virtual ~File();

	////
	// Public Accessor functions
	////
	
	// Unique serial number for the file (used by database).
	virtual uint64_t serial();
	
	// FILE_INFO flags.
	virtual uint32_t info();
	virtual void info_set(uint32_t);
	virtual void info_clr(uint32_t);
	
	// Pointer to the Archive this file belongs to.
	virtual Archive* archive();
	virtual void archive(Archive* archive);
	
	// Path of the file on disk (absolute path).
	// Do not modify or free(3).
	virtual	const char* path();
	
	// Mode of the file, including the file type.
	virtual	mode_t mode();
	
	// Uid of the file.
	virtual	uid_t uid();
	
	// Gid of the file.
	virtual	gid_t gid();
	
	// Size of the file.
	virtual off_t size();
	
	// Digest of the file's data.
	virtual Digest* digest();

	////
	//  Class functions
	////
	
	// Compare two files, setting the appropriate
	// FILE_INFO bits in the return value.
	static uint32_t compare(File* a, File* b);

	////
	//  Member functions
	////

	// Installs the file from the archive into the prefix
	// i.e., for regular files:
	// rename(prefix + this->archive()->uuid() + this->path(), dest + this->path());
	virtual int install(const char* prefix, const char* dest, bool uninstall);
	// only used for directories
	virtual int dirrename(const char* prefix, const char* dest, bool uninstall);
	
	// Sets the mode, uid, and gid of the file in the dest path
	// XXX: rename as repair()?
	virtual int install_info(const char* dest);
	
	// Removes the file
	virtual int remove();

	// Removes any quarantine xattrs present
	int unquarantine(const char *prefix);

	// Prints one line to the output stream indicating
	// the file mode, ownership, digest and name.
	virtual void print(FILE* stream);

	protected:

	uint64_t	m_serial;
	uint32_t	m_info;
	Archive*	m_archive;
	char*		m_path;
	mode_t		m_mode;
	uid_t		m_uid;
	gid_t		m_gid;
	off_t		m_size;
	Digest*		m_digest;
	
	friend struct Depot;
};


////
//  Placeholder for rollback archives in the database.
//  Indicates that the given path had no entry at the time that
//  the archive was created.
////

struct NoEntry : File {
	NoEntry(const char* path);
	NoEntry(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest);
};

////
//  A regular file.
//  Digest is of the data fork of the file.
//  NOTE: Extended attributes are not detected or preserved.
////
struct Regular : File {
	Regular(Archive* archive, FTSENT* ent);
	Regular(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest);
	virtual int remove();
};

////
//  A symbolic link.
//  Digest is of the target obtained via readlink(2).
////
struct Symlink : File {
	Symlink(Archive* archive, FTSENT* ent);
	Symlink(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest);
	virtual int install_info(const char* dest);
	virtual int remove();
};

////
//  A directory.
//  Digest is null.
////
struct Directory : File {
	Directory(Archive* archive, FTSENT* ent);
	Directory(uint64_t serial, Archive* archive, uint32_t info, const char* path, mode_t mode, uid_t uid, gid_t gid, off_t size, Digest* digest);
	virtual int install(const char* prefix, const char* dest, bool uninstall);
	virtual int dirrename(const char* prefix, const char* dest, bool uninstall);
	int _install(const char* prefix, const char* dest, bool uninstall, bool use_rename);
	virtual int remove();
};

#endif
