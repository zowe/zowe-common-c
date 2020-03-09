

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
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>  
#include "metalio.h"
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#endif /* METTLE */

#include "zowetypes.h"
#include "utils.h"

#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif 

#include "impersonation.h"

int startNativeImpersonation(char *username, void *userPointer, void *resultPointer){
#ifdef __ZOWE_OS_ZOS
  return startSafImpersonation(username, (ACEE**)userPointer);
#else
  printf("Implement me: impersonation code for this platform\n");
  return TRUE;
#endif
}
int endNativeImpersonation(char *username, void *userPointer, void *resultPointer){
#ifdef __ZOWE_OS_ZOS
  return endSafImpersonation((ACEE**)userPointer);
#else
  printf("Implement me: impersonation code for this platform\n");
  return TRUE;
#endif
}

#ifdef __ZOWE_OS_ZOS
int startSafImpersonation(char *username, ACEE **newACEE) {
  int racfStatus = 0;
  int racfReason = 0;  
  int impersonationBegan = FALSE;
  int safStatus = safVerify(VERIFY_CREATE|VERIFY_WITHOUT_PASSWORD,
                          username,
                          NULL,
                          newACEE,
                          &racfStatus,
                          &racfReason);
  printf("VERIFY call on %s safStatus %x racfStatus %x reason %x\n",username,safStatus,racfStatus,racfReason);
  if (!safStatus){
    impersonationBegan = TRUE;
  }
  else {
    printf("Failed to do saf Verify\n");
    int safStatus = safVerify(VERIFY_DELETE,NULL,NULL,newACEE,&racfStatus,&racfReason);    
  }  
  return impersonationBegan;
}
int endSafImpersonation(ACEE **acee) {
  int racfStatus = 0;
  int racfReason = 0;    
  int endedImpersonation;
  int safStatus = safVerify(VERIFY_DELETE,NULL,NULL,acee,&racfStatus,&racfReason);    
  endedImpersonation = !safStatus;
  if (!endedImpersonation) {
    printf("**PANIC: Could not end impersonation!\n");
  }
  return endedImpersonation;
}

#define __CREATE_SECURITY_ENV          1
#define __DELETE_SECURITY_ENV          2
#define __TLS_TASK_ACEE                3
#define __TLS_TASK_ACEE_USP            4
#define __DAEMON_SECURITY_ENV          5
#define __USERID_IDENTITY              1
#define __CERTIFICATE_IDENTITY         4

#ifdef _LP64
#pragma linkage(BPX4TLS,OS)
#define BPXTLS BPX4TLS
#else
#pragma linkage(BPX1TLS, OS)
#define BPXTLS BPX1TLS
#endif /*_LP64 */

/*
 * The BPX version is really the one to use unless you have a reviewed need to do something in SAF itself
 */
int bpxImpersonate(char *userid, char *passwordOrNull, int start, int trace) {
  int returnValue = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int *reasonCodePtr;
  int useridLen = strlen(userid);
  int passwordLen = passwordOrNull ? strlen(passwordOrNull) : 0;
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int) &reasonCode));
#else
  reasonCodePtr = &reasonCode;
#endif /*_LP64 */
  if (start) {
    if (passwordOrNull) {
      BPXTLS(__CREATE_SECURITY_ENV, __USERID_IDENTITY,
              useridLen, userid,
              passwordLen, passwordOrNull,
              0,
              &returnValue,
              &returnCode,
              reasonCodePtr);
    } else {
      BPXTLS(__DAEMON_SECURITY_ENV, __USERID_IDENTITY,
              useridLen, userid,
              passwordLen, passwordOrNull,
              0,
              &returnValue,
              &returnCode,
              reasonCodePtr);
    }
  } else {
    BPXTLS(__DELETE_SECURITY_ENV, __USERID_IDENTITY,
            useridLen, userid,
            0, NULL,
            0,
            &returnValue,
            &returnCode,
            reasonCodePtr);
  }
  if (returnValue != 0) {
    printf("BPXTLS failed: rc=%d, return code=%d, reason code=0x%08x\n",
            returnValue, returnCode, reasonCode);
  } else {
    if (trace) {
      printf("BPXTLS OK\n");
    }
  }
  return returnValue;
}

int tlsImpersonate(char *userid, char *passwordOrNull, int start, int trace) {
  int returnValue = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int *reasonCodePtr;
  int useridLen = strlen(userid);
  int passwordLen = passwordOrNull ? strlen(passwordOrNull) : 0;
  ACEE *acee = NULL;
  int racfStatus = 0;
  int racfReason = 0;
  int safStatus = 0;
  int options;
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int) &reasonCode));
#else
  reasonCodePtr = &reasonCode;
#endif /*_LP64 */
  if (trace) {
    printf ("begin %s userid '%s' start %s\n", __FUNCTION__, userid, start ? "TRUE" : "FALSE");
  }
  if (start) {
    options = VERIFY_CREATE;
    if (passwordOrNull == NULL) {
      options |= VERIFY_WITHOUT_PASSWORD;
    }
    safStatus = safVerify(options, userid, passwordOrNull, &acee, &racfStatus, &racfReason);
    if (!safStatus) {
      if (trace) {
        printf("SAF VERIFY CREATE OK\n");
      }
      setTaskAcee(acee);
      int wasProblemState = supervisorMode(TRUE);
      int oldKey = setKey(0);
      BPXTLS(__TLS_TASK_ACEE, __USERID_IDENTITY,
              useridLen, userid,
              passwordLen, passwordOrNull,
              0,
              &returnValue,
              &returnCode,
              reasonCodePtr);
      setKey(oldKey);
      if (wasProblemState) {
        supervisorMode(FALSE);
      }
      if (returnValue != 0) {
        printf("BPXTLS TLS_TASK_ACEE failed: rc=%d, return code=%d, reason code=0x%08x\n",
                returnValue, returnCode, reasonCode);
      } else {
        if (trace) {
          printf("BPXTLS TLS_TASK_ACEE OK\n");
        }
      }
    } else {
      printf("SAF VERIFY CREATE failed: safStatus %d racf status 0x%08x reason 0x%08x\n",
              safStatus, racfStatus, racfReason);
      returnValue = -1;
    }
  } else {
    BPXTLS(__DELETE_SECURITY_ENV, __USERID_IDENTITY,
            useridLen, userid,
            0, NULL,
            0,
            &returnValue,
            &returnCode,
            reasonCodePtr);
    if (returnValue != 0) {
      printf("BPXTLS DELETE_SECURITY_ENV failed: rc=%d, return code=%d, reason code=0x%08x\n",
              returnValue, returnCode, reasonCode);
    } else {
      if (trace) {
        printf("BPXTLS DELETE_SECURITY_ENV OK\n");
      }
    }
    int safStatus = safVerify(VERIFY_DELETE, userid, NULL, &acee, &racfStatus, &racfReason);
    if (!safStatus) {
      if (trace) {
        printf("SAF VERIFY DELETE OK\n");
      }
      setTaskAcee(acee);
    } else {
      printf("SAF VERIFY DELETE failed: safStatus %d racf status 0x%08x reason 0x%08x\n",
              safStatus, racfStatus, racfReason);
      returnValue = -1;
    }
  }
  if (trace) {
    printf ("end %s returnValue %d\n", __FUNCTION__, returnValue);
  }
  return returnValue;
}

#endif /*__ZOWE_OS_ZOS */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
