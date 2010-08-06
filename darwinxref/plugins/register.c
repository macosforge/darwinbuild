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
#include "DBDataStore.h"
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
#include <stdlib.h>
#include <CommonCrypto/CommonDigest.h>
#include "sqlite3.h"

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
	unsigned char md[20];
	CC_SHA1_CTX c;
	CC_SHA1_Init(&c);
	
	memset(md, 0, 20);
	
	ssize_t len;
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
		CC_SHA1_Update(&c, block, (size_t)len);
	}
	
	CC_SHA1_Final(md, &c);
	return format_digest(md);
}

static char* calculate_unprebound_digest(const char* filename);

static int have_undo_prebinding() {
	static int result = -2;
	if (result == -2) {
		struct stat sb;
		result = stat("/usr/bin/redo_prebinding", &sb);
	}
	
	// Not all versions of redo_prebinding support -u
	if (result == 0) {
		char* digest = calculate_unprebound_digest("/bin/sh");
		if (digest) {
			if (strcmp(digest, "ERROR") == 0) {
				result = -1;
			}
			free(digest);
		}
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
			"-z", "-u", "-i", "-s",
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

static int register_mach_header(const char* build, const char* project, const char* path, struct fat_arch* fa, int fd, int* isMachO) {
	int res;
	uint32_t magic;
	int swap = 0;
	
	struct mach_header* mh = NULL;
	struct mach_header_64* mh64 = NULL;

	if (isMachO) *isMachO = 0;

	res = read(fd, &magic, sizeof(uint32_t));
	if (res < sizeof(uint32_t)) { return 0; }
	
	//
	// 32-bit, read the rest of the header
	//
	if (magic == MH_MAGIC || magic == MH_CIGAM) {
		if (isMachO) *isMachO = 1;
		mh = malloc(sizeof(struct mach_header));
		if (mh == NULL) return -1;
		memset(mh, 0, sizeof(struct mach_header));
		mh->magic = magic;
		res = read(fd, &mh->cputype, sizeof(struct mach_header) - sizeof(uint32_t));
		if (res < sizeof(struct mach_header) - sizeof(uint32_t)) { return 0; }
		if (magic == MH_CIGAM) {
			swap = 1;
			swap_mach_header(mh, NXHostByteOrder());
		}
	//
	// 64-bit, read the rest of the header
	//
	} else if (magic == MH_MAGIC_64 || magic == MH_CIGAM_64) {
		if (isMachO) *isMachO = 1;
		mh64 = malloc(sizeof(struct mach_header_64));
		if (mh64 == NULL) return -1;
		memset(mh64, 0, sizeof(struct mach_header_64));
		mh64->magic = magic;
		res = read(fd, &mh64->cputype, sizeof(struct mach_header_64) - sizeof(uint32_t));
		if (res < sizeof(struct mach_header_64) - sizeof(uint32_t)) { return 0; }
		if (magic == MH_CIGAM_64) {
			swap = 1;
			swap_mach_header_64(mh64, NXHostByteOrder());
		}
	//
	// Not a Mach-O
	//
	} else {
		return 0;
	}


	switch (mh64 ? mh64->filetype : mh->filetype) {
		case MH_EXECUTE:
		case MH_DYLIB:
		case MH_BUNDLE:
			break;
		case MH_OBJECT:
		default:
			return 0;
	}

	res = SQL("INSERT INTO mach_o_objects (magic, type, cputype, cpusubtype, flags, build, project, path) VALUES (%u, %u, %u, %u, %u, %Q, %Q, %Q)",
		mh64 ? mh64->magic : mh->magic,
		mh64 ? mh64->filetype : mh->filetype,
		mh64 ? mh64->cputype : mh->cputype,
		mh64 ? mh64->cpusubtype : mh->cpusubtype,
		mh64 ? mh64->flags : mh->flags,
		build, project, path);
	uint64_t serial = sqlite3_last_insert_rowid((sqlite3*)_DBPluginGetDataStorePtr());

	//
	// Information needed to parse the symbol table
	//
	int count_nsect = 0;
	unsigned char text_nsect = NO_SECT;
	unsigned char data_nsect = NO_SECT;
	unsigned char bss_nsect = NO_SECT;

	uint32_t nsyms = 0;
	uint8_t *symbols = NULL;
	
	uint32_t strsize = 0;
	uint8_t *strings = NULL;


	int i;
	uint32_t ncmds = mh64 ? mh64->ncmds : mh->ncmds;
	for (i = 0; i < ncmds; ++i) {
		//
		// Read a generic load command into memory.
		// At first, we only know it has a type and size.
		//
		struct load_command lctmp;

		int res = read(fd, &lctmp, sizeof(struct load_command));
		if (res < sizeof(struct load_command)) { return 0; }

		uint32_t cmd = swap ? OSSwapInt32(lctmp.cmd) : lctmp.cmd;
		uint32_t cmdsize = swap ? OSSwapInt32(lctmp.cmdsize) : lctmp.cmdsize;
		if (cmdsize == 0) continue;
		
		struct load_command* lc = malloc(cmdsize);
		if (lc == NULL) { return 0; }
		memset(lc, 0, cmdsize);
		memcpy(lc, &lctmp, sizeof(lctmp));
		
		// Read the remainder of the load command.
		res = read(fd, (uint8_t*)lc + sizeof(struct load_command), cmdsize - sizeof(struct load_command));
		if (res < (cmdsize - sizeof(struct load_command))) { free(lc); return 0; }

		//
		// LC_LOAD_DYLIB and LC_LOAD_WEAK_DYLIB
		// Add dylibs as unresolved "lib" dependencies.
		//
		if (cmd == LC_LOAD_DYLIB || cmd == LC_LOAD_WEAK_DYLIB) {
			struct dylib_command *dylib = (struct dylib_command*)lc;
			if (swap) swap_dylib_command(dylib, NXHostByteOrder());

			// sections immediately follow the dylib_command structure, and are
			// reflected in the cmdsize.

			int strsize = dylib->cmdsize - sizeof(struct dylib_command);
			char* str = malloc(strsize+1);
			strncpy(str, (char*)((uint8_t*)dylib + dylib->dylib.name.offset), strsize);
			str[strsize] = 0; // NUL-terminate

			res = SQL("INSERT INTO unresolved_dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
			build, project, "lib", str);
			
			free(str);
		
		//
		// LC_LOAD_DYLINKER
		// Add the dynamic linker (usually dyld) as an unresolved "lib" dependency.
		//
		} else if (cmd == LC_LOAD_DYLINKER) {
			struct dylinker_command *dylinker = (struct dylinker_command*)lc;
			if (swap) swap_dylinker_command(dylinker, NXHostByteOrder());

			// sections immediately follow the dylib_command structure, and are
			// reflected in the cmdsize.

			int strsize = dylinker->cmdsize - sizeof(struct dylinker_command);
			char* str = malloc(strsize+1);
			strncpy(str, (char*)((uint8_t*)dylinker + dylinker->name.offset), strsize);
			str[strsize] = 0; // NUL-terminate

			res = SQL("INSERT INTO unresolved_dependencies (build,project,type,dependency) VALUES (%Q,%Q,%Q,%Q)",
			build, project, "lib", str);
			
			free(str);
		
		//
		// LC_SYMTAB
		// Read the symbol table into memory, we'll process it after we're
		// done with the load commands.
		//
		} else if (cmd == LC_SYMTAB && symbols == NULL) {
			struct symtab_command *symtab = (struct symtab_command*)lc;
			if (swap) swap_symtab_command(symtab, NXHostByteOrder());

			nsyms = symtab->nsyms;
			uint32_t symsize = nsyms * (mh64 ? sizeof(struct nlist_64) : sizeof(struct nlist));
			symbols = malloc(symsize);
			
			strsize = symtab->strsize;
			// XXX: check strsize != 0
			strings = malloc(strsize);

			off_t save = lseek(fd, 0, SEEK_CUR);

			off_t origin = fa ? fa->offset : 0;

			lseek(fd, (off_t)symtab->symoff + origin, SEEK_SET);
			res = read(fd, symbols, symsize);
			if (res < symsize) { /* XXX: leaks */ return 0; }
			
			lseek(fd, (off_t)symtab->stroff + origin, SEEK_SET);
			res = read(fd, strings, strsize);
			if (res < strsize) { /* XXX: leaks */ return 0; }
			
			lseek(fd, save, SEEK_SET);
		
		//
		// LC_SEGMENT
		// We're looking for the section number of the text, data, and bss segments
		// in order to parse symbols.
		//
		} else if (cmd == LC_SEGMENT) {
			struct segment_command* seg = (struct segment_command*)lc;
			if (swap) swap_segment_command(seg, NXHostByteOrder());
			
			// sections immediately follow the segment_command structure, and are
			// reflected in the cmdsize.
			int k;
			for (k = 0; k < seg->nsects; ++k) {
				struct section* sect = (struct section*)((uint8_t*)seg + sizeof(struct segment_command) + k * sizeof(struct section));
				if (swap) swap_section(sect, 1, NXHostByteOrder());
				if (strcmp(sect->sectname, SECT_TEXT) == 0 && strcmp(sect->segname, SEG_TEXT) == 0) {
					text_nsect = ++count_nsect;
				} else if (strcmp(sect->sectname, SECT_DATA) == 0 && strcmp(sect->segname, SEG_DATA) == 0) {
					data_nsect = ++count_nsect;
				} else if (strcmp(sect->sectname, SECT_BSS) == 0 && strcmp(sect->segname, SEG_DATA) == 0) {
					bss_nsect = ++count_nsect;
				} else {
					++count_nsect;
				}
			}

		//
		// LC_SEGMENT_64
		// Same as LC_SEGMENT, but for 64-bit binaries.
		//
		} else if (lc->cmd == LC_SEGMENT_64) {
			struct segment_command_64* seg = (struct segment_command_64*)lc;
			if (swap) swap_segment_command_64(seg, NXHostByteOrder());
			
			// sections immediately follow the segment_command structure, and are
			// reflected in the cmdsize.
			int k;
			for (k = 0; k < seg->nsects; ++k) {
				struct section_64* sect = (struct section_64*)((uint8_t*)seg + sizeof(struct segment_command_64) + k * sizeof(struct section_64));
				if (swap) swap_section_64(sect, 1, NXHostByteOrder());
				if (strcmp(sect->sectname, SECT_TEXT) == 0 && strcmp(sect->segname, SEG_TEXT) == 0) {
					text_nsect = ++count_nsect;
				} else if (strcmp(sect->sectname, SECT_DATA) == 0 && strcmp(sect->segname, SEG_DATA) == 0) {
					data_nsect = ++count_nsect;
				} else if (strcmp(sect->sectname, SECT_BSS) == 0 && strcmp(sect->segname, SEG_DATA) == 0) {
					bss_nsect = ++count_nsect;
				} else {
					++count_nsect;
				}
			}
		}
		
		free(lc);
	}

	//
	// Finished processing the load commands, now insert symbols into the database.
	//
	int j;
	for (j = 0; j < nsyms; ++j) {
		struct nlist_64 symbol;
		if (mh64) {
			memcpy(&symbol, (symbols + j * sizeof(struct nlist_64)), sizeof(struct nlist_64));
			if (swap) swap_nlist_64(&symbol, 1, NXHostByteOrder());
		} else {
			symbol.n_value = 0;
			memcpy(&symbol, (symbols + j * sizeof(struct nlist)), sizeof(struct nlist));
			if (swap) swap_nlist_64(&symbol, 1, NXHostByteOrder());
			// we copied a 32-bit nlist into a 64-bit one, adjust the value accordingly
			// all other fields are identical sizes
			symbol.n_value >>= 32;
		}
		char type = '?';
		switch (symbol.n_type & N_TYPE) {
			case N_UNDF:
			case N_PBUD:
				type = 'u';
				if (symbol.n_value != 0) {
					type = 'c';
				}
				break;
			case N_ABS:
				type = 'a';
				break;
			case N_SECT:
				if (symbol.n_sect == text_nsect) {
					type = 't';
				} else if (symbol.n_sect == data_nsect) {
					type = 'd';
				} else if (symbol.n_sect == bss_nsect) {
					type = 'b';
				} else {
					type = 's';
				}
				break;
			case N_INDR:
				type = 'i';
				break;
		}

		// uppercase indicates an externally visible symbol
		if ((symbol.n_type & N_EXT) && type != '?') {
			type = toupper(type);
		}

		if (type != '?' && type != 'u' && type != 'c') {
			const uint8_t* name = (const uint8_t*)"";
			if (symbol.n_un.n_strx != 0) {
				name = (uint8_t*)(strings + symbol.n_un.n_strx);
			}
			res = SQL("INSERT INTO mach_o_symbols VALUES (%lld, \'%c\', %lld, %Q)",
				serial,
				type,
				symbol.n_value,
				name);
		}
	}

	return 0;
}

static int register_libraries(int fd, const char* build, const char* project, const char* filename, int* isMachO) {
	int res;
		
	uint32_t magic;
	
	res = read(fd, &magic, sizeof(uint32_t));
	if (res < sizeof(uint32_t)) { goto error_out; }

	if (magic == FAT_MAGIC || magic == FAT_CIGAM) {
		struct fat_header fh;
		int swap = 0;

		res = read(fd, &fh.nfat_arch, sizeof(struct fat_header) - sizeof(uint32_t));
		if (res < sizeof(uint32_t)) { goto error_out; }

		if (magic == FAT_CIGAM) {
			swap = 1;
			swap_fat_header(&fh, NXHostByteOrder());
		}
		
		int i;
		for (i = 0; i < fh.nfat_arch; ++i) {
			struct fat_arch fa;
			res = read(fd, &fa, sizeof(fa));
			if (res < sizeof(fa)) { goto error_out; }
			
			if (swap) swap_fat_arch(&fa, 1, NXHostByteOrder());
						
			off_t save = lseek(fd, 0, SEEK_CUR);
			lseek(fd, (off_t)fa.offset, SEEK_SET);

			register_mach_header(build, project, filename, &fa, fd, isMachO);
			
			lseek(fd, save, SEEK_SET);
		}
	} else {
		lseek(fd, 0, SEEK_SET);
		register_mach_header(build, project, filename, NULL, fd, isMachO);
	}
error_out:
	return 0;
}


static int create_tables() {
	char* table = "CREATE TABLE files (build text, project text, path text)";
	char* index = "CREATE INDEX files_index ON files (build, project, path)";
	SQL_NOERR(table);
	SQL_NOERR(index);

	table = "CREATE TABLE unresolved_dependencies (build text, project text, type text, dependency)";
	SQL_NOERR(table);

	table = "CREATE TABLE mach_o_objects (serial INTEGER PRIMARY KEY AUTOINCREMENT, magic INTEGER, type INTEGER, cputype INTEGER, cpusubtype INTEGER, flags INTEGER, build TEXT, project TEXT, path TEXT)";
	SQL_NOERR(table);

	table = "CREATE TABLE mach_o_symbols (mach_o_object INTEGER, type INTEGER, value INTEGER, name TEXT)";
	SQL_NOERR(table);
	return 0;
}

static int prune_old_entries(const char* build, const char* project) {
	SQL("DELETE FROM files WHERE build=%Q AND project=%Q",
		  build, project);

	SQL("DELETE FROM unresolved_dependencies WHERE build=%Q AND project=%Q", 
		build, project);

	SQL("DELETE FROM mach_o_objects WHERE build=%Q AND project=%Q", build, project);
	
	SQL("DELETE FROM mach_o_symbols WHERE mach_o_object NOT IN (SELECT serial FROM mach_o_objects)");

	return 0;
}

int register_files(char* build, char* project, char* path) {
	int res = 0;
	int loaded = 0;
	
	create_tables();

	if (SQL("BEGIN")) { return -1; }

	prune_old_entries(build, project);
	
	

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
			res = register_libraries(fd, build, project, filename, &isMachO);
			lseek(fd, (off_t)0, SEEK_SET);
			if (isMachO && have_undo_prebinding() == 0) {
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
	int res = 0;
	int loaded = 0;
	char *line;
	size_t size;

	create_tables();
	
	if (SQL("BEGIN")) { return -1; }
	
	prune_old_entries(build, project);

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
			res = register_libraries(fd, build, project, filename, NULL);
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
