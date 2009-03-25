#!/bin/bash

#
# Run tests on darwinup
#

PREFIX=/tmp/testing/darwinup
ORIG=$PREFIX/orig
DEST=$PREFIX/dest
ROOT=$PREFIX/root
DESTTAR=dest.tar.gz
ROOTTAR=root.tar.gz

echo "INFO: Cleaning up testing area ..."
rm -rf $PREFIX
mkdir -p $PREFIX

echo "INFO: Untarring the test files ..."
tar zxvf $DESTTAR -C $PREFIX
tar zxvf $ROOTTAR -C $PREFIX

mkdir -p $ORIG
cp -R $DEST/* $ORIG/

echo "INFO: Installing test root ..."
darwinup -p $DEST install $ROOT

echo "DIFF: diffing root and dest files (should be no diffs) ..."
diff -qru $ROOT $DEST 2>&1 | grep -v \\.DarwinDepot

echo "INFO: Determining the UUID ..."
UUID=$(darwinup -p $DEST list | tail -1 | awk '{print $1}')
echo "UUID=$UUID"

echo "INFO: Uninstalling test root ..."
darwinup -p $DEST uninstall $UUID

echo "DIFF: diffing original test files to dest (should be no diffs) ..."
diff -qru $ORIG $DEST 2>&1 | grep -v \\.DarwinDepot

echo "INFO: Done testing!"

