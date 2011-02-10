#!/bin/bash
set -e
pushd $(dirname $0) >> /dev/null

for X in *;
do
	if [ -d $X ]; then
		$X/run-tests.sh
	fi
done

echo "INFO: All testing completed!"
