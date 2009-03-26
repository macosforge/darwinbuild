#!/bin/bash

#
# Run tests on darwinup
#

PREFIX=/tmp/testing/darwinup
ORIG=$PREFIX/orig
DEST=$PREFIX/dest
DESTTAR=dest.tar.gz

ROOTS="root root2 root3"

echo "INFO: Cleaning up testing area ..."
rm -rf $PREFIX
mkdir -p $PREFIX

echo "INFO: Untarring the test files ..."
tar zxvf $DESTTAR -C $PREFIX

for R in $ROOTS;
do
	tar zxvf $R.tar.gz -C $PREFIX
done;

mkdir -p $ORIG
cp -R $DEST/* $ORIG/

echo "TEST: Trying roots one at a time ..."
for R in $ROOTS;
do
	echo "INFO: Installing $R ...";
	darwinup -vv -p $DEST install $PREFIX/$R
	if [ $? -gt 0 ]; then exit 1; fi
	UUID=$(darwinup -p $DEST list | head -3 | tail -1 | awk '{print $1}')
	echo "INFO: Uninstalling $R ...";
	darwinup -vv -p $DEST uninstall $UUID
	if [ $? -gt 0 ]; then exit 1; fi
	echo "DIFF: diffing original test files to dest (should be no diffs) ..."
	diff -qru $ORIG $DEST 2>&1 | grep -v \\.DarwinDepot
done

echo "TEST: Trying all roots at once, uninstall in reverse ...";
for R in $ROOTS;
do
	echo "INFO: Installing $R ...";
	darwinup -vv -p $DEST install $PREFIX/$R
	if [ $? -gt 0 ]; then exit 1; fi
done
for R in $ROOTS;
do
	UUID=$(darwinup -p $DEST list | head -3 | tail -1 | awk '{print $1}')
	echo "INFO: Uninstalling $UUID ...";
	darwinup -vv -p $DEST uninstall $UUID
	if [ $? -gt 0 ]; then exit 1; fi
done	
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
diff -qru $ORIG $DEST 2>&1 | grep -v \\.DarwinDepot

echo "TEST: Trying all roots at once, uninstall in install order ..."
for R in $ROOTS;
do
        echo "INFO: Installing $R ...";
        darwinup -vv -p $DEST install $PREFIX/$R
	if [ $? -gt 0 ]; then exit 1; fi
done
for R in $ROOTS;
do
        UUID=$(darwinup -p $DEST list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        darwinup -vv -p $DEST uninstall $UUID
	if [ $? -gt 0 ]; then exit 1; fi
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
diff -qru $ORIG $DEST 2>&1 | grep -v \\.DarwinDepot

echo "TEST: Trying all roots at once, uninstall root2, root3, root ..."
for R in $ROOTS;
do
        echo "INFO: Installing $R ...";
        darwinup -vv -p $DEST install $PREFIX/$R
	if [ $? -gt 0 ]; then exit 1; fi
done
for R in root2 root3 root;
do
        UUID=$(darwinup -p $DEST list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        darwinup -vv -p $DEST uninstall $UUID
	if [ $? -gt 0 ]; then exit 1; fi
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
diff -qru $ORIG $DEST 2>&1 | grep -v \\.DarwinDepot

echo "TEST: Trying roots in reverse, uninstall in install order ..."
for R in root3 root2 root;
do
        echo "INFO: Installing $R ...";
        darwinup -vv -p $DEST install $PREFIX/$R
	if [ $? -gt 0 ]; then exit 1; fi
done
for R in root3 root2 root;
do
        UUID=$(darwinup -p $DEST list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        darwinup -vv -p $DEST uninstall $UUID
	if [ $? -gt 0 ]; then exit 1; fi
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
diff -qru $ORIG $DEST 2>&1 | grep -v \\.DarwinDepot


echo "INFO: Done testing!"

