#!/bin/sh

set -e 

#ROOT_DIR=$(mktemp -dp /tmp -t zftest)
ROOT_DIR="/tmp/zosfiles.$$.$RANDOM"
mkdir -m 777 $ROOT_DIR
echo $ROOT_DIR # for test program to work
echo "Test data is in $ROOT_DIR" >&2 # to stderr for human to doublecheck

# test_dir
#    |
#    +--- dir1 <....................
#    |     |                       .
#    |     +--- file1 <.....       .
#    |                     .       .
#    +--- dir2             .       .
#          |               .       .
#          +--- file1 ......       .
#          |                       .
#          +--- file2              .
#          |                       .
#          +--- dir2_1             .
#                 |                .
#                 +--- file3       .
#                 |                .
#                 +--- dir1 ........
cd $ROOT_DIR

mkdir dir1
touch dir1/file1
mkdir dir2
touch dir2/file2
ln -s ../dir1/file1 dir2/
mkdir dir2/dir2_1/
touch dir2/dir2_1/file3
ln -s ../../dir1 dir2/dir2_1/

find . -exec ls -ld {} >&2 \; # to stderr for human to doublecheck