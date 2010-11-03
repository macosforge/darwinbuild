#!/bin/bash
set -e
set -x
pushd $(dirname $0) >> /dev/null

#
# Run tests on darwinup
#
PREFIX=/tmp/testing/darwinup
ORIG=$PREFIX/orig
DEST=$PREFIX/dest
DESTTAR=dest.tar.gz

HASXAR=$(darwinup 2>&1 | grep xar | wc -l)
HAS386=$(file `which darwinup` | grep i386 | wc -l)
HASX64=$(file `which darwinup` | grep x86_64 | wc -l)

DARWINUP="darwinup $1 -p $DEST "
DIFF="diff -x .DarwinDepot -x broken -qru"

ROOTS="root root2 root3"


function is_file {
	test -f $1 -a ! -L $1
}

function is_dir {
	test -d $1 -a ! -L $1
}

function is_link {
	test -L $1;
}


echo "INFO: Cleaning up testing area ..."
rm -rf $PREFIX
mkdir -p $PREFIX

echo "INFO: Untarring the test files ..."
tar zxvf $DESTTAR -C $PREFIX

for R in $ROOTS;
do
	tar zxvf $R.tar.gz -C $PREFIX
done;

for R in root5 root6 root7 symlinks symlink_update;
do
	tar zxvf $R.tar.gz -C $PREFIX
done;

for R in rep_dir_file rep_dir_link rep_file_dir rep_file_link \
		 rep_link_dir rep_link_file rep_flink_dir rep_flink_file;
do
	tar zxvf $R.tar.gz -C $PREFIX
done;

for R in 300dirs.tbz2 300files.tbz2 deep-rollback.cpgz deep-rollback-2.xar extension.tar.bz2;
do
	cp $R $PREFIX/
done;

cp corrupt.tgz $PREFIX/
cp depotroot.tar.gz $PREFIX/

mkdir -p $ORIG
cp -R $DEST/* $ORIG/

if [ -f /usr/bin/sudo ];
then
	echo "========== TEST: Listing ============="
	/usr/bin/sudo -u nobody $DARWINUP list
	$DARWINUP list
fi

if [ $HAS386 -gt 0 -a $HASX64 -gt 0 ];
then
	echo "========== TEST: Trying both 32 and 64 bit =========="
	for R in $ROOTS;
	do
		echo "INFO: Installing $R ...";
		arch -i386 $DARWINUP install $PREFIX/$R
		UUID=$($DARWINUP list | head -3 | tail -1 | awk '{print $1}')
		echo "INFO: Uninstalling $R ...";
		arch -x86_64 $DARWINUP uninstall $UUID
		echo "DIFF: diffing original test files to dest (should be no diffs) ..."
		$DIFF $ORIG $DEST 2>&1
		echo "INFO: Installing $R ...";
		arch -x86_64 $DARWINUP install $PREFIX/$R
		UUID=$($DARWINUP list | head -3 | tail -1 | awk '{print $1}')
		echo "INFO: Uninstalling $R ...";
		arch -i386 $DARWINUP uninstall $UUID
		echo "DIFF: diffing original test files to dest (should be no diffs) ..."
		$DIFF $ORIG $DEST 2>&1
	done
fi

echo "========== TEST: Try installing a symlink-to-directory =========="
ln -s root2 $PREFIX/root_link
# test without trailing slash
$DARWINUP install $PREFIX/root_link
$DARWINUP uninstall root_link
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
# test with trailing slash
$DARWINUP install $PREFIX/root_link/
$DARWINUP uninstall root_link
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Trying roots one at a time =========="
for R in $ROOTS;
do
	echo "INFO: Installing $R ...";
	$DARWINUP install $PREFIX/$R
	UUID=$($DARWINUP list | head -3 | tail -1 | awk '{print $1}')
	echo "INFO: Uninstalling $R ...";
	$DARWINUP uninstall $UUID
	echo "DIFF: diffing original test files to dest (should be no diffs) ..."
	$DIFF $ORIG $DEST 2>&1
done

echo "========== TEST: Multiple argument test ==========";
$DARWINUP install $PREFIX/root{,2,3}
LINES=$($DARWINUP list | wc -l)
if [ $LINES -lt 5 ]; then
	echo "Failed multiple argument test."
	exit 1;
fi
$DARWINUP uninstall all
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1


echo "========== TEST: Trying all roots at once and verifying ==============";
for R in $ROOTS;
do
	echo "INFO: Installing $R ...";
	$DARWINUP install $PREFIX/$R
done

$DARWINUP verify all
$DARWINUP files  all
$DARWINUP dump

echo "========== TEST: uninstall in reverse ==========";
for R in $ROOTS;
do
	UUID=$($DARWINUP list | head -3 | tail -1 | awk '{print $1}')
	echo "INFO: Uninstalling $UUID ...";
	$DARWINUP uninstall $UUID
done	
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Trying all roots at once, uninstall in install order by serial =========="
for R in $ROOTS;
do
        echo "INFO: Installing $R ...";
        $DARWINUP install $PREFIX/$R
done
for R in $ROOTS;
do
        UUID=$($DARWINUP list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        $DARWINUP uninstall $UUID
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Trying all roots at once, uninstall root2, root3, root by UUID =========="
for R in $ROOTS;
do
        echo "INFO: Installing $R ...";
        $DARWINUP install $PREFIX/$R
done
for R in root2 root3 root;
do
        UUID=$($DARWINUP list | grep $R$ | awk '{print $2}')
        echo "INFO: Uninstalling $UUID ...";
        $DARWINUP uninstall $UUID
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Trying roots in reverse, uninstall in install order =========="
for R in root3 root2 root;
do
        echo "INFO: Installing $R ...";
        $DARWINUP install $PREFIX/$R
done
for R in root3 root2 root;
do
        UUID=$($DARWINUP list | grep $R$ | awk '{print $1}')
        echo "INFO: Uninstalling $UUID ...";
        $DARWINUP uninstall $UUID
done
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: trying large roots ==========";
echo "INFO: installing 300files";
$DARWINUP install $PREFIX/300files.tbz2
$DARWINUP uninstall 300files.tbz2
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
echo "INFO: installing 300dir";
$DARWINUP install $PREFIX/300dirs.tbz2
$DARWINUP uninstall 300dirs.tbz2
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
echo "INFO: installing both 300files and 300dirs";
$DARWINUP install $PREFIX/300dirs.tbz2
$DARWINUP install $PREFIX/300files.tbz2
$DARWINUP uninstall 300dirs.tbz2
$DARWINUP uninstall 300files.tbz2
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Try uninstalling with user data in rollback =========="
echo "INFO: Installing root5 ...";
$DARWINUP install $PREFIX/root5
$DARWINUP install $PREFIX/root6
echo "modification" >> $DEST/d/file
$DARWINUP install $PREFIX/root7
$DARWINUP uninstall root6
$DARWINUP uninstall root5
$DARWINUP uninstall root7
stat $DEST/d/file
rm $DEST/d/file
rmdir $DEST/d
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Deep rollback while saving user data =========="
$DARWINUP install $PREFIX/deep-rollback.cpgz
echo "modified" >> $DEST/d1/d2/d3/d4/d5/d6/file
$DARWINUP install $PREFIX/deep-rollback.cpgz
$DARWINUP uninstall newest
$DARWINUP uninstall newest
stat $DEST/d1/d2/d3/d4/d5/d6/file
rm $DEST/d1/d2/d3/d4/d5/d6/file
rm -rf $DEST/d1
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

if [ $HASXAR -gt 0 ];
then
	$DARWINUP install $PREFIX/deep-rollback.cpgz
	$DARWINUP install $PREFIX/deep-rollback-2.xar ;
	$DARWINUP uninstall all
	echo "DIFF: diffing original test files to dest (should be no diffs) ..."
	$DIFF $ORIG $DEST 2>&1
fi

echo "========== TEST: Testing broken symlink handling =========="
$DARWINUP install $PREFIX/symlinks
$DARWINUP uninstall symlinks
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
$DARWINUP install $PREFIX/symlink_update
stat -L $DEST/broken
$DARWINUP uninstall newest
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Upgrades ============="
$DARWINUP install $PREFIX/root5
$DARWINUP upgrade $PREFIX/root5
$DARWINUP upgrade $PREFIX/root5
$DARWINUP upgrade $PREFIX/root5
C=$($DARWINUP list | grep root5 | wc -l | xargs)
test "$C" == "1" 
$DARWINUP uninstall oldest
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Try to upgrade with non-existent file ============="
$DARWINUP install $PREFIX/root5
mv $PREFIX/root5 $PREFIX/root5.tmp
set +e
$DARWINUP upgrade $PREFIX/root5
set -e
C=$($DARWINUP list | grep root5 | wc -l | xargs)
test "$C" == "1" 
mv $PREFIX/root5.tmp $PREFIX/root5
$DARWINUP uninstall oldest
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Superseded ============="
$DARWINUP install $PREFIX/root5
$DARWINUP install $PREFIX/root6
$DARWINUP install $PREFIX/root5
$DARWINUP install $PREFIX/root2
$DARWINUP install $PREFIX/root6
$DARWINUP install $PREFIX/root6
$DARWINUP install $PREFIX/root5
$DARWINUP list superseded
$DARWINUP uninstall superseded
C=$($DARWINUP list | grep root | wc -l | xargs)
test "$C" == "2" 
$DARWINUP uninstall oldest
$DARWINUP uninstall oldest
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1


echo "========== TEST: Archive Rename ============="
$DARWINUP install $PREFIX/root2
$DARWINUP install $PREFIX/root
$DARWINUP install $PREFIX/root6
$DARWINUP rename root "RENAME1"
C=$($DARWINUP list | grep "RENAME1" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "1" 
$DARWINUP rename oldest "RENAME2"
C=$($DARWINUP list | grep "RENAME2" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "1" 
$DARWINUP uninstall "RENAME1"
C=$($DARWINUP list | grep "RENAME1" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "0" 
C=$($DARWINUP files "RENAME2" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "17" 
C=$($DARWINUP verify "RENAME2" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "17"
$DARWINUP rename root6 RENAME3 RENAME3 RENAME4 RENAME4 RENAME5 RENAME5 RENAME6
C=$($DARWINUP list | grep "root6" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "0" 
C=$($DARWINUP list | grep "RENAME6" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "1" 
C=$($DARWINUP files "RENAME6" | grep -Ev '^Found' | wc -l | xargs)
test "$C" == "8"
$DARWINUP uninstall all
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Modify /System/Library/Extensions =========="
mkdir -p $DEST/System/Library/Extensions/Foo.kext
BEFORE=$(ls -Tld $DEST/System/Library/Extensions/ | awk '{print $6$7$8$9}');
sleep 2;
$DARWINUP install extension.tar.bz2
AFTER=$(ls -Tld $DEST/System/Library/Extensions/ | awk '{print $6$7$8$9}');
test $BEFORE != $AFTER
$DARWINUP uninstall newest
rm -rf $DEST/System
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1


echo "========== TEST: Forcing object change: file to directory ==========" 
is_file $DEST/rep_file
$DARWINUP -f install $PREFIX/rep_file_dir
is_dir $DEST/rep_file
is_file $DEST/rep_file/subfile
is_dir $DEST/rep_file/subdir
is_file $DEST/rep_file/subdir/subsubfile
$DARWINUP uninstall newest
is_file $DEST/rep_file
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Forcing object change: file to symlink ==========" 
is_file $DEST/rep_file
$DARWINUP -f install $PREFIX/rep_file_link
is_link $DEST/rep_file
$DARWINUP uninstall newest
is_file $DEST/rep_file
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Forcing object change: directory to file ==========" 
is_dir $DEST/rep_dir
is_file $DEST/rep_dir/subfile
is_dir $DEST/rep_dir/subdir
is_file $DEST/rep_dir/subdir/subsubfile
$DARWINUP -f install $PREFIX/rep_dir_file
is_file $DEST/rep_dir
$DARWINUP uninstall newest
is_dir $DEST/rep_dir
is_file $DEST/rep_dir/subfile
is_dir $DEST/rep_dir/subdir
is_file $DEST/rep_dir/subdir/subsubfile
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Forcing object change: directory to symlink ==========" 
is_dir $DEST/rep_dir
is_file $DEST/rep_dir/subfile
is_dir $DEST/rep_dir/subdir
is_file $DEST/rep_dir/subdir/subsubfile
$DARWINUP -f install $PREFIX/rep_dir_link
is_link $DEST/rep_dir
$DARWINUP uninstall newest
is_dir $DEST/rep_dir
is_file $DEST/rep_dir/subfile
is_dir $DEST/rep_dir/subdir
is_file $DEST/rep_dir/subdir/subsubfile
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Forcing object change: symlink->dir to file ==========" 
is_link $DEST/rep_link
$DARWINUP -f install $PREFIX/rep_link_file
is_file $DEST/rep_link
$DARWINUP uninstall newest
is_link $DEST/rep_link
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Forcing object change: symlink->dir to directory ==========" 
is_link $DEST/rep_link
$DARWINUP -f install $PREFIX/rep_link_dir
is_dir $DEST/rep_link
is_file $DEST/rep_link/anotherfile
is_dir $DEST/rep_link/anotherdir
is_file $DEST/rep_link/anotherdir/anothersubfile
$DARWINUP uninstall newest
is_link $DEST/rep_link
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Forcing object change: symlink->file to file ==========" 
is_link $DEST/rep_flink
$DARWINUP -f install $PREFIX/rep_flink_file
is_file $DEST/rep_flink
$DARWINUP uninstall newest
is_link $DEST/rep_flink
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1

echo "========== TEST: Forcing object change: symlink->file to directory ==========" 
is_link $DEST/rep_flink
$DARWINUP -f install $PREFIX/rep_flink_dir
is_dir $DEST/rep_flink
is_file $DEST/rep_flink/subfile
is_dir $DEST/rep_flink/subdir
is_file $DEST/rep_flink/subdir/subsubfile
$DARWINUP uninstall newest
is_link $DEST/rep_flink
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1



#
# The following are expected failures
#
echo "========== Expected Failures =========="
set +e

echo "========== TEST: testing early ditto failure ==========";

$DARWINUP install $PREFIX/corrupt.tgz | tee $PREFIX/corrupt.log
C=$(grep -c 'Rolling back' $PREFIX/corrupt.log)
test $C -eq 0
if [ $? -ne 0 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: testing recursive install guards ==========";
$DARWINUP install $PREFIX/depotroot.tar.gz
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi
$DARWINUP install $DEST
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi
darwinup $1 install /
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing File with Directory =========="
$DARWINUP install $PREFIX/rep_file_dir
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing File with Symlink =========="
$DARWINUP install $PREFIX/rep_file_link
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing Directory with Symlink =========="
$DARWINUP install $PREFIX/rep_dir_link
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing Directory with File =========="
$DARWINUP install $PREFIX/rep_dir_file
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing Symlink to directory with Directory =========="
$DARWINUP install $PREFIX/rep_link_dir
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing Symlink to directory with File =========="
$DARWINUP install $PREFIX/rep_link_file
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing Symlink to file with Directory =========="
$DARWINUP install $PREFIX/rep_flink_dir
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

echo "========== TEST: Try replacing Symlink to file with File =========="
$DARWINUP install $PREFIX/rep_flink_file
if [ $? -ne 255 ]; then exit 1; fi
echo "DIFF: diffing original test files to dest (should be no diffs) ..."
$DIFF $ORIG $DEST 2>&1
if [ $? -ne 0 ]; then exit 1; fi

popd >> /dev/null
echo "INFO: Done testing!"

