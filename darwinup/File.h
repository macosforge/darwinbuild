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

#include "Digest.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

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
        virtual int install(const char* prefix, const char* dest);
	
	// Sets the mode, uid, and gid of the file in the dest path
	// XXX: rename as repair()?
	virtual int install_info(const char* dest);
	
	// Removes the file
	virtual int remove();

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
	virtual int install(const char* prefix, const char* dest);
	virtual int remove();
};
