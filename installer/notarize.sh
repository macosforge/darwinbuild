#!/bin/sh

set -e

MY_DIR=$(cd `dirname $0` && pwd)
cd $MY_DIR

if [ "$1" == "-upload" ]; then
	if [ ! -f $MY_DIR/darwinbuild-installer.pkg ]; then
		echo "darwinbuild-installer.pkg not present, please run build.sh to create it" 1>&2
		exit 1
	fi

	exec xcrun altool --notarize-app --primary-bundle-id org.puredarwin.darwinbuild.release \
		--username "ADC Notarization" --password "@keychain:ADC Notarization" \
		--file $MY_DIR/darwinbuild-installer.pkg
elif [ "$1" == "-staple" ]; then
	if [ ! -f $MY_DIR/darwinbuild-installer.pkg ]; then
		echo "darwinbuild-installer.pkg not present, please run build.sh to create it" 1>&2
		exit 1
	fi

	exec xcrun stapler staple $MY_DIR/darwinbuild-installer.pkg
elif [ "$1" == "-status" ]; then
	exec xcrun altool --notarization-history 0 -u "ADC Notarization" -p "@keychain:ADC Notarization"
else
	echo "usage: $0 { -upload | -staple | -status }" 1>&2
	echo "Wraps altool to ease notarization of the darwinbuild installer package" 1>&2
	exit 1
fi
