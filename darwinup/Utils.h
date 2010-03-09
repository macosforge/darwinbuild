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

#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <sys/types.h>
#include <fts.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/stat.h>


const uint32_t VERBOSE		    = 0x1;
const uint32_t VERBOSE_DEBUG	= 0x2;
const uint32_t VERBOSE_SQL      = 0x4;

#define IF_DEBUG(...) do { extern uint32_t verbosity; if (verbosity & VERBOSE_DEBUG) fprintf(stderr, "DEBUG: " __VA_ARGS__); } while (0)
#define IF_SQL(...) do { extern uint32_t verbosity; if (verbosity & VERBOSE_SQL) fprintf(stderr, "DEBUG: " __VA_ARGS__); } while (0)

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
int exec_with_args_pipe(const char** args, int fd);
int exec_with_args_fa(const char** args, posix_spawn_file_actions_t* fa);

int join_path(char** out, const char* p1, const char* p2);
int compact_slashes(char* orig, int slashes);

char* fetch_url(const char* srcpath, const char* dstpath);
char* fetch_userhost(const char* srcpath, const char* dstpath);

int build_number_for_path(char** build, const char* path);

void __data_hex(FILE* f, uint8_t* data, uint32_t size);

inline int INFO_TEST(uint32_t word, uint32_t flag) { return ((word & flag) != 0); }
inline int INFO_SET(uint32_t word, uint32_t flag) { return (word | flag); }
inline int INFO_CLR(uint32_t word, uint32_t flag) { return (word & (~flag)); }

#endif
