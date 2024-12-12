#ifdef METTLE
#include <metal/metal.h>
#include <metal/ctype.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include "ctype.h"
#include "stddef.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#endif

/*

Metal 64-bit:

xlc -S -M -qmetal -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"ZWE"' \
 -qreserved_reg=r12 -Wc,"arch(8),roconst,longname,lp64" -I ../h -\
 ../c/alloc.c \
 ../c/isgenq.c \
 ../c/qsam.c \
 ../c/metalio.c \
 ../c/timeutls.c \
 ../c/utils.c \
 ../c/zos.c \
 isgenqtest.c && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o alloc.o alloc.s && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o isgenq.o isgenq.s && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o qsam.o qsam.s && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o metalio.o metalio.s && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o timeutls.o timeutls.s && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o utils.o utils.s && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o zos.o zos.s && \
as -mgoff -mobject -mflag=nocont --TERM --RENT -o isgenqtest.o isgenqtest.s && \
_LD_SYSLIB="//'SYS1.CSSLIB'" ld -V -b rent -b case=mixed -b map -b xref \
 -b reus -e main -o "//'$USER.LOADLIB(ISGENQTS)'"  \
 alloc.o isgenq.o qsam.o metalio.o timeutls.o utils.o zos.o isgenqtest.o \
 > isgenqtest.lnk

LE 64-bit:

xlc "-Wc,LANGLVL(EXTC99),LP64,ASM,ASMLIB('SYS1.MACLIB'),ASMLIB('CEE.SCEEMAC')" \
-I ../h -o isgenqtest isgenqtest.c ../c/isgenq.c

*/

#include "zowetypes.h"
#include "isgenq.h"

#ifdef METTLE
static void sleep(int seconds) {
  int waitValue = seconds * 100;
  __asm(
      ASM_PREFIX
      "       STIMER WAIT,BINTVL=%[interval]                                   \n"
      :
      : [interval]"m"(waitValue)
      : "r0", "r1", "r14", "r15"
  );
}
#else
#include <unistd.h>
#endif


#define FIELD_SIZE(str, field) sizeof(((str *)0)->field)

static bool lockTimeValid(const char *value) {
  size_t valueLength = strlen(value);
  if (valueLength == 0 || valueLength > 4) {
    return false;
  }
  for (size_t i = 0; i < valueLength; i++) {
    if (!isdigit(value[i])) {
      return false;
    }
  }
  return true;
}

int main(int argc, char **argv) {

#ifndef METTLE

  if (argc < 5) {
    printf("error: bad parms, use > isgenqtest <qname> <rname> <x|s> <lock-time-sec>}\n");
    return 8;
  }

  const char *qnameStr = argv[1];
  size_t qnameLength = strlen(qnameStr);
  if (qnameLength > FIELD_SIZE(QName, value)) {
    printf("error: qname is too long\n");
    return 8;
  }

  const char *rnameStr = argv[2];
  size_t rnameLength = strlen(rnameStr);
  if (rnameLength > FIELD_SIZE(RName, value)) {
    printf("error: rname is too long\n");
    return 8;
  }

  char mode = argv[3][0];
  if (mode != 'x' && mode != 's') {
    printf("error: bad parms, use {x, s}\n");
    return 8;
  }

  const char *lockTimeStr = argv[4];
  if (!lockTimeValid(lockTimeStr)) {
    printf("error: bad lock time \'%s\'\n", lockTimeStr);
    return 8;
  }
  int lockTime = atoi(lockTimeStr);

  QName qname = {.value = "        "};
  RName rname;
  memcpy(qname.value, qnameStr, qnameLength);
  memcpy(rname.value, rnameStr, rnameLength);
  rname.length = rnameLength;

#else

  QName qname = {.value = "ZWE     "};
  RName rname = {.value = "ISGENQTEST", .length = strlen(rname.value)};
  char mode = 'x';
  int lockTime = 10;

#endif

  ENQToken token;

  int rc = 0, rsn = 0;

  if (mode == 'x') {

    rc = isgenqTestExclusiveLock(&qname, &rname, ISGENQ_SCOPE_SYSTEM, &rsn);
    printf("exclusive test rc = %d, rsn = %08X\n", rc, rsn);

    rc = isgenqTryExclusiveLock(&qname, &rname, ISGENQ_SCOPE_SYSTEM, &token, &rsn);
    printf("exclusive lock or fail rc = %d, rsn = %08X\n", rc, rsn);

    rc = isgenqGetExclusiveLock(&qname, &rname, ISGENQ_SCOPE_SYSTEM, &token, &rsn);
    printf("exclusive lock rc = %d, rsn = %08X\n", rc, rsn);

    sleep(lockTime);

  } else {

    rc = isgenqTestSharedLock(&qname, &rname, ISGENQ_SCOPE_SYSTEM, &rsn);
    printf("shared test rc = %d, rsn = %08X\n", rc, rsn);

    rc = isgenqTrySharedLock(&qname, &rname, ISGENQ_SCOPE_SYSTEM, &token, &rsn);
    printf("shared lock or fail rc = %d, rsn = %08X\n", rc, rsn);

    rc = isgenqGetSharedLock(&qname, &rname, ISGENQ_SCOPE_SYSTEM, &token, &rsn);
    printf("shared lock rc = %d, rsn = %08X\n", rc, rsn);

    sleep(lockTime);

  }

  rc = isgenqReleaseLock(&token, &rsn);
  printf("release rc = %d, rsn = %08X\n", rc, rsn);

  return 0;
}
