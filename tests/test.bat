@echo off

echo ********************************************************************************

set LIBYAML="../../deps/configmgr/libyaml"
set LIBYAML_LOCATION="git@github.com:yaml/libyaml.git"
set LIBYAML_BRANCH="0.2.5"

set DEPS="QUICKJS LIBYAML"





set LIBYAML=../../deps
set LIBYAML_LOCATION=git@github.com:yaml/libyaml.git
set LIBYAML_BRANCH="0.2.5"


echo %LIBYAML%

echo "Running convtest.c"

clang -I../h \
  -I../platform/windows \
  -D_CRT_SECURE_NO_WARNINGS \
  -Dstrdup=_strdup \
  -DYAML_DECLARE_STATIC=1 \
  -Wdeprecated-declarations \
  --rtlib=compiler-rt \
  -o convtest.exe \
  convtest.c \
  ../c/charsets.c \
  ../platform/windows/winfile.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c
      
echo "Running charsetguesser.c"

clang -I../../deps/include \
  -I./src -I../h -I \
  ../platform/windows\
  -Dstrdup=_strdup \
  -D_CRT_SECURE_NO_WARNINGS \
  -o charsetguesser.exe \
  charsetguesser.c \
  ../c/xlate.c \
  ../c/charsets.c \
  ../c/winskt.c \
  ../c/logging.c \
  ../c/collections.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c 

echo "Running jqtest.c"

clang -I../platform/windows \
  -I ..\h -Wdeprecated-declarations \
  -D_CRT_SECURE_NO_WARNINGS \
  -o jqtest.exe jqtest.c \
  ../c/microjq.c \
  ../c/parsetools.c \
  ../c/json.c \
  ../c/xlate.c \
  ../c/charsets.c \
  ../c/winskt.c \
  ../c/logging.c \
  ../c/collections.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c
          

echo "Running jsonmergetest.c"

clang -I./src \
  -I../h -I ../platform/windows \
  -Dstrdup=_strdup \
  -D_CRT_SECURE_NO_WARNINGS \
  -o jsonmergetest.exe \
  jsonmergetest.c \
  ../c/json.c \
  ../c/xlate.c \
  ../c/charsets.c \
  ../c/winskt.c \
  ../c/logging.c \
  ../c/collections.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c

echo "Running jstest.c"

clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

clang -I ../../quickjs/porting \
  -I../../deps/include \
  -I../platform/windows \
  -I ../../quickjs \
  -I ..\h -DCONFIG_VERSION=\"2021-03-27\" \
  -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup \
  -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 \
  -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" \
  -DYAML_DECLARE_STATIC=1 -Wdeprecated-declarations \
  --rtlib=compiler-rt -o jstest.exe jstest.c \
  ../../quickjs/quickjs.c \
  ../../quickjs/cutils.c \
  ../../quickjs/quickjs-libc.c \
  ../../quickjs/libbf.c \
  ../../quickjs/libregexp.c \
  ../../quickjs/libunicode.c \
  ../../quickjs/porting\winpthread.c \
  ../../quickjs/porting\wintime.c \
  ../../quickjs/porting\windirent.c \
  ../../quickjs/porting\winunistd.c \
  ../../quickjs/repl.c \
  ../../deps/src/api.c \
  ../../deps/src/reader.c \
  ../../deps/src/scanner.c \
  ../../deps/src/parser.c \
  ../../deps/src/loader.c \
  ../../deps/src/writer.c \
  ../../deps/src/emitter.c \
  ../../deps/src/dumper.c \
  ../c/yaml2json.c \
  ../c/embeddedjs.c \
  ../c/jsonschema.c \
  ../c/json.c \
  ../c/xlate.c \
  ../c/charsets.c \
  ../c/winskt.c \
  ../c/logging.c \
  ../c/collections.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c \
  cppregex.o \
  winregex.o \
  ../platform/windows/winfile.c


echo "Running parsetest.c"

clang -I../h \
  -I../platform/windows \
  -D_CRT_SECURE_NO_WARNINGS \
  -Dstrdup=_strdup \
  -DYAML_DECLARE_STATIC=1 \
  -Wdeprecated-declarations \
  --rtlib=compiler-rt \
  -o parsetest.exe parsetest.c \
  ../c/parsetools.c \
  ../c/json.c \
  ../c/winskt.c \
  ../c/xlate.c \
  ../c/charsets.c \
  ../c/collections.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c


echo "Running pattest.c"

clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

clang -I../../deps/include \
  -I./src \
  -I../h \
  -I ../platform/windows \
  -Dstrdup=_strdup \
  -D_CRT_SECURE_NO_WARNINGS \
  -DYAML_VERSION_MAJOR=0 \
  -DYAML_VERSION_MINOR=2 \
  -DYAML_VERSION_PATCH=5 \
  -DYAML_VERSION_STRING=\"0.2.5\" \
  -DYAML_DECLARE_STATIC=1 \
  -o pattest.exe pattest.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c \
  cppregex.o \
  winregex.o


echo "Running qjstest.c"

clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp
clang -I ../../quickjs/porting \
  -I../../deps/include \
  -I ../../quickjs \
  -I./src \
  -I../h \
  -I ../platform/windows \
  -DCONFIG_VERSION=\"2021-03-27\" \
  -DCONFIG_BIGNUM=1 \
  -Dstrdup=_strdup \
  -D_CRT_SECURE_NO_WARNINGS \
  -DYAML_VERSION_MAJOR=0 \
  -DYAML_VERSION_MINOR=2 \
  -DYAML_VERSION_PATCH=5 \
  -DYAML_VERSION_STRING=\"0.2.5\" \
  -DYAML_DECLARE_STATIC=1 \
  --rtlib=compiler-rt \
  -o qjstest.exe \
  qjstest.c ../c/embeddedjs.c \
  ../../quickjs/quickjs.c \
  ../../quickjs/cutils.c \
  ../../quickjs/quickjs-libc.c \
  ../../quickjs/libbf.c \
  ../../quickjs/libregexp.c \
  ../../quickjs/libunicode.c \
  ../../quickjs/porting\winpthread.c \
  ../../quickjs/porting\wintime.c \
  ../../quickjs/porting\windirent.c \
  ../../quickjs/porting\winunistd.c \
  ../../deps/src/api.c \
  ../../deps/src/reader.c \
  ../../deps/src/scanner.c \
  ../../deps/src/parser.c \
  ../../deps/src/loader.c \
  ../../deps/src/writer.c \
  ../../deps/src/emitter.c \
  ../../deps/src/dumper.c \
  ../c/yaml2json.c ../c/microjq.c \
  ../c/parsetools.c ../c/jsonschema.c \
  ../c/json.c \
  ../c/xlate.c \
  ../c/charsets.c \
  ../c/winskt.c \
  ../c/logging.c \
  ../c/collections.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c \
  ../platform/windows/winfile.c \
  cppregex.o \
  winregex.o


echo "Running schematest.c"

clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp
clang -I../../deps/include \
  -I./src \
  -I../h \
  -I ../platform/windows \
  -Dstrdup=_strdup \
  -D_CRT_SECURE_NO_WARNINGS \
  -DYAML_VERSION_MAJOR=0 \
  -DYAML_VERSION_MINOR=2 \
  -DYAML_VERSION_PATCH=5 \
  -DYAML_VERSION_STRING=\"0.2.5\" \
  -DYAML_DECLARE_STATIC=1 \
  -o schematest.exe schematest.c \
  ../../deps/src/api.c \
  ../../deps/src/reader.c \
  ../../deps/src/scanner.c \
  ../../deps/src/parser.c \
  ../../deps/src/loader.c \
  ../../deps/src/writer.c \
  ../../deps/src/emitter.c \
  ../../deps/src/dumper.c \
  ../c/yaml2json.c \
  ../c/jsonschema.c \
  ../c/json.c \
  ../c/xlate.c \
  ../c/charsets.c \
  ../c/winskt.c \
  ../c/logging.c \
  ../c/collections.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c \
  cppregex.o \
  winregex.o


echo "Running setkeytest.c"

xlclang -q64 \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DSUBPOOL=132 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  "-Wl,ac=1" \
  -I ../h \
  -I ../platform/posix \
  -Wbitwise-op-parentheses \
  -o setkeytest \
  setkeytest.c \
  ../c/zos.c \
  ../c/timeutls.c \
  ../c/utils.c \
  ../c/alloc.c 


