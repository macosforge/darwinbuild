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

#include "SerialSet.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

SerialSet::SerialSet() {
	capacity = 0;
	count = 0;
	values = (uint64_t*)malloc(0);
}

SerialSet::~SerialSet() {
	if (values) free(values);
}

int SerialSet::add(uint64_t value) {
	// If the serial already exists in the set, then there's nothing to be done
	uint32_t i;
	for (i = 0; i < this->count; ++i) {
		if (this->values[i] == value) {
			return 0;
		}
	}

	// Otherwise, append it to the end of the set
	this->count++;
	if (this->count > this->capacity) {
		this->capacity += 10;
		this->values = (uint64_t*)realloc(this->values, this->capacity * sizeof(uint64_t));
		assert(this->values != NULL);
	}
	this->values[this->count-1] = value;

	return 0;
}
