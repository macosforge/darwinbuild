#!/bin/sh

INDEX=0
COUNT=$#
for ARG in "$@" ; do
	INDEX=$(($INDEX + 1))
	if [ "$ARG" == "-c" -o "$ARG" == "-x" ]; then
		echo "Error: -c and -x are not supported by this ditto." 1>&2
		exit 1
	elif [ "${ARG:0:1}" != "-" -o "$ARG" == "--" ]; then
		break
	fi
done

DST=${@:$COUNT:1}
for SRC in ${@:$INDEX:$(($COUNT - $INDEX))} ; do
	if [ -d "$SRC" ]; then
		if [ ! -d "$DST" ]; then
			mkdir -p "$DST"
		fi
		tar cf - -C "$SRC" . | tar xpf - -C "$DST"
	elif [ -f "$SRC" ]; then
		cp -p "$SRC" "$DST"
	fi
done
