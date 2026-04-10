

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __ASINFO__
#define __ASINFO__ 1

#include <stdint.h>

/*
  Address space info — one per active ASID.
  Populated by walking CVT -> ASVT -> ASCB chain.
  Fields come from ASCB, ASXB, ASSB, JSAB, and OUCB.
*/
typedef struct ASInfo_tag {
  char     jobName[9];      /* job/STC/TSU name, null-terminated */
  uint16_t asid;            /* address space ID */
  int16_t  dispPriority;    /* dispatch priority (ascbdph) */
  uint64_t cpuTime;         /* elapsed job step time in TOD units (ascbejst) */
  uint64_t cpuTimePrev;     /* previous snapshot for rate calc */
  uint16_t excpCount;       /* EXCP count (ascbxcnt) */
  uint8_t  dsp1;            /* dispatch flags (ascbdsp1) */
  uint32_t realFrames;      /* real storage frames (from OUCB) */
  uint32_t ioCount;         /* I/O count (ascbiosc) */
  uint8_t  isSystem;        /* 1 if system/master addr space */
  uint8_t  isTSO;           /* 1 if TSO user (has TSB) */
  uint8_t  isSTC;           /* 1 if started task */
  char     userid[9];       /* TSO userid from ASXB (asxbuser) */
  /* computed fields (updated on refresh) */
  double   cpuPercent;      /* CPU% since last sample */
  double   ioRate;          /* I/O per second since last sample */
  struct ASInfo_tag *next;
} ASInfo;

/*
  Initialize and populate address space list.
  Walks the ASVT and returns a linked list of ASInfo.
  Caller must free with asInfoFree().
  Returns number of address spaces found, or -1 on error.
*/
int asInfoGetAll(ASInfo **result);

/*
  Re-scan address spaces and compute rate-based metrics.
  Pass the previous list; it will be freed and replaced.
  elapsedMicros is microseconds since last call (for rate calc).
  Returns new count, or -1 on error.
*/
int asInfoRefresh(ASInfo **list, uint64_t elapsedMicros);

/*
  Free a chain of ASInfo.
*/
void asInfoFree(ASInfo *list);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
