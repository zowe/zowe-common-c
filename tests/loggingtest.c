#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <pthread.h>
#include <inttypes.h>

/*
 BUILD
        c89 -D_OPEN_THREADS -D_XOPEN_SOURCE=600 -DAPF_AUTHORIZED=0 -DNOIBMHTTP \
        "-Wa,goff" "-Wc,langlvl(EXTC99),float(HEX),agg,list(),so(),search(),\
        goff,xref,gonum,roconst,gonum,asm,asmlib('SYS1.MACLIB'),asmlib('CEE.SCEEMAC')" \
        -I ../h \
        -o loggingtest \
        ./loggingtest.c \
        ../c/utils.c \
        ../c/logging.c \
        ../c/alloc.c \
        ../c/collections.c \
        ../c/zos.c \
        ../c/timeutls.c \
        ../c/le.c \
        ../c/scheduling.c \
        ../c/recovery.c

 */

/*
 * Test that logging is functional with and without new helper macros. Prove backwards compatibility.
 */
void main() {
  LoggingContext *context = NULL;
  //LoggingContext *context = makeLoggingContext();
  logConfigureStandardDestinations(context);
  logConfigureComponent(context, LOG_COMP_DATASERVICE, "DATASERVICE", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);

  ZOWELOG_SEVERE(LOG_COMP_DATASERVICE, "Hello from zowelog2(). This log should have every field.");
  zowelog(context, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO, "Hello from zowelog(). This log should lack fileLocation information.");

  LoggingContext *context2 = makeLoggingContext();
  /* Using outdated logConfigureDestination. */
  logConfigureDestination(context2, LOG_DEST_PRINTF_STDOUT,"printf(stdout)", NULL, printStdout);
  logConfigureComponent(context2, LOG_COMP_DATASERVICE, "DATASERVICE", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  zowelog(context2, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO, "Hello from zowelog(). This log should lack level and fileLocation information.");
}


