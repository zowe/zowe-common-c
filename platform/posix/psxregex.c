/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include "psxregex.h"
#include "zowetypes.h"
#include "alloc.h"

regex_t *regexAlloc(){
  return (regex_t*)safeMalloc(sizeof(regex_t),"regex_t");
}

void regexFree(regex_t *r){
  regfree(r);
  safeFree((char*)r,sizeof(regex_t));
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
