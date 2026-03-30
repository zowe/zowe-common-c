

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdio.h>
#include <metal/string.h>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "zos.h"
#include "ssi.h"
#include "jobservice.h"

#include "dynalloc.h"

/*
  IBM system headers for SSI 80 (Extended Status) and SSI 79 (SAPI).
  These provide the wire-format structs returned by JES.
*/
#include "iazssst.h"
#include "iazsss2.h"

/* ----------------------------------------------------------------
   Internal helpers
   ---------------------------------------------------------------- */

/*
  Copy an EBCDIC fixed-length field to a null-terminated C string.
  Trims trailing blanks.
*/
static void copyAndTrim(char *dst, const unsigned char *src, int len) {
  memcpy(dst, src, len);
  dst[len] = '\0';
  for (int i = len - 1; i >= 0; i--) {
    if (dst[i] == ' ') {
      dst[i] = '\0';
    } else {
      break;
    }
  }
}

/*
  Decode STTRMXRC into compType + compCode.
*/
static void decodeCompletion(uint32_t sttrmxrc,
                             uint8_t *compType, uint32_t *compCode) {
  uint8_t indicator = (sttrmxrc >> 24) & 0xFF;
  uint32_t code = sttrmxrc & 0x00FFFFFF;
  int hasAbend = (indicator & 0x80) != 0;
  int hasCC    = (indicator & 0x40) != 0;

  if (hasAbend) {
    *compType = JOB_COMP_ABEND;
    *compCode = code;
  } else if (hasCC) {
    *compType = JOB_COMP_NORMAL;
    *compCode = code & 0x0FFF;
  } else {
    /* use the 4-bit type nibble */
    uint8_t xtyp = indicator & 0x0F;
    if (xtyp <= JOB_COMP_FLUSHED) {
      *compType = xtyp;
    } else {
      *compType = JOB_COMP_UNKNOWN;
    }
    *compCode = code;
  }
}

/* ----------------------------------------------------------------
   SSI 80 stat extension — we need to manage our own copy since
   the IBM struct is named 'struct stat' which clashes with POSIX.
   We use the IBM header types by casting.
   ---------------------------------------------------------------- */

/*
  Size of the SSI 80 extension block. We use version 6 which gives
  us STATOUTT/STATOUTV/STATDLST support.
*/
#define STAT_EXT_VERSION  STATV060
#define STAT_EXT_SIZE     STATSIZ6

struct JobService_tag {
  Addr31 storageAnchor;    /* STATSTRP — JES storage management anchor */
  Addr31 storageAnchor64;  /* STATSTP2 — 64-bit storage anchor */
};

/* ----------------------------------------------------------------
   Walk the SYSOUT verbose chain for one STATSE (per-dataset info)
   ---------------------------------------------------------------- */

static JobSysoutDataset *walkVerboseSysoutChain(void *__ptr32 firstStatvo) {
  JobSysoutDataset *head = NULL;
  JobSysoutDataset *tail = NULL;

  struct statvo *vo = (struct statvo *)firstStatvo;
  while (vo) {
    /* Navigate: prolog -> header -> verbose section */
    char *base = (char *)vo;
    int ohdr = vo->stvoohdr;
    struct statsvhd *hdr = (struct statsvhd *)(base + ohdr);
    struct statsevb *vb = (struct statsevb *)((char *)hdr + STSVSIZE);

    JobSysoutDataset *ds = (JobSysoutDataset *)safeMalloc(sizeof(JobSysoutDataset), "JobSysoutDataset");
    memset(ds, 0, sizeof(JobSysoutDataset));

    copyAndTrim(ds->procName, vb->stvsprcd, 8);
    copyAndTrim(ds->stepName, vb->stvsstpd, 8);
    copyAndTrim(ds->ddName,   vb->stvsddnd, 8);
    copyAndTrim(ds->dsName,   vb->stvsdsn, 44);
    ds->recfm = vb->stvsrecf;
    ds->lrecl = vb->stvsmlrl;
    ds->lineCount = vb->stvslnct;
    ds->pageCount = vb->stvspgct;
    ds->recordCount = vb->stvsrcct;
    ds->countsValid = (vb->stvsdscl) ? 1 : 0;
    ds->isSystemDS  = (vb->stvs1sys) ? 1 : 0;
    ds->isSpinDS    = (vb->stvs1spn) ? 1 : 0;
    if (!vb->stvs2civ) {
      memcpy(ds->clientToken, vb->stvsctkn, 80);
      ds->tokenValid = 1;
    }

    ds->next = NULL;
    if (tail) {
      tail->next = ds;
    } else {
      head = ds;
    }
    tail = ds;

    vo = (struct statvo *)vo->stvosnxt;
  }
  return head;
}

/* ----------------------------------------------------------------
   Walk the SYSOUT element chain for one STATJQ
   ---------------------------------------------------------------- */

static JobSysout *walkSysoutChain(void *__ptr32 firstStatse, int *count) {
  JobSysout *head = NULL;
  JobSysout *tail = NULL;
  int n = 0;

  struct statse *se = (struct statse *)firstStatse;
  while (se) {
    /* Navigate: prolog -> header -> terse section */
    char *base = (char *)se;
    int ohdr = se->stseohdr;
    struct statsehd *hdr = (struct statsehd *)(base + ohdr);
    struct statsetr *tr = (struct statsetr *)((char *)hdr + STSHSIZE);

    JobSysout *so = (JobSysout *)safeMalloc(sizeof(JobSysout), "JobSysout");
    memset(so, 0, sizeof(JobSysout));

    copyAndTrim(so->owner,       tr->ststouid, 8);
    copyAndTrim(so->dest,        tr->ststdest, 18);
    copyAndTrim(so->sysoutClass, tr->ststclas, 8);
    so->recordCount = tr->ststnrec;
    so->pageCount   = tr->ststpage;
    so->lineCount   = tr->ststlnct;
    copyAndTrim(so->forms, tr->ststform, 8);
    so->priority = tr->ststprio;
    copyAndTrim(so->sysoutId, tr->ststsoid, 44);

    if (!tr->stst2civ) {
      memcpy(so->clientToken, tr->ststctkn, 80);
      so->tokenValid = 1;
    }

    so->holdOpr   = (tr->ststhopr) ? 1 : 0;
    so->holdSys   = (tr->ststhsys) ? 1 : 0;
    so->dispHold  = (tr->ststdhld) ? 1 : 0;
    so->dispLeave = (tr->ststdlve) ? 1 : 0;
    so->dispWrite = (tr->ststdwrt) ? 1 : 0;
    so->dispKeep  = (tr->ststdkep) ? 1 : 0;
    so->isSpin    = (tr->stst1spn) ? 1 : 0;

    /* If verbose data was requested, walk the verbose chain */
    if (se->stsevrbo) {
      so->datasets = walkVerboseSysoutChain(se->stsevrbo);
    }

    so->next = NULL;
    if (tail) {
      tail->next = so;
    } else {
      head = so;
    }
    tail = so;
    n++;

    se = (struct statse *)se->stsejnxt;
  }
  *count = n;
  return head;
}

/* ----------------------------------------------------------------
   Walk the STATJQ chain — one job per element
   ---------------------------------------------------------------- */

static JobInfo *walkJobChain(void *__ptr32 firstStatjq, int *totalJobs) {
  JobInfo *head = NULL;
  JobInfo *tail = NULL;
  int n = 0;

  struct statjq *jq = (struct statjq *)firstStatjq;
  while (jq) {
    /* Navigate: prolog -> header -> terse section */
    char *base = (char *)jq;
    int ohdr = jq->stjqohdr;
    struct statjqhd *hdr = (struct statjqhd *)(base + ohdr);
    struct statjqtr *tr = (struct statjqtr *)((char *)hdr + STHDSIZE);

    JobInfo *job = (JobInfo *)safeMalloc(sizeof(JobInfo), "JobInfo");
    memset(job, 0, sizeof(JobInfo));

    copyAndTrim(job->jobName,      tr->sttrname, 8);
    copyAndTrim(job->jobId,        tr->sttrjid, 8);
    copyAndTrim(job->origJobId,    tr->sttrojid, 8);
    copyAndTrim(job->jobClass,     tr->sttrclas, 8);
    copyAndTrim(job->owner,        tr->sttrouid, 8);
    copyAndTrim(job->secLabel,     tr->sttrsecl, 8);
    copyAndTrim(job->originNode,   tr->sttronod, 8);
    copyAndTrim(job->execNode,     tr->sttrxnod, 8);
    copyAndTrim(job->activeSys,    tr->sttrsys, 8);
    copyAndTrim(job->activeMember, tr->sttrmem, 8);
    copyAndTrim(job->correlator,   tr->sttrjcor, 64);

    job->phase     = tr->sttrphaz;
    job->holdState = tr->sttrhold;
    job->jobType   = tr->sttrjtyp;
    job->priority  = tr->sttrprio;
    job->jobNumber = tr->sttrjnum;
    job->spoolTrackGroups = tr->sttrspac;

    decodeCompletion(tr->sttrmxrc, &job->compType, &job->compCode);

    /* Walk sysout chain if present */
    if (jq->stjqse) {
      job->sysouts = walkSysoutChain(jq->stjqse, &job->sysoutCount);
    }

    job->next = NULL;
    if (tail) {
      tail->next = job;
    } else {
      head = job;
    }
    tail = job;
    n++;

    jq = (struct statjq *)jq->stjqnext;
  }
  *totalJobs = n;
  return head;
}

/* ----------------------------------------------------------------
   Public API
   ---------------------------------------------------------------- */

int jobServiceInit(JobService **service) {
  JobService *svc = (JobService *)safeMalloc(sizeof(JobService), "JobService");
  if (!svc) {
    return -1;
  }
  memset(svc, 0, sizeof(JobService));
  *service = svc;
  return 0;
}

void jobServiceTerm(JobService *service) {
  if (service) {
    safeFree((char *)service, sizeof(JobService));
  }
}

/*
  Pad a null-terminated string into a fixed-length EBCDIC field (blank padded).
*/
static void padField(unsigned char *dst, const char *src, int len) {
  int slen = (int)strlen(src);
  if (slen > len) {
    slen = len;
  }
  memcpy(dst, src, slen);
  memset(dst + slen, ' ', len - slen);
}

int jobServiceGetJobs(JobService *service,
                      JobServiceFilter *filter,
                      JobInfo **jobs,
                      int *jobCount) {
  *jobs = NULL;
  *jobCount = 0;

  /* Allocate SSOB, SSIB, and STAT extension in 31-bit storage */
  int allocSize = sizeof(IEFSSOBH) + sizeof(IEFJSSIB) + STAT_EXT_SIZE;
  char *__ptr32 block = (char *__ptr32)safeMalloc31(allocSize, "SSI80req");
  if (!block) {
    return -1;
  }
  memset(block, 0, allocSize);

  IEFSSOBH *__ptr32 ssob = (IEFSSOBH *__ptr32)block;
  IEFJSSIB *__ptr32 ssib = (IEFJSSIB *__ptr32)(block + sizeof(IEFSSOBH));
  struct stat *__ptr32 statExt = (struct stat *__ptr32)(block + sizeof(IEFSSOBH) + sizeof(IEFJSSIB));

  /* Initialize SSIB and SSOB */
  initSSIB(ssib);
  memcpy(ssib->ssibssnm, "JES2", 4);
  initSSOB(ssob, 80, ssib, (Addr31)statExt);

  /* Set up the STAT extension */
  memcpy(statExt->stateye, "STAT", 4);
  statExt->statlen = STAT_EXT_SIZE;
  statExt->statver = STAT_EXT_VERSION;

  /* Carry forward the storage anchor from previous calls */
  statExt->statstrp = (void *__ptr32)service->storageAnchor;

  /* Set the request type based on detail level.
     STATOUTV and STATDLST require either a single job ID (STATJBIL=STATJBIH)
     or a STATJQ pointer from a prior terse call.  If neither is available,
     fall back to STATOUTT so the call doesn't fail. */
  int singleJobId = (filter->flags & JOB_SELECT_BY_JOBID) &&
                    (memcmp(filter->jobIdLow, filter->jobIdHigh, 8) == 0);

  switch (filter->detailLevel) {
  case JOB_DETAIL_TERSE:
    statExt->stattype = STATTERS;
    break;
  case JOB_DETAIL_VERBOSE:
    statExt->stattype = singleJobId ? STATOUTV : STATOUTT;
    break;
  case JOB_DETAIL_DSLIST:
    statExt->stattype = singleJobId ? STATDLST : STATOUTT;
    break;
  case JOB_DETAIL_SYSOUT:
  default:
    statExt->stattype = STATOUTT;
    break;
  }

  /* Apply selection filters */
  if (filter->flags & JOB_SELECT_BY_NAME) {
    statExt->statsjbn = 1;
    padField(statExt->statjobn, filter->jobName, 8);
  }
  if (filter->flags & JOB_SELECT_BY_OWNER) {
    statExt->statsown = 1;
    padField(statExt->statownr, filter->owner, 8);
  }
  if (filter->flags & JOB_SELECT_BY_JOBID) {
    statExt->statsjbi = 1;
    padField((unsigned char *)statExt->statjbil, filter->jobIdLow, 8);
    padField((unsigned char *)statExt->statjbih, filter->jobIdHigh, 8);
  }
  if (filter->flags & JOB_SELECT_BY_PHASE) {
    statExt->statsphz = 1;
    statExt->statphaz = filter->phase;
  }

  /* Job type selection: if any type bits set, apply them.
     If none set, SSI returns all types by default. */
  if (filter->flags & (JOB_SELECT_STC | JOB_SELECT_TSU | JOB_SELECT_JOB)) {
    if (filter->flags & JOB_SELECT_STC) statExt->statsstc = 1;
    if (filter->flags & JOB_SELECT_TSU) statExt->statstsu = 1;
    if (filter->flags & JOB_SELECT_JOB) statExt->statsjob = 1;
  }

  /* Job count limit */
  if (filter->maxJobs > 0) {
    statExt->stat1lmt = 1;
    statExt->statjqlm = filter->maxJobs;
  }

  /* Make the SSI call */
  int rc = callSSI(ssob);

  /* Save storage anchor for future calls / freeing */
  service->storageAnchor = (Addr31)statExt->statstrp;

  if (rc != 0) {
    safeFree31(block, allocSize);
    return rc;
  }

  int ssobReturn = (int)(intptr_t)ssob->ssobretn;
  if (ssobReturn != 0) {
    safeFree31(block, allocSize);
    return ssobReturn;
  }

  /* Walk the returned job chain */
  if (statExt->statjobf) {
    *jobs = walkJobChain(statExt->statjobf, jobCount);
  }

  safeFree31(block, allocSize);
  return 0;
}

void jobServiceFreeJobs(JobService *service, JobInfo *jobs) {
  /* First, make a STATMEM call to release JES-owned storage */
  if (service->storageAnchor) {
    int memSize = sizeof(IEFSSOBH) + sizeof(IEFJSSIB) + STAT_EXT_SIZE;
    char *__ptr32 block = (char *__ptr32)safeMalloc31(memSize, "SSI80mem");
    if (block) {
      memset(block, 0, memSize);
      IEFSSOBH *__ptr32 ssob = (IEFSSOBH *__ptr32)block;
      IEFJSSIB *__ptr32 ssib = (IEFJSSIB *__ptr32)(block + sizeof(IEFSSOBH));
      struct stat *__ptr32 statExt = (struct stat *__ptr32)(block + sizeof(IEFSSOBH) + sizeof(IEFJSSIB));

      initSSIB(ssib);
      memcpy(ssib->ssibssnm, "JES2", 4);
      initSSOB(ssob, 80, ssib, (Addr31)statExt);

      memcpy(statExt->stateye, "STAT", 4);
      statExt->statlen = STAT_EXT_SIZE;
      statExt->statver = STAT_EXT_VERSION;
      statExt->stattype = STATMEM;
      statExt->statstrp = (void *__ptr32)service->storageAnchor;

      callSSI(ssob);

      service->storageAnchor = 0;
      safeFree31(block, memSize);
    }
  }

  /* Then free our own structures */
  JobInfo *job = jobs;
  while (job) {
    JobInfo *nextJob = job->next;

    JobSysout *so = job->sysouts;
    while (so) {
      JobSysout *nextSo = so->next;

      JobSysoutDataset *ds = so->datasets;
      while (ds) {
        JobSysoutDataset *nextDs = ds->next;
        safeFree((char *)ds, sizeof(JobSysoutDataset));
        ds = nextDs;
      }

      safeFree((char *)so, sizeof(JobSysout));
      so = nextSo;
    }

    safeFree((char *)job, sizeof(JobInfo));
    job = nextJob;
  }
}

/* ----------------------------------------------------------------
   SAPI (SSI 79) — Sysout record reading
   ---------------------------------------------------------------- */

/*
  Direct SVC 99 allocation for SAPI spool browse.
  Bypasses LE dynalloc() wrapper. All pointers are __ptr32 throughout —
  no 64-bit widening. Text units: DALDSNAM, DALRTDDN, DALSSREQ, DALBRTKN.
*/
ZOWE_PRAGMA_PACK

typedef struct SAPIAllocRBX_tag {
  /* S99RBX — 36 bytes */
  char       id[6];              /* "S99RBX" */
  char       ver;                /* version = 0x01 */
  char       options;            /* processing options */
  char       subpool;
  char       key;
  char       sev;                /* severity for message processing */
  char       numMsgs;
  void     * __ptr32  cppl;
  int        msgReturn;
  int        wtoReturn;
  void     * __ptr32  msgBlock;
  int        infoReturn;         /* HH: error reason, LL: info reason */
  int        smsFeedback;
} SAPIAllocRBX;

typedef struct SAPIAllocParms_tag {
  /* S99RB — 20 bytes */
  char       rbLen;
  char       verbCode;
  short      flags1;
  short      errorCode;
  short      infoCode;
  TextUnit * __ptr32 * __ptr32 tuListPtr;
  SAPIAllocRBX * __ptr32  rbx;
  int        flags2;
  /* end of S99RB */
  void     * __ptr32  rbPtr;      /* S99RBPTR: pointer to S99RB | HOB */
  TextUnit * __ptr32  tuList[4];  /* DALDSNAM, DALRTDDN, DALSSREQ, DALBRTKN */
  SAPIAllocRBX  rbxData;          /* inline S99RBX for extended diagnostics */
} SAPIAllocParms;

ZOWE_PRAGMA_PACK_RESET

static int sapiAlloc(char *dsn, char *ddnameResult,
                     char *ssname,
                     TextUnit * __ptr32 browseToken,
                     int *errorOut, int *infoOut) {
  int allocSize = sizeof(SAPIAllocParms);
  SAPIAllocParms * __ptr32 p =
      (SAPIAllocParms * __ptr32)safeMalloc31(allocSize, "sapiAlloc");
  if (!p) return -1;
  memset(p, 0, allocSize);

  /* S99RBX — extended error info */
  memset(&p->rbxData, 0, sizeof(SAPIAllocRBX));
  memcpy(p->rbxData.id, "\xe2\xf9\xf9\xd9\xc2\xe7", 6); /* "S99RBX" in EBCDIC */
  p->rbxData.ver = 0x01;
  p->rbx = &p->rbxData;

  /* S99RB header */
  p->rbLen = 20;
  p->verbCode = 0x01;           /* S99VRBAL = allocate */
  p->tuListPtr = p->tuList;
  p->rbPtr = (void * __ptr32)((unsigned int)p | 0x80000000);

  /* Text units — all __ptr32 */
  p->tuList[0] = createSimpleTextUnit(DALDSNAM, dsn);
  p->tuList[1] = createSimpleTextUnit(DALRTDDN, ddnameResult);
  p->tuList[2] = createSimpleTextUnit(DALSSREQ, ssname);
  p->tuList[3] = browseToken;   /* __ptr32 → __ptr32, no widening */

  /* HOB on last entry */
  p->tuList[3] = (TextUnit * __ptr32)
      ((unsigned int)p->tuList[3] | 0x80000000);

  printf("SAPI-ALLOC: tu[0]=%08X tu[1]=%08X tu[2]=%08X tu[3]=%08X(btok)\n",
         (unsigned int)p->tuList[0],
         (unsigned int)p->tuList[1],
         (unsigned int)p->tuList[2],
         (unsigned int)(p->tuList[3]) & 0x7FFFFFFF);
  printf("SAPI-ALLOC: rbPtr=%08X tuListPtr=%08X\n",
         (unsigned int)p->rbPtr, (unsigned int)p->tuListPtr);
  fflush(stdout);

  if (!p->tuList[0] || !p->tuList[1] || !p->tuList[2]) {
    if (p->tuList[0]) freeTextUnit(p->tuList[0]);
    if (p->tuList[1]) freeTextUnit(p->tuList[1]);
    if (p->tuList[2]) freeTextUnit(p->tuList[2]);
    safeFree31((char *)p, allocSize);
    return -1;
  }

  /* Issue SVC 99 directly — no LE wrapper */
  int rc = 0;
  void * __ptr32 rbPtrAddr = &p->rbPtr;
  __asm(
      ASM_PREFIX
      "         LA    1,0(,%[rb])                                              \n"
      "         SVC   99                                                       \n"
      "         ST    15,%[rc]                                                 \n"
      : [rc]"=m"(rc)
      : [rb]"r"(rbPtrAddr)
      : "r0", "r1", "r14", "r15"
  );

  *errorOut = (int)p->errorCode;
  *infoOut  = (int)p->infoCode;

  printf("SAPI-ALLOC: SVC99 rc=%d error=%04X info=%04X\n",
         rc, (unsigned)p->errorCode, (unsigned)p->infoCode);
  printf("SAPI-ALLOC: S99RBX infoReturn=%08X smsFeedback=%08X\n",
         p->rbxData.infoReturn, p->rbxData.smsFeedback);
  fflush(stdout);

  if (rc == 0) {
    /* Extract returned DD name from DALRTDDN text unit */
    TextUnit * __ptr32 ddTU = p->tuList[1];
    memcpy(ddnameResult, ((char *)ddTU) + sizeof(TextUnit), 8);
  }

  /* Free text units 0-2 only — tuList[3] is the JES browse token */
  for (int i = 0; i < 3; i++) {
    p->tuList[i] = (TextUnit * __ptr32)
        ((unsigned int)p->tuList[i] & 0x7FFFFFFF);
    freeTextUnit(p->tuList[i]);
  }

  safeFree31((char *)p, allocSize);
  return rc;
}

/*
  Make an SSI 79 (SAPI) call.
  The SSS2 block must already be in 31-bit addressable storage.
  Returns the IEFSSREQ R15 return code. On success (0), check
  *funcRC for the SAPI function return code (ssobretn).
*/
static int callSAPI(struct sss2 *__ptr32 sss2Block, int *funcRC) {
  int allocSize = sizeof(IEFSSOBH) + sizeof(IEFJSSIB);
  char *__ptr32 block = (char *__ptr32)safeMalloc31(allocSize, "SAPI SSOB+SSIB");
  if (!block) {
    return -1;
  }
  memset(block, 0, allocSize);

  IEFSSOBH *__ptr32 ssob = (IEFSSOBH *__ptr32)block;
  IEFJSSIB *__ptr32 ssib = (IEFJSSIB *__ptr32)(block + sizeof(IEFSSOBH));

  initSSIB(ssib);
  memcpy(ssib->ssibssnm, "JES2", 4);
  initSSOB(ssob, SSOBSOU2, ssib, (Addr31)sss2Block);

  int rc = callSSI(ssob);
  *funcRC = (int)(intptr_t)ssob->ssobretn;

  safeFree31(block, allocSize);
  return rc;
}

int jobServiceReadSysout(JobService *service,
                         const char *clientToken,
                         int maxRecords,
                         SysoutRecordHandler handler,
                         void *userData,
                         int *recordsRead) {
  if (!service || !clientToken || !handler) {
    return -1;
  }
  if (maxRecords <= 0) {
    maxRecords = SYSOUT_READ_DEFAULT_MAX;
  }

  int totalRead = 0;
  int result = 0;

  /*
    Allocate the SSS2 block in 31-bit storage.
    Also allocate space for the 80-byte client token copy (must be
    addressable in AMODE 31).
  */
  int sss2AllocSize = SSS2SIZE + 80;
  char *sss2Raw = (char *)safeMalloc31(sss2AllocSize, "SAPI SSS2");
  if (!sss2Raw) {
    return -1;
  }
  memset(sss2Raw, 0, sss2AllocSize);

  struct sss2 *sss2 = (struct sss2 *)sss2Raw;
  char *tokenCopy = sss2Raw + SSS2SIZE;  /* 80 bytes after the SSS2 block */
  memcpy(tokenCopy, clientToken, 80);

  /* Initialize SSS2 header */
  sss2->sss2len = SSS2SIZE;
  sss2->sss2ver = SSS2VJCR;  /* version 3 */
  memcpy(sss2->sss2eye, "\xE2\xE2\xE2\xF2", 4);  /* "SSS2" in EBCDIC */
  sss2->sss2type = SSS2PUGE;

  /* Selection: use client token, read-only browse */
  sss2->sss2sctk = 1;
  sss2->sss2sbro = 1;
  sss2->sss2sron = 1;
  sss2->sss2shld = 1;  /* include HOLD/LEAVE queue */
  sss2->sss2swtr = 1;  /* include WRITE/KEEP queue */
  sss2->sss2ctkn = (void *__ptr32)tokenCopy;

  /* Disposition: keep the dataset (critical — without this SAPI may delete it) */
  sss2->sss2dkpe = 1;

  /* Call SAPI to select the dataset */
  int funcRC = 0;
  int rc = callSAPI(sss2, &funcRC);

  printf("SAPI: callSSI rc=%d funcRC=%d reas=%d\n",
         rc, funcRC, (int)sss2->sss2reas);
  fflush(stdout);

  if (rc != 0) {
    result = rc;
    goto cleanup;
  }

  if (funcRC == SSS2EODS) {
    /* Token refers to sysout that no longer exists */
    result = SSS2EODS;
    goto cleanup;
  }

  if (funcRC != SSS2RTOK) {
    result = funcRC;
    goto cleanup;
  }

  /* SAPI returned a dataset — allocate it for reading */
  char ddname[9];
  memset(ddname, ' ', 8);
  ddname[8] = '\0';

  char dsn[45];
  memcpy(dsn, sss2->sss2dsn, 44);
  dsn[44] = '\0';
  /* Trim trailing EBCDIC spaces from dsn */
  for (int i = 43; i >= 0 && dsn[i] == ' '; i--) {
    dsn[i] = '\0';
  }

  /* sss2btok is void *__ptr32 — keep it __ptr32 throughout,
     never widen to 64-bit. This IS the JES-initialized DALBRTKN
     text unit; we pass it directly to SVC 99. */
  TextUnit * __ptr32 btok = (TextUnit * __ptr32)sss2->sss2btok;

  printf("SAPI: dsn='%s' btok=%08X key=%04X count=%d\n",
         dsn, (unsigned int)btok,
         (unsigned)btok->key, (int)btok->number_of_parameters);
  /* Hex dump first 36 bytes */
  printf("SAPI: btok hex:");
  unsigned char *__ptr32 braw = (unsigned char *__ptr32)btok;
  for (int i = 0; i < 36; i++) {
    printf(" %02X", braw[i]);
  }
  printf("\n");
  fflush(stdout);

  int allocErr = 0, allocInfo = 0;
  rc = sapiAlloc(dsn, ddname, "JES2", btok, &allocErr, &allocInfo);
  if (rc != 0) {
    printf("SAPI: sapiAlloc failed rc=%d error=%04X info=%04X\n",
           rc, allocErr, allocInfo);
    fflush(stdout);
    result = -1;
    goto cleanup;
  }
  printf("SAPI: allocated DD='%.8s'\n", ddname);
  fflush(stdout);

  /* Null-terminate the returned DDname */
  for (int i = 7; i >= 0 && ddname[i] == ' '; i--) {
    ddname[i] = '\0';
  }

  /* Open the allocated sysout dataset for reading */
  char ddPath[16];
  sprintf(ddPath, "//DD:%s", ddname);

  FILE *fp = fopen(ddPath, "rb,type=record");
  if (!fp) {
    DeallocDDName(ddname);
    result = -1;
    goto cleanup;
  }

  /* Read records up to the limit */
  char recordBuf[32768];  /* max LRECL we'll handle */
  int lineNum = 0;

  while (lineNum < maxRecords) {
    int bytesRead = fread(recordBuf, 1, sizeof(recordBuf), fp);
    if (bytesRead <= 0) {
      break;
    }
    lineNum++;

    int stopRequested = handler(recordBuf, bytesRead, lineNum, userData);
    if (stopRequested) {
      break;
    }
  }

  totalRead = lineNum;
  fclose(fp);
  DeallocDDName(ddname);

cleanup:
  /* Tell SAPI we're done — set control flag and make final call */
  sss2->sss2ctrl = 1;
  callSAPI(sss2, &funcRC);

  safeFree31(sss2Raw, sss2AllocSize);

  if (recordsRead) {
    *recordsRead = totalRead;
  }
  return result;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
