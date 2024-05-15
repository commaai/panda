#!/bin/bash

OPTIONS="-A8 -t8 --lineend=linux"

ASTYLE=$(which astyle)
if [ $? -ne 0 ]; then
	echo "[!] astyle not installed. Unable to check source file format policy." >&2
	exit 1
fi

RETURN=0
git diff --cached --name-status --diff-filter=ACMR |
{
	# Command grouping to workaround subshell issues. When the while loop is
	# finished, the subshell copy is discarded, and the original variable
	# RETURN of the parent hasn't changed properly.
	while read STATUS FILE; do
	if [[ "$FILE" =~ ^.+(c|cpp|h)$ ]]; then
		$ASTYLE $OPTIONS < $FILE > $FILE.beautified
		md5sum -b $FILE | { read stdin; echo $stdin.beautified; } | md5sum -c --status -
		if [ $? -ne 0 ]; then
			echo "[!] $FILE does not respect the agreed coding standards." >&2
			RETURN=1
		fi
		rm $FILE.beautified
	fi
	done

	if [ $RETURN -eq 1 ]; then
		echo ""
		echo "Make sure you have run astyle with the following options:" >&2
		echo $OPTIONS >&2
	fi

	exit $RETURN
}