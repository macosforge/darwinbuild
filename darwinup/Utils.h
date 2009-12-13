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

#include <stdint.h>
#include <sys/types.h>
#include <fts.h>

#include <stdarg.h>
#include <stdio.h>

const uint32_t VERBOSE		= 0x1;
const uint32_t VERBOSE_DEBUG	= 0x2;

#define IF_DEBUG(...) do { extern uint32_t verbosity; if (verbosity & VERBOSE_DEBUG) fprintf(stderr, "DEBUG: " __VA_ARGS__); } while (0)

int fts_compare(const FTSENT **a, const FTSENT **b);
int ftsent_filename(FTSENT* ent, char* filename, size_t bufsiz);
int mkdir_p(const char* path);
int remove_directory(const char* path);
int is_directory(const char* path);
int is_regular_file(const char* path);
int is_url_path(const char* path);
int is_userhost_path(const char* path);
int has_suffix(const char* str, const char* sfx);
int exec_with_args(const char** args);

int join_path(char** out, const char* p1, const char* p2);
int compact_slashes(char* orig, int slashes);

char* fetch_url(const char* srcpath, const char* dstpath);
char* fetch_userhost(const char* srcpath, const char* dstpath);

inline int INFO_TEST(uint32_t word, uint32_t flag) { return ((word & flag) != 0); }
inline int INFO_SET(uint32_t word, uint32_t flag) { return (word | flag); }
inline int INFO_CLR(uint32_t word, uint32_t flag) { return (word & (~flag)); }

