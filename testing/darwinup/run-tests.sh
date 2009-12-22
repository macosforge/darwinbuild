#!/bin/bash
set -e
set -x

#
# Run tests on darwinup
#

PREFIX=/tmp/testing/darwinup
ORIG=$PREFIX/orig
DEST=$PREFIX/dest
DESTTAR=dest.tar.gz

DIFF="diff -x .DarwinDepot -qru"

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

for R in root4 root5 root6 root7;
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
	UUID=$(darwinup -p $DEST list | head -3 | tail -1 | awk '{print $1}')
	echo "INFO: Uninstalling $R ...";
	darwinup -vv -p $DEST uninstall $UUID
	echo "DIFF: diffing original test files to dest (should be no diffs) ..."
	$DIFF $ORIG $DEST 2>&1
done

echo "TEST: Trying all roots at once, uninstall in reverse ...";
for R in $ROOTS;
do
	echo "INFO: Installing $R ...";
	darwinup -vv -p $DEST install $PREFIX/$R
done
for R in $ROOTS;
do
	UUID=$(darwinup -p $DEST list | head -3 | tail -1 | awk '{print $1}')
	echo "INFO: Uninstalling $UUID ...";
	darwinup -vv -p $DEST uninstall $UUID
done	
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "TEST: Trying all roots at once, uninstall in install order ..."
for R in $ROOTS;
do
        echo "INFO: Installing $R ...";
        darwinup -vv -p $DEST install $PREFIX/$R
done
for R in $ROOTS;
do
        UUID=$(darwinup -p $DEST list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        darwinup -vv -p $DEST uninstall $UUID
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "TEST: Trying all roots at once, uninstall root2, root3, root ..."
for R in $ROOTS;
do
        echo "INFO: Installing $R ...";
        darwinup -vv -p $DEST install $PREFIX/$R
done
for R in root2 root3 root;
do
        UUID=$(darwinup -p $DEST list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        darwinup -vv -p $DEST uninstall $UUID
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "TEST: Trying roots in reverse, uninstall in install order ..."
for R in root3 root2 root;
do
        echo "INFO: Installing $R ...";
        darwinup -vv -p $DEST install $PREFIX/$R
done
for R in root3 root2 root;
do
        UUID=$(darwinup -p $DEST list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        darwinup -vv -p $DEST uninstall $UUID
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "TEST: Try uninstalling with user data in rollback"
echo "INFO: Installing root5 ...";
darwinup -vv -p $DEST install $PREFIX/root5
darwinup -vv -p $DEST install $PREFIX/root6
echo "modification" >> $DEST/d/file
darwinup -vv -p $DEST install $PREFIX/root7
darwinup -vv -p $DEST uninstall root6
darwinup -vv -p $DEST uninstall root5
darwinup -vv -p $DEST uninstall root7


set +e

echo "TEST: Trying a root that will fail due to object change ..."
darwinup -vv -p $DEST install $PREFIX/root4
if [ $? -ne 1 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "INFO: Done testing!"

