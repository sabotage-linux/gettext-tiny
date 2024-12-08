#!/bin/sh
#
# Written by Rich Felker, originally as part of musl libc.
# Multi-licensed under MIT, 0BSD, and CC0.
#
# This is an actually-safe install command which installs the new
# file atomically in the new location, rather than overwriting
# existing files.
#
# Enhanced by Haelwenn (lanodan) Monnier to support multiple src arguments

progname="$0"

usage() {
printf "usage: %s [-D] [-l] [-m mode] src... dest\n" "$progname" 1>&2
exit 1
}

install() {
	src="$1"
	dst="$2"
	tmp="$3"

	umask 077

	test -d "$dst" && {
		printf "%s: Destination file '%s' is a directory\n" "$progname" "$dst" 1>&2
		exit 1
	}
	test -d "$src" && {
		printf "%s: Source file '%s' is a directory\n" "$progname" "$src" 1>&2
		exit 1
	}

	set -C
	set -e

	trap 'rm -f "$tmp"' EXIT INT QUIT TERM HUP

	if test "$symlink" ; then
		ln -s "$src" "$tmp"
	else
		cat < "$src" > "$tmp"
		chmod "$mode" "$tmp"
	fi

	mv -f "$tmp" "$dst"
}

mkdirp=
symlink=
mode=755

while getopts Dlm: name ; do
case "$name" in
D) mkdirp=yes ;;
l) symlink=yes ;;
m) mode=$OPTARG ;;
?) usage ;;
esac
done
shift $(($OPTIND - 1))

test 1 -lt "$#" || usage

for dst in "$@" ; do :; done

dir=
test 2 -lt "$#" && dir="${dst}/"

set -C
set -e

while test 1 -lt "$#"; do
	src="$1"
	dst="$2"
	shift

	if test -n "$dir"; then
		dst="${dir}${src#*/}"
	else
		case "$dst" in
		*/) dst="${dst}${src#*/}" ;;
		esac
	fi

	if test "$mkdirp" ; then
	umask 022
	case "$dst" in
	*/*) mkdir -p "${dst%/*}" ;;
	esac
	fi

	tmp="$dst.tmp.$$.$#"

	install "${src:?}" "${dst:?}" "${tmp:?}"
done

exit 0
