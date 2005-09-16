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

PREFIX=/usr/local
XREFDB=.build/xref.db
DARWINXREF=$PREFIX/bin/darwinxref
DATADIR=$PREFIX/share/darwinbuild
COMMONFILE=$DATADIR/darwinbuild.common

NORUN=""

###
### Interpret our arguments:
###   -n  Don't actually package anything, just say what we would do.
function PrintUsage() {
	echo "usage: $(basename $0) [-n]" 1>&2
	exit
}

for ARG in "$@"; do
	if [ "$NORUN" == "" ]; then
		if [ "$ARG" == "-n" ]; then
			NORUN="YES"
		else
			PrintUsage "$0"
		fi
	else
		PrintUsage "$0"
	fi
done


###
### Include some common subroutines
###
. "$COMMONFILE"

shopt -s nullglob

###
### Check that we're property situated in an initialized directory
###
CheckDarwinBuildRoot


if [ "$NORUN" == "YES" ]; then
	CMD="echo SKIPPING tar czf"
else
	CMD="tar czf"
fi

if [ ! -d "$DARWIN_BUILDROOT/Packages" ]; then
	mkdir "$DARWIN_BUILDROOT/Packages"
fi

function PackageThem() {
	local DIR="$1"
	local SFX="$2"
	echo "*** Packaging $DIR"
	"$DARWINXREF" version '*' | while read X; do
		Y="${X/-*/}"
		build_version=$(GetBuildVersion $DARWIN_BUILDROOT/$DIR/$Y/$X*)
		if [ "$build_version" != "0" -a \
		     "$DARWIN_BUILDROOT/$DIR/$Y/$X$SFX~$build_version" -nt \
		     "$DARWIN_BUILDROOT/Packages/$X$SFX.tar.gz" ]; then
			echo "$X$SFX~$build_version"
			cd "$DARWIN_BUILDROOT/$DIR/$Y/$X$SFX~$build_version"
			eval $CMD "$DARWIN_BUILDROOT/Packages/$Y$SFX.tar.gz" .
		fi
	done
}
PackageThem Headers .hdrs
PackageThem Roots .root
