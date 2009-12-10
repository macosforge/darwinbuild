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

#include "Utils.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/stat.h>

extern char** environ;

int fts_compare(const FTSENT **a, const FTSENT **b) {
	return strcmp((*a)->fts_name, (*b)->fts_name);
}

int ftsent_filename(FTSENT* ent, char* filename, size_t bufsiz) {
	if (ent == NULL) return 0;
	if (ent->fts_level > 1) {
		bufsiz = ftsent_filename(ent->fts_parent, filename, bufsiz);
	}
	strlcat(filename, "/", bufsiz);
	bufsiz -= 1;
	if (ent->fts_name) {
		strlcat(filename, ent->fts_name, bufsiz);
		bufsiz -= strlen(ent->fts_name);
	}
	return bufsiz;
}

int mkdir_p(const char* path) {
        int res;

        for (;;) {
                // Use 0777, let the umask decide.
                res = mkdir(path, 0777);

                if (res != 0 && errno == ENOENT) {
                        char tmp[PATH_MAX];
                        strlcpy(tmp, path, PATH_MAX);
                        char* slash = strrchr(tmp, '/');
                        if (slash) { *slash = 0; }
                        res = mkdir_p(tmp);
                        if (res != 0) {
                                break;
                        }
                } else {
                        break;
                }
        }
        return res;
}

int remove_directory(const char* directory) {
	int res = 0;
	const char* path_argv[] = { directory, NULL };
	FTS* fts = fts_open((char**)path_argv, FTS_PHYSICAL | FTS_COMFOLLOW | FTS_XDEV, fts_compare);
	FTSENT* ent = fts_read(fts); // throw away the entry for the DSTROOT itself
	while (res == 0 && (ent = fts_read(fts)) != NULL) {
		switch (ent->fts_info) {
			case FTS_D:
				break;
			case FTS_F:
			case FTS_SL:
			case FTS_SLNONE:
			case FTS_DEFAULT:
				res = unlink(ent->fts_accpath);
				break;
			case FTS_DP:
				res = rmdir(ent->fts_accpath);
				break;
			default:
				fprintf(stderr, "%s:%d: unexpected fts_info type %d\n", __FILE__, __LINE__, ent->fts_info);
				break;
		}
	}
	fts_close(fts);
	return res;
}

int is_directory(const char* path) {
	struct stat sb;
	int res = stat(path, &sb);
	return (res == 0 && S_ISDIR(sb.st_mode));
}

int is_regular_file(const char* path) {
	struct stat sb;
	int res = stat(path, &sb);
	return (res == 0 && S_ISREG(sb.st_mode));
}

int has_suffix(const char* str, const char* sfx) {
	str = strstr(str, sfx);
	return (str && strcmp(str, sfx) == 0);
}

int exec_with_args(const char** args) {
	int res = 0;
	pid_t pid;
	int status;
	
	IF_DEBUG("Spawning %s \n", args[0]);
		
	res = posix_spawn(&pid, args[0], NULL, NULL, (char**)args, environ);
	if (res != 0) fprintf(stderr, "Error: Failed to spawn %s: %s (%d)\n", args[0], strerror(res), res);
	
	IF_DEBUG("Running %s on pid %d \n", args[0], (int)pid);

	do {
		res = waitpid(pid, &status, 0);
	} while (res == -1 && errno == EINTR);
	if (res != -1) {
		if (WIFEXITED(status)) {
			res = WEXITSTATUS(status);
		} else {
			res = -1;
		}
	}
	
	IF_DEBUG("Done running %s \n", args[0]);
	
	return res;
}

#define compact_slashes(buf, count) do { memmove(buf - count + 1, buf, strlen(buf) + 1); buf -= count; } while (0)

/**
 * join_path joins two paths and removes any extra slashes,
 *  even internal ones in p1 or p2. It allocates memory
 *  for the string and the caller is responsible for freeing.
 */
int join_path(char **out, const char *p1, const char *p2) {
	asprintf(out, "%s/%s", p1, p2);
	if (!out) {
	        fprintf(stderr, "Error: join_path is out of memory!\n");
		return -1;
	}
	
	int slashes = 0;
	char *cur = *out;
	while (*cur != '\0') {
	        if (*cur == '/') {
		        slashes++;
		} else {
		        // we found the next non-slash
		        if (slashes > 1) {
			  compact_slashes(cur, slashes);
			} 
			slashes = 0;
		}
		cur++;
	}
	// see if we had extra slashes at the very end of p2
	if (slashes > 1) {
	        compact_slashes(cur, slashes);
	} 
	return 0;
}
