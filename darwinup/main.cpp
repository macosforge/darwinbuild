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

#include "Archive.h"
#include "Depot.h"
#include "Utils.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

void usage(char* progname) {
	fprintf(stderr, "usage:    %s [-v] [-p DIR] [command] [args]          \n", progname);
	fprintf(stderr, "                                                               \n");
	fprintf(stderr, "options:                                                       \n");
	fprintf(stderr, "          -f         force operation to succeed at all costs   \n");
	fprintf(stderr, "          -p DIR     operate on roots under DIR (default: /)   \n");
	fprintf(stderr, "          -v         verbose (use -vv for extra verbosity)     \n");
	fprintf(stderr, "                                                               \n");
	fprintf(stderr, "commands:                                                      \n");
	fprintf(stderr, "          install    <path>                                    \n");
	fprintf(stderr, "          list                                                 \n");
	fprintf(stderr, "          files      <uuid>|<serial>|newest|oldest             \n");
	fprintf(stderr, "          uninstall  <uuid>|<serial>|newest|oldest             \n");
	fprintf(stderr, "          verify     <uuid>|<serial>|newest|oldest             \n");
	exit(1);
}

// our globals
uint32_t verbosity;
uint32_t force;

int main(int argc, char* argv[]) {
	char* progname = strdup(basename(argv[0]));      

	char* path = NULL;

	int ch;
	while ((ch = getopt(argc, argv, "fp:vh")) != -1) {
		switch (ch) {
		case 'f':
				IF_DEBUG("forcing operations\n");
				force = 1;
				break;
		case 'p':
				if (optarg[0] != '/') {
					fprintf(stderr, "Error: -p option must be an absolute path\n");
					exit(4);
				}
				if (strlen(optarg) > (PATH_MAX - 1)) {
					fprintf(stderr, "Error: -p option value is too long \n");
					exit(4);
				}
				join_path(&path, optarg, "/");
				break;
		case 'v':
				verbosity <<= 1;
				verbosity |= VERBOSE;
				break;
		case '?':
		case 'h':
		default:
				usage(progname);
		}
	}
	argc -= optind;
	argv += optind;

	// you must be root
	uid_t uid = getuid();
	if (uid) {
		fprintf(stderr, "You must be root to run this tool.\n");
		exit(3);
	}
	
	int res = 0;

	if (!path) {
		asprintf(&path, "/");
	}
		
	Depot* depot = new Depot(path);
	res = depot->initialize();
	if (res) {
		fprintf(stderr, "Error: unable to initialize storage.\n");
		exit(2);
	}
	
	if (argc == 2 && strcmp(argv[0], "install") == 0) {
		char uuid[37];
		Archive* archive = ArchiveFactory(argv[1]);
		if (archive) {
			res = depot->install(archive);
			if (res == 0) {
				uuid_unparse_upper(archive->uuid(), uuid);
				fprintf(stdout, "%s\n", uuid);
			} else {
				fprintf(stderr, "Error: Install failed. Rolling back installation.\n");
				res = depot->uninstall(archive);
				if (res) {
					fprintf(stderr, "Error: Unable to rollback installation. "
							"Your system is in an inconsistent state! File a bug!\n");
				} else {
					fprintf(stderr, "Rollback successful.\n");
				}
				res = 1;
			}
		} else {
			fprintf(stderr, "Archive not found: %s\n", argv[1]);
		}
	} else if (argc == 1 && strcmp(argv[0], "list") == 0) {
		depot->list();
	} else if (argc == 1 && strcmp(argv[0], "dump") == 0) {
		depot->dump();
	} else if (argc == 2 && strcmp(argv[0], "files") == 0) {
		Archive* archive = depot->archive(argv[1]);
		if (archive) {
			res = depot->files(archive);
			delete archive;
		} else {
			fprintf(stderr, "Archive not found: %s\n", argv[1]);
			res = 1;
		}
	} else if (argc == 2 && strcmp(argv[0], "uninstall") == 0) {
		Archive* archive = depot->archive(argv[1]);
		if (archive) {
			if (verbosity & VERBOSE_DEBUG) {
				char uuid[37];
				uuid_unparse_upper(archive->uuid(), uuid);
				fprintf(stderr, "[uninstall] found archive: %s\n", uuid);
			}
			res = depot->uninstall(archive);
			if (res != 0) {
				fprintf(stderr, "An error occurred.\n");
				res = 1;
			}
			delete archive;
		} else {
			fprintf(stderr, "Archive not found: %s\n", argv[1]);
			res = 1;
		}
	} else if (argc == 2 && strcmp(argv[0], "verify") == 0) {
		Archive* archive = depot->archive(argv[1]);
		if (archive) {
			if (verbosity & VERBOSE_DEBUG) {
				char uuid[37];
				uuid_unparse_upper(archive->uuid(), uuid);
				fprintf(stderr, "[uninstall] found archive: %s\n", uuid);
			}			
			res = depot->verify(archive);
			if (res != 0) {
				fprintf(stderr, "An error occurred.\n");
				res = 1;
			}
			delete archive;
		} else {
			fprintf(stderr, "Archive not found: %s\n", argv[1]);
			res = 1;
		}
	} else {
		usage(progname);
	}

	free(path);
	exit(res);
	return res;
}
