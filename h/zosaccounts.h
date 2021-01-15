/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOSACCOUNTS__
# define __ZOSACCOUNTS__

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "unixfile.h"

#define USER_NAME_LEN  8
#define GROUP_NAME_LEN 8

/* Structure definitioss */
#pragma pack(packed)
typedef struct BPXYGIDS_tag {
  int     GIDS_G_LEN;                  // Group name length (always 8)
  char    GIDS_G_NAME[GROUP_NAME_LEN]; // Group name
  int     GIDS_G_LEN_ID;               // Group ID length (always 4)
  int     GIDS_GROUPID;                // Group ID
  int     GIDS_COUNT;                  // Count of array element
  struct {
    int   GIDS_M_LEN;                  // Length of member name (always 8)
    char  GIDS_M_NAME[USER_NAME_LEN];  // Name of member
  } GIDS_MEMBERS[];
}BPXYGIDS;

typedef struct BPXYGIDN_tag {
  int     GIDN_U_LEN;                 // User name length (always 8)
  char    GIDN_U_NAME[USER_NAME_LEN]; // User name
  int     GIDN_U_LEN_ID;              // User ID length (always 4)
  int     GIDN_USERID;                // User ID
  int     GIDN_G_LEN;                 // Group ID length (always 4)
  int     GIDN_GROUPID;               // Group ID
  int     GIDN_D_LEN;                 // Length of GIDN_D_NAME
  char    GIDN_D_NAME[];              // Initial working directory name
  /*
  GIDN_P_LEN and GIDN_P_NAME cannot be included into the struct.
  GIDN_D_NAME is a flexible array member and it must be the last member.

  To access GIDN_P_LEN and GIDN_P_NAME use:
  int GIDN_P_LEN = *(int*)(userInfo->GIDN_D_NAME + userInfo->GIDN_D_LEN);
  char *GIDN_P_NAME = userInfo->GIDN_D_NAME + userInfo->GIDN_D_LEN + 4;
 
  int     GIDN_P_LEN;                 // Length of GIDN_P_NAME
  char    GIDN_P_NAME[];              // Initial user program name
  
  */
}BPXYGIDN;
#pragma pack(reset)

typedef BPXYGIDS GroupInfo;
typedef BPXYGIDN UserInfo;

/* Function Prototype */
UserInfo *gidGetUserInfo(const char *userName, int *returnCode, int *reasonCode);
UserInfo *getUserInfo(int uid, int *returnCode, int *reasonCode);
GroupInfo *gidGetGroupInfo(const char *groupName, int *returnCode, int *reasonCode);
GroupInfo *getGroupInfo(int gid, int *returnCode, int *reasonCode);
int userInfoGetUserId (UserInfo *info);
void userInfoGetUserName (UserInfo *info, char *userNameBuffer);
int groupInfoGetGroupId (GroupInfo *info);
void groupInfoGetGroupName (GroupInfo *info, char *groupNameBuffer);
int userIdGet (char *string, int *returnCode, int *reasonCode);
int userGetName(int uid, char *userNameBuffer, int *returnCode, int *reasonCode);
int groupIdGet (char *string, int *returnCode, int *reasonCode);
int groupGetName(int gid, char *groupNameBuffer, int *returnCode, int *reasonCode);
int resetZosUserPassword(const char *userName,  const char *password, const char *newPassword,
                         int *returnCode, int *reasonCode);
/**
 * @brief Get list of groups to which a user belongs.
 * @param userName User name.
 * @param groups Up to *groupCount groups are returned in array groups.
 * @param groupCount If *groupCount is 0 then on return it contains number of groups found for user.
 * If *groupCount greater or equal than number of groups then on return it contains actual number of groups for user.
 * In case of error *groupCount is -1 on return.
 * @return -1 on failure , 0 on success.
 */
int getGroupList(const char *userName, int *groups, int *groupCount, int *returnCode, int *reasonCode);

#endif  /*  __ZOSACCOUNTS__ */



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


