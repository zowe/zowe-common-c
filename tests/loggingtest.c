#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <pthread.h>
#include <inttypes.h>

/*
 * Test that logging is functional with and without new helper macros.
 */
void main() {
  LoggingContext *context = makeLoggingContext();
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


