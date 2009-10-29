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

#include <sys/types.h>
#include <stdint.h>
#include <openssl/evp.h>

////
//  Digest
//
//  Digest is the root class for all message digest algorithms
//  supported by darwinup.
//
//  Conceptually it's an abstract class, although that
//  hasn't been formalized.
//
//  SHA1Digest is the only concrete subclass.  There are two
//  subclasses of SHA1Digest which add convenience functions
//  for digesting a canonicalized Mach-O binary, and the
//  target of a symlink obtained by readlink(2).
//
//  NOTE: It might be more appropriate to use the CommonCrypto
//  implementation of these algorithms rather than the OpenSSL
//  implementation.  However, CommonCrypto is only available on
//  Tiger.
////

struct Digest {
	Digest();
	Digest(const EVP_MD* md, int fd);
	Digest(const EVP_MD* md, uint8_t* data, uint32_t size);
	
	////
	//  Accessor functions
	////
	
	// Returns the raw digest.
	virtual	uint8_t*	data();

	// Returns the size of the raw digest.
	virtual uint32_t	size();
	
	// Returns the digest as an ASCIZ string, represented in hexidecimal.
	virtual char*		string();
	
	////
	//  Class functions
	////
	
	// Compares two digest objects for equality.
	// Returns 1 if equal, 0 if not.
	static	int		equal(Digest* a, Digest* b);


	protected:

	virtual	void	digest(const EVP_MD* md, int fd);
	virtual	void	digest(const EVP_MD* md, uint8_t* data, uint32_t size);

	uint8_t		m_data[EVP_MAX_MD_SIZE];
	uint32_t	m_size;
	
	friend struct Depot;
};

////
//  SHA1Digest
////
struct SHA1Digest : Digest {
	static const EVP_MD* m_md;
	
	// Creates an empty digest.
	SHA1Digest();
	
	// Computes the SHA-1 digest of data read from the stream.
	SHA1Digest(int fd);
	
	// Computes the SHA-1 digest of data in the file.
	SHA1Digest(const char* filename);
	
	// Computes the SHA-1 digest of the block of memory.
	SHA1Digest(uint8_t* data, uint32_t size);
};

////
//  SHA1DigestMachO
//  Digests of canonicalized Mach-O file formats.
////
struct SHA1DigestMachO : SHA1Digest {
	// Computes the SHA-1 digest of the data in the file.
	// If the file is a Mach-O executable or dynamic library,
	// the SHA-1 digest is computed from its canonical
	// representation.
	SHA1DigestMachO(const char* filename);
};

////
//  SHA1DigestSymlink
//  Digests of the target of a symlink.
////
struct SHA1DigestSymlink : SHA1Digest {
	// Computes the SHA-1 digest of the target of the symlink.
	// The target is obtained via readlink(2).
	SHA1DigestSymlink(const char* filename);
};
