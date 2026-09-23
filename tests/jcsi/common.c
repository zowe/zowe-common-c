#include <stdlib.h>
#include <stdio.h>
#include "logging.h"
#include "jcsi.h"

// https://www.ibm.com/docs/en/zos/3.2.0?topic=directory-catalog-field-names
const char* const TESTFIELDS[] ={
  "NAME    ",
  "LRECL   ",
  "TYPE    ",
  "VOLSER  ",
  "VOLFLG  "
};
const int NTESTFIELDS = sizeof(TESTFIELDS)/sizeof(TESTFIELDS[0]);

// https://www.ibm.com/docs/en/zos/3.2.0?topic=fields-csidtyps-entry-types
char const TYPES[] = {
  'A', // non-VSAM data set
  'B', // Generation data group
  'C', // Cluster
  'G', // Alternate index
  'H', // Generation data set
  'L', // Tape volume catalog library entry
  'R', // VSAM path
  'U', // User catalog connector entry
  'W', // Tape volume catalog volume entry
  'X', // Alias -- commented out, too noisy
  'Z'  // Catalog control block data
};
const int NTYPES = sizeof(TYPES)/sizeof(TYPES[0]);

void initLoggings(int level) {
  LoggingContext *logContext = makeLoggingContext();
  logConfigureStandardDestinations(logContext);
  logConfigureComponent(NULL, LOG_COMP_RESTDATASET, "JCSI", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  logSetLevel(NULL, LOG_COMP_RESTDATASET, level);
}

void uninitLoggings() {
  removeLoggingContext();
}

void initCSI() {
  int loadcsi = loadCsi();
  if (loadcsi) {
    fprintf (stderr, "failed to load IGGCSI00, status = 0x%x\n", loadcsi);
    exit(loadcsi);
  }
}

const char* getCStringName(const char* name44) {
  static char cstrName[45];
  int i = 0;
  for (; i < 44 && cstrName[i] != ' '; i++) cstrName[i] = name44[i];
  cstrName[i] = '\0';
  return cstrName;
}
