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
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <openssl/evp.h>

extern char** environ;

int register_files(char* build, char* project, char* path);
int register_files_from_stdin(char* build, char* project, char* path);

static int run(CFArrayRef argv) {
	int res = 0;
       int i = 0, doStdin = 0;
	CFIndex count = CFArrayGetCount(argv);
       if (count < 2 || count > 3)  return -1;
	char* build = strdup_cfstr(DBGetCurrentBuild());

       CFStringRef stdinArg = CFArrayGetValueAtIndex(argv, 0);
       if(CFEqual(stdinArg, CFSTR("-stdin"))) {
         i++;
         doStdin = 1;
       }

       char* project = strdup_cfstr(CFArrayGetValueAtIndex(argv, i++));
       char* dstroot = strdup_cfstr(CFArrayGetValueAtIndex(argv, i++));

       if(doStdin)
         res = register_files_from_stdin(build, project, dstroot);
       else
         res = register_files(build, project, dstroot);

	free(build);
	free(project);
	free(dstroot);
	return res;
}

static CFStringRef usage() {
       return CFRetain(CFSTR("[-stdin] <project> <dstroot>"));
}

int initialize(int version) {
	//if ( version < kDBPluginCurrentVersion ) return -1;
	
	DBPluginSetType(kDBPluginBasicType);
	DBPluginSetName(CFSTR("register"));
	DBPluginSetRunFunc(&run);
	DBPluginSetUsageFunc(&usage);
	return 0;
}

static char* format_digest(const unsigned char* m) {
        char* result = NULL;
		// SHA-1
		asprintf(&result,
                "%02x%02x%02x%02x"
                "%02x%02x%02x%02x"
                "%02x%02x%02x%02x"
                "%02x%02x%02x%02x"
                "%02x%02x%02x%02x",
                m[0], m[1], m[2], m[3],
                m[4], m[5], m[6], m[7],
                m[8], m[9], m[10], m[11],
                m[12], m[13], m[14], m[15],
				m[16], m[17], m[18], m[19]
				);
        return result;
}

static int compare(const FTSENT **a, const FTSENT **b) {
	return strcmp((*a)->fts_name, (*b)->fts_name);
}

static int ent_filename(FTSENT* ent, char* filename, size_t bufsiz) {
	if (ent == NULL) return 0;
	if (ent->fts_level > 1) {
		bufsiz = ent_filename(ent->fts_parent, filename, bufsiz);
	}
	strncat(filename, "/", bufsiz);
	bufsiz -= 1;
	if (ent->fts_name) {
		strncat(filename, ent->fts_name, bufsiz);
		bufsiz -= strlen(ent->fts_name);
	}
	return bufsiz;
}

static char* calculate_digest(int fd) {
	unsigned char digstr[EVP_MAX_MD_SIZE];
	memset(digstr, 0, sizeof(digstr));
	
	EVP_MD_CTX ctx;
	static const EVP_MD* md;
	
	if (md == NULL) {
		EVP_MD_CTX_init(&ctx);
		OpenSSL_add_all_digests();
		md = EVP_get_digestbyname("sha1");
		if (md == NULL) return NULL;
	}

	EVP_DigestInit(&ctx, md);

	unsigned int len;
	const unsigned int blocklen = 8192;
	static unsigned char* block = NULL;
	if (block == NULL) {
		block = malloc(blocklen);
	}
	while(1) {
		len = read(fd, block, blocklen);
		if (len == 0) { close(fd); break; }
		if ((len < 0) && (errno == EINTR)) continue;
		if (len < 0) { close(fd); return NULL; }
		EVP_DigestUpdate(&ctx, block, len);
	}

	EVP_DigestFinal(&ctx, digstr, &len);
	return format_digest(digstr);
}

static int have_redo_prebinding() {
	static int result = -2;
	if (result == -2) {
		struct stat sb;
		result = stat("/usr/bin/redo_prebinding", &sb);
	}
	return result;
}

static char* calculate_unprebound_digest(const char* filename) {
	pid_t pid;
	int status;
	int fds[2];

	assert(pipe(fds) != -1);
	
	pid = fork();
	assert(pid != -1);
	if (pid == 0) {
		close(fds[0]);
		assert(dup2(fds[1], STDOUT_FILENO) != -1);
		const char* args[] = {
			"/usr/bin/redo_prebinding",
			"-z", "-u", "-s",
			filename,
			NULL
		};
		assert(execve(args[0], (char**)args, environ) != -1);
		// NOT REACHED
	}
	close(fds[1]);
	
	char* checksum = calculate_digest(fds[0]);

	close(fds[0]);
	waitpid(pid, &status, 0);
	if (status != 0) {
		checksum = strdup("ERROR");
	}
	
	return checksum;
}

// If the path points to a Mach-O file, records all dylib
// link commands as library dependencies in the database.
// XXX
// Ideally library dependencies are tracked per-architecture.
// For now, we're assuming all architectures contain identical images.
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/swap.h>

static int register_mach_header(char* build, char* project, struct mach_header* mh, int fd, int* isMachO) {
	int swap = 0;
	if (isMachO) *isMachO = 0;
	if (mh->magic != MH_MAGIC && mh->magic != MH_CIGAM) return 0;
	if (isMachO) *isMachO = 1;
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
		struct dylinker_command lcdyld;
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
						
			res = SQL("INSERT INTO unresolved_dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
			build, project, "lib", str);
			
			free(str);
			
			lseek(fd, save - sizeof(struct dylib_command) + lc.cmdsize, SEEK_SET);
			
		} else if (cmd == LC_LOAD_DYLINKER) {
		  lcdyld.cmd = lc.cmd;
		  lcdyld.cmdsize = lc.cmdsize;
			res = read(fd, &lcdyld.name, sizeof(union lc_str));
			if (res < sizeof(union lc_str)) { return 0; }

			if (swap) swap_dylinker_command(&lcdyld, NXHostByteOrder());

			off_t save = lseek(fd, 0, SEEK_CUR);
			off_t offset = lcdyld.name.offset - sizeof(struct dylinker_command);

			if (offset > 0) { lseek(fd, offset, SEEK_CUR); }
			int strsize = lcdyld.cmdsize - sizeof(struct dylinker_command);

			char* str = malloc(strsize+1);
			res = read(fd, str, strsize);
			if (res < sizeof(strsize)) { return 0; }
			str[strsize] = 0; // NUL-terminate
						
			res = SQL("INSERT INTO unresolved_dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
			build, project, "lib", str);
			
			free(str);
			
			lseek(fd, save - sizeof(struct dylinker_command) + lcdyld.cmdsize, SEEK_SET);
		} else {
			uint32_t cmdsize = swap ? NXSwapLong(lc.cmdsize) : lc.cmdsize;
			lseek(fd, cmdsize - sizeof(struct load_command), SEEK_CUR);
		}
	}
	return 0;
}

static int register_libraries(int fd, char* build, char* project, int* isMachO) {
	int res;
	
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
			register_mach_header(build, project, &mh, fd, isMachO);
			
			lseek(fd, save, SEEK_SET);
		}
	} else {
		register_mach_header(build, project, &mh, fd, isMachO);
	}
error_out:
	return 0;
}


int register_files(char* build, char* project, char* path) {
	char* errmsg;
	int res;
	int loaded = 0;
	
	char* table = "CREATE TABLE files (build text, project text, path text)";
	char* index = "CREATE INDEX files_index ON files (build, project, path)";
	SQL_NOERR(table);
	SQL_NOERR(index);
	table = "CREATE TABLE unresolved_dependencies (build text, project text, type text, dependency)";
	SQL_NOERR(table);
	
	if (SQL("BEGIN")) { return -1; }
	
	res = SQL("DELETE FROM files WHERE build=%Q AND project=%Q",
		  build, project);

	SQL("DELETE FROM unresolved_dependencies WHERE build=%Q AND project=%Q", 
		build, project);

	//
	// Enumerate the files in the path (DSTROOT) and associate them
	// with the project name and version in the sqlite database.
	// Uses ent->fts_number to mark which files we have and have
	// not seen before.
	//
	// Skip the first result, since that is . of the DSTROOT itself.
	char* path_argv[] = { path, NULL };
	FTS* fts = fts_open(path_argv, FTS_PHYSICAL | FTS_COMFOLLOW | FTS_XDEV, compare);
	FTSENT* ent = fts_read(fts); // throw away the entry for the DSTROOT itself
	while ((ent = fts_read(fts)) != NULL) {
		char filename[MAXPATHLEN+1];
		char symlink[MAXPATHLEN+1];
		int len;
		off_t size;
		
		// Filename
		filename[0] = 0;
		ent_filename(ent, filename, MAXPATHLEN);

		// Symlinks
		symlink[0] = 0;
		if (ent->fts_info == FTS_SL || ent->fts_info == FTS_SLNONE) {
			len = readlink(ent->fts_accpath, symlink, MAXPATHLEN);
			if (len >= 0) symlink[len] = 0;
		}
		
		// Default to empty SHA-1 checksum
		char* checksum = strdup("                                        ");
		
		// Checksum regular files
		if (ent->fts_info == FTS_F) {
			int fd = open(ent->fts_accpath, O_RDONLY);
			if (fd == -1) {
				perror(filename);
				return -1;
			}
			int isMachO;
			res = register_libraries(fd, build, project, &isMachO);
			lseek(fd, (off_t)0, SEEK_SET);
			if (isMachO && have_redo_prebinding() == 0) {
				checksum = calculate_unprebound_digest(ent->fts_accpath);
			} else {
				checksum = calculate_digest(fd);
			}
			close(fd);
		}

		// register regular files and symlinks in the DB
		if (ent->fts_info == FTS_F || ent->fts_info == FTS_SL || ent->fts_info == FTS_SLNONE) {
			res = SQL("INSERT INTO files (build, project, path) VALUES (%Q,%Q,%Q)",
				build, project, filename);
			++loaded;
		}
		
		// add all regular files, directories, and symlinks to the manifest
		if (ent->fts_info == FTS_F || ent->fts_info == FTS_D ||
			ent->fts_info == FTS_SL || ent->fts_info == FTS_SLNONE) {
			fprintf(stdout, "%s %o %d %d %lld .%s%s%s\n",
				checksum,
				ent->fts_statp->st_mode,
				ent->fts_statp->st_uid,
				ent->fts_statp->st_gid,
				(ent->fts_info != FTS_D) ? ent->fts_statp->st_size : (off_t)0,
				filename,
				symlink[0] ? " -> " : "",
				symlink[0] ? symlink : "");
		}
		free(checksum);
	}
	fts_close(fts);
	
	if (SQL("COMMIT")) { return -1; }

	fprintf(stderr, "%s - %d files registered.\n", project, loaded);
	
	return res;
}

int register_files_from_stdin(char* build, char* project, char* path) {
	char* errmsg;
	int res;
	int loaded = 0;
	char *line;
	size_t size;


	char* table = "CREATE TABLE files (build text, project text, path text) ";
	char* index = "CREATE INDEX files_index ON files (build, project, path)";
	SQL_NOERR(table);
	SQL_NOERR(index);
	table = "CREATE TABLE unresolved_dependencies (build text, project text, type text, dependency)";
	SQL_NOERR(table);
	
	if (SQL("BEGIN")) { return -1; }
	
	res = SQL("DELETE FROM files WHERE build=%Q AND project=%Q",
		  build, project);

	SQL("DELETE FROM unresolved_dependencies WHERE build=%Q AND project=%Q", 
		build, project);

	//
	// Enumerate the files in the path (DSTROOT) and associate them
	// with the project name and version in the sqlite database.
	//
	// Skip the first result, since that is . of the DSTROOT itself.
        while ((line = fgetln(stdin, &size)) != NULL) {
		char filename[MAXPATHLEN+1];
		char fullpath[MAXPATHLEN+1];
		char symlink[MAXPATHLEN+1];
		int len;
		struct stat sb;
		char *lastpathcomp = NULL;

		if (size > 0 && line[size-1] == '\n') line[--size] = 0; // chomp newline
		if (size > 0 && line[size-1] == '/') line[--size] = 0; // chomp trailing slash

		if(0 == strcmp(line, "."))
		  continue;

		// Filename
		filename[0] = 0;
		strcpy(filename, line+1); /* skip over leading "." */

		lastpathcomp = strrchr(filename, '/');
		if(lastpathcomp && 0 == strncmp(lastpathcomp+1, "._", 2))
		  continue;


		sprintf(fullpath, "%s/%s", path, filename);
		res = lstat(fullpath, &sb);
		if(res != 0) {
		  perror(fullpath);
		  return -1;
		}
		  
		// Symlinks
		symlink[0] = 0;
		if (S_ISLNK(sb.st_mode)) {
			len = readlink(fullpath, symlink, MAXPATHLEN);
			if (len >= 0) symlink[len] = 0;
		}
		
		// Default to empty SHA-1 checksum
		char* checksum = strdup("                                        ");
		
		// Checksum regular files
		if (S_ISREG(sb.st_mode)) {
			int fd = open(fullpath, O_RDONLY);
			if (fd == -1) {
				perror(filename);
				return -1;
			}
			res = register_libraries(fd, build, project, NULL);
			/* For -stdin mode, we don't calculate checksums
			lseek(fd, (off_t)0, SEEK_SET);
			checksum = calculate_digest(fd);
			*/
			close(fd);
		}

		// register regular files and symlinks in the DB
		if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode)) {
			res = SQL("INSERT INTO files (build,project, path) VALUES (%Q,%Q,%Q)",
				build, project, filename);
			++loaded;
		}
		
		// add all regular files, directories, and symlinks to the manifest
		if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode) || S_ISDIR(sb.st_mode)) {
			fprintf(stdout, "%s %o %d %d %lld .%s%s%s\n",
				checksum,
				sb.st_mode,
				sb.st_uid,
				sb.st_gid,
				!S_ISDIR(sb.st_mode) ? sb.st_size : (off_t)0,
				filename,
				symlink[0] ? " -> " : "",
				symlink[0] ? symlink : "");
		}
		free(checksum);
	}
	
	if (SQL("COMMIT")) { return -1; }

	fprintf(stderr, "%s - %d files registered.\n", project, loaded);
	
	return res;
}
