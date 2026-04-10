

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __JOBSERVICE__
#define __JOBSERVICE__ 1

#ifndef __LONGNAME__
#define jobServiceInit        JBSVINIT
#define jobServiceTerm        JBSVTERM
#define jobServiceGetJobs     JBSVGTJB
#define jobServiceFreeJobs    JBSVFRJB
#define jobServiceReadSysout  JBSVRDSO
#define sapiCall              SAPICALL
#define sapiAllocDataset      SAPIALOC
#endif

#include "zowetypes.h"

/*
  Job completion info, decoded from STTRMXRC.
  The completion type tells you how the job ended;
  the code carries the RC or abend code when applicable.
*/
#define JOB_COMP_UNKNOWN   0
#define JOB_COMP_NORMAL    1   /* ended normally, code = max RC */
#define JOB_COMP_CC        2   /* ended by CC */
#define JOB_COMP_JCLERR    3   /* JCL error */
#define JOB_COMP_CANCELED  4   /* canceled */
#define JOB_COMP_ABEND     5   /* ABENDed, code = abend code */
#define JOB_COMP_CNVABEND  6   /* converter ABENDed */
#define JOB_COMP_SECERR    7   /* security error */
#define JOB_COMP_EOM       8   /* failed in EOM */
#define JOB_COMP_CNVERR    9   /* converter error */
#define JOB_COMP_SYSFAIL  10   /* system failure */
#define JOB_COMP_FLUSHED  11   /* flushed */

/* Job types */
#define JOB_TYPE_STC   1
#define JOB_TYPE_TSU   2
#define JOB_TYPE_JOB   3
#define JOB_TYPE_APPC  4

/* Job hold states */
#define JOB_HOLD_NONE  1
#define JOB_HOLD_HELD  2
#define JOB_HOLD_DUP   3

/* Job phases — detailed sub-phases (sttrphaz output values 1-27) */
#define JOB_PHASE_NOSUB      1   /* no subchain exists */
#define JOB_PHASE_FSSCI      2   /* active in CI in FSS address space */
#define JOB_PHASE_PSCBAT     3   /* awaiting postscan (batch) */
#define JOB_PHASE_PSCDSL     4   /* awaiting postscan (demsel) */
#define JOB_PHASE_FETCH      5   /* awaiting volume fetch */
#define JOB_PHASE_VOLWT      6   /* awaiting start setup */
#define JOB_PHASE_SYSSEL     7   /* awaiting/active MDS system select */
#define JOB_PHASE_ALLOC      8   /* awaiting resource allocation */
#define JOB_PHASE_VOLUAV     9   /* awaiting unavailable volume(s) */
#define JOB_PHASE_VERIFY    10   /* awaiting volume mounts */
#define JOB_PHASE_SYSVER    11   /* awaiting/active MDS system verify */
#define JOB_PHASE_ERROR     12   /* error during MDS processing */
#define JOB_PHASE_SELECT    13   /* awaiting selection on main */
#define JOB_PHASE_ONMAIN    14   /* scheduled on main */
#define JOB_PHASE_BRKDWN    17   /* awaiting breakdown */
#define JOB_PHASE_RESTRT    18   /* awaiting MDS restart */
#define JOB_PHASE_DONE      19   /* main and MDS processing complete */
#define JOB_PHASE_OUTPT     20   /* awaiting output service */
#define JOB_PHASE_OUTQUE    21   /* awaiting output service WTR */
#define JOB_PHASE_OSWAIT    22   /* awaiting reserved services */
#define JOB_PHASE_CMPLT     23   /* output service complete */
#define JOB_PHASE_DEMSEL    24   /* awaiting selection (demand select) */
#define JOB_PHASE_EFWAIT    25   /* ending function waiting or I/O */
#define JOB_PHASE_EFBAD     26   /* ending function not processed */

/* Job phases — composite/high-level (filter input and sttrphaz output 128+) */
#define JOB_PHASE_INPUT    128
#define JOB_PHASE_WTCONV   129
#define JOB_PHASE_CONV     130
#define JOB_PHASE_SETUP    131
#define JOB_PHASE_SPIN     132
#define JOB_PHASE_WTBKDN   133
#define JOB_PHASE_WTPURG   134
#define JOB_PHASE_PURG     135
#define JOB_PHASE_RECV     136
#define JOB_PHASE_WTXMIT   137
#define JOB_PHASE_XMIT     138
#define JOB_PHASE_EXEC     253   /* not completed execution */
#define JOB_PHASE_POSTEX   254   /* completed execution */

/*
  Per-sysout-dataset verbose info.
  This is the individual data set within a SYSOUT group (JES2 PDDB level).
  Only populated when verbose data is requested (STATOUTV / STATDLST).
*/
typedef struct JobSysoutDataset_tag {
  char     procName[9];    /* proc step name, null-terminated */
  char     stepName[9];    /* step name, null-terminated */
  char     ddName[9];      /* DD name, null-terminated */
  char     dsName[45];     /* sysout dataset name, null-termed */
  uint8_t  recfm;          /* record format byte */
  uint16_t lrecl;          /* max logical record length */
  uint32_t lineCount;      /* line count (if available) */
  uint32_t pageCount;      /* page count (if available) */
  uint32_t recordCount;    /* record count (JES3, if available) */
  uint8_t  countsValid;    /* 1 if line/page/byte counts are accurate */
  uint8_t  isSystemDS;     /* 1 if JES system dataset (JESMSGLG etc) */
  uint8_t  isSpinDS;       /* 1 if SPIN dataset */
  char     clientToken[80];/* SYSOUT dataset token for SAPI access */
  uint8_t  tokenValid;     /* 1 if clientToken is usable */
  struct JobSysoutDataset_tag *next;
} JobSysoutDataset;

/*
  Per-SYSOUT-element terse info.
  In JES2, a SYSOUT element corresponds to a JOE (a group of
  datasets processed together). Each element may have a chain
  of verbose datasets underneath.
*/
typedef struct JobSysout_tag {
  char     owner[9];       /* SYSOUT owner/creator, null-terminated */
  char     dest[19];       /* destination, null-terminated */
  char     sysoutClass[9]; /* SYSOUT class, null-terminated */
  uint32_t recordCount;    /* record count */
  uint32_t pageCount;      /* page count */
  uint32_t lineCount;      /* line count */
  char     forms[9];       /* forms name */
  uint8_t  priority;       /* SYSOUT priority */
  char     sysoutId[45];   /* EBCDIC SYSOUT identifier */
  char     clientToken[80];/* SYSOUT client token for element-level access */
  uint8_t  tokenValid;     /* 1 if clientToken is usable */
  uint8_t  holdOpr;        /* held by operator */
  uint8_t  holdSys;        /* system hold */
  uint8_t  dispHold;       /* OUTDISP=HOLD */
  uint8_t  dispLeave;      /* OUTDISP=LEAVE */
  uint8_t  dispWrite;      /* OUTDISP=WRITE */
  uint8_t  dispKeep;       /* OUTDISP=KEEP */
  uint8_t  isSpin;         /* SPIN data set */
  JobSysoutDataset *datasets; /* chain of verbose dataset entries (or NULL) */
  struct JobSysout_tag *next;
} JobSysout;

/*
  Job info — one per job returned by SSI 80.
*/
typedef struct JobInfo_tag {
  char     jobName[9];     /* job name, null-terminated */
  char     jobId[9];       /* job identifier (JOBnnnnn / TSUnnnnn / STCnnnnn) */
  char     origJobId[9];   /* original job ID */
  char     jobClass[9];    /* job class */
  char     owner[9];       /* owning userid */
  char     secLabel[9];    /* SECLABEL */
  char     originNode[9];  /* node of submittal */
  char     execNode[9];    /* execution node */
  char     activeSys[9];   /* MVS system active on (blank if not) */
  char     activeMember[9];/* JES member active on (blank if not) */
  char     correlator[65]; /* job correlator */
  uint8_t  phase;          /* job phase (JOB_PHASE_*) */
  uint8_t  holdState;      /* hold indicator (JOB_HOLD_*) */
  uint8_t  jobType;        /* job type (JOB_TYPE_*) */
  uint8_t  priority;       /* job priority */
  uint8_t  compType;       /* completion type (JOB_COMP_*) */
  uint32_t compCode;       /* completion code (RC or abend code) */
  uint32_t jobNumber;      /* binary job number */
  int32_t  spoolTrackGroups; /* spool space used (-1 if unknown) */
  uint32_t queuePos;       /* position on class/phase queue (sttrqpos) */
  char     printDest[19];  /* default print destination node+remote */
  char     device[19];     /* device name if active on device */
  /* WLM scheduling fields (from statschd section, if present) */
  char     serviceClass[9]; /* WLM service class (stscsrvc) */
  char     schedEnv[17];   /* WLM scheduling environment (stscsenv) */
  int32_t  wlmQueuePos;   /* WLM queue position (stscqpos), -1 if N/A */
  uint16_t execASID;       /* ASID where executing (stscasid) */
  uint8_t  wlmMode;        /* 0=JES managed, 1=WLM managed (stsc1jcm) */
  uint8_t  delayFlags;     /* why job won't run (stscahld byte) */
  int      sysoutCount;    /* number of sysout elements */
  JobSysout *sysouts;      /* linked list of sysout elements (or NULL) */
  struct JobInfo_tag *next;
} JobInfo;

/*
  Selection/filter criteria for job enumeration.
  Set fields and corresponding flag bits to filter.
  Leave flags at 0 for "return everything".
*/
#define JOB_SELECT_BY_NAME   0x01
#define JOB_SELECT_BY_OWNER  0x02
#define JOB_SELECT_BY_JOBID  0x04
#define JOB_SELECT_BY_PHASE  0x08
#define JOB_SELECT_STC       0x10
#define JOB_SELECT_TSU       0x20
#define JOB_SELECT_JOB       0x40

#define JOB_DETAIL_TERSE     1  /* job-level terse only (STATTERS) */
#define JOB_DETAIL_SYSOUT    2  /* terse + sysout elements (STATOUTT) */
#define JOB_DETAIL_VERBOSE   3  /* verbose: sysout + per-dataset info (STATOUTV) */
#define JOB_DETAIL_DSLIST    4  /* dataset list: all datasets incl SYSIN (STATDLST) */

typedef struct JobServiceFilter_tag {
  uint32_t flags;           /* JOB_SELECT_* bits */
  char     jobName[9];      /* filter by job name (supports * wildcard) */
  char     owner[9];        /* filter by owner */
  char     jobIdLow[9];     /* low job ID for range */
  char     jobIdHigh[9];    /* high job ID for range */
  uint8_t  phase;           /* filter by phase */
  int      detailLevel;     /* JOB_DETAIL_* */
  int      maxJobs;         /* limit on jobs returned, 0 = unlimited */
} JobServiceFilter;

/*
  Opaque handle for the job service.
  Holds the SSI 80 storage management anchor across calls.
*/
typedef struct JobService_tag JobService;

/*
  Initialize a job service handle. Must be called before getJobs.
  Returns 0 on success.
*/
int jobServiceInit(JobService **service);

/*
  Enumerate jobs matching the filter criteria.
  Results are allocated with safeMalloc and returned as a linked list.
  Call jobServiceFreeJobs to release.
  Returns 0 on success, sets *jobs to first element.
  On error, *jobs is NULL and return code indicates the problem:
    4  = SSI function not supported
    8  = SSI logic error
    12 = SSI not present
    >0 = other SSI return code
*/
int jobServiceGetJobs(JobService *service,
                      JobServiceFilter *filter,
                      JobInfo **jobs,
                      int *jobCount);

/*
  Free a chain of JobInfo returned by jobServiceGetJobs.
  Also frees attached sysout and dataset chains.
  Also makes the SSI STATMEM call to release JES-owned storage.
*/
void jobServiceFreeJobs(JobService *service,
                        JobInfo *jobs);

/*
  Tear down the job service handle.
*/
void jobServiceTerm(JobService *service);

/*
  Callback for sysout record reading.
  Called once per record with the record text and its length.
  Return 0 to continue reading, non-zero to stop early.
*/
typedef int (*SysoutRecordHandler)(const char *record, int recordLen,
                                   int lineNumber, void *userData);

#define SYSOUT_READ_DEFAULT_MAX  10000

/*
  Read sysout content using a client token obtained from SSI 80.
  Uses SAPI (SSI 79) to select the dataset, dynamic allocation
  to map it, and standard I/O to read records.

  Parameters:
    service      - initialized job service handle
    clientToken  - 80-byte SYSOUT client token (from JobSysout or
                   JobSysoutDataset tokenValid/clientToken fields)
    maxRecords   - maximum records to read (0 = SYSOUT_READ_DEFAULT_MAX)
    handler      - callback invoked for each record
    userData     - opaque pointer passed to handler
    recordsRead  - output: number of records actually read (may be NULL)

  Returns 0 on success.  Non-zero indicates an error:
    4  = SAPI end-of-data (token may refer to deleted sysout)
    8  = SAPI invalid arguments
    12 = SAPI unavailable
    32 = SAPI logic error
    -1 = allocation or I/O failure
*/
int jobServiceReadSysout(JobService *service,
                         const char *clientToken,
                         int maxRecords,
                         SysoutRecordHandler handler,
                         void *userData,
                         int *recordsRead);

/*
  Walk all sysout datasets for a job using SAPI (SSI 79) only.
  No SSI 80 client token needed — selects by jobid directly.
  Calls handler for each record of each dataset found.
  Returns 0 on success.
*/
int jobServiceSAPIRead(JobService *service,
                       const char *jobId,
                       const char *jobName,
                       int maxRecords,
                       SysoutRecordHandler handler,
                       void *userData);

/* ----------------------------------------------------------------
   SAPI primitives — used by logreader and other SAPI consumers
   ---------------------------------------------------------------- */

struct sss2;  /* forward decl, defined in iazsss2.h */

/*
  Make an SSI 79 (SAPI) call.
  The SSS2 block must be in 31-bit addressable storage.
  Returns the IEFSSREQ R15 return code.
  On success (0), check *funcRC for the SAPI function return code.
*/
int sapiCall(struct sss2 *__ptr32 sss2Block, int *funcRC);

/*
  DYNALLOC a spool dataset for reading via SAPI browse token.
  dsn:          dataset name from SSS2 output (sss2dsn)
  ddnameResult: 8-byte buffer, receives allocated DD name
  ssname:       subsystem name (e.g. "JES2")
  browseToken:  text unit pointer from sss2btok
  errorOut:     receives DYNALLOC error code
  infoOut:      receives DYNALLOC info code
  Returns 0 on success.
*/
int sapiAllocDataset(char *dsn, char *ddnameResult,
                     char *ssname,
                     void * __ptr32 browseToken,
                     int *errorOut, int *infoOut);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
