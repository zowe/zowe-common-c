

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
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include "stddef.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "logging.h"
#include "lpa.h"
#include "utils.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS is supported only
#endif

#ifndef METTLE
/* TODO LE support */
#error LE is not supported
#endif

#ifndef LPA_LOG_DEBUG_MSG_ID
#define LPA_LOG_DEBUG_MSG_ID ""
#endif

__asm("GLBDYLPA    CSVDYLPA MF=(L,GLBDYLPA)" : "DS"(GLBDYLPA));

int lpaAdd(LPMEA * __ptr32 lpmea,
           EightCharString  * __ptr32 ddname,
           EightCharString  * __ptr32 dsname,
           int *reasonCode) {

  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" about to add %8.8s - %8.8s to LPA\n",
          ddname->text, dsname->text);

  __asm(" CSVDYLPA MF=L" : "DS"(csvdylpaParmList));
  csvdylpaParmList = GLBDYLPA;

  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" csvdylpaParmList @ 0x%p, size=%zu\n",
          &csvdylpaParmList, sizeof(csvdylpaParmList));
  zowedump(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
           (char *)&csvdylpaParmList, sizeof(csvdylpaParmList));

  memset(lpmea, 0, sizeof(LPMEA));
  memcpy(lpmea->inputInfo.name, dsname->text, 8);
  lpmea->inputInfo.inputFlags0 |= LPMEA_INPUT_FLAGS0_FIXED;

  char requestor[16];
  memset(requestor, ' ', sizeof(requestor));
  memcpy(requestor, "ROCKET CM SERVER", sizeof(requestor));

  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" lpmea @ 0x%p, size=%zu\n",
          lpmea, sizeof(LPMEA));
  zowedump(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG, (char *)lpmea, sizeof(LPMEA));

  int moduleCount = 1;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         CSVDYLPA REQUEST=ADD,MODINFOTYPE=MEMBERLIST,"
      "MODINFO=(%2),NUMMOD=(%3),"
      "DDNAME=(%4),"
      "SECMODCHECK=NO,"
      "REQUESTOR=(%5),"
      "RETCODE=%0,RSNCODE=%1,"
      "MF=(E,(%6))\n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif

      : "=m"(rc), "=m"(rsn)
        : "r"(lpmea), "r"(&moduleCount), "r"(&ddname->text),
          "r"(&requestor), "r"(&csvdylpaParmList)
          : "r0", "r1", "r14", "r15"
  );

  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" lpmea rc=%d, rsn=%d\n", rc, rsn);
  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" lpmea @ 0x%p, size=%zu\n",
          lpmea, sizeof(LPMEA));
  zowedump(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG, (char *)lpmea, sizeof(LPMEA));

  *reasonCode = rsn;
  return rc;
}

int lpaDelete(LPMEA * __ptr32 lpmea, int *reasonCode) {

  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" about to delete using LPMEA @ 0x%p\n", lpmea);
  zowedump(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG, (char *)lpmea, sizeof(LPMEA));

  __asm(" CSVDYLPA MF=L" : "DS"(csvdylpaParmList));
  csvdylpaParmList = GLBDYLPA;

  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" csvdylpaParmList @ 0x%p, size=%zu\n",
          &csvdylpaParmList, sizeof(csvdylpaParmList));
  zowedump(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
           (char *)&csvdylpaParmList, sizeof(csvdylpaParmList));

  LPMED lpmed;
  memset(&lpmed, 0, sizeof(LPMED));
  memcpy(lpmed.inputInfo.name, lpmea->inputInfo.name,
         sizeof(lpmed.inputInfo.name));
  memcpy(lpmed.inputInfo.deleteToken,
         lpmea->outputInfo.stuff.successInfo.deleteToken,
         sizeof(lpmed.inputInfo.deleteToken));

  int moduleCount = 1;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         CSVDYLPA REQUEST=DELETE,"
      "MODINFO=(%2),NUMMOD=(%3),SECMODCHECK=NO,"
      "RETCODE=%0,RSNCODE=%1,"
      "MF=(E,(%4))\n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif

      : "=m"(rc), "=m"(rsn)
        : "r"(&lpmed), "r"(&moduleCount), "r"(&csvdylpaParmList)
          : "r0", "r1", "r14", "r15"
  );

  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" lpmed rc=%d, rsn=%d\n", rc, rsn);
  zowelog(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG,
          LPA_LOG_DEBUG_MSG_ID" lpmed @ 0x%p, size=%zu\n",
          &lpmed, sizeof(LPMED));
  zowedump(NULL, LOG_COMP_LPA, ZOWE_LOG_DEBUG, (char *)&lpmed, sizeof(LPMED));

  *reasonCode = rsn;
  return rc;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

