#!/bin/bash
#
# Run test suite for darwintrace
#
set -e
set -x
pushd $(dirname $0) >> /dev/null

PREFIX=/tmp/testing/darwintrace
LOGS=$PREFIX/logs
ROOT=$PREFIX/root
BIN=$PREFIX/bin

DARWINTRACE="/usr/local/share/darwinbuild/darwintrace.dylib"
export DYLD_FORCE_FLAT_NAMESPACE=1
export DYLD_INSERT_LIBRARIES=$DARWINTRACE
export DARWINTRACE_LOG="${LOGS}/trace.log"

echo "INFO: Cleaning up testing area ..."
rm -rf $PREFIX
mkdir -p $PREFIX
mkdir -p $LOGS
mkdir -p $BIN

cp realpath $BIN/realpath
REALPATH=$BIN/realpath

cp exec $BIN/exec
EXEC=$BIN/exec

cp close-test $BIN/close-test


echo "========== TEST: execve() Trace =========="
for FILE in cp echo chmod  date df expr hostname ls ps pwd test;
do
	# some of these commands will error out when run without arguments,
	#  so just ignore that since all we want is the execve() call to happen
	set +e	
	$EXEC /bin/$FILE 2>&1 >> /dev/null
	set -e
	LOGPAT="Python\[[0-9]+\][[:space:]]execve[[:space:]]/bin/${FILE}"
	C=$(grep -cE $LOGPAT $DARWINTRACE_LOG)
    test $C -eq 1
done
set -e

echo "========== TEST: close() Safety =========="
$BIN/close-test

echo "========== TEST: open() Trace =========="
for FILE in /System/Library/LaunchDaemons/*.plist;
do
	cat $FILE >> /dev/null;
	RP=$($REALPATH $FILE);
	LOGPAT="cat\[[0-9]+\][[:space:]]open[[:space:]]${RP}"
	C=$(grep -cE $LOGPAT $DARWINTRACE_LOG)
    test $C -eq 1
done

echo "========== TEST: readlink() Trace =========="
for FILE in $(find /System/Library/Frameworks/*Foundation.framework -type l | xargs);
do
	readlink $FILE
	LOGPAT="readlink\[[0-9]+\][[:space:]]readlink[[:space:]]${FILE}"
	C=$(grep -cE $LOGPAT $DARWINTRACE_LOG)
    test $C -eq 1
done


#echo "========== TEST: Redirection =========="
#cp $DARWINTRACE $ROOT/
#DARWINTRACE_REDIRECT="${ROOT}"
#DARWINTRACE_LOG="${LOGS}/Redirection.log"


popd >> /dev/null
echo "INFO: Done testing!"
