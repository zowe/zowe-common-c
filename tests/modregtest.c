
#include <metal/metal.h>
#include <metal/stdarg.h>
#include <metal/stddef.h>

#include "zowetypes.h"
#include "lpa.h"
#include "utils.h"
#include "modreg.h"

/*

### Build

Run build_modreg.sh; the module will be placed in `<tso_user>.LOADLIB`.

### Run

**IMPORTANT: running this test will consume common storage which is a limited
resource.**

* Make sure `<tso_user>.LOADLIB` is APF-authorized;
* Use the following JCL to run the program:

```
//MODREG   JOB  USER=&SYSUID,NOTIFY=&SYSUID
//TEST     EXEC PGM=MODREG
//STEPLIB  DD   DSNAME=&SYSUID..LOADLIB,DISP=SHR
//SYSPRINT DD   SYSOUT=*
```

*/

void zowelog(void *context, uint64 compID, int level, char *formatString, ...) {
  va_list argPointer;
  va_start(argPointer, formatString);
  vfprintf(NULL, formatString, argPointer);
  va_end(argPointer);
  printf("\n");
}

void zowedump(void *context, uint64 compID, int level, void *data,
              int dataSize) {
  dumpbuffer(data, dataSize);
}

int main() {

  MODREG_MARK_MODULE();

  LPMEA lpaInfo;
  int rc;
  uint64_t rsn;

  rc = modregRegister((EightCharString) {"STEPLIB "},
                      (EightCharString) {"MODREG  "},
                      &lpaInfo, &rsn);
  printf("modregRegister() -> %d\n", rc);
  if (rc != RC_MODREG_OK) {
    return 8;
  }

  rc = modregRegister((EightCharString) {"STEPLIB "},
                      (EightCharString) {"MODREG  "},
                      &lpaInfo, &rsn);
  printf("modregRegister() -> %d\n", rc);
  if (rc != RC_MODREG_ALREADY_REGISTERED) {
    return 8;
  }

  return 0;
}