@echo off

echo "********************************************************************************"
echo "Running Test"
echo ".."
ls ..
echo "../.."
ls ../../deps
echo "../../deps"
ls ../../deps

echo "Running convtest.c"
clang -I../h -I../platform/windows -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup -DYAML_DECLARE_STATIC=1 -Wdeprecated-declarations --rtlib=compiler-rt -o convtest.exe convtest.c ../c/charsets.c ../platform/windows/winfile.c ../c/timeutls.c ../c/utils.c ../c/alloc.c
