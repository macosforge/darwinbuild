#!/bin/bash
set -e
set -x
pushd $(dirname $0) >> /dev/null

for X in *;
do
	$X/run-tests.sh
done
