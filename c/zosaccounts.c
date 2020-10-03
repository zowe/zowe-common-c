
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
# pragma linkage(BPX4GGI,OS)
# pragma linkage(BPX4GPN,OS)
# pragma linkage(BPX4GPU,OS)
# pragma linkage(BPX4PWD,OS)

# define BPXGGN BPX4GGN
# define BPXGGI BPX4GGI
# define BPXGPN BPX4GPN
# define BPXGPU BPX4GPU
# define BPXPWD BPX4PWD

#else
# pragma linkage(BPX1GGN,OS)
# pragma linkage(BPX1GGI,OS)
# pragma linkage(BPX1GPN,OS)
# pragma linkage(BPX1GPU,OS)
# pragma linkage(BPX1PWD,OS)

# define BPXGGN BPX1GGN
# define BPXGGI BPX1GGI
# define BPXGPN BPX1GPN
# define BPXGPU BPX1GPU
# define BPXPWD BPX1PWD
#endif



/* Obtain the user information structure from user name */
UserInfo *gidGetUserInfo(const char *userName, int *returnCode, int *reasonCode) {
  int nameLength = strlen(userName);
  int *reasonCodePtr;
  int returnValue;
  int retValue = -1;
  UserInfo *info = NULL;

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
    info = (UserInfo*) returnValue;
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

  return info;
}

/* Obtain the user information structure using UID */
UserInfo *getUserInfo(int uid, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;
  int retValue = -1;

  UserInfo *info = NULL;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXGPU(uid,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (returnValue != 0) {
    info = (UserInfo*) returnValue;
  }

  if (accountTrace) {
    if(returnValue == 0) {
#ifdef METTLE
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG,
              "BPXGPU (%d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",
              uid, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG,
              "BPXGPU (%d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
              uid, returnValue, *returnCode, *reasonCode, 
              strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, "BPXGPU (%d) OK: returnVal: %d\n", uid, returnValue);
    }
  }

  return info;
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
GroupInfo *gidGetGroupInfo(const char *groupName, int *returnCode, int *reasonCode) {
  int groupLength = strlen(groupName);
  int *reasonCodePtr;
  int returnValue;
  int retValue = -1;
  *returnCode = *reasonCode = 0;
  GroupInfo *info = NULL;

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
    info = (GroupInfo *) returnValue;
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

  return info;
}

/* Obtain the group information structure using GID */
GroupInfo *getGroupInfo(int gid, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;
  int retValue = -1;
  *returnCode = *reasonCode = 0;
  GroupInfo *info = NULL;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXGGI(gid,
         &returnValue,
         returnCode,
         reasonCodePtr);

  if (returnValue >  0) {
    info = (GroupInfo*) returnValue;
  }

  if (accountTrace) {
    if(returnValue == 0) {
#ifdef METTLE
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, 
              "BPXGGI (%d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n",,
              gid, returnValue, *returnCode, *reasonCode);
#else
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, 
              "BPXGGI (%d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n",
              gid, returnValue, *returnCode, *reasonCode, 
              strerror(*returnCode));
#endif
    }
    else {
      zowelog(NULL, LOG_COMP_ZOS, ZOWE_LOG_DEBUG, "BPXGGI (%d) OK: returnVal: %d\n", gid, returnValue);
    }
  }

  return info;
}

/* Return userId from user info structure */
int userInfoGetUserId (UserInfo *info) {
  return info->GIDN_USERID;
}

/* Copy user name from User Info structure into userNameBuffer */
void userInfoGetUserName (UserInfo *info, char *userNameBuffer) {
  memcpy(userNameBuffer, info->GIDN_U_NAME, info->GIDN_U_LEN);
}

/* Return groupId from group info structure */
int groupInfoGetGroupId (GroupInfo *info) {
  return info->GIDS_GROUPID;
}

/* Copy group name from Group Info structure into groupNameBuffer*/
void groupInfoGetGroupName (GroupInfo *info, char *groupNameBuffer) {
  memcpy(groupNameBuffer, info->GIDS_G_NAME, info->GIDS_G_LEN);
}

/* Obtain userId from character string */
int userIdGet (char *string, int *returnCode, int *reasonCode) {
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
    UserInfo  *userInfo = gidGetUserInfo(string, returnCode, reasonCode);
    if (userInfo != NULL) {
      userId = userInfoGetUserId ( userInfo);
    }
  }
  return userId;
}

/* Obtain user name by UID. userNameBuffer must be at least 8 chars long */
int userGetName(int uid, char *userNameBuffer, int *returnCode, int *reasonCode) {
  int status = -1;
  UserInfo *userInfo = getUserInfo(uid, returnCode, reasonCode);
  if (userInfo != NULL) {
    status = 0;
    userInfoGetUserName(userInfo, userNameBuffer);
  }
  return status;
}

/* Obtain groupId from character string */
int groupIdGet (char *string, int *returnCode, int *reasonCode) {
  int groupId = -1;
  
  /* Evaluate user ID */
  if (string == NULL) {
    groupId = -1;
  }
  else if (stringIsDigit(string)) {
    groupId = atoi(string);
  }
  else {
    /* get group info by name */
    GroupInfo *groupInfo = gidGetGroupInfo(string, returnCode, reasonCode);
    if (groupInfo != NULL) {
      groupId = groupInfoGetGroupId (groupInfo);
    }
  }
  return groupId;
}

/* Obtain group name by GID, groupNameBuffer must be at least 8 chars long */
int groupGetName(int gid, char *groupNameBuffer, int *returnCode, int *reasonCode) {
  int status = -1;
  GroupInfo *groupInfo = getGroupInfo(gid, returnCode, reasonCode);
  if (groupInfo != NULL) {
    status = 0;
    groupInfoGetGroupName(groupInfo, groupNameBuffer);
  }
  return status;
}


/*
  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this distribution,
  and is available at https://www.eclipse.org/legal/epl-v20.hml

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

