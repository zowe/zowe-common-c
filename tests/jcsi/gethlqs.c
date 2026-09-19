#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

#include "zowetypes.h"
#include "logging.h"
#include "alloc.h"
#include "jcsi.h"
#include "common.h"

#define NCSIPARMBLKS 29

#ifdef TRACK_MEMORY
extern int safeBytes;
extern int rawBytes;
extern int k8Bytes;
#endif

typedef csi_parmblock * __ptr32 csi_parmblock_ptr32;
typedef csi_parmblock_ptr32 * __ptr32 csi_parmblock_array_ptr32;

int main(int argc, char* argv[]) {
  initCSI();
  csi_parmblock_array_ptr32 csi_parms_array = (csi_parmblock_array_ptr32)safeMalloc31(sizeof(csi_parmblock_ptr32) * NCSIPARMBLKS, "csi_parms_array");
  EntryDataSet* entrySets = getHLQs((char*)TYPES, NTYPES, 0, (char**)TESTFIELDS, NTESTFIELDS, csi_parms_array);
  printf("flags    type    name\n");
  for (int i = 0; i < entrySets->length; i++) {
    EntryData *entry = entrySets->entries[i];
    printf("   %2X       %c    %s\n", entry->flags, entry->type, getCStringName(entry->name));
  }
  freeEntryDataSet(entrySets);
  for (int i = 0; i < NCSIPARMBLKS; i++) {
    safeFree((char*)csi_parms_array[i], sizeof(csi_parmblock));
  }
  safeFree((char*)csi_parms_array, sizeof(csi_parmblock_ptr32) * NCSIPARMBLKS);
#ifdef TRACK_MEMORY
  printf("memory tracking values when exiting:\n"
         "  safeBytes: %d\n"
         "  rawBytes: %d\n"
         "  k8Bytes: %d\n",
         safeBytes, rawBytes, k8Bytes);
  showOutstanding();
#endif
  return 0;
}