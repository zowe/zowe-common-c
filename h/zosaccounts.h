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

/* Structure definitioss */
typedef struct BPXYGIDS_tag {
  int     GIDS_G_LEN;        // Group name length
  char    GIDS_G_NAME;       // Group name
  int     GIDS_G_LEN_ID;     // Group ID length
  int     GIDS_GROUPID;      // Group ID
  int     GIDS_COUNT;        // Count of array element
  int     GIDS_M_LEN;        // Length of member names
  char    GIDS_M_NAME;       // Name of members
}BPXYGIDS;

typedef struct BPXYGIDN_tag {
  int     GIDN_U_LEN;        // User name length
  char    GIDN_U_NAME;       // User name
  int     GIDN_U_LEN_ID;     // User ID length
  int     GIDN_USERID;       // User ID
  int     GIDN_G_LEN;        // Group name length
  int     GIDN_GROUPID;      // Group ID
  int     GIDN_D_LEN;        // Length of GIDN_D_NAME
  char    GIDN_D_NAME;       // Initial working directory name
  int     GIDN_P_LEN;        // Length of GIDN_P_NAME
  char    GIDN_P_NAME;       // Initial user program name
}BPXYGIDN;

typedef BPXYGIDS GroupInfo;
typedef BPXYGIDN UserInfo;

#define USER_NAME_LEN 8
#define GROUP_NAME_LEN 8

/* Function Prototype */
int gidGetUserInfo(const char *userName,  UserInfo * info,
                         int *returnCode, int *reasonCode);
int getUserInfo(int uid, UserInfo *info, int *returnCode, int *reasonCode);
int gidGetGroupInfo(const char *groupName,  GroupInfo *info,
                   int *returnCode, int *reasonCode);
int getGroupInfo(int gid, GroupInfo *info, int *returnCode, int *reasonCode);
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


