#!/bin/bash

KERNEL_INCLUDE="../include"
LIB_INCLUDE="include"

STRING_LIB="../lib/str/string.O"
OS047_LIB="lib/lib.a"

LIBS="$STRING_LIB $OS047_LIB"

LINKFILE="link.ld"
LDFLAGS="-nostdlib -m elf_i386 -T${LINKFILE}"
LD=ld

CFLAGS="-m32 -I${KERNEL_INCLUDE} -I${LIB_INCLUDE} -O0 -g3 -Wall -c -nostdlib -nostdinc -fno-builtin \
	  -fno-stack-protector -nostartfiles -nodefaultlibs -D_USER_PROGRAM"
CC=gcc
files=""

OUTFILE=$1
if [ -f "$OUTFILE" ]; then
	echo "dosya var: $OUTFILE"
	exit
fi

if [ $# -lt 2 ]; then
	echo "hatali parametre"
	echo "kullanim: $0 PROGRAM kod.c [kod2.c ...]"
	exit
fi

for file in "${@:2}"; do
	fileo="$(basename $file .c).o"
	files="$files $fileo"

	CMD="$CC $CFLAGS -c $file -o $fileo"
	echo $CMD
	$CMD
done

CMD="$LD $LDFLAGS -o $OUTFILE $fileo $LIBS"
echo $CMD
$CMD
