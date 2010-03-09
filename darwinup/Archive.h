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

#ifndef _ARCHIVE_H
#define _ARCHIVE_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <uuid/uuid.h>

typedef char* archive_name_t;

enum archive_keyword_t {
	DEPOT_ARCHIVE_NEWEST,
	DEPOT_ARCHIVE_OLDEST
};


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
//  by the file's suffix. The tmppath parameter
//  is the path where files can be stored during
//  processing, such as fetching remote archives. 
////

Archive* ArchiveFactory(const char* path, const char* tmppath);

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
	
	// the OS build this archive was installed onto
	virtual const char* build();
	
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

	// Constructor for subclasses and Depot to use when 
	//  unserializing an archive from the database.
	Archive(uint64_t serial, uuid_t uuid, const char* name, const char* path, 
			uint64_t info, time_t date_installed, const char* build);
	
	uint64_t	m_serial;
	uuid_t		m_uuid;
	char*		m_name;
	char*		m_path;
	char*       m_build;
	uint64_t	m_info;
	time_t		m_date_installed;
	
	friend struct Depot;
	friend struct DarwinupDatabase;
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
//  DittoXArchive
//
//  Handles any file that `ditto -x` can handle. Intended to be the parent
//  of suffix-specific archive objects. 
////
struct DittoXArchive : public Archive {
	DittoXArchive(const char* path);
	virtual int extract(const char* destdir);
};


////
//  CpioArchive
//
//  Corresponds to the cpio(1) file format.  
//  This installs archives using the ditto(1) command line tool with
//  the -x option.
////
struct CpioArchive : public DittoXArchive {
	CpioArchive(const char* path);
};

////
//  CpioGZArchive
//
//  Corresponds to the cpio(1) file format, compressed with gzip(1).
//  This installs archives using the ditto(1) command line tool with
//  the -x option.
////
struct CpioGZArchive : public DittoXArchive {
	CpioGZArchive(const char* path);
};

////
//  CpioBZ2Archive
//
//  Corresponds to the cpio(1) file format, compressed with bzip2(1).
//  This installs archives using the ditto(1) command line tool with
//  the -x option.
////
struct CpioBZ2Archive : public DittoXArchive {
	CpioBZ2Archive(const char* path);
};


////
//  PaxArchive
//
//  Corresponds to the pax(1) file format.  
//  This installs archives using the ditto(1) command line tool with
//  the -x option.
////
struct PaxArchive : public DittoXArchive {
	PaxArchive(const char* path);
};


////
//  PaxGZArchive
//
//  Corresponds to the pax(1) file format, compressed with gzip(1).
//  This installs archives using the ditto(1) command line tool with
//  the -x option.
////
struct PaxGZArchive : public DittoXArchive {
	PaxGZArchive(const char* path);
};


////
//  PaxBZ2Archive
//
//  Corresponds to the pax(1) file format, compressed with bzip2(1).
//  This installs archives using the ditto(1) command line tool with
//  the -x option.
////
struct PaxBZ2Archive : public DittoXArchive {
	PaxBZ2Archive(const char* path);
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


////
//  XarArchive
//
//  Corresponds to the xar(1) file format.  This handles uncompressed cpio
//  archives by using the xar(1) command line tool.
////
struct XarArchive : public Archive {
	XarArchive(const char* path);
	virtual int extract(const char* destdir);
};


////
//  ZipArchive
//
//  Corresponds to a zip archive. We use the -k option to ditto(1)
//  to handle it. 
////
struct ZipArchive : public Archive {
	ZipArchive(const char* path);
	virtual int extract(const char* destdir);
};

#endif

