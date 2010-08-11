/*
 * Copyright (c) 2010 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CommonCrypto/CommonDigest.h>


void print_usage() {
	fprintf(stdout, "digest [-1]                               \n");
	fprintf(stdout, "   Print digest hash of stdin to stdout.  \n");
	fprintf(stdout, "                                          \n");
	fprintf(stdout, "     -1       Use SHA1 hash (default)     \n");
	fprintf(stdout, "                                          \n");
}

char* format_digest(const unsigned char* m) {
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

char* calculate_digest(int fd) {
	unsigned char md[CC_SHA1_DIGEST_LENGTH];
	CC_SHA1_CTX c;
	CC_SHA1_Init(&c);
	
	memset(md, 0, CC_SHA1_DIGEST_LENGTH);
	
	ssize_t len;
	const unsigned int blocklen = 8192;
	unsigned char* block = (unsigned char*)malloc(blocklen);
	if (!block) {
		errno = ENOMEM;
		return NULL;
	}
	while(1) {
		len = read(fd, block, blocklen);
		if (len == 0) { close(fd); break; }
		if ((len < 0) && (errno == EINTR)) continue;
		if (len < 0) { close(fd); return NULL; }
		CC_SHA1_Update(&c, block, (size_t)len);
	}
	
	CC_SHA1_Final(md, &c);
	free(block);
	return format_digest(md);
}


int main(int argc, char* argv[]) {
	
	int digest = 1; // default to SHA1
	
	int ch;
	while ((ch = getopt(argc, argv, "1")) != -1) {
		switch (ch) {
			case '1':
				digest = 1;
				break;
			case '?':
			default:
				print_usage();
				exit(1);
		}
	}
	argc -= optind;
	argv += optind;
	
	fprintf(stdout, "%s\n", calculate_digest(fileno(stdin)));

	return 0;
}
