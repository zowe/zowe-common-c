
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accomp
anies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.h
tml

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <errno.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "unixfile.h"
#include "timeutls.h"
#include "logging.h"

static int accountTrace = FALSE;

#ifdef _LP64
# pragma linkage(BPX4GGN,OS)
# pragma linkage(BPX4GPN,OS)
# pragma linkage(BPX4PWD,OS)

# define BPXGGN BPX4GGN
# define BPXGPN BPX4GPN
# define BPXPWD BPX4PWD

#else
# pragma linkage(BPX1GGN,OS)
# pragma linkage(BPX1GPN,OS)
# pragma linkage(BPX1PWD,OS)

# define BPXGGN BPX1GGN
# define BPXGPN BPX1GPN
# define BPXPWD BPX1PWD
#endif



/* Obtain the user information structure from user name */
int gidGetUserInfo(const char *userName,  UserInfo * info,
                         int *returnCode, int *reasonCode) {
  int nameLength = strlen(userName);
  int *reasonCodePtr;
  int returnValue;
  int retValue = -1;

  UserInfo *ptrInfo;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXGPN(&nameLength,
         userName,
         &returnValue,
         returnCode,
         reasonCodePtr);

  /* Copy returned structure */
  if (returnValue != 0) {
    memcpy (info, (char *)returnValue, sizeof (UserInfo));
    retValue = 0;
  }

  if (accountTrace) {
    if(returnValue == 0) {
#ifdef METTLE
      /* Do not add a comma */
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG,
              "BPXGPN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
              userName, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG,
              "BPXGPN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
              userName, returnValue, *returnCode, *reasonCode, 
              strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, "BPXGPN (%s) OK: returnVal: %d\n", userName, returnValue);
    }
  }

  return retValue;
}

/* Obtain the user information structure from user name */
int resetZosUserPassword(const char *userName,  const char *password, const char *newPassword,
                         int *returnCode, int *reasonCode) {
  int nameLength = strlen(userName);
  int passwordLength = strlen(password);
  int newPasswordLength = strlen(newPassword);
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXPWD(&nameLength,
         userName,
         &passwordLength,
         password,
         &newPasswordLength,
         newPassword,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (accountTrace) {
    if(returnValue == 0) {
#ifdef METTLE
      /* Do not add a comma */
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG,
              "BPXPWD (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
              userName, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG,
              "BPXPWD (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
              userName, returnValue, *returnCode, *reasonCode,
              strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG,"BPXPWD (%s) OK: returnVal: %d\n", userName, returnValue);
    }
  }

  return returnValue;
}

/* Obtain the group information structure from group name */
int gidGetGroupInfo(const char *groupName,  GroupInfo *info,
                   int *returnCode, int *reasonCode) {
  int groupLength = strlen(groupName);
  int *reasonCodePtr;
  int returnValue;
  int retValue = -1;
  *returnCode = *reasonCode = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXGGN(&groupLength,
         groupName,
         &returnValue,
         returnCode,
         reasonCodePtr);

  /* Copy returned structure */
  if (returnValue >  0) {
    memcpy (info, (char *)returnValue, sizeof (GroupInfo));
    retValue = 0;
  }

  if (accountTrace) {
    if(returnValue == 0) {
#ifdef METTLE
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, 
              "BPXGGN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",,
              groupName, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, 
              "BPXGGN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
              groupName, returnValue, *returnCode, *reasonCode, 
              strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, "BPXGGN (%s) OK: returnVal: %d\n", groupName, returnValue);
    }
  }

  return retValue;
}

/* Return userId from user info structure */
int userInfoGetUserId (UserInfo *info) {
  int *temp = (int *)info;
  int unameLength = info->GIDN_U_LEN;
  int unameLengthindex = (unameLength + 3) /4;
  int userId = temp[unameLengthindex + 2];
  return userId;
}

/* Return groupId from group info structure */
int groupInfoGetGroupId (GroupInfo *info) {
  int *temp = (int *)info;
  int groupLength = info->GIDS_G_LEN;
  int groupLengthindex = (groupLength + 3) /4;
  int groupId = temp[groupLengthindex + 2];
  return groupId;
}

/* Obtain userId from character string */
int userIdGet (char *string, int *returnCode, int *reasonCode) {
  UserInfo  userInfo = {0};
  int userId = -1;
  int status;
  
  /* Evaluate user ID */
  if (string == NULL) {
    userId = -1;
  }
  else if (stringIsDigit(string)) {
    userId = atoi(userId);
  }
  else {
    /* get user info by name */
    status = gidGetUserInfo(string, &userInfo, returnCode, reasonCode);
    if (status == 0) {
      userId = userInfoGetUserId ( &userInfo);
    }
  }
  return userId;
}

/* Obtain groupId from character string */
int groupIdGet (char *string, int *returnCode, int *reasonCode) {
  GroupInfo  groupInfo = {0};
  int groupId = -1;
  int status;
  
  /* Evaluate user ID */
  if (string == NULL) {
    groupId = -1;
  }
  else if (stringIsDigit(string)) {
    groupId = atoi(string);
  }
  else {
    /* get group info by name */
    status = gidGetGroupInfo(string, &groupInfo, returnCode, reasonCode);
    if (status == 0) {
      groupId = groupInfoGetGroupId (&groupInfo);
    }
  }
  return groupId;
}


/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this distribution,
  and is available at https://www.eclipse.org/legal/epl-v20.hml

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

