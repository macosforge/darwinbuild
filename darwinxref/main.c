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

#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"
#include <fcntl.h>
#include <dlfcn.h>
#include <getopt.h>

#include "DBPluginPriv.h"

// user environment global
extern char** environ;

char* readBuildFile();
char* determineHostBuildVersion();

int main(int argc, char* argv[]) {
	char* progname = argv[0];
	char* dbfile = getenv("DARWINXREF_DB_FILE");
	char* build = getenv("DARWINBUILD_BUILD");
	const char* plugins = getenv("DARWINXREF_PLUGIN_PATH");
	
	if (dbfile == NULL) dbfile = DEFAULT_DB_FILE;
	if (plugins == NULL) plugins = DEFAULT_PLUGIN_PATH;

	int ch;
	while ((ch = getopt(argc, argv, "f:b:")) != -1) {
		switch (ch) {
		case 'f':
			dbfile = optarg;
			break;
		case 'b':
			build = optarg;
			break;
		case '?':
		default:
			print_usage(progname, argc, argv);
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (build == NULL) build = readBuildFile();
	if (build == NULL) build = determineHostBuildVersion();

	// special built-in command
	if (argc == 1 && strcmp(argv[0], "info") == 0) {
		printf("%s/%s\n", basename(progname), "" VERSION "");
		printf("\tcurrent build: %s\n", build);
		printf("\tsqlite/%s (%s)\n", sqlite3_version, "UTF-8");
		printf("\tCoreFoundation/%g%s\n", kCFCoreFoundationVersionNumber, NSIsSymbolNameDefined("_CFNotificationCenterGetTypeID") ? "" : " (CF-Lite)");
		exit(1);
	}

	DBDataStoreInitialize(dbfile);
	DBSetCurrentBuild(build);
	DBPluginLoadPlugins(plugins);
	if (run_plugin(argc, argv) == -1) {
		print_usage(progname, argc, argv);
		exit(1);
	}
	return 0;
}

char* readBuildFile() {
	char* build = NULL;
	int fd = open(".build/build", O_RDONLY);
	if (fd != -1) {
		size_t size = 1000; // should be bigger than any likely build number
		build = malloc(size);
		if (build) {
			ssize_t len = read(fd, build, size-1);
			if (len == -1) {
				free(build);
				build = NULL;
			} else {
				build[len] = 0;
				if (build[len-1] == '\n') build[len-1] = 0;
			}
		}
	}
	close(fd);
	return build;
}

char* determineHostBuildVersion()
{
  char *currentBuild = NULL;

  // The following is Private API.
  // Please don't use this in your programs as it may break.
  // Notice the careful dance around these symbols as they may
  // someday disappear entirely, in which case this program
  // will need to be revved.
  CFDictionaryRef (*fptr)() = dlsym(RTLD_DEFAULT, "_CFCopySystemVersionDictionary");
  if (fptr) {
    CFDictionaryRef dict = fptr();
    if (dict != NULL) {
      CFStringRef str = CFDictionaryGetValue(dict, CFSTR("ProductBuildVersion"));
      currentBuild = strdup_cfstr(str);
      CFRelease(dict);
    }
  }

  return currentBuild;
}
