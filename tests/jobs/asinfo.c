

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  asinfo.c — Walk CVT -> ASVT -> ASCB chain to enumerate active address spaces.
  Reads SQA-resident control blocks (ASCB, ASXB, ASSB, JSAB) which are
  readable from unauthorized programs.

  OUCB (SRM user control block) provides real storage frame counts.
  OUCB is at ascboucb; it's not fully mapped in zos.h so we read
  specific offsets manually.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zowetypes.h"
#include "zos.h"
#include "asinfo.h"

/*
  OUCB real frame count.
  The OUCB (IRAOUCB) is pointed to by ascboucb.
  Offset 0x58 (OUCBFMC) holds the real frame count as a 32-bit integer
  on z/OS 2.4+.  Earlier releases had this at the same offset.
  We read it carefully with a validity check on the eyecatcher.
*/
#define OUCB_EYECATCHER_OFFSET  0x00
#define OUCB_REALFRAMES_OFFSET  0x58

static uint32_t getOUCBRealFrames(void *oucbPtr) {
  if (!oucbPtr) return 0;

  /* Check eyecatcher "OUCB" at offset 0 */
  char *p = (char *)oucbPtr;
  if (p[0] != 'O' || p[1] != 'U' || p[2] != 'C' || p[3] != 'B') {
    return 0;
  }

  uint32_t *framePtr = (uint32_t *)(p + OUCB_REALFRAMES_OFFSET);
  return *framePtr;
}

/*
  Build a single ASInfo entry from an ASCB.
*/
static ASInfo *buildASInfo(ASCB *ascb) {
  ASInfo *info = (ASInfo *)malloc(sizeof(ASInfo));
  if (!info) return NULL;
  memset(info, 0, sizeof(ASInfo));

  /* ASID */
  info->asid = ascb->ascbasid;

  /* Dispatch priority */
  info->dispPriority = ascb->ascbdph;

  /* CPU time (TOD format, bit 51 = 1 microsecond) */
  info->cpuTime = ascb->ascbejst;

  /* EXCP count */
  info->excpCount = ascb->ascbxcnt;

  /* I/O count */
  info->ioCount = ascb->ascbiosc;

  /* Dispatch flags */
  info->dsp1 = ascb->ascbdsp1;

  /* TSO check: has TSB */
  info->isTSO = (ascb->ascbtsb != 0) ? 1 : 0;

  /* STC check: has JBNS but not JBNI and not TSB */
  info->isSTC = (!info->isTSO && ascb->ascbjbns != 0 && ascb->ascbjbni == 0) ? 1 : 0;

  /* System address space: ASID <= 3 or no jobname at all */
  info->isSystem = (ascb->ascbasid <= 3) ? 1 : 0;

  /* Jobname — use getASCBJobname from zos.c */
  char *jn = getASCBJobname(ascb);
  if (jn) {
    memcpy(info->jobName, jn, 8);
    info->jobName[8] = '\0';
    /* trim trailing spaces */
    for (int i = 7; i >= 0 && info->jobName[i] == ' '; i--) {
      info->jobName[i] = '\0';
    }
  } else {
    strcpy(info->jobName, "*NONE*");
  }

  /* Userid from ASXB */
  ASXB *asxb = ascb->ascbasxb;
  if (asxb) {
    memcpy(info->userid, asxb->asxbuser, 8);
    info->userid[8] = '\0';
    for (int i = 7; i >= 0 && info->userid[i] == ' '; i--) {
      info->userid[i] = '\0';
    }
  }

  /* Real frames from OUCB */
  info->realFrames = getOUCBRealFrames((void *)INT2PTR(ascb->ascboucb));

  return info;
}

/*
  Walk CVT -> ASVT -> ASCB chain.
*/
int asInfoGetAll(ASInfo **result) {
  *result = NULL;

  CVT *cvt = getCVT();
  if (!cvt) return -1;

  ASVT *asvt = (ASVT *)cvt->cvtasvt;
  if (!asvt) return -1;

  int count = 0;
  ASInfo *head = NULL;
  ASInfo *tail = NULL;

  ASCB *ascb = (ASCB *)INT2PTR(asvt->asvtenty);
  while (ascb) {
    /* Quick validity check */
    if (ascb->ascbascb[0] != 'A' || ascb->ascbascb[1] != 'S' ||
        ascb->ascbascb[2] != 'C' || ascb->ascbascb[3] != 'B') {
      break;
    }

    ASInfo *info = buildASInfo(ascb);
    if (info) {
      if (!head) {
        head = tail = info;
      } else {
        tail->next = info;
        tail = info;
      }
      count++;
    }

    ascb = (ASCB *)ascb->ascbfwdp;
  }

  *result = head;
  return count;
}

/*
  Refresh: re-walk and compute rate-based metrics.
  The old list provides previous cpuTime snapshots for delta calculation.
*/
int asInfoRefresh(ASInfo **list, uint64_t elapsedMicros) {
  ASInfo *oldList = *list;
  ASInfo *newList = NULL;
  int count = asInfoGetAll(&newList);
  if (count < 0) return -1;

  /* Match old entries by ASID to compute deltas */
  if (elapsedMicros > 0 && oldList) {
    for (ASInfo *cur = newList; cur; cur = cur->next) {
      /* Find matching ASID in old list */
      for (ASInfo *old = oldList; old; old = old->next) {
        if (old->asid == cur->asid) {
          cur->cpuTimePrev = old->cpuTime;

          /* CPU% = delta CPU time / elapsed time.
             ascbejst is in TOD units: bit 51 = 1 microsecond.
             So 1 microsecond = 4096 TOD units. */
          if (cur->cpuTime >= old->cpuTime) {
            uint64_t deltaTOD = cur->cpuTime - old->cpuTime;
            double deltaMicros = (double)deltaTOD / 4096.0;
            cur->cpuPercent = (deltaMicros / (double)elapsedMicros) * 100.0;
          }

          /* I/O rate */
          if (cur->ioCount >= old->ioCount) {
            double deltaIO = (double)(cur->ioCount - old->ioCount);
            cur->ioRate = deltaIO / ((double)elapsedMicros / 1000000.0);
          }
          break;
        }
      }
    }
  }

  asInfoFree(oldList);
  *list = newList;
  return count;
}

void asInfoFree(ASInfo *list) {
  while (list) {
    ASInfo *next = list->next;
    free(list);
    list = next;
  }
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
