

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  dumptest1 — Read N lines from a file, then abend (S0C4).

  To get an SVC dump, pre-arm a SLIP trap before running:

      SLIP SET,COMP=0C4,ACTION=SVCD,JOBNAME=<yourjob>,END

  Usage:  dumptest1 <file> <linecount>
*/

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: dumptest1 <file> <linecount>\n");
    return 1;
  }

  const char *path = argv[1];
  int maxLines = atoi(argv[2]);

  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "dumptest1: cannot open %s\n", path);
    return 1;
  }

  char line[1024];
  int count = 0;
  while (fgets(line, sizeof(line), f) && count < maxLines) {
    count++;
    printf("%4d: %s", count, line);
  }
  fclose(f);

  printf("dumptest1: read %d lines, crashing now...\n", count);
  fflush(stdout);

  /* cause a real program check — write to protected storage */
  volatile char *bad = (volatile char *)0xFFCC;
  bad[0] = 1;

  /* not reached */
  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
