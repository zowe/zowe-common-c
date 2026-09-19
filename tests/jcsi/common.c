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

/*
  Fake or simplified stub functions --- they are part of zowe-common-c but unrelated to/unused by the
  current unit tests. These fake stub functions can noticeably simplify the Makefile.
*/
void abortIfUnsupportedCAA() {
}

char *getCAA(void) {
  return "";
}

void zowelog(LoggingContext *context, uint64 compID, int level, char *formatString, ...){
  va_list ap;
  va_start(ap, formatString);
  vprintf(formatString, ap);
  va_end(ap);
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
  for (; i < 44; i++) cstrName[i] = name44[i];
  cstrName[i] = '\0';
  return cstrName;
}
