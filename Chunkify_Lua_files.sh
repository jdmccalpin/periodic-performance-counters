#!/bin/bash

if [ $# -ne 1 ]
then
	echo "Usage: $0 <lua_file_name>"
	exit
fi

# check to see if the file has already been processed
# by looking for the string CHUNK on the first line
# Note that 0 is TRUE and non-zero is FALSE
head -1 $1 | grep -q CHUNK
ALREADY_CHUNKED=$?
if [ $ALREADY_CHUNKED -eq 0 ]
then
	echo "It looks like this file has already been processed -- exiting"
	exit 1
fi

split --lines=100000 $1  CHUNK_

rm -f foo
for i in CHUNK_*
do
	echo "function $i()" >> foo
	cat $i >> foo
	echo "end" >> foo
	echo "$i()" >>foo
done
mv foo $1
rm -f CHUNK_*
