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

#include "Utils.h"

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
	int res = lstat(path, &sb);
	return (res == 0 && S_ISDIR(sb.st_mode));
}

int is_regular_file(const char* path) {
	struct stat sb;
	int res = stat(path, &sb);
	return (res == 0 && S_ISREG(sb.st_mode));
}

int is_url_path(const char* path) {
	if (strncmp("http://", path, 7) == 0) {
		return 1;
	}
	if (strncmp("https://", path, 8) == 0) {
		return 1;
	}
	return 0;
}

int is_userhost_path(const char* path) {
	// look for user@host:path
	char *at = strchr(path, '@');
	char *colon = strchr(path, ':');
	return at && colon && at < colon;	
}

int has_suffix(const char* str, const char* sfx) {
	str = strstr(str, sfx);
	return (str && strcmp(str, sfx) == 0);
}

int exec_with_args(const char** args) {
	return exec_with_args_fa(args, NULL);
}

int exec_with_args_pipe(const char** args, int fd) {
	int res = 0;
	posix_spawn_file_actions_t fa;

	res = posix_spawn_file_actions_init(&fa);
	if (res) {
		fprintf(stderr, "Error: unable to initialize file actions: %d \n", res);
		return res;
	}

	res = posix_spawn_file_actions_adddup2(&fa, fd, 1); // pipe stdout
	if (res) {
		fprintf(stderr, "Error: (%d) unable to add dup2 for %d \n", res, fd);
		return res;
	}

	res = posix_spawn_file_actions_addclose(&fa, 2); // close stderr
	if (res) {
		fprintf(stderr, "Error: (%d) unable to add close for stderr.\n", res);
		return res;
	}

	res = posix_spawn_file_actions_addclose(&fa, 0); // close stdin
	if (res) {
		fprintf(stderr, "Error: (%d) unable to add close for stdin.\n", res);
		return res;
	}
	
	res = exec_with_args_fa(args, &fa);
	posix_spawn_file_actions_destroy(&fa);

	return res;
}

int exec_with_args_fa(const char** args, posix_spawn_file_actions_t* fa) {
	int res = 0;
	pid_t pid;
	int status;
	
	IF_DEBUG("Spawn: %s \n", args[0]);
		
	res = posix_spawn(&pid, args[0], fa, NULL, (char**)args, environ);
	if (res != 0) fprintf(stderr, "Error: Failed to spawn %s: %s (%d)\n", args[0], strerror(res), res);
	
	IF_DEBUG("Running: %s on pid %d \n", args[0], (int)pid);

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
	
	IF_DEBUG("Done: %s \n", args[0]);
	
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

char* fetch_url(const char* srcpath, const char* dstpath) {
	extern uint32_t verbosity;
	char* localfile;
	int res = join_path(&localfile, dstpath, basename((char*)srcpath));
	if (res || !localfile) return NULL;
	
	const char* args[] = {
		"/usr/bin/curl",
		(verbosity ? "-v" : "-s"),
		"-L", srcpath,
		"-o", localfile,
		NULL
	};
	if (res == 0) res = exec_with_args(args);
	if (res == 0) return localfile;
	return NULL;
}

char* fetch_userhost(const char* srcpath, const char* dstpath) {
	extern uint32_t verbosity;
	int res = 0;

	// clean up srcpath by adding trailing slash
	char* cleansrc;
	res = join_path(&cleansrc, srcpath, "/");
	if (res != 0) return NULL;

	// make sure dstpath ends in basename of cleansrc for consistent rsync behavior
	char* cleandst;
	char* srccopy = strdup(cleansrc); // basename might modify input, so make a copy
	res = join_path(&cleandst, dstpath, basename(srccopy));
	if (res != 0) return NULL;

	IF_DEBUG("rsync -a --delete %s %s %s \n", 
			 (verbosity ? "-v" : "-q"), cleansrc, cleandst);
	
	const char* args[] = {
		"/usr/bin/rsync",
		(verbosity ? "-v" : "-q"),
		"-a", "--delete",
		cleansrc,
		cleandst,
		NULL
	};

	if (res == 0) res = exec_with_args(args);
	free(srccopy);
	free(cleansrc);
	if (res == 0) return cleandst;
	return NULL;	
}

int find_base_system_path(char** output, const char* path) {
	// find the first /System as we walk up path
	char system[PATH_MAX];
	char parent[PATH_MAX];
	strlcpy(parent, path, PATH_MAX);
	int res = -1;
	struct stat sb;
	while (res) {
		// walk up path
		snprintf(system, PATH_MAX, "%s%sSystem", 
				 parent, (parent[1] == '\0' ? "" : "/"));
		res = stat(system, &sb);
		if (parent[1] == '\0') {
			// we hit the top of the filesystem
			break;
		}
		if (res) snprintf(parent, PATH_MAX, "%s", dirname(parent));
	}
	if (res) {
		fprintf(stderr, "Error: (%d) unable to find base system path.\n", res);
		return res;
	}
	
	asprintf(output, "%s", parent);
	return 0;
}

int update_dyld_shared_cache(const char* path) {
	extern uint32_t verbosity;
	int res;
	char* base;
	res = find_base_system_path(&base, path);
	if (res) return res;
	
	if (verbosity) {
		fprintf(stdout, "Updating dyld shared cache for %s ... ", base);
		fflush(stdout);
	}
	
	// exec the tool from our target system
	char* toolpath;
	join_path(&toolpath, base, "/usr/bin/update_dyld_shared_cache");

	struct stat sb;
	res = stat(toolpath, &sb);
	if (res) {
		return 1;
	}
		
	const char* args[] = {
		toolpath,
		"-root", base,
		NULL
	};
	res = exec_with_args(args);

	if (verbosity) fprintf(stdout, "Done updating dyld shared cache\n");
	
	free(toolpath);
	free(base);
	return res;
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
int build_number_for_path(char** build, const char* path) {
	int res = 0;
	char system[PATH_MAX];
	char* base;
	
	*build = (char*)calloc(1, 16);
	
	// find the version plist for our target path
	find_base_system_path(&base, path);
	if (!base) return 1;	
	snprintf(system, PATH_MAX, "%s/System/Library/CoreServices/SystemVersion.plist", base);
	free(base);

	struct stat sb;
	res = stat(system, &sb);
	if (res) {
		snprintf(*build, 16, " ");
		return 1;
	}

	// read version plist to get build number
	const char* args[] = {
		"/usr/libexec/PlistBuddy",
		"-c", "Print ProductBuildVersion",
		system,
		NULL
	};
	int pfd[2];
	res = pipe(pfd);
	if (res) {
		fprintf(stderr, "Error: (%d) failed to create pipe.\n", res);
		return res;
	}
	exec_with_args_pipe(args, pfd[1]);
	
	// read from the pipe
	close(pfd[1]);
	res = 1;
	while (res > 0 && res < 15) {
		res = read(pfd[0], *build, 15);
		// strip newline
		if (res > 1 && (*build)[res-1] == '\n') (*build)[res-1] = '\0';
	}
	close(pfd[0]);

	if (res == 0) return res; // success
	
	if (res == -1) {
		fprintf(stderr, "Error: failed to read build from plist.\n");
		return res;
	}

	return -1;
}

int tell_finder_to_restart() {
	int res = 0;
	
	const char *args[] = {
		"/usr/bin/osascript", "-e",
		"tell app \"Finder\" to restart",
		NULL,
	};
	
	res = exec_with_args(args);
	
	return res;
}
#endif

void __data_hex(FILE* f, uint8_t* data, uint32_t size) {
	if (!size) return;
	for (uint32_t i=0; i < size; i++) {
		if (!(i%8)) {
			if (i<10) fprintf(f, " ");
			fprintf(f, "%d", i);
		} else {
			fprintf(f, "  ");
		}
	}
	fprintf(f, "\n");
	for (uint32_t i=0; i < size; i++) {
		fprintf(f, "%02x", data[i]);
	}
	fprintf(f, "\n");
}

void hr() {
	fprintf(stdout, "=============================================="
			"=======================================\n");	
}
