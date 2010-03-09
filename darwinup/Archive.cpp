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
#include "Utils.h"

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char** environ;

Archive::Archive(const char* path) {
	m_serial = 0;
	uuid_generate_random(m_uuid);
	m_path = strdup(path);
	m_name = strdup(basename(m_path));
	m_info = 0;
	m_date_installed = time(NULL);
}

Archive::Archive(uint64_t serial, uuid_t uuid, const char* name, const char* path, 
				 uint64_t info, time_t date_installed, const char* build) {
	m_serial = serial;
	uuid_copy(m_uuid, uuid);
	m_name = name ? strdup(name) : NULL;
	m_path = path ? strdup(path) : NULL;
	m_build = build ? strdup(build) : NULL;
	m_info = info;
	m_date_installed = date_installed;
}


Archive::~Archive() {
	if (m_path) free(m_path);
	if (m_name) free(m_name);
	if (m_build) free(m_build);
}

uint64_t	Archive::serial()		{ return m_serial; }
uint8_t*	Archive::uuid()			{ return m_uuid; }
const char*	Archive::name()			{ return m_name; }
const char*	Archive::path()			{ return m_path; }
const char*	Archive::build()		{ return m_build; }
uint64_t	Archive::info()			{ return m_info; }
time_t		Archive::date_installed()	{ return m_date_installed; }

char* Archive::directory_name(const char* prefix) {
	char* path = NULL;
	char uuidstr[37];
	uuid_unparse_upper(m_uuid, uuidstr);
	asprintf(&path, "%s/%s", prefix, uuidstr);
	if (path == NULL) {
		fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__);
	}
	return path;
}

char* Archive::create_directory(const char* prefix) {
	int res = 0;
	char* path = this->directory_name(prefix);
	if (path && res == 0) res = mkdir(path, 0777);
	if (res != 0) {
		fprintf(stderr, "%s:%d: could not create directory: %s: %s (%d)\n", __FILE__, __LINE__, path, strerror(errno), errno);
		free(path);
		path = NULL;
	}
	if (res == 0) res = chown(path, 0, 0);
	return path;
}

int Archive::compact_directory(const char* prefix) {
	int res = 0;
	char* tarpath = NULL;
	char uuidstr[37];
	uuid_unparse_upper(m_uuid, uuidstr);
	asprintf(&tarpath, "%s/%s.tar.bz2", prefix, uuidstr);
	if (tarpath) {
		const char* args[] = {
			"/usr/bin/tar",
			"cjf", tarpath,
			"-C", prefix,
			uuidstr,
			NULL
		};
		res = exec_with_args(args);
		free(tarpath);
	} else {
		fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__);
		res = -1;
	}
	return res;
}

int Archive::expand_directory(const char* prefix) {
	int res = 0;
	char* tarpath = NULL;
	char uuidstr[37];
	uuid_unparse_upper(m_uuid, uuidstr);
	asprintf(&tarpath, "%s/%s.tar.bz2", prefix, uuidstr);
	if (tarpath) {
		const char* args[] = {
			"/usr/bin/tar",
			"xjf", tarpath,
			"-C", prefix,
			"-p",	// --preserve-permissions
			NULL
		};
		res = exec_with_args(args);
		free(tarpath);
	} else {
		fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__);
		res = -1;
	}
	return res;
}


int Archive::extract(const char* destdir) {
	// not implemented
	return -1;
}



RollbackArchive::RollbackArchive() : Archive("<Rollback>") {
	m_info = ARCHIVE_INFO_ROLLBACK;
}



DittoArchive::DittoArchive(const char* path) : Archive(path) {}

int DittoArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/ditto",
		m_path, destdir,
		NULL
	};
	return exec_with_args(args);
}


DittoXArchive::DittoXArchive(const char* path) : Archive(path) {}

int DittoXArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/ditto",
		"-x", m_path,
		destdir,
		NULL
	};
	return exec_with_args(args);
}

CpioArchive::CpioArchive(const char* path) : DittoXArchive(path) {}

CpioGZArchive::CpioGZArchive(const char* path) : DittoXArchive(path) {}

CpioBZ2Archive::CpioBZ2Archive(const char* path) : DittoXArchive(path) {}

PaxArchive::PaxArchive(const char* path) : DittoXArchive(path) {}

PaxGZArchive::PaxGZArchive(const char* path) : DittoXArchive(path) {}

PaxBZ2Archive::PaxBZ2Archive(const char* path) : DittoXArchive(path) {}


TarArchive::TarArchive(const char* path) : Archive(path) {}

int TarArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/tar",
		"xf", m_path,
		"-C", destdir,
		NULL
	};
	return exec_with_args(args);
}


TarGZArchive::TarGZArchive(const char* path) : Archive(path) {}

int TarGZArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/tar",
		"xzf", m_path,
		"-C", destdir,
		NULL
	};
	return exec_with_args(args);
}


TarBZ2Archive::TarBZ2Archive(const char* path) : Archive(path) {}

int TarBZ2Archive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/tar",
		"xjf", m_path,
		"-C", destdir,
		NULL
	};
	return exec_with_args(args);
}


XarArchive::XarArchive(const char* path) : Archive(path) {}

int XarArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/xar",
		"-xf", m_path,
		"-C", destdir,
		NULL
	};
	return exec_with_args(args);
}


ZipArchive::ZipArchive(const char* path) : Archive(path) {}

int ZipArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/ditto",
		"-xk", m_path,
		destdir,
		NULL
	};
	return exec_with_args(args);
}


Archive* ArchiveFactory(const char* path, const char* tmppath) {
	Archive* archive = NULL;

	// actual path to archive
	char* actpath = NULL; 
	
	// fetch remote archives if needed
	if (is_url_path(path)) {
		actpath = fetch_url(path, tmppath);
		if (!actpath) {
			fprintf(stderr, "Error: could not fetch remote URL: %s \n", path);
			return NULL;
		}
	} else if (is_userhost_path(path)) {
		actpath = fetch_userhost(path, tmppath);
		if (!actpath) {
			fprintf(stderr, "Error: could not fetch remote file from: %s \n", path);
			return NULL;
		}
	} else {
		actpath = (char *)path;
	}

	
	// make sure the archive exists
	struct stat sb;
	int res = stat(actpath, &sb);
	if (res == -1 && errno == ENOENT) {
		return NULL;
	}
	
	// use file extension to guess archive format
	if (is_directory(actpath)) {
		archive = new DittoArchive(actpath);
	} else if (has_suffix(actpath, ".cpio")) {
		archive = new CpioArchive(actpath);
	} else if (has_suffix(actpath, ".cpio.gz") || has_suffix(actpath, ".cpgz")) {
		archive = new CpioGZArchive(actpath);
	} else if (has_suffix(actpath, ".cpio.bz2") || has_suffix(actpath, ".cpbz2")) {
		archive = new CpioBZ2Archive(actpath);
	} else if (has_suffix(actpath, ".pax")) {
		archive = new PaxArchive(actpath);
	} else if (has_suffix(actpath, ".pax.gz") || has_suffix(actpath, ".pgz")) {
		archive = new PaxGZArchive(actpath);
	} else if (has_suffix(actpath, ".pax.bz2") || has_suffix(actpath, ".pbz2")) {
		archive = new PaxBZ2Archive(actpath);		
	} else if (has_suffix(actpath, ".tar")) {
		archive = new TarArchive(actpath);
	} else if (has_suffix(actpath, ".tar.gz") || has_suffix(actpath, ".tgz")) {
		archive = new TarGZArchive(actpath);
	} else if (has_suffix(actpath, ".tar.bz2") || has_suffix(actpath, ".tbz2")) {
		archive = new TarBZ2Archive(actpath);		
	} else if (has_suffix(actpath, ".xar")) {
		archive = new XarArchive(actpath);
	} else if (has_suffix(actpath, ".zip")) {
		archive = new ZipArchive(actpath);
	} else {
		fprintf(stderr, "Error: unknown archive type: %s\n", path);
	}
	
	if (actpath && actpath != path) free(actpath);
	return archive;
}
