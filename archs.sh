#!/bin/sh

#
# Detect which arches we should build for
#

CRYPTO_ARCHS=`lipo -info /usr/lib/libcrypto.dylib | cut -d : -f 3`
SQLITE_ARCHS=`lipo -info /usr/lib/libsqlite3.dylib | cut -d : -f 3`
TCL_ARCHS=`lipo -info /usr/lib/libtcl.dylib | cut -d : -f 3`
SYSTEM_ARCHS=`lipo -info /usr/lib/libSystem.dylib | cut -d : -f 3`

# start with one set of archs
FINAL_ARCHS=$SYSTEM_ARCHS

for ARCH in $SYSTEM_ARCHS;
do
	# crosscheck against the remaining sets...
	for ALIST in "$CRYPTO_ARCHS" "$SQLITE_ARCHS" "$TCL_ARCHS";
	do
		# see if ARCH is not in ALIST
		if [[ ${ALIST/$ARCH} == $ALIST ]];
		then
			# ARCH was not found, so remove from final archs
			FINAL_ARCHS="${FINAL_ARCHS/$ARCH}";
		fi
	done;
done;

# print what is left over
echo $FINAL_ARCHS

