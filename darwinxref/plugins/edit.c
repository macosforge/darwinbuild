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
#include <sys/stat.h>

int editPlist(void* session, CFStringRef project);
static int execEditor(const char* tmpfile);

static int run(void* session, CFArrayRef argv) {
	CFStringRef project = NULL;
	CFIndex count = CFArrayGetCount(argv);
	if (count > 1)  return -1;
	if (count == 1) {
		project = CFArrayGetValueAtIndex(argv, 0);
	}
	int res = editPlist(session, project);
	return res;
}

static CFStringRef usage(void* session) {
	return CFRetain(CFSTR("[<project>]"));
}

DBPlugin* initialize(int version) {
	DBPlugin* plugin = NULL;

	if (version != kDBPluginCurrentVersion) return NULL;
	
	plugin = malloc(sizeof(DBPlugin));
	if (plugin == NULL) return NULL;
	
	plugin->version = kDBPluginCurrentVersion;
	plugin->type = kDBPluginType;
	plugin->name = CFSTR("edit");
	plugin->run = &run;
	plugin->usage = &usage;

	return plugin;
}

static inline int min(int a, int b) {
        return (a < b) ? a : b;
}


int editPlist(void* session, CFStringRef project) {
		CFStringRef build = DBGetCurrentBuild(session);
        struct stat before, after;
        CFDictionaryRef p = NULL;
        if (project) {
                p = DBCopyProjectPlist(session, build, project);
        } else {
                p = DBCopyBuildPlist(session, build);
        }

        ////
        //// Create a temp file and record the mtime
        ////
        char tmpfile[PATH_MAX];
        strcpy(tmpfile, "/tmp/darwinxref.project.XXXXXX");
        int fd = mkstemp(tmpfile);
        FILE* f = fdopen(fd, "w");
        writePlist(f, p, 0);
        CFRelease(p);
        fclose(f);
        if (stat(tmpfile, &before) == -1) {

                perror(tmpfile);
                unlink(tmpfile);
                return -1;
        }

        int status = execEditor(tmpfile);

        if (status == 0) {
                // Look at the mtime of the file to see if we should re-import
                if (stat(tmpfile, &after) == -1) {
                        perror(tmpfile);
                        unlink(tmpfile);
                        return -1;
                }

                // Change in mtime, so re-import
                if (before.st_mtimespec.tv_sec != after.st_mtimespec.tv_sec ||
                        before.st_mtimespec.tv_nsec != after.st_mtimespec.tv_nsec) {
                        int done = 0;

                        while (!done) {
                                p = read_plist(tmpfile);
                                // Check if plist parsed successfully, if so import it
								if (DBSetPlist(session, build, p) == 0) {
										done = 1;
                                        CFRelease(p);
                                } else {
                                        for (;;) {
                                                fprintf(stderr, "Invalid property list\n");
                                                fprintf(stderr, "e)dit, q)uit\n");
                                                fprintf(stderr, "Action: (edit) ");
                                                size_t size;
                                                char* line = fgetln(stdin, &size);
                                                if (strncmp(line, "q", min(size, 1)) == 0 || line == NULL) {
                                                        fprintf(stderr, "darwinxref [edit cancelled]: cancelled by user\n");
                                                        done = 1;
                                                        break;
                                                } else if (strncmp(line, "e", min(size, 1)) == 0 ||
                                                                        strncmp(line, "\n", min(size, 1)) == 0) {
                                                        execEditor(tmpfile);
                                                        break;
                                                } else {
                                                       fprintf(stderr, "Unknown input\n\n");
                                                }
                                        }
                                }
                        }
                }
        } else {
                fprintf(stderr, "darwinxref [edit cancelled]: cancelled by user\n");
        }        unlink(tmpfile);
        return 0;
}

static int execEditor(const char* tmpfile) {
        pid_t pid = fork();
        if (pid == -1) {
                return -1;
        } else if (pid == 0) {
                char* editor = getenv("VISUAL");
                if (!editor) editor = getenv("EDITOR");
                if (!editor) editor = "vi";

                execlp(editor, editor, tmpfile, NULL);
                _exit(127);
                // NOT REACHED
        }
        int status = 0;
        while (wait4(pid, &status, 0, NULL) == -1) {
                if (errno == EINTR) continue;
                if (errno == ECHILD) status = -1;
                if (errno == EFAULT || errno == EINVAL) abort();
                break;
        }
        if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
        } else {
                return -1;
        }
}

