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

DIFF="diff -x .DarwinDepot -x broken -qru"

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

for R in root4 root5 root6 root7 symlinks symlink_update;
do
	tar zxvf $R.tar.gz -C $PREFIX
done;

for R in 300dirs.tbz2 300files.tbz2 deep-rollback.cpgz deep-rollback-2.xar;
do
	cp $R $PREFIX/
done;

mkdir -p $ORIG
cp -R $DEST/* $ORIG/

echo "========== TEST: Trying roots one at a time =========="
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

echo "========== TEST: Multiple argument test ==========";
darwinup -vv -p $DEST install $PREFIX/root{,2,3}
LINES=$(darwinup -p $DEST list | wc -l)
if [ $LINES -lt 5 ]; then
	echo "Failed multiple argument test."
	exit 1;
fi
darwinup -vv -p $DEST uninstall all
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1


echo "========== TEST: Trying all roots at once, uninstall in reverse ==========";
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

echo "========== TEST: Trying all roots at once, uninstall in install order =========="
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

echo "========== TEST: Trying all roots at once, uninstall root2, root3, root =========="
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

echo "========== TEST: Trying roots in reverse, uninstall in install order =========="
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

echo "========== TEST: trying large roots ==========";
echo "INFO: installing 300files";
darwinup -vv -p $DEST install $PREFIX/300files.tbz2
darwinup -vv -p $DEST uninstall 300files.tbz2
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
echo "INFO: installing 300dir";
darwinup -vv -p $DEST install $PREFIX/300dirs.tbz2
darwinup -vv -p $DEST uninstall 300dirs.tbz2
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
echo "INFO: installing both 300files and 300dirs";
darwinup -vv -p $DEST install $PREFIX/300dirs.tbz2
darwinup -vv -p $DEST install $PREFIX/300files.tbz2
darwinup -vv -p $DEST uninstall 300dirs.tbz2
darwinup -vv -p $DEST uninstall 300files.tbz2
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Try uninstalling with user data in rollback =========="
echo "INFO: Installing root5 ...";
darwinup -vv -p $DEST install $PREFIX/root5
darwinup -vv -p $DEST install $PREFIX/root6
echo "modification" >> $DEST/d/file
darwinup -vv -p $DEST install $PREFIX/root7
darwinup -vv -p $DEST uninstall root6
darwinup -vv -p $DEST uninstall root5
darwinup -vv -p $DEST uninstall root7
stat $DEST/d/file
rm $DEST/d/file
rmdir $DEST/d
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Deep rollback while saving user data =========="
darwinup -vv -p $DEST install $PREFIX/deep-rollback.cpgz
echo "modified" >> $DEST/d1/d2/d3/d4/d5/d6/file
darwinup -vv -p $DEST install $PREFIX/deep-rollback.cpgz
darwinup -vv -p $DEST uninstall newest
darwinup -vv -p $DEST uninstall newest
stat $DEST/d1/d2/d3/d4/d5/d6/file
rm $DEST/d1/d2/d3/d4/d5/d6/file
rm -rf $DEST/d1
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

darwinup -vv -p $DEST install $PREFIX/deep-rollback.cpgz
darwinup -vv -p $DEST install $PREFIX/deep-rollback-2.xar
darwinup -vv -p $DEST uninstall all
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1


echo "========== TEST: Testing broken symlink handling =========="
darwinup -vv -p $DEST install $PREFIX/symlinks
darwinup -vv -p $DEST uninstall symlinks
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
darwinup -vv -p $DEST install $PREFIX/symlink_update
stat -L $DEST/broken
darwinup -vv -p $DEST uninstall newest
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1


#
# The following are expected failures
#
set +e
echo "========== TEST: Trying a root that will fail due to object change =========="
darwinup -vv -p $DEST install $PREFIX/root4
if [ $? -ne 1 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "INFO: Done testing!"

