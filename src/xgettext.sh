#!/bin/sh

outputfile=
outputdir=.

spliteq() {
	arg=$1
	echo "${arg#*=}"
	#alternatives echo "$arg" | cut -d= -f2-
	# or echo "$arg" | sed 's/[^=]*=//'
}

syntax() {
	printf "%s\n" "Usage: xgettext [OPTION] [INPUTFILE]..."
	exit 1
}

show_version() {
	printf "%s\n", "these are not (GNU gettext-tools) 99.9999.9999\n"
	exit 0
}

parsearg() {
	case $1 in
	#--files-from=*) readfile `spliteq "$1"`;;
	#-f) expectfilefrom=1;;
	--version) show_version;;
	-V) show_version;;
	--default-domain=*) : ;; #outputfiles=`spliteq "$1"`.po ;;
	-d) shift ;;
	--files-from=*) : ;;
	-f) shift ;;
	--directory=*) : ;;
	-D) shift ;;
	-o) shift ; outputfile="$1" ;;
	--output=*) outputfile=`spliteq "$1"` ;;
	--output-dir=*) outputdir=`spliteq "$1"` ;;
	-p) shift ; outputdir=`spliteq "$1"` ;;
	--language=*) : ;;
	-L) shift ;;
	--C) : ;;
	--c++) : ;;
	--from-code=*) : ;;
	--join-existing) : ;;
	-j) : ;;
	--exclude-file=*) : ;;
	-x) shift;;
	--add-comments=*) : ;;
	-cTAG) shift;;
	--add-comments) : ;;
	-c) : ;;
	--extract-all) : ;;
	-a) : ;;
	--keyword=*) : ;;
	-k*) : ;;
	--keyword) : ;;
	-k) : ;;
	--flag=*) : ;;
	--trigraphs) : ;;
	-T) : ;;
	--qt) : ;;
	--kde) : ;;
	--boost) : ;;
	--debug) : ;;
	--color) : ;;
	--color=*) : ;;
	--style=*) : ;;
	--no-escape) : ;;
	-e) : ;;
	--escape) : ;;
	-E) : ;;
	--force-po) force=1 ;;
	--indent) : ;;
	-i) : ;;
	--no-location) : ;;
	--add-location) : ;;
	-n) : ;;
	--strict) : ;;
	--properties-output) : ;;
	--stringtable-output) : ;;
	--width=*) : ;;
	-w) : ;;
	--no-wrap) : ;;
	--sort-output) : ;;
	-s) : ;;
	--sort-by-file) : ;;
	-F) : ;;
	--omit-header) : ;;
	--copyright-holder=*) : ;;
	--foreign-user) : ;;
	--package-name=*) : ;;
	--package-version=*) : ;;
	--msgid-bugs-address=*) : ;;
	--msgstr-prefix*) : ;;
	-m*) : ;;
	--msgstr-suffix*) : ;;
	-M*) : ;;
	--help) syntax ;;
	-h) syntax ;;
	*) syntax ;;
	esac
}

while true ; do
	case $1 in
	-*) parsearg "$1" ; shift ;;
	*) break ;;
	esac
done

[ -z "$outputfile" ] && outputfile=messages.po
[ "$outputfile" = "-" ] && exit 0
if [ "$force" = "1" ] ; then
	touch $outputdir/$outputfile
fi


