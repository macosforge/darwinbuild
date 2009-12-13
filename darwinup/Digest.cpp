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

#include "Digest.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// For SHA1DigestMachO
#include <mach/mach_init.h>
#include <mach/vm_map.h>
extern "C" {
// <rdar://problem/4319807> redo_prebinding.h should use extern "C"
//#include <mach-o/redo_prebinding.h> // from cctools_ofiles
#include "redo_prebinding.h"
}

Digest::Digest() {
	memset(m_data, 0, sizeof(m_data));
	m_size = 0;
}

Digest::Digest(const EVP_MD* md, int fd) {
	digest(md, fd);
}

void Digest::digest(const EVP_MD* md, int fd) {
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit(&ctx, md);

	int len;
	const unsigned int blocklen = 8192;
	static uint8_t* block = NULL;
	if (block == NULL) {
		block = (uint8_t*)malloc(blocklen);
	}
	while(1) {
		len = read(fd, block, blocklen);
		if (len == 0) { close(fd); break; }
		if ((len < 0) && (errno == EINTR)) continue;
		if (len < 0) { close(fd); return; }
		EVP_DigestUpdate(&ctx, block, len);
	}
	if (len >= 0) {
		EVP_DigestFinal(&ctx, m_data, &m_size);
	}
}

Digest::Digest(const EVP_MD* md, uint8_t* data, uint32_t size) {
	digest(md, data, size);
}

void Digest::digest(const EVP_MD* md, uint8_t* data, uint32_t size) {
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit(&ctx, md);
	EVP_DigestUpdate(&ctx, data, size);
	EVP_DigestFinal(&ctx, m_data, &m_size);
}

uint8_t*	Digest::data() { return m_data; }
uint32_t	Digest::size() { return m_size; }

char* Digest::string() {
	static const char* hexabet = "0123456789abcdef";
	char* result = (char*)malloc(2*m_size+1);
	uint32_t i, j;
	
	for (i = 0, j = 0; i < m_size; ++i) {
		result[j++] = hexabet[(m_data[i] & 0xF0) >> 4];
		result[j++] = hexabet[(m_data[i] & 0x0F)];
	}
	result[j] = 0;
	
	return result;
}

int Digest::equal(Digest* a, Digest* b) {
	if (a == b) return 1;
	if (a == NULL) return 0;
	if (b == NULL) return 0;
	uint32_t a_size = a->size();
	if (a_size != b->size()) {
		return 0;
	} 
	return (memcmp(a->data(), b->data(), a_size) == 0);
}


const EVP_MD* SHA1Digest::m_md;

SHA1Digest::SHA1Digest() {
	if (m_md == NULL) {
		OpenSSL_add_all_digests();
		m_md = EVP_get_digestbyname("sha1");
		assert(m_md != NULL);
	}
}

SHA1Digest::SHA1Digest(int fd) {
	if (m_md == NULL) {
		OpenSSL_add_all_digests();
		m_md = EVP_get_digestbyname("sha1");
		assert(m_md != NULL);
	}
	digest(m_md, fd);
}

SHA1Digest::SHA1Digest(const char* filename) {
	int fd = open(filename, O_RDONLY);
	if (m_md == NULL) {
		OpenSSL_add_all_digests();
		m_md = EVP_get_digestbyname("sha1");
		assert(m_md != NULL);
	}
	digest(m_md, fd);
}

SHA1Digest::SHA1Digest(uint8_t* data, uint32_t size) {
	if (m_md == NULL) {
		OpenSSL_add_all_digests();
		m_md = EVP_get_digestbyname("sha1");
		assert(m_md != NULL);
	}
	digest(m_md, data, size);
}


SHA1DigestMachO::SHA1DigestMachO(const char* filename) {
	char* error = NULL;
	
	// Check for Mach-O
	int type = object_file_type(filename, NULL, &error);
	if (type == OFT_EXECUTABLE ||
		type == OFT_DYLIB ||
		type == OFT_BUNDLE) {
		// XXX - type == OFT_ARCHIVE?
		void* block = NULL;
		unsigned long blocklen = 0;
		int ret = unprebind(filename,
			NULL,
			NULL,
			&error,
			1,
			NULL,
			0,
			&block,
			&blocklen);
		if (ret == REDO_PREBINDING_SUCCESS && block != NULL) {
			digest(SHA1Digest::m_md, (uint8_t*)block, blocklen);
		} else {
			//fprintf(stderr, "%s:%d: unexpected unprebind result: %s: %s (%d)\n", __FILE__, __LINE__, filename, error, ret);
			int fd = open(filename, O_RDONLY);
			digest(SHA1Digest::m_md, fd);
			close(fd);
		}
		if (block != NULL) {
			kern_return_t ret = vm_deallocate(mach_task_self(), (vm_address_t)block, (vm_size_t)blocklen);
			assert(ret == 0);
		}
	} else {
		int fd = open(filename, O_RDONLY);
		digest(SHA1Digest::m_md, fd);
		close(fd);
	}
}

SHA1DigestSymlink::SHA1DigestSymlink(const char* filename) {
	char link[PATH_MAX];
	int res = readlink(filename, link, PATH_MAX);
	if (res == -1) {
		fprintf(stderr, "%s:%d: readlink: %s: %s (%d)\n", __FILE__, __LINE__, filename, strerror(errno), errno);
	} else {
		digest(SHA1Digest::m_md, (uint8_t*)link, res);
	}
}
