#!/bin/sh

if [ $# -ne 1 ]; then
	echo "Usage: $0 projects.txt" 1>&2
	exit 1
fi

PROJECTS="$1"

function SkipBuild() {
    local project="$1"
    for exclude in `darwinxref group nobuild`; do
	if [ "$project" = "$exclude" ]; then
	    return 0
	fi
    done
    return 1
}

cat "$PROJECTS" | while read proj; do
    SkipBuild $proj
    if [ $? -eq 0 ]; then
	continue
    fi
    echo -n "Building $proj..."
    mkdir -p WholeLogs
    darwinbuild -noload $proj > WholeLogs/$proj 2>&1
    if [ $? -eq 0 ]; then
	echo " done"
    else
	echo " FAILED"
    fi

    rm -rf BuildRoot/SourceCache
    rm -rf BuildRoot/var/tmp/$proj

done
