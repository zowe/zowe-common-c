

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef H_NAMETOKEN_H_
#define H_NAMETOKEN_H_

#include "zowetypes.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS is supported only
#endif

ZOWE_PRAGMA_PACK

typedef enum NameTokenLevel_tag {
  NAME_TOKEN_LEVEL_TASK = 1,
  NAME_TOKEN_LEVEL_HOME = 2,
  NAME_TOKEN_LEVEL_PRIMARY = 3,
  NAME_TOKEN_LEVEL_SYSTEM = 4,
  NAME_TOKEN_LEVEL_TASKAUTH = 11,
  NAME_TOKEN_LEVEL_HOMEAUTH = 12,
  NAME_TOKEN_LEVEL_PRIMARYAUTH = 13,
} NameTokenLevel;

typedef struct NameTokenUserName_tag {
  char name[16];
} NameTokenUserName;

typedef struct NameTokenUserToken_tag {
  char token[16];
} NameTokenUserToken;

typedef enum NameTokenPersistOption_tag {
  NAME_TOKEN_PERSIST_OPT_NO_PERSISTS_NO_CHECK_POINT = 0,
  NAME_TOKEN_PERSIST_OPT_PERSIST = 1,
  NAME_TOKEN_PERSIST_CHECKPOINT_PERMITTED = 2,
} NameTokenPersistOption;

ZOWE_PRAGMA_PACK_RESET

#define RC_NAMETOKEN_OK               0
#define RC_NAMETOKEN_DUP_NAME         4
#define RC_NAMETOKEN_NOT_FOUND        4
#define RC_NAMETOKEN_24BITMODE        8
#define RC_NAMETOKEN_NOT_AUTH         16
#define RC_NAMETOKEN_SRB_MODE         20
#define RC_NAMETOKEN_LOCK_HELD        24
#define RC_NAMETOKEN_LEVEL_INVALID    28
#define RC_NAMETOKEN_NAME_INVALID     32
#define RC_NAMETOKEN_PERSIST_INVALID  36
#define RC_NAMETOKEN_AR_INVALID       40
#define RC_NAMETOKEN_UNEXPECTED_ERR   64

#ifndef __LONGNAME__

#define nameTokenCreate NTPCREF
#define nameTokenCreatePersistent NTPCRTPF
#define nameTokenDelete NTPDELF
#define nameTokenRetrieve NTPRETF

#endif

int nameTokenCreate(NameTokenLevel level, const NameTokenUserName *name, const NameTokenUserToken *token);
int nameTokenCreatePersistent(NameTokenLevel level, const NameTokenUserName *name, const NameTokenUserToken *token);
int nameTokenDelete(NameTokenLevel level, const NameTokenUserName *name);
int nameTokenRetrieve(NameTokenLevel level, const NameTokenUserName *name, NameTokenUserToken *token);

#endif /* H_NAMETOKEN_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

