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
# Kevin Van Vechten <kvv@apple.com>

PREFIX=/usr/local
XREFDB=.build/xref.db
DARWINXREF=$PREFIX/bin/darwinxref
DATADIR=$PREFIX/share/darwinbuild
COMMONFILE=$DATADIR/darwinbuild.common

THINFILE="$DATADIR/thinFile"

###
### Interpret our arguments:
###   the architecture to thin to
ARCH="$1"

if [ "$ARCH" != "ppc" -a "$ARCH" != "i386" ]; then
        echo "usage: $(basename $0) <arch>" 1>&2
        exit 1
fi

###
### Include some common subroutines
###
. "$COMMONFILE"

shopt -s nullglob

###
### Check that we're property situated in an initialized directory
###
CheckDarwinBuildRoot

PKGDIR="$DARWIN_BUILDROOT/Packages"
DESTDIR="${PKGDIR}_$ARCH"

for X in $PKGDIR/*-*.tar.gz ; do
	Y=$(basename $X .gz).bz2
	if [ $X -nt $DESTDIR/$Y ]; then
		echo "Extracting $Y ..."
		rm -rf "$DESTDIR/tmp"
		mkdir -p "$DESTDIR/tmp"
		tar xzf $X -C "$DESTDIR/tmp" .
		echo "Thinning $Y ..."
		find "$DESTDIR/tmp" -type f -exec "$THINFILE" "$ARCH" {} ';'
		echo "Archiving $Y ..."
		tar cjf "$DESTDIR/$Y" -C "$DESTDIR/tmp" .
	#else
	#	echo $Y is up to date
	fi
done
rm -rf "$DESTDIR/tmp"
