#!/bin/sh

# expects input on stdin

TRACE_TYPES='\(execve\|open\)[[:space:]]\+'
DARWIN_BUILDROOT=$(pwd -P)

sort -u | \
    sed "s|$DARWIN_BUILDROOT/BuildRoot||" | \
    grep -i  "^$TRACE_TYPES/" | \
    grep -v "^$TRACE_TYPES/SourceCache/" | \
    grep -vi "^$TRACE_TYPES\(/private\)\?\(/var\)\?/tmp/" | \
    grep -vi  "^$TRACE_TYPES/XCD/" | \
    grep -vi  "^$TRACE_TYPES/dev/" | \
    sort -u
