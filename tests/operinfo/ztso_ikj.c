#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "zowetypes.h"
#include "alloc.h"
#include "dynalloc.h"

/* Minimal test: IKJTSOEV + IKJEFTSR + terminate + read SYSTSPRT */

#ifdef _LP64
#define SAM31_IF_NEEDED " SAM31 \n"
#define SAM64_IF_NEEDED " SAM64 \n"
#else
#define SAM31_IF_NEEDED ""
#define SAM64_IF_NEEDED ""
#endif

static uint32_t a31(const void *p) {
  return (uint32_t)((uintptr_t)p & 0x7FFFFFFFU);
}
static uint32_t a31Last(const void *p) {
  return a31(p) | 0x80000000U;
}

/* Generic OS-linkage caller: R1=parmlist, R13=savearea, R15=entry, BASR 14,15 */
static int callSvc31(uint32_t ep, uint32_t plAddr) {
  int rc = 0, r13;
#ifdef _LP64
  char *sa = (char *__ptr32)safeMalloc31(72, "svc-sa");
#else
  char sa[72];
#endif
  memset(sa, 0, 72);
  uint32_t saAddr = a31(sa);
  __asm(" ST 13,%0 " : : "m"(r13) :);
  ((int *)sa)[1] = r13;

  __asm(ASM_PREFIX
    " LLGF  1,%[pl]  \n"
    " LLGF  13,%[sa]  \n"
    " LLGF  15,%[ep]  \n"
    SAM31_IF_NEEDED
    " BASR  14,15     \n"
    SAM64_IF_NEEDED
    " ST    15,%[rc]  \n"
    : [rc]"=m"(rc)
    : [pl]"m"(plAddr), [sa]"m"(saAddr), [ep]"m"(ep)
    : "r0", "r1", "r13", "r14", "r15");
#ifdef _LP64
  safeFree31(sa, 72);
#endif
  return rc;
}

static int loadMod(const char *name, uint32_t *ep) {
  int rc = 0;
  uint32_t entry = 0;
  ALLOC_STRUCT31(STRUCT31_NAME(p), STRUCT31_FIELDS(char n[8];));
  memset(p->n, ' ', 8);
  memcpy(p->n, name, strlen(name) < 8 ? strlen(name) : 8);

  __asm(ASM_PREFIX
    " LA 0,%[mod] \n"
    " XGR 1,1     \n"
    SAM31_IF_NEEDED
    " SVC 8        \n"
    SAM64_IF_NEEDED
    " ST 15,%[rc]  \n"
    " ST 0,%[ep]   \n"
    : [rc]"=m"(rc), [ep]"=m"(entry)
    : [mod]"m"(p->n)
    : "r0","r1","r14","r15");
  FREE_STRUCT31(STRUCT31_NAME(p));
  *ep = entry;
  return rc;
}

int main() {
  uint32_t epTsoev, epEftsr;
  int rc;
  char errBuf[256];
  char tmpPath[] = "/tmp/ztso_ikj_test.txt";

  /* Load modules */
  rc = loadMod("IKJTSOEV", &epTsoev);
  printf("LOAD IKJTSOEV rc=%d ep=0x%08X\n", rc, epTsoev);
  rc = loadMod("IKJEFTSR", &epEftsr);
  printf("LOAD IKJEFTSR rc=%d ep=0x%08X\n", rc, epEftsr);

  /* Allocate SYSTSPRT and SYSTSIN */
  rc = dynallocUSSOutput("SYSTSPRT", tmpPath, errBuf);
  printf("SYSTSPRT alloc rc=%d %s\n", rc, rc ? errBuf : "OK");
  rc = dynallocUSSOutput("SYSTSIN ", "/dev/null", errBuf);
  printf("SYSTSIN alloc rc=%d %s\n", rc, rc ? errBuf : "OK");

  /* IKJTSOEV: establish background TSO/E environment */
  ALLOC_STRUCT31(STRUCT31_NAME(tsoev), STRUCT31_FIELDS(
    uint32_t pl[5];
    uint32_t reserved;
    uint32_t rc;
    uint32_t rsn;
    uint32_t info;
    uint32_t cppl;
  ));
  tsoev->reserved = 0;
  tsoev->rc = 0;
  tsoev->rsn = 0;
  tsoev->info = 0;
  tsoev->cppl = 0;
  tsoev->pl[0] = a31(&tsoev->reserved);
  tsoev->pl[1] = a31(&tsoev->rc);
  tsoev->pl[2] = a31(&tsoev->rsn);
  tsoev->pl[3] = a31(&tsoev->info);
  tsoev->pl[4] = a31Last(&tsoev->cppl);

  int svcRC = callSvc31(epTsoev, a31(tsoev->pl));
  printf("IKJTSOEV: svcRC=%d funcRC=%d rsn=%d cppl=0x%08X\n",
         svcRC, tsoev->rc, tsoev->rsn, tsoev->cppl);
  uint32_t cpplAddr = tsoev->cppl;
  FREE_STRUCT31(STRUCT31_NAME(tsoev));

  if (svcRC != 0) {
    printf("IKJTSOEV failed, aborting\n");
    return 1;
  }

  /* IKJEFTSR: try TIME command with 9 parameters (including CPPL) */
  ALLOC_STRUCT31(STRUCT31_NAME(eftsr), STRUCT31_FIELDS(
    uint32_t pl[9];
    uint32_t flags;
    char     cmd[256];
    uint32_t cmdLen;
    uint32_t funcRC;
    uint32_t tsfReason;
    uint32_t abendCode;
    uint32_t parm7Zero;
    uint32_t cppl[4];    /* 4-fullword dummy or real CPPL */
  ));

  /* flags: unisolated(0x01 byte2), no-dump(0x00 byte3), command(0x01 byte4) */
  eftsr->flags = 0x00010001;
  memset(eftsr->cmd, ' ', 256);
  memcpy(eftsr->cmd, "TIME", 4);
  eftsr->cmdLen = 4;
  eftsr->funcRC = 0xDEADBEEF;     /* sentinel to detect if it's written */
  eftsr->tsfReason = 0xDEADBEEF;
  eftsr->abendCode = 0xDEADBEEF;
  eftsr->parm7Zero = 0;

  /* Pass real CPPL from IKJTSOEV */
  /* CPPL is at cpplAddr. It's 4 fullwords: CBuf,UPT,PSCB,ECT */
  /* Copy the 4 fullwords from the CPPL address */
  if (cpplAddr != 0) {
    uint32_t *cpplPtr = (uint32_t *)(uintptr_t)(cpplAddr & 0x7FFFFFFF);
    printf("CPPL at 0x%08X: [0]=0x%08X [1]=0x%08X [2]=0x%08X [3]=0x%08X\n",
           cpplAddr, cpplPtr[0], cpplPtr[1], cpplPtr[2], cpplPtr[3]);
    memcpy(eftsr->cppl, cpplPtr, 16);
  } else {
    memset(eftsr->cppl, 0, 16);
  }

  /* 9-parameter form */
  eftsr->pl[0] = a31(&eftsr->flags);
  eftsr->pl[1] = a31(eftsr->cmd);
  eftsr->pl[2] = a31(&eftsr->cmdLen);
  eftsr->pl[3] = a31(&eftsr->funcRC);
  eftsr->pl[4] = a31(&eftsr->tsfReason);
  eftsr->pl[5] = a31(&eftsr->abendCode);
  eftsr->pl[6] = a31(&eftsr->parm7Zero);
  eftsr->pl[7] = a31(eftsr->cppl);
  eftsr->pl[8] = a31Last(eftsr->cppl);  /* no token, but mark as last */

  /* Actually: param 8 = CPPL, param 9 is optional token.
     If no token, HOB should be on param 8 */
  eftsr->pl[7] = a31Last(eftsr->cppl);

  printf("right before IKJEFTSR \n");
  fflush(stdout);
  svcRC = callSvc31(epEftsr, a31(eftsr->pl));
  printf("IKJEFTSR TIME: svcRC=%d funcRC=0x%08X tsfRsn=0x%08X abend=0x%08X\n",
         svcRC, eftsr->funcRC, eftsr->tsfReason, eftsr->abendCode);

  /* Try STATUS */
  memset(eftsr->cmd, ' ', 256);
  memcpy(eftsr->cmd, "STATUS", 6);
  eftsr->cmdLen = 6;
  eftsr->funcRC = 0xDEADBEEF;
  eftsr->tsfReason = 0xDEADBEEF;
  eftsr->abendCode = 0xDEADBEEF;

  svcRC = callSvc31(epEftsr, a31(eftsr->pl));
  printf("IKJEFTSR STATUS: svcRC=%d funcRC=0x%08X tsfRsn=0x%08X abend=0x%08X\n",
         svcRC, eftsr->funcRC, eftsr->tsfReason, eftsr->abendCode);

  /* Try LISTDS */
  memset(eftsr->cmd, ' ', 256);
  char *ldCmd = "LISTDS 'SYS1.PARMLIB'";
  memcpy(eftsr->cmd, ldCmd, strlen(ldCmd));
  eftsr->cmdLen = strlen(ldCmd);
  eftsr->funcRC = 0xDEADBEEF;
  eftsr->tsfReason = 0xDEADBEEF;
  eftsr->abendCode = 0xDEADBEEF;

  svcRC = callSvc31(epEftsr, a31(eftsr->pl));
  printf("IKJEFTSR LISTDS: svcRC=%d funcRC=0x%08X tsfRsn=0x%08X abend=0x%08X\n",
         svcRC, eftsr->funcRC, eftsr->tsfReason, eftsr->abendCode);

  FREE_STRUCT31(STRUCT31_NAME(eftsr));

  /* Deallocate DDs to force CLOSE (flush QSAM buffers) */
  printf("\nDeallocating DDs...\n");
  DeallocDDName("SYSTSPRT");
  DeallocDDName("SYSTSIN ");

  /* Now check the file */
  printf("Checking %s:\n", tmpPath);
  fflush(stdout);
  {
    FILE *f = fopen(tmpPath, "r");
    if (!f) {
      printf("  Cannot open temp file\n");
      perror("  ");
    } else {
      char line[256];
      int n = 0;
      while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        while (len > 0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len]=0;
        printf("  [%d] %s\n", n++, line);
      }
      if (n == 0) printf("  (empty)\n");
      fclose(f);
    }
  }

  /* unlink(tmpPath); -- leave file so we can check post-exit */
  printf("Done.\n");
  return 0;
}
