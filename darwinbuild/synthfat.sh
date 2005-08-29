#!/bin/sh
#
# Copyright (c) 2005, Apple Computer, Inc. All rights reserved.
#
# @APPLE_BSD_LICENSE_HEADER_START@
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# @APPLE_BSD_LICENSE_HEADER_END@
#
# Kevin Van Vechten <kevin@opendarwin.org>
#
# Adapted from OpenDarwin src/build/Makefile by:
#
# Shantonu Sen      <ssen@opendarwin.org>

ARCH="$1"
CROSS_ARCH="$2"
DYLIB="$3"
DESTDIR="$4"

###
### Validate parameters.
###
if [ -z "$ARCH" -o -z "$CROSS_ARCH" -o \
     -z "$DYLIB" -o -z "$DESTDIR" ]; then
	echo usage: $(basename "$0") \
		"<arch> <cross arch> <dylib> <destdir>"  1>&2
	exit 1
fi
if [ "$ARCH" != "ppc" -a "$ARCH" != "ppc64" -a \
	"$ARCH" != "ppc64" ]; then
	echo Error: invalid architecture: $ARCH 1>&2
	exit 1
fi
if [ ! -f "$DYLIB" ]; then
	echo Error: invalid input dylib: $DYLIB 1>&2
	exit 1
fi
if [ ! -d "$DESTDIR" ]; then
	echo Error: invalid destination directory: $DESTDIR 1>&2
	exit 1
fi

###
### Examine the input ($DYLIB).
###
base_name=$(basename "$DYLIB")
arch_dylib=/tmp/$(echo "$base_name" | sed "s/.dylib\$/.$ARCH.dylib/")
cross_dylib=/tmp/$(echo "$base_name" | sed "s/.dylib\$/.$CROSS_ARCH.dylib/")
fat_dylib="$DESTDIR/$base_name"
temp_dylib=$(mktemp -t "$base_name").c

compat_vers=$(otool -Lv "$DYLIB" |
		head -n2 | tail -n1 |
		sed 's/.*compatibility version \(.*\),.*/\1/')

current_vers=$(otool -Lv "$DYLIB" |
		head -n2 | tail -n1 |
		sed 's/.*current version \(.*\)).*/\1/')

is_fat=$(file "$DYLIB" | grep "fat file")


###
### Thin existing architectures.
### These will be re-assembled with the newly synthesized architecture.
###
existing_archs=$(lipo -info "$DYLIB" | sed 's/^.*: //')

if [ -n "$is_fat" ]; then
	for exarch in $existing_archs; do
		thin_dylib=/tmp/$(echo "$base_name" | sed "s/.dylib\$/.$exarch.dylib/")
		lipo "$DYLIB" -thin "$exarch" -output "$thin_dylib"
	done
else
	cp "$DYLIB" "$arch_dylib"
fi

###
### Synthesize the new architecture ($CROSS_ARCH).
### Use the specified architecture ($ARCH) as a template.
###
nm -g "$arch_dylib" | grep " D " |
	awk '{print $3}' | sed 's/^_//' |
	awk '{print "int "$1" = 0;";}' >> "$temp_dylib"
nm -g "$arch_dylib" | grep " S " |
	awk '{print $3}' | sed 's/^_//' |
	awk '{print "int "$1";";}' >> "$temp_dylib"
nm -g "$arch_dylib" | grep " T " |
	awk '{print $3}' | sed 's/^_//' |
	awk '{print "void "$1"() {}";}' >> "$temp_dylib"

gcc -dynamiclib \
	-nostdlib \
	-fno-builtin \
	-fno-common \
	-arch "$CROSS_ARCH" \
	-o "$cross_dylib" \
	"$temp_dylib" \
	-install_name "$DYLIB" \
	-compatibility_version "$compat_vers" \
	-current_version "$current_vers"

###
### Lipo the synthesized architecture together with the existing architectures.
###
thin_dylibs=""
for exarch in $existing_archs; do
	thin_dylibs="$thin_dylibs "/tmp/$(echo "$base_name" | sed "s/.dylib\$/.$exarch.dylib/")
done
lipo $thin_dylibs "$cross_dylib" -create -output "$fat_dylib"

###
### Remove temporary files.
###
rm -f "$temp_dylib"
rm -f /tmp/$(basename "$temp_dylib" .c)
rm -f "$cross_dylib"
rm -f $thin_dylibs

echo "$base_name ($compat_vers, $current_vers)"
