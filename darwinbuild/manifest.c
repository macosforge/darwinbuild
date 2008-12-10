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
#include <openssl/evp.h>


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

static char* calculate_digest(int fd) {
	unsigned char digstr[EVP_MAX_MD_SIZE];
	memset(digstr, 0, sizeof(digstr));
	
	EVP_MD_CTX ctx;
	static const EVP_MD* md;
	
	if (md == NULL) {
		EVP_MD_CTX_init(&ctx);
		OpenSSL_add_all_digests();
		md = EVP_get_digestbyname("sha1");
		if (md == NULL) return NULL;
	}

	EVP_DigestInit(&ctx, md);

	unsigned int len;
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
		EVP_DigestUpdate(&ctx, block, len);
	}

	EVP_DigestFinal(&ctx, digstr, &len);
	return format_digest(digstr);
}

static int compare(const FTSENT **a, const FTSENT **b) {
	return strcmp((*a)->fts_name, (*b)->fts_name);
}

#if 0
static void ent_filename(FTSENT* ent, char* filename) {
	if (ent->fts_level > 1) {
		ent_filename(ent->fts_parent, filename);
		strcat(filename, "/");
	} else if (ent->fts_level == 1) {
		strcat(filename, "./");
	}
	strcat(filename, ent->fts_name);
}
#endif

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


int main(int argc, char* argv[]) {
	if (argc != 2) {
		char* progname = basename(argv[0]);
		fprintf(stderr, "usage: %s <dir>\n", progname);
		return 1;
	}
	char* path[] = { argv[1], NULL };
	FTS* fts = fts_open(path, FTS_PHYSICAL | FTS_COMFOLLOW, compare);
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
			checksum = calculate_digest(fd);
			close(fd);
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

	return 0;
}
