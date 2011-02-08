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
mkdir -p $ROOT
mkdir -p $LOGS
mkdir -p $BIN

REALPATH=$BIN/realpath
cp realpath $REALPATH

EXEC=$BIN/exec
cp exec $EXEC

CLOSETEST=$BIN/close-test
cp close-test $CLOSETEST

REDIRECTIONTEST=$BIN/redirection-test
cp redirection-test $REDIRECTIONTEST


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
$CLOSETEST

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

echo "========== TEST: Redirection =========="
mkdir -p $ROOT/$PREFIX
mkdir -p $ROOT/usr/lib
cp /usr/lib/libSystem.B.dylib $ROOT/usr/lib/libSystem.B.dylib
mkdir -p $ROOT/bin
cp /bin/cat $ROOT/bin/cat
echo "Outside of root" > $PREFIX/datafile
echo "Inside of root" > $ROOT/$PREFIX/datafile
export DARWINTRACE_REDIRECT="${ROOT}"
export DARWIN_BUILDROOT="${ROOT}"
$REDIRECTIONTEST $PREFIX
unset DARWINTRACE_REDIRECT
unset DARWIN_BUILDROOT
# test that execve(/bin/cat) was redirected
RP=$($REALPATH ${ROOT}/bin/cat)
LOGPAT="bash\[[0-9]+\][[:space:]]execve[[:space:]]${RP}"
C=$(grep -cE $LOGPAT $DARWINTRACE_LOG)
test $C -eq 1
# test that open(/tmp/.../datafile) does not get redirected
#  since /tmp/ is one of the redirection exceptions
RP=$($REALPATH ${PREFIX}/datafile)
LOGPAT="cat\[[0-9]+\][[:space:]]open[[:space:]]${RP}"
C=$(grep -cE $LOGPAT $DARWINTRACE_LOG)
test $C -eq 1

popd >> /dev/null
echo "INFO: Done testing!"
