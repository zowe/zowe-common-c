/*
  sapitest - Standalone SAPI (SSI 79) spool reader.

  Based on the Xephon/CBT model documented in zos_sapi_dynalloc_spool_guide.md.
  No dependency on zowe-common-c. Uses IBM system headers directly.

  Usage:
    sapitest <jobname> <jobid>

  Example:
    sapitest SIMPLJOB JOB06658
*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* IBM system headers */
#include "iefssobh.h"
#include "iefjssib.h"
#include "iazsss2.h"

/* ----------------------------------------------------------------
   Inline SVC 99 for dynamic allocation.
   Everything below the bar, all __ptr32.
   ---------------------------------------------------------------- */

#pragma pack(packed)
typedef struct {
  short key;
  short count;
  short parmLen;
  /* followed by parmLen bytes of data */
} TU;
#pragma pack(reset)

/* Allocate below-the-bar storage */
static void * __ptr32 malloc31(size_t size) {
  void * __ptr32 p;
  /* __malloc31 is the IBM LE function for 31-bit storage */
  p = (void * __ptr32)__malloc31(size);
  return p;
}

/* Build a simple text unit: key + 1 parameter of given length */
static TU * __ptr32 buildTU(int key, const void *data, int dataLen) {
  int totalLen = 6 + dataLen;
  TU * __ptr32 tu = (TU * __ptr32)malloc31(totalLen);
  if (!tu) return NULL;
  memset(tu, 0, totalLen);
  tu->key = (short)key;
  tu->count = (data && dataLen > 0) ? 1 : 0;
  tu->parmLen = (short)dataLen;
  if (data && dataLen > 0) {
    memcpy((char *)tu + 6, data, dataLen);
  }
  return tu;
}

/* Build a 1-byte text unit */
static TU * __ptr32 buildTU1(int key, unsigned char val) {
  return buildTU(key, &val, 1);
}

#define DALDSNAM 0x0002
#define DALSTATS 0x0004
#define DALRTDDN 0x0055
#define DALSSREQ 0x005C
#define DALUASSR 0x0075
#define DALBRTKN 0x006E
#define DISP_SHR 0x08

#pragma pack(packed)
typedef struct {
  /* S99RB - 20 bytes */
  unsigned char rbLen;
  unsigned char verbCode;
  unsigned short flags1;
  unsigned short errorCode;
  unsigned short infoCode;
  TU * __ptr32 * __ptr32 tuListPtr;
  void * __ptr32 reserved;
  int flags2;
} S99RB;
#pragma pack(reset)

static int doSVC99(S99RB * __ptr32 rb) {
  int rc = 0;
  /* S99RBPTR must be in 31-bit storage: address of RB with HOB set */
  void * __ptr32 * __ptr32 parmWord =
      (void * __ptr32 * __ptr32)malloc31(4);
  if (!parmWord) return -1;
  *parmWord = (void * __ptr32)((unsigned int)rb | 0x80000000);

  __asm(
      " LA 1,0(,%[pw])  \n"
      " SVC 99           \n"
      " ST 15,%[rc]      \n"
      : [rc]"=m"(rc)
      : [pw]"r"(parmWord)
      : "r0", "r1", "r14", "r15"
  );
  free(parmWord);
  return rc;
}

/*
  Allocate a spool dataset for browse using SDSB.
  dsn      - dataset name from SSS2DSN (EBCDIC, null-terminated)
  btokAddr - address from SSS2BTOK (opaque JES text unit)
  ssname   - subsystem name, e.g. "JES2"
  ddOut    - 9-byte buffer for returned DD name

  Text units per IBM doc:
    DALDSNAM - dataset name
    DALSTATS - SHR (0x08)
    DALSSREQ - subsystem name
    DALBRTKN - browse token from SSS2BTOK
    DALRTDDN - return DD name
*/
static int sdsbAlloc(const char *dsn, void * __ptr32 btokAddr,
                     const char *ssname, char *ddOut,
                     int *errOut, int *infoOut) {

  /* 5 text unit pointers + S99RB, all below the bar */
  int rbSize = sizeof(S99RB);
  int ptrListSize = 5 * sizeof(TU * __ptr32);
  int totalSize = rbSize + ptrListSize;

  char * __ptr32 block = (char * __ptr32)malloc31(totalSize);
  if (!block) return -1;
  memset(block, 0, totalSize);

  S99RB * __ptr32 rb = (S99RB * __ptr32)block;
  TU * __ptr32 * __ptr32 tuList =
      (TU * __ptr32 * __ptr32)(block + rbSize);

  rb->rbLen = 20;
  rb->verbCode = 0x01;  /* allocate */
  rb->tuListPtr = tuList;

  /* Build text units */
  int dsnLen = strlen(dsn);
  tuList[0] = buildTU(DALDSNAM, dsn, dsnLen);

  tuList[1] = buildTU1(DALSTATS, DISP_SHR);

  tuList[2] = buildTU(DALUASSR, ssname, strlen(ssname));

  /* DALBRTKN: the opaque JES browse token — pass address directly */
  tuList[3] = (TU * __ptr32)btokAddr;

  /* DALRTDDN: 8-byte blank buffer for returned DD name */
  char blanks[8];
  memset(blanks, ' ', 8);
  tuList[4] = buildTU(DALRTDDN, blanks, 8);

  /* HOB on last entry */
  tuList[4] = (TU * __ptr32)((unsigned int)tuList[4] | 0x80000000);

  /* Diagnostics */
  for (int i = 0; i < 5; i++) {
    TU * __ptr32 t = (TU * __ptr32)((unsigned int)tuList[i] & 0x7FFFFFFF);
    printf("  SDSB tu[%d] @%08X key=%04X cnt=%d plen=%d\n",
           i, (unsigned int)t, (unsigned)t->key,
           (int)t->count, (int)t->parmLen);
  }
  printf("  SDSB rb @%08X tuList @%08X\n",
         (unsigned int)rb, (unsigned int)tuList);
  fflush(stdout);

  int rc = doSVC99(rb);

  *errOut = (int)rb->errorCode;
  *infoOut = (int)rb->infoCode;

  printf("  SDSB SVC99 rc=%d error=%04X info=%04X\n",
         rc, rb->errorCode, rb->infoCode);
  fflush(stdout);

  if (rc == 0) {
    /* Extract returned DD name */
    TU * __ptr32 ddtu = (TU * __ptr32)
        ((unsigned int)tuList[4] & 0x7FFFFFFF);
    memcpy(ddOut, (char *)ddtu + 6, 8);
    ddOut[8] = '\0';
  }

  /* Free our text units (0, 1, 2, 4) — not 3, that's JES-owned */
  free((void *)((unsigned int)tuList[0]));
  free((void *)((unsigned int)tuList[1]));
  free((void *)((unsigned int)tuList[2]));
  free((void *)((unsigned int)tuList[4] & 0x7FFFFFFF));
  free(block);

  return rc;
}

/* ----------------------------------------------------------------
   SAPI: issue IEFSSREQ (SSI function 79)
   The SSS2 block is caller-owned and reused across calls.

   Calling convention (from working ssi.c in zowe-common-c):
     - parm list + 72-byte save area in 31-bit storage
     - LLGF for parm pointer, LLGT for CVT
     - SAM31 before BASR, SAM64 after
     - JESSSREQ is at JESCT+20 (not 24)
     - R13 must point to save area for IEFSSREQ linkage
   ---------------------------------------------------------------- */

static int callSAPI(struct sss2 * __ptr32 sss2Block) {
  /* Build SSOB + SSIB below the bar */
  int allocSize = sizeof(struct ssob) + sizeof(struct ssib);
  char * __ptr32 block = (char * __ptr32)malloc31(allocSize);
  if (!block) return -1;
  memset(block, 0, allocSize);

  struct ssob * __ptr32 ssob = (struct ssob * __ptr32)block;
  struct ssib * __ptr32 ssib = (struct ssib * __ptr32)(block + sizeof(struct ssob));

  /* SSIB */
  memcpy(ssib->ssibid, "\xE2\xE2\xC9\xC2", 4);  /* "SSIB" EBCDIC */
  ssib->ssiblen = sizeof(struct ssib);
  memcpy(ssib->ssibssnm, "\xD1\xC5\xE2\xF2", 4); /* "JES2" EBCDIC */

  /* SSOB */
  memcpy(ssob->ssobid, "\xE2\xE2\xD6\xC2", 4);  /* "SSOB" EBCDIC */
  ssob->ssoblen = sizeof(struct ssob);
  ssob->ssobfunc = SSOBSOU2;  /* function 79 */
  ssob->ssobssib = (struct ssib * __ptr32)ssib;
  ssob->ssobindv = (void * __ptr32)sss2Block;

  /* Parm list (4 bytes) + 72-byte save area, all below the bar */
  char * __ptr32 data31 = (char * __ptr32)malloc31(72 + 4);
  if (!data31) { free(block); return -1; }

  char * __ptr32 parmlist = (char * __ptr32)&data31[0];
  char * __ptr32 saveArea = (char * __ptr32)&data31[4];
  *((int *)parmlist) = ((int)(unsigned int)ssob) | 0x80000000;
  memset(saveArea, 0, 72);

  /* Save R13, set up back chain */
  int r13;
  __asm(" ST 13,%0 " : : "m"(r13) :);
  ((int *)saveArea)[1] = r13;  /* back chain */

  int returnCode = 0;
  __asm(
      " SYSSTATE ARCHLVL=2,AMODE64=YES \n"
      " IEABRCX DEFINE                 \n"
      " LLGF 1,%0                      \n"
      " L    13,%1                     \n"
      " LLGT 15,16(0,0)               \n"  /* CVT */
      " L    15,296(0,15)             \n"  /* CVTJESCT */
      " L    15,20(0,15)              \n"  /* JESSSREQ (offset 20) */
      " SAM31                          \n"
      " BASR 14,15                     \n"
      " SAM64                          \n"
      " ST   15,%2                     \n"
      :
      : "m"(parmlist), "m"(saveArea), "m"(returnCode)
      : "r15"
  );

  printf("callSAPI: IEFSSREQ rc=%d\n", returnCode);
  fflush(stdout);

  int funcRC = (int)(intptr_t)ssob->ssobretn;

  free(data31);
  free(block);
  return funcRC;
}

/* ----------------------------------------------------------------
   Main
   ---------------------------------------------------------------- */

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: sapitest [-list] <jobname> [jobid] [owner]\n");
    fprintf(stderr, "  -list: enumerate only, skip DYNALLOC\n");
    fprintf(stderr, "  e.g. sapitest SIMPLJOB JOB06658 JDEVLIN\n");
    return 1;
  }

  int listOnly = 0;
  int argStart = 1;
  if (argc >= 2 && strcmp(argv[1], "-list") == 0) {
    listOnly = 1;
    argStart = 2;
  }
  if (argStart >= argc) {
    fprintf(stderr, "Missing jobname\n");
    return 1;
  }

  const char *jobName = argv[argStart];
  const char *jobId = (argc > argStart + 1) ? argv[argStart + 1] : NULL;
  const char *owner = (argc > argStart + 2) ? argv[argStart + 2] : NULL;

  printf("sapitest: jobname='%s' jobid='%s' owner='%s'\n",
         jobName, jobId ? jobId : "(none)", owner ? owner : "(none)");
  fflush(stdout);

  /* Allocate SSS2 below the bar */
  struct sss2 * __ptr32 sss2 =
      (struct sss2 * __ptr32)malloc31(SSS2SIZE);
  if (!sss2) {
    fprintf(stderr, "malloc31 failed for SSS2\n");
    return 1;
  }

  /* Zero once (rule 3.2) */
  memset(sss2, 0, SSS2SIZE);

  /* Initialize header */
  sss2->sss2len = SSS2SIZE;
  sss2->sss2ver = SSS2VJCR;
  memcpy(sss2->sss2eye, "\xE2\xE2\xE2\xF2", 4);  /* "SSS2" EBCDIC */
  sss2->sss2type = SSS2PUGE;

  /* Selection: jobname */
  memset(sss2->sss2jobn, 0x40, 8);
  int nameLen = strlen(jobName);
  if (nameLen > 8) nameLen = 8;
  memcpy(sss2->sss2jobn, jobName, nameLen);
  sss2->sss2sjbn = 1;

  /* Selection: jobid range (exact match) — optional */
  if (jobId) {
    memset(sss2->sss2jbil, 0x40, 8);
    memset(sss2->sss2jbih, 0x40, 8);
    int idLen = strlen(jobId);
    if (idLen > 8) idLen = 8;
    memcpy(sss2->sss2jbil, jobId, idLen);
    memcpy(sss2->sss2jbih, jobId, idLen);
    sss2->sss2sjbi = 1;
  }

  /* Selection: owner — optional */
  if (owner) {
    memset(sss2->sss2crea, 0x40, 8);
    int ownerLen = strlen(owner);
    if (ownerLen > 8) ownerLen = 8;
    memcpy(sss2->sss2crea, owner, ownerLen);
    sss2->sss2scre = 1;
  }

  /* Browse + read-only — per research doc section 5 */
  sss2->sss2sbro = 1;
  sss2->sss2sron = 1;

  /* Disposition: keep (critical — without this SAPI deletes) */
  sss2->sss2dkpe = 1;

  /* Include all job types */
  sss2->sss2sstc = 1;
  sss2->sss2stsu = 1;
  sss2->sss2sjob = 1;
  sss2->sss2sapc = 1;

  /* Include all queues */
  sss2->sss2shld = 1;
  sss2->sss2sxwh = 1;
  sss2->sss2swtr = 1;

  /* Dump selection state + hex dump of selection bytes */
  printf("SSS2: len=%d ver=%d type=%d\n",
         sss2->sss2len, sss2->sss2ver, sss2->sss2type);
  printf("SSS2: jobn hex:");
  for (int i = 0; i < 8; i++) printf(" %02X", sss2->sss2jobn[i]);
  printf("\nSSS2: jbil hex:");
  for (int i = 0; i < 8; i++) printf(" %02X", sss2->sss2jbil[i]);
  printf("\nSSS2: sel1=%02X sel2=%02X sel3=%02X sel5=%02X\n",
         sss2->sss2sel1, sss2->sss2sel2, sss2->sss2sel3,
         sss2->sss2sel5);
  printf("SSS2: dkpe=%d sron=%d sbro=%d sjbn=%d sjbi=%d\n",
         sss2->sss2dkpe, sss2->sss2sron, sss2->sss2sbro,
         sss2->sss2sjbn, sss2->sss2sjbi);

  /* Hex dump offsets of key fields for cross-reference */
  unsigned char *raw = (unsigned char *)sss2;
  printf("SSS2: offsetof(sss2jobn)=%d offsetof(sss2jbil)=%d\n",
         (int)((char *)sss2->sss2jobn - (char *)sss2),
         (int)((char *)sss2->sss2jbil - (char *)sss2));
  /* Dump bytes 0x24-0x50 (type through early selection) */
  printf("SSS2: raw[0x24..0x4F]:");
  for (int i = 0x24; i < 0x50; i++) printf(" %02X", raw[i]);
  printf("\n");
  fflush(stdout);

  /* Walk sysout datasets */
  int dsCount = 0;
  int maxDS = 10;  /* safety limit */

  for (;;) {
    if (dsCount >= maxDS) {
      printf("SAPI: hit max %d datasets, stopping.\n", maxDS);
      break;
    }
    printf("\n--- SAPI call %d ---\n", dsCount + 1);
    fflush(stdout);

    int funcRC = callSAPI(sss2);

    printf("SAPI: funcRC=%d reas=%d\n", funcRC, (int)sss2->sss2reas);
    fflush(stdout);

    if (funcRC == SSS2EODS) {
      printf("SAPI: end of data (%d datasets found). reas=%d\n",
             dsCount, (int)sss2->sss2reas);
      break;
    }

    if (funcRC != SSS2RTOK) {
      printf("SAPI: unexpected funcRC=%d reas=%d\n",
             funcRC, (int)sss2->sss2reas);
      break;
    }

    dsCount++;

    /* Extract returned fields */
    char dsn[45], step[9], dd[9];
    memcpy(dsn, sss2->sss2dsn, 44);
    dsn[44] = '\0';
    for (int i = 43; i >= 0 && dsn[i] == ' '; i--) dsn[i] = '\0';

    memcpy(step, sss2->sss2stpd, 8);
    step[8] = '\0';
    for (int i = 7; i >= 0 && step[i] == ' '; i--) step[i] = '\0';

    memcpy(dd, sss2->sss2ddnd, 8);
    dd[8] = '\0';
    for (int i = 7; i >= 0 && dd[i] == ' '; i--) dd[i] = '\0';

    void * __ptr32 btok = sss2->sss2btok;

    printf("SAPI: ds[%d] step='%s' dd='%s'\n", dsCount, step, dd);
    printf("SAPI: dsn='%s'\n", dsn);
    printf("SAPI: btok=%08X\n", (unsigned int)btok);

    /* Hex dump browse token */
    unsigned char * __ptr32 braw = (unsigned char * __ptr32)btok;
    printf("SAPI: btok hex:");
    for (int i = 0; i < 36; i++) printf(" %02X", braw[i]);
    printf("\n");
    fflush(stdout);

    if (listOnly) {
      /* Tell JES to skip this dataset on the next call */
      sss2->sss2rnpr = 1;
      continue;
    }
    sss2->sss2rnpr = 0;

    /* Try SDSB DYNALLOC */
    char allocDD[9];
    int allocErr = 0, allocInfo = 0;
    int rc = sdsbAlloc(dsn, btok, "JES2", allocDD, &allocErr, &allocInfo);

    if (rc != 0) {
      printf("SDSB: FAILED rc=%d error=%04X info=%04X\n",
             rc, allocErr, allocInfo);
      fflush(stdout);
      sss2->sss2rnpr = 1;  /* advance past this dataset */
      continue;
    }
    sss2->sss2rnpr = 0;

    printf("SDSB: OK dd='%.8s'\n", allocDD);
    fflush(stdout);

    /* Trim DD name */
    for (int i = 7; i >= 0 && allocDD[i] == ' '; i--) allocDD[i] = '\0';

    /* Open and read */
    char ddPath[16];
    sprintf(ddPath, "//DD:%s", allocDD);

    FILE *fp = fopen(ddPath, "rb,type=record");
    if (!fp) {
      printf("SDSB: fopen failed for '%s'\n", ddPath);
      continue;
    }

    char buf[32768];
    int lines = 0;
    while (lines < 20) {
      int n = fread(buf, 1, sizeof(buf), fp);
      if (n <= 0) break;
      lines++;
      /* Trim trailing blanks */
      while (n > 0 && buf[n-1] == ' ') n--;
      printf("  %4d: %.*s\n", lines, n, buf);
    }

    printf("SDSB: read %d lines\n", lines);
    fclose(fp);
    /* TODO: dealloc DD */
  }

  /* Terminate SAPI thread (rule 3.1) */
  sss2->sss2ctrl = 1;
  callSAPI(sss2);
  printf("\nSAPI thread terminated.\n");

  free(sss2);
  return 0;
}
