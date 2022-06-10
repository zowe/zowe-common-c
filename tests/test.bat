@echo off

echo "********************************************************************************"

set LIBYAML="../../deps/configmgr/libyaml"
set LIBYAML_LOCATION="git@github.com:yaml/libyaml.git"
set LIBYAML_BRANCH="0.2.5"

set DEPS="QUICKJS LIBYAML"

echo ".."
ls ..
echo "../.."
ls ../..
echo "../../.."
ls ../../..




LIBYAML="$../deps"
LIBYAML_LOCATION="git@github.com:yaml/libyaml.git"
LIBYAML_BRANCH="0.2.5"

Rem git clone --branch 0.2.5 git@github.com:yaml/libyaml.git ../../deps/configmgr/libyaml

echo "Running convtest.c"
clang -I../h -I../platform/windows -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup -DYAML_DECLARE_STATIC=1 -Wdeprecated-declarations --rtlib=compiler-rt -o convtest.exe convtest.c ../c/charsets.c ../platform/windows/winfile.c ../c/timeutls.c ../c/utils.c ../c/alloc.c

echo "Running charsetguesser.c"
clang -I%LIBYAML%/include -I./src -I../h -I ../platform/windows -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -o charsetguesser.exe charsetguesser.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c 
