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

#include "DBPlugin.h"
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <fcntl.h>

int register_files(void* db, char* build, char* project, char* path);

static int run(void* session, CFArrayRef argv) {
	int res = 0;
	CFIndex count = CFArrayGetCount(argv);
	if (count != 2)  return -1;
	char* build = strdup_cfstr(DBGetCurrentBuild(session));
	char* project = strdup_cfstr(CFArrayGetValueAtIndex(argv, 0));
	char* dstroot = strdup_cfstr(CFArrayGetValueAtIndex(argv, 1));
	res = register_files(session, build, project, dstroot);
	free(build);
	free(project);
	free(dstroot);
	return res;
}

static CFStringRef usage(void* session) {
	return CFRetain(CFSTR("<project> <dstroot>"));
}

DBPlugin* initialize(int version) {
	DBPlugin* plugin = NULL;

	if (version != kDBPluginCurrentVersion) return NULL;
	
	plugin = malloc(sizeof(DBPlugin));
	if (plugin == NULL) return NULL;
	
	plugin->version = kDBPluginCurrentVersion;
	plugin->type = kDBPluginType;
	plugin->name = CFSTR("register");
	plugin->run = &run;
	plugin->usage = &usage;

	return plugin;
}

static int buildpath(char* path, size_t bufsiz, FTSENT* ent) {
	if (ent->fts_parent != NULL && ent->fts_level > 1) {
		bufsiz = buildpath(path, bufsiz, ent->fts_parent);
	}
	strncat(path, "/", bufsiz);
	bufsiz -= 1;
	if (ent->fts_name) {
		strncat(path, ent->fts_name, bufsiz);
		bufsiz -= strlen(ent->fts_name);
	}
	return bufsiz;
}

// If the path points to a Mach-O file, records all dylib
// link commands as library dependencies in the database.
// XXX
// Ideally library dependencies are tracked per-architecture.
// For now, we're assuming all architectures contain identical images.
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>

static int register_mach_header(void* db, char* build, char* project, struct mach_header* mh, int fd) {
	int swap = 0;
	if (mh->magic != MH_MAGIC && mh->magic != MH_CIGAM) return 0;
	if (mh->magic == MH_CIGAM) { 
		swap = 1;
		swap_mach_header(mh, NXHostByteOrder());
	}
	
	switch (mh->filetype) {
		case MH_EXECUTE:
		case MH_DYLIB:
		case MH_BUNDLE:
			break;
		case MH_OBJECT:
		default:
			return 0;
	}
	
	int i;
	for (i = 0; i < mh->ncmds; ++i) {
		struct dylib_command lc;
		int res = read(fd, &lc, sizeof(struct load_command));
		if (res < sizeof(struct load_command)) { return 0; }

		uint32_t cmd = swap ? NXSwapLong(lc.cmd) : lc.cmd;

		if (cmd == LC_LOAD_DYLIB || cmd == LC_LOAD_WEAK_DYLIB) {
			res = read(fd, &lc.dylib, sizeof(struct dylib));
			if (res < sizeof(struct dylib)) { return 0; }

			if (swap) swap_dylib_command(&lc, NXHostByteOrder());

			off_t save = lseek(fd, 0, SEEK_CUR);
			off_t offset = lc.dylib.name.offset - sizeof(struct dylib_command);

			if (offset > 0) { lseek(fd, offset, SEEK_CUR); }
			int strsize = lc.cmdsize - sizeof(struct dylib_command);

			char* str = malloc(strsize+1);
			res = read(fd, str, strsize);
			if (res < sizeof(strsize)) { return 0; }
			str[strsize] = 0; // NUL-terminate
						
			res = SQL(db,
			"INSERT INTO unresolved_dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
			build, project, "lib", str);
			
			//fprintf(stdout, "\t%s\n", str);
			free(str);
			
			lseek(fd, save - sizeof(struct dylib_command) + lc.cmdsize, SEEK_SET);
		} else {
			uint32_t cmdsize = swap ? NXSwapLong(lc.cmdsize) : lc.cmdsize;
			lseek(fd, cmdsize - sizeof(struct load_command), SEEK_CUR);
		}
	}
	return 0;
}

int register_libraries(void* db, char* project, char* version, char* path) {
	int res;
	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		perror(path);
		return -1;
	}
	
	struct mach_header mh;
	struct fat_header fh;
	
	// The magic logic assumes mh is bigger than fh.
	assert(sizeof(mh) >= sizeof(fh));
	res = read(fd, &mh, sizeof(mh));
	if (res < sizeof(mh)) { goto error_out; }
	
	// It's a fat file.  copy mh over to fh, and dereference.
	if (mh.magic == FAT_MAGIC || mh.magic == FAT_CIGAM) {
		int swap = 0;
		memcpy(&fh, &mh, sizeof(fh));
		if (fh.magic == FAT_CIGAM) {
			swap = 1;
			swap_fat_header(&fh, NXHostByteOrder());
		}
		lseek(fd, (off_t)sizeof(fh), SEEK_SET);

		int i;
		for (i = 0; i < fh.nfat_arch; ++i) {
			struct fat_arch fa;
			res = read(fd, &fa, sizeof(fa));
			if (res < sizeof(fa)) { goto error_out; }
			
			if (swap) swap_fat_arch(&fa, 1, NXHostByteOrder());
						
			off_t save = lseek(fd, 0, SEEK_CUR);
			lseek(fd, (off_t)fa.offset, SEEK_SET);

			res = read(fd, &mh, sizeof(mh));
			if (res < sizeof(mh)) { goto error_out; }
			register_mach_header(db, project, version, &mh, fd);
			
			lseek(fd, save, SEEK_SET);
		}
	} else {
		register_mach_header(db, project, version, &mh, fd);
	}
error_out:
	close(fd);
	return 0;
}


int register_files(void* db, char* build, char* project, char* path) {
	char* errmsg;
	int res;
	int loaded = 0;
	
	char* table = "CREATE TABLE files (build text, project text, path text)";
	char* index = "CREATE INDEX files_index ON files (build, project, path)";
	SQL_NOERR(db, table);
	SQL_NOERR(db, index);
	
	if (SQL(db, "BEGIN")) { return -1; }
	
	res = SQL(db,
		"DELETE FROM files WHERE build=%Q AND project=%Q",
		build, project);

	SQL(db,
		"DELETE FROM unresolved_dependencies WHERE build=%Q AND project=%Q", 
		build, project);

	//
	// Enumerate the files in the path (DSTROOT) and associate them
	// with the project name and version in the sqlite database.
	// Uses ent->fts_number to mark which files we have and have
	// not seen before.
	//
	// Skip the first result, since that is . of the DSTROOT itself.
	int skip = 1;
	char* path_argv[] = { path, NULL };
	FTS* fts = fts_open(path_argv, FTS_PHYSICAL | FTS_XDEV, NULL);
	if (fts != NULL) {
		FTSENT* ent;
		while (ent = fts_read(fts)) {
			if (ent->fts_number == 0) {
				char path[PATH_MAX];
				path[0] = 0;
				buildpath(path, PATH_MAX-1, ent);

				// don't bother to store $DSTROOT
				if (!skip) {
					printf("%s\n", path);
					++loaded;
					res = SQL(db,
	"INSERT INTO files (build, project, path) VALUES (%Q,%Q,%Q)",
						build, project, path);
				} else {
					skip = 0;
				}

				ent->fts_number = 1;

				if (ent->fts_info == FTS_F) {
					res = register_libraries(db, build, project, ent->fts_accpath);
				}
			}
		}
		fts_close(fts);
	}
	
	if (SQL(db, "COMMIT")) { return -1; }

	fprintf(stdout, "%d files registered.\n", loaded);
	
	return res;
}
