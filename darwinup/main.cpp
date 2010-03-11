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

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "Archive.h"
#include "Depot.h"
#include "Utils.h"
#include "DB.h"


void usage(char* progname) {
	fprintf(stderr, "usage:    %s [-v] [-p DIR] [command] [args]          \n", progname);
	fprintf(stderr, "version: 16                                                    \n");
	fprintf(stderr, "                                                               \n");
	fprintf(stderr, "options:                                                       \n");
	fprintf(stderr, "          -d        do not update dyld cache after (un)install \n");	
	fprintf(stderr, "          -f        force operation to succeed at all costs    \n");
	fprintf(stderr, "          -p DIR    operate on roots under DIR (default: /)    \n");
	fprintf(stderr, "          -v        verbose (use -vv for extra verbosity)      \n");
	fprintf(stderr, "                                                               \n");
	fprintf(stderr, "commands:                                                      \n");
	fprintf(stderr, "          files      <archive>                                 \n");
	fprintf(stderr, "          install    <path>                                    \n");
	fprintf(stderr, "          list       [archive]                                 \n");
	fprintf(stderr, "          uninstall  <archive>                                 \n");
	fprintf(stderr, "          upgrade    <path>                                    \n");
	fprintf(stderr, "          verify     <archive>                                 \n");
	fprintf(stderr, "                                                               \n");
	fprintf(stderr, "<path> is one of:                                              \n");
	fprintf(stderr, "          /path/to/local/dir-or-file                           \n");
	fprintf(stderr, "          user@host:/path/to/remote/dir-or-file                \n");
	fprintf(stderr, "          http[s]://host/path/to/remote/file                   \n");
	fprintf(stderr, "                                                               \n");
	fprintf(stderr, "Files must be in one of the supported archive formats:         \n");
	fprintf(stderr, "          cpio, cpio.gz, cpio.bz2                              \n");
	fprintf(stderr, "          pax, pax.gz, pax.bz2                                 \n");
	fprintf(stderr, "          tar, tar.gz, tar.bz2                                 \n");
	fprintf(stderr, "          xar, zip                                             \n");
	fprintf(stderr, "                                                               \n");
	fprintf(stderr, "archive is one of:                                             \n");
	fprintf(stderr, "          <serial>     the Serial number                       \n");
	fprintf(stderr, "          <uuid>       the UUID                                \n");
	fprintf(stderr, "          <name>       the last root installed with that name  \n");
	fprintf(stderr, "          newest       the newest (last) root installed        \n");
	fprintf(stderr, "          oldest       the oldest root installed               \n");
	fprintf(stderr, "          superseded   all roots that have been fully replaced \n");
	fprintf(stderr, "                        by newer roots                         \n");
	fprintf(stderr, "          all          all installed roots                     \n");
	fprintf(stderr, "                                                               \n");
	exit(1);
}

// our globals
uint32_t verbosity;
uint32_t force;


int main(int argc, char* argv[]) {
	char* progname = strdup(basename(argv[0]));      

	char* path = NULL;
	
	bool update_dyld = true;

	int ch;
	while ((ch = getopt(argc, argv, "dfp:vh")) != -1) {
		switch (ch) {
		case 'd':
				update_dyld = false;
				break;
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
	if (argc == 0) usage(progname);
	
	int res = 0;

	if (!path) {
		asprintf(&path, "/");
	}

	Depot* depot = new Depot(path);
		
	// list handles args optional and in special ways
	if (strcmp(argv[0], "list") == 0) {
		res = depot->initialize(false);
		if (res == -2) {
			// we are not asking to write, 
			// but no depot exists yet either,
			// so print the apparent truth
			fprintf(stdout, "Nothing has been installed yet.\n");
			exit(0);
		}
		if (res == 0) depot->list(argc-1, (char**)(argv+1));
	} else if (argc == 1) {
		// other commands which take no arguments
		if (strcmp(argv[0], "dump") == 0) {
			if (depot->initialize(false)) exit(11);
			depot->dump();
		} else {
			usage(progname);
		}
	} else {
		// loop over arguments
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[0], "install") == 0) {
				if (i==1 && depot->initialize(true)) exit(13);
				res = depot->install(argv[i]);
				if (update_dyld && res == 0) res = update_dyld_shared_cache(path);
			} else if (strcmp(argv[0], "upgrade") == 0) {
				if (i==1 && depot->initialize(true)) exit(14);
				// find most recent matching archive by name
				Archive* old = depot->get_archive(basename(argv[i]));
				if (!old) {
					fprintf(stderr, "Error: unable to find a matching root to upgrade.\n");
					res = 5;
				}
				// install new archive
				if (res == 0) res = depot->install(argv[i]);
				// uninstall old archive
				if (res == 0) res = depot->uninstall(old);
				if (update_dyld && res == 0) res = update_dyld_shared_cache(path);
			} else if (strcmp(argv[0], "files") == 0) {
				if (i==1 && depot->initialize(false)) exit(12);
				res = depot->process_archive(argv[0], argv[i]);
			} else if (strcmp(argv[0], "uninstall") == 0) {
				if (i==1 && depot->initialize(true)) exit(15);
				res = depot->process_archive(argv[0], argv[i]);
				if (update_dyld && res == 0) res = update_dyld_shared_cache(path);
			} else if (strcmp(argv[0], "verify") == 0) {
				if (i==1 && depot->initialize(true)) exit(16);
				res = depot->process_archive(argv[0], argv[i]);
			} else {
				usage(progname);
			}
		}
	}
	
	free(path);
	exit(res);
	return res;
}
