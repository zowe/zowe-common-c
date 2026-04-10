

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  zdisplay.c — MVS DISPLAY command parser and executor.

  Reads z/OS control blocks to produce output that approximates
  the real MVS console response for each supported D command.
  This lets users and AI systems use familiar MVS command syntax
  to query system state without needing operator authority.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>

#include "zowetypes.h"
#include "zos.h"
#include "zdisplay.h"
#include "jobservice.h"

/* ----------------------------------------------------------------
   Command catalog — static table of known D commands
   ---------------------------------------------------------------- */

static const ZDispCommand catalog[] = {
  /* System Identity & IPL */
  { "D IPLINFO",
    "IPL date/time, z/OS release, LOADxx, IODF, IPL volume",
    NULL, 1 },
  { "D PARMLIB",
    "PARMLIB concatenation (dataset, volume, flags)",
    NULL, 1 },
  { "D SYMBOLS",
    "System symbols and substitution values",
    NULL, 1 },

  /* Address Spaces */
  { "D A,L",
    "Active address spaces (long form)",
    NULL, 1 },
  { "D A",
    "Active address spaces (short form)",
    NULL, 1 },

  /* Storage */
  { "D CSA",
    "Common storage area utilization (CSA and ECSA)",
    NULL, 1 },
  { "D SQA",
    "Shared queue area utilization (SQA and ESQA)",
    NULL, 1 },
  { "D VIRTSTOR",
    "Virtual storage layout summary",
    NULL, 1 },

  /* Subsystems */
  { "D SUBSYS",
    "Registered subsystems (SSCT chain)",
    NULL, 1 },

  /* Time */
  { "D T",
    "Current date and time, timezone offset",
    NULL, 1 },

  /* Security */
  { "D RACF",
    "RACF status (basic)",
    NULL, 1 },

  /* SMF */
  { "D SMF",
    "SMF recording status",
    NULL, 1 },

  /* Devices — placeholder, pending DFSMS research */
  { "D U,DASD,ONLINE",
    "Online DASD devices",
    "D U,devtype,ONLINE|OFFLINE|ALLOC  (devtype: DASD, TAPE, etc.)",
    0 },

  /* Programs */
  { "D PROG,APF",
    "APF-authorized library list",
    NULL, 1 },
  { "D PROG,LPA",
    "LPA library list",
    NULL, 1 },
  { "D PROG,LNKLST",
    "LNKLST concatenation",
    NULL, 1 },

  /* JES */
  { "D JOB",
    "Job status (equivalent to SDSF ST)",
    "D JOB,jobname  or  D JOB,jobid",
    1 },

  /* Sysplex */
  { "D XCF",
    "XCF group/member information",
    NULL, 0 },

  /* WLM */
  { "D WLM",
    "Workload Manager status and policy",
    NULL, 1 },

  /* Network — CMD-ONLY */
  { "D TCPIP",
    "TCP/IP stack information (requires command authority)",
    "D TCPIP,stackname,cmd",
    0 },
  { "D NET",
    "VTAM status (requires command authority)",
    NULL, 0 },

  /* GRS */
  { "D GRS",
    "Global resource serialization (requires command authority)",
    "D GRS,RES=(qname,rname)",
    0 },
};

#define CATALOG_COUNT (sizeof(catalog) / sizeof(catalog[0]))

const ZDispCommand *zdispGetCatalog(int *count) {
  *count = CATALOG_COUNT;
  return catalog;
}

/* ----------------------------------------------------------------
   Result helpers
   ---------------------------------------------------------------- */

static ZDispResult *allocResult(void) {
  ZDispResult *r = (ZDispResult *)malloc(sizeof(ZDispResult));
  if (!r) return NULL;
  memset(r, 0, sizeof(ZDispResult));
  r->maxLines = ZDISP_MAX_LINES;
  r->lines = (char **)calloc(r->maxLines, sizeof(char *));
  if (!r->lines) { free(r); return NULL; }
  return r;
}

static void addLine(ZDispResult *r, const char *fmt, ...) {
  if (r->lineCount >= r->maxLines) return;
  char buf[ZDISP_MAX_LINE];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  r->lines[r->lineCount] = strdup(buf);
  r->lineCount++;
}

void zdispFreeResult(ZDispResult *result) {
  if (!result) return;
  if (result->lines) {
    for (int i = 0; i < result->lineCount; i++) {
      free(result->lines[i]);
    }
    free(result->lines);
  }
  free(result);
}

/* ----------------------------------------------------------------
   String helpers
   ---------------------------------------------------------------- */

static void trimField(const char *src, int srcLen, char *dst, int dstLen) {
  int len = srcLen;
  if (len > dstLen - 1) len = dstLen - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
  for (int i = len - 1; i >= 0 && dst[i] == ' '; i--) {
    dst[i] = '\0';
  }
}

static void upperCase(char *s) {
  for (; *s; s++) {
    if (*s >= 'a' && *s <= 'z') *s -= 32;
  }
}

/* Mask high bit from 31-bit z/OS pointers (AMODE 31 addresses
   stored in 4-byte fields may have bit 0 set as a flag) */
static void *ptr31(void *p) {
  return (void *)((uintptr_t)p & 0x7FFFFFFF);
}

/* ----------------------------------------------------------------
   D IPLINFO
   ---------------------------------------------------------------- */

/*
  IHAIPA struct — copied from ipl.c.
  Lives at ECVT→ecvtipa.  Readable without authorization.
*/
#pragma pack(packed)

typedef struct IPAPLI_disp_tag {
  char   ipaplidsn[44];
  char   reserved1[1];
  char   ipaplivol[6];
  char   reserved2[12];
  char   ipapliflg;
} IPAPLI_disp;

typedef struct IHAIPA_disp_tag {
  char     ipaid[4];
  uint16_t ipalen;
  char     ipasp;
  char     ipaver;
  uint64_t ipaictod;    /* STCK TOD at system initialization */
  /* Offset 0x10 */
  char     ipaiodfu[4]; /* IODF Unit Address */
  char     ipaloads[2]; /* LOADxx suffix */
  char     ipapromt;
  char     ipanucid;    /* Nucleus ID */
  char     ipahwnam[8];
  char     ipalpnam[8];
  char     ipavmnam[8];
  /* Offset 0x30 */
  char     ipalpdsn[44]; /* IPL loadparm dataset name */
  char     ipalpddv[4];  /* Load device number EBCDIC */
  /* Offset 0x60 */
  char     ipaiodf[64];
  /* Offset 0xA0 */
  char     ipasparm[64]; /* IEASYS card */
  /* Offset 0xE0 */
  char     ipascat[64];  /* SYSCAT card */
  /* Offset 0x120 */
  char     ipasym[64];   /* IEASYM card */
  /* Offset 0x160 */
  char     ipaplex[64];  /* SYSPLEX card */
  /* Offset 0x1A0 */
  IPAPLI_disp ipaplib[17];
} IHAIPA_disp;

#pragma pack(reset)

static void doDisplayIPLINFO(ZDispResult *r) {
  CVT *cvt = getCVT();
  ECVT *ecvt = getECVT();
  if (!cvt || !ecvt) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS CVT/ECVT");
    r->rc = 4;
    return;
  }

  IHAIPA_disp *ipa = (IHAIPA_disp *)ecvt->ecvtipa;

  /* Format IPL time from TOD clock value */
  char iplTimeStr[32] = "UNKNOWN";
  if (ipa && ipa->ipaictod != 0) {
    /* TOD to Unix epoch: TOD epoch is 1900-01-01, Unix is 1970-01-01
       Difference = 2208988800 seconds = 0x7D91048BCA000000 TOD units */
    uint64_t todEpochDiff = 0x7D91048BCA000000ULL;
    if (ipa->ipaictod > todEpochDiff) {
      time_t unixTime = (time_t)((ipa->ipaictod - todEpochDiff) / 4096000000ULL);
      struct tm *tm = localtime(&unixTime);
      if (tm) {
        snprintf(iplTimeStr, sizeof(iplTimeStr), "%02d.%02d.%02d ON %02d/%02d/%04d",
                 tm->tm_hour, tm->tm_min, tm->tm_sec,
                 tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900);
      }
    }
  }

  char sysname[9], hwname[9], lpname[9], vmname[9];
  trimField(cvt->cvtsname, 8, sysname, 9);
  trimField(ecvt->ecvthdnm, 8, hwname, 9);
  trimField(ecvt->ecvtlpnm, 8, lpname, 9);
  trimField(ecvt->ecvtvmnm, 8, vmname, 9);

  char pown[17], pnam[17];
  trimField(ecvt->ecvtpown, 16, pown, 17);
  trimField(ecvt->ecvtpnam, 16, pnam, 17);

  addLine(r, " IEE254I IPLINFO DISPLAY");
  addLine(r, "  SYSTEM IPLED AT %s", iplTimeStr);
  addLine(r, "  RELEASE %s    LICENSE = %s", pnam, pown);

  if (ipa) {
    char loadsuf[3], nucid[2], lpdsn[45], lpddv[5];
    trimField(ipa->ipaloads, 2, loadsuf, 3);
    snprintf(nucid, sizeof(nucid), "%d", (int)(unsigned char)ipa->ipanucid);
    trimField(ipa->ipalpdsn, 44, lpdsn, 45);
    trimField(ipa->ipalpddv, 4, lpddv, 5);

    addLine(r, "  USED LOAD%s IN %s ON %s", loadsuf, lpdsn, lpddv);
    addLine(r, "  NUCLEUS = %s", nucid);

    char ieasym[65], ieasys[65];
    trimField(ipa->ipasym, 64, ieasym, 65);
    trimField(ipa->ipasparm, 64, ieasys, 65);
    addLine(r, "  IEASYM LIST = %s", ieasym);
    addLine(r, "  IEASYS LIST = %s", ieasys);

    char iodf[65];
    trimField(ipa->ipaiodf, 64, iodf, 65);
    addLine(r, "  IODF   = %s", iodf);

    char sysplex[65];
    trimField(ipa->ipaplex, 64, sysplex, 65);
    addLine(r, "  SYSPLEX = %s", sysplex);

    char syscat[65];
    trimField(ipa->ipascat, 64, syscat, 65);
    addLine(r, "  SYSCAT = %s", syscat);
  }

  addLine(r, "  SYSTEM NAME = %s", sysname);
  addLine(r, "  CPC NAME = %s   LPAR = %s   VM = %s",
          hwname, lpname, vmname);
  addLine(r, "  PRODUCT SEQUENCE = %08X", ecvt->ecvtpseq);
  addLine(r, "  SYSPLEX NAME = %.8s   CLONE ID = %.2s",
          ecvt->ecvtsplx, ecvt->ecvtclon);
}

/* ----------------------------------------------------------------
   D PARMLIB
   ---------------------------------------------------------------- */

static void doDisplayPARMLIB(ZDispResult *r) {
  ECVT *ecvt = getECVT();
  if (!ecvt || !ecvt->ecvtipa) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS ECVTIPA");
    r->rc = 4;
    return;
  }

  IHAIPA_disp *ipa = (IHAIPA_disp *)ecvt->ecvtipa;

  addLine(r, " IEF468I PARMLIB DATA SET INFORMATION");
  addLine(r, "         ENTRY  FLAGS  VOLUME  DATA SET");

  int entryNum = 0;
  for (int i = 0; i < 17; i++) {
    IPAPLI_disp *pl = &ipa->ipaplib[i];
    /* Check if entry is in use (non-blank dsname) */
    int blank = 1;
    for (int j = 0; j < 44; j++) {
      if (pl->ipaplidsn[j] != ' ' && pl->ipaplidsn[j] != '\0') {
        blank = 0;
        break;
      }
    }
    if (blank) continue;

    entryNum++;
    char dsname[45], volser[7];
    trimField(pl->ipaplidsn, 44, dsname, 45);
    trimField(pl->ipaplivol, 6, volser, 7);

    char flags[4] = "   ";
    if (pl->ipapliflg & 0x80) flags[0] = 'S';  /* in use */
    if (pl->ipapliflg & 0x40) flags[1] = 'D';  /* defaulted */

    addLine(r, "         %4d     %s  %s  %s",
            entryNum, flags, volser, dsname);
  }
}

/* ----------------------------------------------------------------
   D SYMBOLS
   ---------------------------------------------------------------- */

static void doDisplaySYMBOLS(ZDispResult *r) {
  ECVT *ecvt = getECVT();
  if (!ecvt || !ecvt->ecvtsymt) {
    addLine(r, " ZDISPLAY ERROR: SYMBOL TABLE NOT ACCESSIBLE");
    r->rc = 4;
    return;
  }

  SymbTable *st = ecvt->ecvtsymt;
  int numSymbols = st->numberOfSymbols;

  addLine(r, " IEA007I STATIC SYSTEM SYMBOL DEFINITIONS");
  addLine(r, "         SYMBOL    SUBSTITUTION TEXT");

  if (numSymbols <= 0 || numSymbols > 2000) {
    addLine(r, "         (no symbols or invalid count: %d)", numSymbols);
    return;
  }

  /* Walk SymbTableEntry array starting at st->firstEntry */
  SymbTableEntry *entry = &st->firstEntry;
  for (int i = 0; i < numSymbols && i < 200; i++) {
    char symBuf[64] = "", subBuf[64] = "";
    int symLen = entry->symbolLength;
    int subLen = entry->subtextLength;

    int useOffsets = (st->flag0 & SYMBT_PTRS_ARE_OFFSETS);
    char *base = (char *)&st->firstEntry;  /* offsets are from start of entry area */

    if (symLen > 0 && symLen < 60) {
      char *symData = NULL;
      if (useOffsets) {
        symData = base + entry->symbolOffset;
      } else if (entry->symbolPtr) {
        symData = entry->symbolPtr;
      }
      if (symData) {
        int copyLen = symLen > 63 ? 63 : symLen;
        memcpy(symBuf, symData, copyLen);
        symBuf[copyLen] = '\0';
      }
    }

    if (subLen > 0 && subLen < 60) {
      char *subData = NULL;
      if (useOffsets) {
        subData = base + entry->subtextOffset;
      } else if (entry->subtextPtr) {
        subData = entry->subtextPtr;
      }
      if (subData) {
        int copyLen = subLen > 63 ? 63 : subLen;
        memcpy(subBuf, subData, copyLen);
        subBuf[copyLen] = '\0';
      }
    }

    if (symBuf[0]) {
      addLine(r, "         %-16s \"%s\"", symBuf, subBuf);
    }

    entry++;
  }
}

/* ----------------------------------------------------------------
   D A / D A,L
   ---------------------------------------------------------------- */

static void doDisplayActive(ZDispResult *r, int longForm) {
  CVT *cvt = getCVT();
  if (!cvt) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS CVT");
    r->rc = 4;
    return;
  }

  ASVT *asvt = (ASVT *)cvt->cvtasvt;
  if (!asvt) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS ASVT");
    r->rc = 4;
    return;
  }

  if (longForm) {
    addLine(r, " IEE114I ACTIVE ADDRESS SPACES");
    addLine(r, "  JOBNAME  ASID  DP  CPU-TIME   EXCP   REAL  STATUS");
  } else {
    addLine(r, " IEE114I ACTIVE ADDRESS SPACES");
    addLine(r, "  JOBNAME  ASID  STATUS");
  }

  int count = 0;
  ASCB *ascb = (ASCB *)INT2PTR(asvt->asvtenty);
  while (ascb) {
    if (ascb->ascbascb[0] != 'A' || ascb->ascbascb[1] != 'S' ||
        ascb->ascbascb[2] != 'C' || ascb->ascbascb[3] != 'B') {
      break;
    }

    char *jn = getASCBJobname(ascb);
    char jobname[9];
    if (jn) {
      trimField(jn, 8, jobname, 9);
    } else {
      strcpy(jobname, "*NONE*");
    }

    const char *status = "";
    if (ascb->ascbdsp1 & 0x80) status = "DISPATCHED";
    else if (ascb->ascbdsp1 & 0x20) status = "SWAPPED";
    else status = "IN";

    if (longForm) {
      /* CPU time: ascbejst in TOD units, convert to seconds */
      double cpuSec = (double)ascb->ascbejst / 4096000000.0;
      int cpuMin = (int)(cpuSec / 60.0);
      double cpuRem = cpuSec - cpuMin * 60.0;

      addLine(r, "  %-8s %04X  %3d  %3d:%05.2f  %5u  %5s  %s",
              jobname, ascb->ascbasid, ascb->ascbdph,
              cpuMin, cpuRem,
              ascb->ascbxcnt,
              "",  /* real frames would come from OUCB */
              status);
    } else {
      addLine(r, "  %-8s %04X  %s", jobname, ascb->ascbasid, status);
    }

    count++;
    ascb = (ASCB *)ascb->ascbfwdp;
  }

  addLine(r, "  %d ADDRESS SPACE(S) DISPLAYED", count);
}

/* ----------------------------------------------------------------
   D CSA
   ---------------------------------------------------------------- */

static void doDisplayCSA(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt) { r->rc = 4; return; }

  GDA *gda = (GDA *)cvt->cvtgda;
  if (!gda || memcmp(gda->gdaid, "GDA ", 4) != 0) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS GDA");
    r->rc = 4;
    return;
  }

  addLine(r, " IEE174I COMMON STORAGE UTILIZATION");
  addLine(r, "");
  addLine(r, "  CSA   BASE: 0x%08X   SIZE: %d K",
          (unsigned int)(uintptr_t)gda->gdacsa, gda->gdacsasz / 1024);
  addLine(r, "  ECSA  BASE: 0x%08X   SIZE: %d K",
          (unsigned int)(uintptr_t)gda->gdaecsa, gda->gdaecsas / 1024);
  addLine(r, "  CSA CONVERTED TO SQA: %d K", gda->gdacsacv / 1024);
}

/* ----------------------------------------------------------------
   D SQA
   ---------------------------------------------------------------- */

static void doDisplaySQA(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt) { r->rc = 4; return; }

  GDA *gda = (GDA *)cvt->cvtgda;
  if (!gda || memcmp(gda->gdaid, "GDA ", 4) != 0) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS GDA");
    r->rc = 4;
    return;
  }

  addLine(r, " IEE174I SHARED QUEUE AREA UTILIZATION");
  addLine(r, "");
  addLine(r, "  SQA   BASE: 0x%08X   SIZE: %d K",
          (unsigned int)(uintptr_t)gda->gdasqa, gda->gdasqasz / 1024);
  addLine(r, "  ESQA  BASE: 0x%08X   SIZE: %d K",
          (unsigned int)(uintptr_t)gda->gdaesqa, gda->gdaesqas / 1024);
}

/* ----------------------------------------------------------------
   D VIRTSTOR
   ---------------------------------------------------------------- */

static void doDisplayVIRTSTOR(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt) { r->rc = 4; return; }

  GDA *gda = (GDA *)cvt->cvtgda;
  if (!gda || memcmp(gda->gdaid, "GDA ", 4) != 0) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS GDA");
    r->rc = 4;
    return;
  }

  addLine(r, " IEE174I VIRTUAL STORAGE LAYOUT");
  addLine(r, "");
  addLine(r, "  AREA     BASE          SIZE (K)");
  addLine(r, "  ------   ----------    --------");
  addLine(r, "  SQA      0x%08X    %d",
          (unsigned int)(uintptr_t)gda->gdasqa, gda->gdasqasz / 1024);
  addLine(r, "  CSA      0x%08X    %d",
          (unsigned int)(uintptr_t)gda->gdacsa, gda->gdacsasz / 1024);
  addLine(r, "  ESQA     0x%08X    %d",
          (unsigned int)(uintptr_t)gda->gdaesqa, gda->gdaesqas / 1024);
  addLine(r, "  ECSA     0x%08X    %d",
          (unsigned int)(uintptr_t)gda->gdaecsa, gda->gdaecsas / 1024);
  addLine(r, "  LPVT     0x%08X    %d",
          (unsigned int)(uintptr_t)gda->gdapvt, gda->gdapvtsz / 1024);
  addLine(r, "  EPVT     0x%08X    %d",
          (unsigned int)(uintptr_t)gda->gdaepvt, gda->gdaepvts / 1024);

  int realK = cvt->cvtrlstg;
  addLine(r, "");
  addLine(r, "  REAL STORAGE AT IPL: %d K (%d MB)", realK, realK / 1024);
}

/* ----------------------------------------------------------------
   D SUBSYS
   ---------------------------------------------------------------- */

static void doDisplaySUBSYS(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt) { r->rc = 4; return; }

  JESCT *jesct = (JESCT *)cvt->cvtjesct;
  if (!jesct) {
    addLine(r, " ZDISPLAY ERROR: CANNOT ACCESS JESCT");
    r->rc = 4;
    return;
  }

  addLine(r, " IEF196I SUBSYSTEM NAMES AND STATUS");
  addLine(r, "  NAME  STATUS    SSVT     SUSE");

  SSCT *ssct = jesct->jesssct;
  int count = 0;
  while (ssct) {
    char name[5];
    memcpy(name, ssct->sname, 4);
    name[4] = '\0';

    addLine(r, "  %-4s  ACTIVE    %08X %08X",
            name,
            (unsigned int)(uintptr_t)ssct->ssvt,
            (unsigned int)ssct->ssctsuse);

    count++;
    ssct = ssct->scta;
    if (count > 500) break;  /* safety */
  }

  addLine(r, "  %d SUBSYSTEM(S) FOUND", count);
}

/* ----------------------------------------------------------------
   D T
   ---------------------------------------------------------------- */

static void doDisplayTime(ZDispResult *r) {
  CVT *cvt = getCVT();

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  if (!tm) {
    addLine(r, " ZDISPLAY ERROR: CANNOT GET TIME");
    r->rc = 4;
    return;
  }

  addLine(r, " IEE136I LOCAL: TIME=%02d.%02d.%02d DATE=%04d.%03d",
          tm->tm_hour, tm->tm_min, tm->tm_sec,
          tm->tm_year + 1900, tm->tm_yday + 1);

  if (cvt) {
    int tzOffsetSec = cvt->cvttz;
    int tzHours = tzOffsetSec / 3600;
    int tzMins = abs(tzOffsetSec % 3600) / 60;
    addLine(r, "  GMT OFFSET: %+03d:%02d", tzHours, tzMins);
  }

  /* Also show as formatted date */
  addLine(r, "  %04d-%02d-%02d %02d:%02d:%02d",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
          tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/* ----------------------------------------------------------------
   D RACF — RACF status from RCVT
   ---------------------------------------------------------------- */

/*
  Inline RCVT fields — from ichprcvt.h.
  Only the fields we display are mapped.
*/
/*
  RCVT field offsets verified against ichprcvt.h (#pragma pack(1)):
    rcvtid     +0x00  char[4]
    rcvtdsnl   +0x34  uint8_t    (after 12 ptr32 fields + int32)
    rcvtstat   +0x35  uint8_t    (bitfield union, 1 byte packed)
    rcvtnrec   +0x36  int16_t
    rcvtdsn    +0x38  char[44]
    rcvtuads   +0x64  char[44]
    rcvtuvol   +0x90  char[6]
    rcvtsta1   +0x96  uint8_t
    rcvtauop   +0x97  uint8_t
    rcvtaxta   +0x98  uint8_t
    rcvtflgs   +0x99  uint8_t
    rcvterop   +0x9A  uint8_t
    rcvtpinv   +0x9B  uint8_t
    rcvtvers   +0xAC  uint8_t    (after 4 ptr32 fields)
    rcvthist   +0xF0  uint8_t    (after many ptr32 fields)
    rcvtrvok   +0xF1
    rcvtwarn   +0xF2
    rcvtinac   +0xF3
    rcvtslen   +0xF4
    rcvtelen   +0xF5
    rcvtflg3   +0x279 uint8_t   (from zos.h, deep in struct)
*/
#pragma pack(packed)
typedef struct RCVT_disp_tag {
  char     rcvtid[4];        /* 0x000: "RCVT" */
  char     unmapped04[0x34 - 0x04];
  uint8_t  rcvtdsnl;         /* 0x034: length of RACF dataset name */
  uint8_t  rcvtstat;         /* 0x035: status byte */
  int16_t  rcvtnrec;         /* 0x036: records per track */
  char     rcvtdsn[44];      /* 0x038: dataset name of primary RACF DB */
  char     rcvtuads[44];     /* 0x064: UADS dataset name */
  char     rcvtuvol[6];      /* 0x090: UADS volume */
  uint8_t  rcvtsta1;         /* 0x096: protection options */
  uint8_t  rcvtauop;         /* 0x097: audit options */
  uint8_t  rcvtaxta;         /* 0x098: reserved */
  uint8_t  rcvtflgs;         /* 0x099: status flags */
  uint8_t  rcvterop;         /* 0x09A: terminal options */
  uint8_t  rcvtpinv;         /* 0x09B: max password interval */
  char     unmapped9C[0xAC - 0x9C];
  uint8_t  rcvtvers;         /* 0x0AC: version/release nibbles */
  char     unmappedAD[0xF0 - 0xAD];
  uint8_t  rcvthist;         /* 0x0F0: password history count */
  uint8_t  rcvtrvok;         /* 0x0F1: revoke attempts count */
  uint8_t  rcvtwarn;         /* 0x0F2: password warning days */
  uint8_t  rcvtinac;         /* 0x0F3: inactive interval days */
  uint8_t  rcvtslen;         /* 0x0F4: min password length */
  uint8_t  rcvtelen;         /* 0x0F5: max password length */
  char     unmappedF6[0x279 - 0xF6];
  uint8_t  rcvtflg3;         /* 0x279: misc flags */
} RCVT_disp;
#pragma pack(reset)

static void doDisplayRACF(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt || !cvt->cvtrac) {
    addLine(r, " ZDISPLAY ERROR: RACF CVT NOT ACCESSIBLE");
    r->rc = 4;
    return;
  }

  RCVT_disp *rcvt = (RCVT_disp *)cvt->cvtrac;

  /* Verify eyecatcher */
  if (memcmp(rcvt->rcvtid, "RCVT", 4) != 0) {
    addLine(r, " ZDISPLAY ERROR: RCVT EYECATCHER INVALID");
    r->rc = 4;
    return;
  }

  int version = (rcvt->rcvtvers >> 4) & 0x0F;
  int release = rcvt->rcvtvers & 0x0F;

  char dsname[45];
  trimField(rcvt->rcvtdsn, 44, dsname, 45);

  addLine(r, " ICH90001I RACF STATUS INFORMATION");
  addLine(r, "");
  addLine(r, "  RACF VERSION %d RELEASE %d", version + 1, release);
  addLine(r, "  RACF DATA SET: %s", dsname);
  addLine(r, "");
  addLine(r, "  STATUS:");
  addLine(r, "   RACF IS %s",
          (rcvt->rcvtstat & 0x80) ? "NOT ACTIVE" : "ACTIVE");
  addLine(r, "   RACF HAS %sBEEN DEACTIVATED BY RVARY",
          (rcvt->rcvtflgs & 0x80) ? "" : "NOT ");
  addLine(r, "");
  addLine(r, "  SETROPTS OPTIONS:");
  addLine(r, "   STATISTICS:  LOGON=%s  DATASET=%s  TAPEVOL=%s  DASDVOL=%s",
          (rcvt->rcvtstat & 0x40) ? "NO" : "YES",
          (rcvt->rcvtstat & 0x20) ? "NO" : "YES",
          (rcvt->rcvtstat & 0x10) ? "NO" : "YES",
          (rcvt->rcvtstat & 0x08) ? "NO" : "YES");
  addLine(r, "   PROTECTION:  TAPE=%s  DASD=%s  TERMINAL=%s",
          (rcvt->rcvtsta1 & 0x80) ? "YES" : "NO",
          (rcvt->rcvtsta1 & 0x40) ? "YES" : "NO",
          (rcvt->rcvterop & 0x80) ? "YES" : "NO");
  addLine(r, "   GENERIC DATASET: %s   GENERIC COMMANDS: %s",
          (rcvt->rcvtsta1 & 0x20) ? "YES" : "NO",
          (rcvt->rcvtsta1 & 0x10) ? "YES" : "NO");
  addLine(r, "   ADSP: %s   EGN: %s",
          (rcvt->rcvtstat & 0x02) ? "NO" : "YES",
          (rcvt->rcvtstat & 0x01) ? "YES" : "NO");
  addLine(r, "   GLOBAL ACCESS CHECKING: %s",
          (rcvt->rcvtflgs & 0x01) ? "YES" : "NO");
  addLine(r, "   DYNAMIC CDT: %s   LOWER-CASE PASSWORDS: %s",
          (rcvt->rcvtflg3 & 0x80) ? "YES" : "NO",
          (rcvt->rcvtflg3 & 0x40) ? "YES" : "NO");
  addLine(r, "");
  addLine(r, "  PASSWORD RULES:");
  addLine(r, "   MIN LENGTH: %d   MAX LENGTH: %d",
          rcvt->rcvtslen, rcvt->rcvtelen);
  addLine(r, "   HISTORY: %d   MAX INTERVAL: %d DAYS",
          rcvt->rcvthist, rcvt->rcvtpinv);
  addLine(r, "   REVOKE AFTER: %d ATTEMPTS   WARNING: %d DAYS",
          rcvt->rcvtrvok, rcvt->rcvtwarn);
  if (rcvt->rcvtinac > 0) {
    addLine(r, "   INACTIVE REVOKE: %d DAYS", rcvt->rcvtinac);
  }
}

/* ----------------------------------------------------------------
   D SMF — SMF recording status from SMCA
   ---------------------------------------------------------------- */

/*
  SMCA — read fields at known offsets via byte pointer.
  Offsets from ieesmca.h (struct smcabase, #pragma pack(1)):
    +0x00: smcaopt  (uint8) — background recording options
    +0x04: smcasmca (char[4]) — eyecatcher "SMCA"
    +0x10: smcasid  (char[4]) — system ID
    +0x52: smcafopt (uint8) — foreground recording options (decimal 82)
  smcaparm is deep and hard to verify; skip for now.
*/

static void doDisplaySMF(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt || !cvt->cvtsmca) {
    addLine(r, " ZDISPLAY ERROR: SMCA NOT ACCESSIBLE");
    r->rc = 4;
    return;
  }

  unsigned char *smca = (unsigned char *)ptr31(cvt->cvtsmca);

  /* Verify eyecatcher at +0x04 */
  if (memcmp(smca + 0x04, "SMCA", 4) != 0) {
    addLine(r, " ZDISPLAY ERROR: SMCA EYECATCHER INVALID (%.4s)",
            smca + 0x04);
    r->rc = 4;
    return;
  }

  char sid[5];
  trimField((char *)smca + 0x10, 4, sid, 5);

  uint8_t bgOpt = smca[0x00];
  uint8_t fgOpt = smca[0x52];

  addLine(r, " IEE900I SMF STATUS");
  addLine(r, "");
  addLine(r, "  SYSTEM ID: %s", sid);
  addLine(r, "");
  addLine(r, "  RECORDING OPTIONS (BACKGROUND):");
  addLine(r, "   JOB ACCOUNTING:     %s", (bgOpt & 0x80) ? "YES" : "NO");
  addLine(r, "   STEP ACCOUNTING:    %s", (bgOpt & 0x40) ? "YES" : "NO");
  addLine(r, "   EXITS:              %s", (bgOpt & 0x20) ? "YES" : "NO");
  addLine(r, "   DATA SET:           %s", (bgOpt & 0x10) ? "YES" : "NO");
  addLine(r, "   VOLUME:             %s", (bgOpt & 0x08) ? "YES" : "NO");
  addLine(r, "");
  addLine(r, "  RECORDING OPTIONS (FOREGROUND):");
  addLine(r, "   JOB ACCOUNTING:     %s", (fgOpt & 0x80) ? "YES" : "NO");
  addLine(r, "   STEP ACCOUNTING:    %s", (fgOpt & 0x40) ? "YES" : "NO");
  addLine(r, "   EXITS:              %s", (fgOpt & 0x20) ? "YES" : "NO");
  addLine(r, "   DATA SET:           %s", (fgOpt & 0x10) ? "YES" : "NO");
  addLine(r, "   VOLUME:             %s", (fgOpt & 0x08) ? "YES" : "NO");
}

/* ----------------------------------------------------------------
   D PROG,APF — APF authorized library list
   ---------------------------------------------------------------- */

/*
  The static APF table (CVT+0x144, cvtapftl) has format:
    +0: 2-byte count of entries
    +2: entries, each:  1-byte DSN length, N-byte DSN, 6-byte VOLSER
  Modern systems use the dynamic APF list (via CSVAPF service),
  but the static table is always readable without authorization.
*/

static void doDisplayAPF(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt) { r->rc = 4; return; }

  /* Try the static APF table first */
  char *apfTab = (char *)cvt->cvtapftl;
  if (!apfTab) {
    addLine(r, " ZDISPLAY: APF TABLE POINTER IS NULL");
    addLine(r, " SYSTEM MAY USE DYNAMIC APF LIST (CSVAPF)");
    r->rc = 4;
    return;
  }

  /* First 2 bytes = number of entries */
  int count = ((unsigned char)apfTab[0] << 8) | (unsigned char)apfTab[1];
  if (count <= 0 || count > 10000) {
    addLine(r, " ZDISPLAY: APF TABLE APPEARS EMPTY OR INVALID (COUNT=%d)", count);
    addLine(r, " SYSTEM MAY USE DYNAMIC APF LIST EXCLUSIVELY");
    r->rc = 4;
    return;
  }

  addLine(r, " CSV410I APF AUTHORIZED LIBRARY LIST");
  addLine(r, "  ENTRY  VOLUME  DATA SET NAME");

  char *ptr = apfTab + 2;
  int displayed = 0;
  for (int i = 0; i < count && displayed < 500; i++) {
    int dsnLen = (unsigned char)*ptr;
    ptr++;
    if (dsnLen <= 0 || dsnLen > 44) break;

    char dsname[45], volser[7];
    trimField(ptr, dsnLen, dsname, 45);
    ptr += dsnLen;
    trimField(ptr, 6, volser, 7);
    ptr += 6;

    displayed++;
    addLine(r, "  %4d   %-6s  %s", displayed, volser, dsname);
  }

  addLine(r, "  %d ENTR%s IN STATIC APF LIST",
          displayed, displayed == 1 ? "Y" : "IES");
}

/* ----------------------------------------------------------------
   D PROG,LNKLST — LNKLST concatenation from CSVDLCB
   ---------------------------------------------------------------- */

/*
  CSVDLCB — The LNKLST control block format is not well-documented
  publicly.  Rather than guessing at offsets, we'll hex-dump the
  first bytes to find the eyecatcher and entry chain.  For now,
  report what we can see safely.
*/

static void doDisplayLNKLST(ZDispResult *r) {
  ECVT *ecvt = getECVT();
  if (!ecvt || !ecvt->ecvtdlcb) {
    addLine(r, " ZDISPLAY ERROR: LNKLST CONTROL BLOCK NOT ACCESSIBLE");
    r->rc = 4;
    return;
  }

  unsigned char *dlcb = (unsigned char *)ptr31(ecvt->ecvtdlcb);

  /* The CSVDLCB may or may not have a "DLCB" eyecatcher.
     Scan first 32 bytes looking for it. */
  int eyeOff = -1;
  for (int i = 0; i <= 28; i++) {
    if (memcmp(dlcb + i, "DLCB", 4) == 0) {
      eyeOff = i;
      break;
    }
  }

  if (eyeOff < 0) {
    addLine(r, " ZDISPLAY: LNKLST CONTROL BLOCK AT 0x%08X",
            (unsigned int)(uintptr_t)dlcb);
    addLine(r, " EYECATCHER NOT FOUND — STRUCTURE FORMAT UNKNOWN");
    addLine(r, "");
    addLine(r, " NOTE: LNKLST display requires knowledge of the CSVDLCB");
    addLine(r, " internal layout which varies by z/OS release.");
    r->rc = 4;
    return;
  }

  addLine(r, " CSV450I LNKLST INFORMATION");
  addLine(r, "  DLCB AT 0x%08X (EYECATCHER AT +%d)",
          (unsigned int)(uintptr_t)dlcb, eyeOff);
  addLine(r, "");
  addLine(r, " NOTE: Detailed LNKLST dataset enumeration requires");
  addLine(r, " CSVQUERY service or documented CSVDLCB offsets.");
}

/* ----------------------------------------------------------------
   D PROG,LPA — LPA library list
   ---------------------------------------------------------------- */

/*
  The LPA list is managed by CSV services.  There's no simple
  linked-list structure like LNKLST.  The LPDE chain (IHALPDE)
  in ECVT->ecvtlpda is an option, but it lists individual modules,
  not library datasets.  For now, report what we can from CVT/ECVT.
*/

static void doDisplayLPA(ZDispResult *r) {
  CVT *cvt = getCVT();
  if (!cvt) { r->rc = 4; return; }

  addLine(r, " CSV460I LPA INFORMATION");
  addLine(r, "");
  addLine(r, "  LPA DIRECTORY: 0x%08X",
          (unsigned int)(uintptr_t)cvt->cvtlpdia);
  addLine(r, "");
  addLine(r, "  NOTE: Library-level LPA list display requires");
  addLine(r, "  CSVQUERY service.  Module-level display is available");
  addLine(r, "  via individual module lookup.");
}

/* ----------------------------------------------------------------
   D JOB — Job status using SSI 80
   ---------------------------------------------------------------- */

static void doDisplayJOB(ZDispResult *r, const char *arg) {
  /* Parse optional jobname/jobid filter after "JOB" */
  const char *filter = NULL;
  if (arg[3] == ',') {
    filter = arg + 4;
    while (*filter == ' ') filter++;
  }

  JobService *svc = NULL;
  int rc = jobServiceInit(&svc);
  if (rc != 0) {
    addLine(r, " ZDISPLAY ERROR: JOB SERVICE INIT FAILED RC=%d", rc);
    r->rc = 4;
    return;
  }

  JobServiceFilter jf;
  memset(&jf, 0, sizeof(jf));
  jf.detailLevel = JOB_DETAIL_TERSE;
  jf.maxJobs = 200;

  if (filter && filter[0]) {
    strncpy(jf.jobName, filter, 8);
    jf.jobName[8] = '\0';
    jf.flags = JOB_SELECT_BY_NAME;
  }

  JobInfo *jobs = NULL;
  int jobCount = 0;
  rc = jobServiceGetJobs(svc, &jf, &jobs, &jobCount);
  if (rc != 0) {
    addLine(r, " ZDISPLAY ERROR: JOB ENUMERATION FAILED RC=%d", rc);
    jobServiceTerm(svc);
    r->rc = 4;
    return;
  }

  addLine(r, " IEE403I JOB STATUS");
  addLine(r, "  JOBNAME  JOBID     OWNER    TYPE  STATUS    CLASS  DEST");

  JobInfo *job = jobs;
  int count = 0;
  while (job) {
    const char *phaseStr = "";
    if (job->phase == JOB_PHASE_INPUT || job->phase == JOB_PHASE_CONV)
      phaseStr = "INPUT";
    else if (job->phase == JOB_PHASE_EXEC || job->phase == JOB_PHASE_ONMAIN)
      phaseStr = "ACTIVE";
    else if (job->phase >= JOB_PHASE_OUTPT && job->phase <= JOB_PHASE_CMPLT)
      phaseStr = "OUTPUT";
    else if (job->phase == JOB_PHASE_POSTEX)
      phaseStr = "OUTPUT";
    else
      phaseStr = "OTHER";

    const char *typeStr = "";
    switch (job->jobType) {
    case JOB_TYPE_JOB:  typeStr = "JOB"; break;
    case JOB_TYPE_STC:  typeStr = "STC"; break;
    case JOB_TYPE_TSU:  typeStr = "TSU"; break;
    case JOB_TYPE_APPC: typeStr = "APPC"; break;
    default: typeStr = "???"; break;
    }

    addLine(r, "  %-8s %-8s  %-8s %-3s   %-8s  %-5s  %s",
            job->jobName, job->jobId, job->owner,
            typeStr, phaseStr, job->jobClass, job->printDest);

    count++;
    job = job->next;
  }

  addLine(r, "  %d JOB(S) DISPLAYED", count);

  if (jobs) jobServiceFreeJobs(svc, jobs);
  jobServiceTerm(svc);
}

/* ----------------------------------------------------------------
   D WLM — Workload Manager status (basic)
   ---------------------------------------------------------------- */

static void doDisplayWLM(ZDispResult *r) {
  ECVT *ecvt = getECVT();
  if (!ecvt) { r->rc = 4; return; }

  addLine(r, " IWM90001I WLM STATUS");
  addLine(r, "");
  addLine(r, "  WLM VECTOR TABLE: %s",
          ecvt->ecvtwlm ? "PRESENT" : "NOT AVAILABLE");
  addLine(r, "  GRS MODE: %s",
          ecvt->ecvtgmod == 0 ? "NONE" :
          ecvt->ecvtgmod == 1 ? "RING" :
          ecvt->ecvtgmod == 2 ? "STAR" : "UNKNOWN");
  addLine(r, "");
  addLine(r, "  NOTE: Detailed WLM service class and policy information");
  addLine(r, "  requires IWM4QRYS service (authorized).");
}

/* ----------------------------------------------------------------
   Command dispatcher
   ---------------------------------------------------------------- */

ZDispResult *zdispExecute(const char *cmdText) {
  ZDispResult *r = allocResult();
  if (!r) return NULL;

  /* Copy and normalize the command */
  char cmd[256];
  memset(cmd, 0, sizeof(cmd));

  /* Skip leading spaces */
  while (*cmdText == ' ') cmdText++;
  strncpy(cmd, cmdText, 255);

  /* Trim trailing spaces */
  int len = strlen(cmd);
  while (len > 0 && cmd[len - 1] == ' ') cmd[--len] = '\0';

  /* Uppercase for matching */
  char ucmd[256];
  strcpy(ucmd, cmd);
  upperCase(ucmd);

  /* Strip leading "D " or "DISPLAY " */
  char *arg = ucmd;
  if (strncmp(arg, "DISPLAY ", 8) == 0) {
    arg += 8;
  } else if (strncmp(arg, "D ", 2) == 0) {
    arg += 2;
  } else {
    snprintf(r->message, sizeof(r->message),
             "Unknown command format. Use 'D command' or 'DISPLAY command'.");
    r->rc = 8;
    addLine(r, " ZDISPLAY: %s", r->message);
    return r;
  }

  /* Skip spaces after D/DISPLAY */
  while (*arg == ' ') arg++;

  /* Dispatch */
  if (strcmp(arg, "IPLINFO") == 0) {
    doDisplayIPLINFO(r);
  } else if (strcmp(arg, "PARMLIB") == 0) {
    doDisplayPARMLIB(r);
  } else if (strcmp(arg, "SYMBOLS") == 0 || strcmp(arg, "SYM") == 0) {
    doDisplaySYMBOLS(r);
  } else if (strcmp(arg, "A,L") == 0) {
    doDisplayActive(r, 1);
  } else if (strcmp(arg, "A") == 0) {
    doDisplayActive(r, 0);
  } else if (strcmp(arg, "CSA") == 0) {
    doDisplayCSA(r);
  } else if (strcmp(arg, "SQA") == 0) {
    doDisplaySQA(r);
  } else if (strcmp(arg, "VIRTSTOR") == 0) {
    doDisplayVIRTSTOR(r);
  } else if (strcmp(arg, "SUBSYS") == 0) {
    doDisplaySUBSYS(r);
  } else if (strcmp(arg, "T") == 0) {
    doDisplayTime(r);
  } else if (strcmp(arg, "RACF") == 0) {
    doDisplayRACF(r);
  } else if (strcmp(arg, "SMF") == 0) {
    doDisplaySMF(r);
  } else if (strcmp(arg, "PROG,APF") == 0) {
    doDisplayAPF(r);
  } else if (strcmp(arg, "PROG,LNKLST") == 0) {
    doDisplayLNKLST(r);
  } else if (strcmp(arg, "PROG,LPA") == 0) {
    doDisplayLPA(r);
  } else if (strncmp(arg, "JOB", 3) == 0 &&
             (arg[3] == '\0' || arg[3] == ',')) {
    doDisplayJOB(r, arg);
  } else if (strcmp(arg, "WLM") == 0) {
    doDisplayWLM(r);
  } else {
    /* Check if it's a known but unimplemented command */
    int found = 0;
    for (int i = 0; i < (int)CATALOG_COUNT; i++) {
      char catCmd[64];
      strcpy(catCmd, catalog[i].syntax);
      upperCase(catCmd);
      /* Skip past "D " in catalog syntax */
      char *catArg = catCmd + 2;
      if (strcmp(arg, catArg) == 0) {
        found = 1;
        addLine(r, " ZDISPLAY: COMMAND '%s' RECOGNIZED BUT NOT YET IMPLEMENTED", cmd);
        if (catalog[i].argHelp) {
          addLine(r, " SYNTAX: %s", catalog[i].argHelp);
        }
        r->rc = 4;
        break;
      }
    }
    if (!found) {
      addLine(r, " ZDISPLAY: UNKNOWN COMMAND '%s'", cmd);
      addLine(r, " USE 'HELP' TO LIST AVAILABLE COMMANDS");
      r->rc = 8;
    }
  }

  return r;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
