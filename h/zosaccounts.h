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



/* Function Prototype */
int gidGetUserInfo(const char *userName,  UserInfo * info,
                         int *returnCode, int *reasonCode);
int gidGetGroupInfo(const char *groupName,  GroupInfo *info,
                   int *returnCode, int *reasonCode);
int userInfoGetUserId (UserInfo *info);
int groupInfoGetGroupId (GroupInfo *info);
int userIdGet (char *string, int *returnCode, int *reasonCode);
int groupIdGet (char *string, int *returnCode, int *reasonCode);
int resetZosUserPassword(const char *userName,  char *password, char *newPassword,
                         int *returnCode, int *reasonCode);

#endif  /*  __ZOSACCOUNTS__ */



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which 
  accompanies this distribution, and is available at 
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/


