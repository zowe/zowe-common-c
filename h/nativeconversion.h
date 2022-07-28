/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include "zowetypes.h"

#ifdef __ZOWE_OS_ZOS

int __atoe_l(char *bufferptr, int leng);
int __etoa_l(char *bufferptr, int leng);

#endif

static int convertToNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __atoe_l(buf, size);
#endif
  return 0;
}

static int convertFromNative(char *buf, size_t size) {
#ifdef __ZOWE_OS_ZOS
  return __etoa_l(buf, size);
#endif
  return 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
