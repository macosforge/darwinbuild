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
# ISO generation adapted from buildcd.sh by:
#
# Shantonu Sen      <ssen@opendarwin.org>
# Felix Kronlage    <fkr@opendarwin.org>
# Chuck Remes       <cremes@opendarwin.org>

ARCH="$1"
VOLNAME="$2"

if [ "$ARCH" != "ppc" -a "$ARCH" != "i386" \
	-o "$VOLNAME" == "" ]; then
	cat 1>&2 <<- EOB
usage: $(basename $0) <arch> <volname>
       arch = {ppc, i386}
EOB
	exit 1
fi

PREFIX=/usr/local
XREFDB=.build/xref.db
DARWINXREF=$PREFIX/bin/darwinxref
DATADIR=$PREFIX/share/darwinbuild
COMMONFILE=$DATADIR/darwinbuild.common

###
### Include some common subroutines
###
. "$COMMONFILE"

shopt -s nullglob

###
### Check that we're property situated in an initialized directory
###
CheckDarwinBuildRoot

DMG="/tmp/$VOLNAME.dmg"
CDR="/tmp/$VOLNAME.cdr"
ISO="/tmp/$VOLNAME.iso"
PKGDIR="$DARWIN_BUILDROOT/Packages"
MKISOFS="/opt/local/bin/mkisofs"
SIZE="650m"

###
### Must be run as root.  Enforce this.
###
if [ "$EUID" != "0" ]; then
	echo "Error: $(basename $0) must be run as root." 1>&2
	exit 1
fi

###
### Update binary packages
###
"$DATADIR/packageRoots"

###
### Update thin packages
###
"$DATADIR/thinPackages" "$ARCH"


###
### Mount a disk image, create if necessary
###
if [ ! -f "$DMG" ]; then
	hdiutil create "$DMG" \
		-size "$SIZE" \
		-layout SPUD \
		-fs HFS+ \
		-volname "$VOLNAME"
fi
DESTDIR=$(hdiutil attach "$DMG" \
		-readwrite \
		-owners on | tail -1 | awk '{print $3}')
RECEIPTS="$DESTDIR/.receipts"

###
### Copy roots necessary for booting / installation onto disk image
###
cd "$DESTDIR" || exit
chown root:admin "$DESTDIR"
chmod 1775 "$DESTDIR"

echo "Installing Roots ..."
[ -d "$RECEIPTS" ] || mkdir "$RECEIPTS"
#for X in $PKGDIR/files-*.tar.gz $PKGDIR/*-*.tar.gz ; do
for Z in $(cat "$DARWIN_BUILDROOT/boot.txt") ; do
	X=$DARWIN_BUILDROOT/BinaryDrivers_${ARCH}/$Z-*.tar.bz2
	if [ ! -f "$X" ]; then
		X=$DARWIN_BUILDROOT/Packages_${ARCH}/$Z-*.tar.bz2
	fi
	if [ -f "$X" ]; then
		RECEIPT=$RECEIPTS/$(basename $X)
		if [ "$RECEIPT" -ot $X ]; then
			echo ... $(basename $X)
			tar xjf $X
			touch $RECEIPT
		fi
	fi
done

###
### Generate the mkext cache
###
echo "Generating MKext ..."

export TMPDIR="$DESTDIR/private/tmp"
kextcache -a $ARCH -k -K "$DESTDIR/mach_kernel" -m "$DESTDIR/System/Library/Extensions.mkext" "$DESTDIR/System/Library/Extensions"
export -n TMPDIR

###
### Remove extraneous files
###
echo "Pruning ..."
find "$DESTDIR" -type f \( -name '*.[ch]' -or -name '*.cp' -or -name '*.cpp' \) -print -exec rm -f {} ';'
rm -Rf "$DESTDIR/AppleInternal"
rm -Rf "$DESTDIR/Developer"
rm -Rf "$DESTDIR/usr/local"
rm -Rf "$DESTDIR/usr/share/doc"
rm -Rf "$DESTDIR"/usr/share/man/man3/*

###
### Modify the root password to blank
###
echo "Modifying Root Password ..."
nicl -raw "$DESTDIR/var/db/netinfo/local.nidb" -create /users/root passwd ''

###
### Copy installation packages
###
mkdir -p "$DESTDIR/System/Installation/BinaryDrivers_${ARCH}"
echo "Copying binary drivers ..."
for X in $DARWIN_BUILDROOT/BinaryDrivers_${ARCH}/*-*.tar.bz2 ; do
	Y="$DESTDIR"/System/Installation/BinaryDrivers_${ARCH}/$(basename $X)
	if [ $X -nt $Y ]; then
		cp $X $Y
	fi
done

echo "Copying packages ..."
mkdir -p "$DESTDIR"/System/Installation/Packages_${ARCH}
for X in $DARWIN_BUILDROOT/Packages_${ARCH}/*-*.tar.bz2 ; do
	f=$(basename $X)
	if [ "${f/-*/}" != "DarwinInstaller" ]; then
	Y="$DESTDIR"/System/Installation/Packages_${ARCH}/$(basename $X)
	if [ $X -nt $Y ]; then
		echo $f
		cp $X $Y
	fi
	fi
done

###
### Architecture-specific boot setup
###
if [ "$ARCH" == "ppc" ]; then
	###
	### Bless the volume for powerpc booting
	###
	echo "Blessing Volume ..."
	bless   --verbose \
		--label "$VOLNAME" \
		--folder "$DESTDIR/System/Library/CoreServices" \
		--bootinfo "$DESTDIR/usr/standalone/ppc/bootx.bootinfo"

	### Generate CDR image
	rm -f "$CDR"
        hdiutil convert \
		"$DMG" \
		-format UDTO \
                -o "$CDR"


elif [ "$ARCH" == "i386" ]; then
	###
	### Create a bootable ISO filesystem
	###
	ditto "$DESTDIR/usr/standalone/i386" /tmp/i386
	rm -f "$ISO.boot"
	$MKISOFS -R \
		-V "$VOLNAME" \
		-no-emul-boot \
		-T \
		-J \
		-c boot.cat \
		-b cdboot \
		-hide-joliet-trans-tbl \
		-quiet \
		-o "$ISO.boot" \
		/tmp/i386
	SECTORS=$(du "$ISO.boot" | tail -1 | awk '{print $1}')

	###
	### Create a hybrid image
	###
	rm -f "$ISO.dmg"
	hdiutil create "$ISO.dmg" -size "$SIZE" -layout NONE
	DEV=$(hdid -nomount "$ISO.dmg" | tail -1 | awk '{print $1}')
	RDEV=$(echo $DEV | sed s/disk/rdisk/)

	pdisk "$RDEV" -initialize
	BLOCKS=$(pdisk "$RDEV" -dump | grep 2: | awk -F" " '{print $4}')

	pdisk "$RDEV" -createPartition "$VOLNAME" Apple_HFS $SECTORS $(($BLOCKS - $SECTORS))
	SLICE=$(pdisk "$RDEV" -dump | grep "$VOLNAME" | awk -F: '{print $1}' | awk -F" " '{print $1}')

	### Copy ISO boot data onto the disk image
	dd if="$ISO.boot" of="$RDEV" skip=64 seek=64 bs=512

	newfs_hfs -v "$VOLNAME" "${RDEV}s${SLICE}"
	mkdir -p /mnt
	mount -t hfs -o perm "${DEV}s${SLICE}" /mnt
	ditto -rsrc "$DESTDIR" /mnt
	bless -folder /mnt/System/Library/CoreServices \
	      -bootinfo /mnt/usr/standalone/ppc/bootx.bootinfo \
	      -label "$VOLNAME"
	umount /mnt
	hdiutil eject "$DEV"
	mv "$ISO.dmg" "$ISO"
fi
