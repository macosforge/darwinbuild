#!/bin/sh

set -e

MY_DIR=$(cd `dirname $0` && pwd)
cd $MY_DIR

xcodebuild install \
	-project $MY_DIR/../darwinbuild.xcodeproj \
	-scheme world -configuration Release \
	DSTROOT=$MY_DIR/payload

pkgbuild \
	--ownership recommended \
	--identifier org.puredarwin.darwinbuild.component \
	--version 2.0 \
	--root $MY_DIR/payload \
	--install-location / \
	$MY_DIR/darwinbuild-component.pkg

productbuild \
	--distribution $MY_DIR/distribution.xml \
	--identifier org.puredarwin.darwinbuild.release \
	--version 2.0 \
	--sign 'Developer ID Installer' --timestamp \
	--package-path $MY_DIR \
	--resources $MY_DIR \
	$MY_DIR/darwinbuild-installer.pkg

echo "Complete! Your installer is located at: $MY_DIR/darwinbuild-installer.pkg"
