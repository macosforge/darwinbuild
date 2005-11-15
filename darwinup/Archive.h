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

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <uuid/uuid.h>


//
// ARCHIVE_INFO flags stored in the database
//
const uint64_t ARCHIVE_INFO_ROLLBACK	= 0x0001;

struct Archive;
struct Depot;

////
//  Archive
//
//  Archive is the root class of all archive formats
//  supported by darwinup.
//
//  Conceptually it's an abstract class, although that
//  hasn't been formalized.
//
//  ArchiveFactory exists to return the correct
//  concrete subclass for a given archive to be
//  installed.  Currently this is determined
//  by the file's suffix.
////

Archive* ArchiveFactory(const char* path);

struct Archive {
	Archive(const char* path);
	virtual ~Archive();

	////
	//  Public Accessor functions
	////

	// Unique serial number for the archive (used by database).
	virtual	uint64_t serial();
	
	// Universally unique identifier for the archive.
	virtual	uint8_t* uuid();
	
	// The name of the archive as it was installed.
	// Determined by basename(3).
	// Do not modify or free(3).
	virtual const char* name();
	
	// The path to the archive as it was installed.
	// Do not modify or free(3).
	virtual const char* path();
	
	// ARCHIVE_INFO flags.
	virtual uint64_t info();
	
	// The epoch seconds when the archive was installed.
	virtual time_t date_installed();


	////
	//  Member functions
	////
	
	// Extracts the archive into the specified destination.
	// Not implemented for Archive, expected to be implemented
	// by concrete subclasses.
	virtual int extract(const char* destdir);

	// Returns the backing-store directory name for the archive.
	// This is prefix/uuid.
	// The result should be released with free(3).
	char* directory_name(const char* prefix);
	
	// Creates the backing-store directory for the archive.
	// Same directory name as returned by directory_name().
	char* create_directory(const char* prefix);
	
	// Compacts the backing-store directory into a single file.
	int compact_directory(const char* prefix);
	
	// Expands the backing-store directory from its single file.
	int expand_directory(const char* prefix);

	protected:

	// Constructor for subclasses and Depot to use when unserializing an archive from the database.
	Archive(uint64_t serial, uuid_t uuid, const char* name, const char* path, uint64_t info, time_t date_installed);

	uint64_t	m_serial;
	uuid_t		m_uuid;
	char*		m_name;
	char*		m_path;
	uint64_t	m_info;
	time_t		m_date_installed;
	
	friend struct Depot;
};


////
//  RollbackArchive
//
//  Not a file format.  RollbackArchive is an internal representation
//  of archives that are created to store the user-data that darwinup
//  archives as part of installation.
////
struct RollbackArchive : public Archive {
	RollbackArchive();
};


////
//  DittoArchive
//
//  Not a file format, but this allows a directory tree to be installed
//  using the ditto(1) command line tool.  This is useful for installing
//  Darwin roots built with DarwinBuild.
////
struct DittoArchive : public Archive {
	DittoArchive(const char* path);
	virtual int extract(const char* destdir);
};


////
//  PkgArchive
//
//  Corresponds to the Mac OS X installer's .pkg bundle format.
//  Installs the archive using the pax(1) command line tool.
//  NOTE: this does not make any attempt to perform any of the
//  volume checks or run any preflight or postflight actions.
////
struct PkgArchive : public Archive {
	PkgArchive(const char* path);
	virtual int extract(const char* destdir);
};

////
//  TarArchive
//
//  Corresponds to the tar(1) file format.  This handles uncompressed tar
//  archives by using the tar(1) command line tool.
////
struct TarArchive : public Archive {
	TarArchive(const char* path);
	virtual int extract(const char* destdir);
};


////
//  TarGZArchive
//
//  Corresponds to the tar(1) file format, compressed with gzip(1).
//  This installs archives using the tar(1) command line tool with
//  the -z option.
////
struct TarGZArchive : public Archive {
	TarGZArchive(const char* path);
	virtual int extract(const char* destdir);
};


////
//  TarBZ2Archive
//
//  Corresponds to the tar(1) file format, compressed with bzip2(1).
//  This installs archives using the tar(1) command line tool with
//  the -j option.
////
struct TarBZ2Archive : public Archive {
	TarBZ2Archive(const char* path);
	virtual int extract(const char* destdir);
};
