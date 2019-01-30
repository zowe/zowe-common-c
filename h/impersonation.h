

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __IMPERSONATION__
#define __IMPERSONATION__ 1

#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif

int startNativeImpersonation(char *username, void *userPointer, void *resultPointer);
int endNativeImpersonation(char *username, void *userPointer, void *resultPointer);

#ifdef __ZOWE_OS_ZOS
int startSafImpersonation(char *username, ACEE **newACEE);
int endSafImpersonation(ACEE **acee);
/*
 * The BPX version is really the one to use unless you have a reviewed need to do something in SAF itself
 */
int bpxImpersonate(char *userid, char *passwordOrNull, int start, int trace);
int tlsImpersonate(char *userid, char *passwordOrNull, int start, int trace);
#endif /* __ZOWE_OS_ZOS */

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

