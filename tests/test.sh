
WORKING_DIR=$(dirname "$0")

echo "********************************************************************************"
echo "Running Test"

date_stamp=$(date +%Y%m%d%S)

COMMON="../.."

QUICKJS="${COMMON}/deps/configmgr/quickjs"
QUICKJS_LOCATION="git@github.com:joenemo/quickjs-portable.git"
QUICKJS_BRANCH="main"

LIBYAML="${COMMON}/deps/configmgr/libyaml"
LIBYAML_LOCATION="git@github.com:yaml/libyaml.git"
LIBYAML_BRANCH="0.2.5"

DEPS="QUICKJS LIBYAML"

IFS=" "
for dep in ${DEPS}; do
  eval directory="\$${dep}"
  echo "Check if dir exist=$directory"
  if [ ! -d "$directory" ]; then
    eval echo Clone: \$${dep}_LOCATION @ \$${dep}_BRANCH to \$${dep}
    eval git clone --branch "\$${dep}_BRANCH" "\$${dep}_LOCATION" "\$${dep}"
  fi
done
IFS=$OLDIFS

echo "Running charsetguesser.c"
clang \
  -I "${LIBYAML}/include" \
  -I./src \
  -I../h \
  -I ../platform/windows \
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

echo "Running convtest.c"
clang \
  -I../h \
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
  
echo "Running jqtest.c"
clang \
  -I../platform/windows \
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

  
  ygsiudgus
  
  
  
