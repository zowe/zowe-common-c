

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef SRC_CPOOL64_H_
#define SRC_CPOOL64_H_

#ifndef __LONGNAME__

#define iarcp64Create CPL64CRE
#define iarcp64Get CPL64GET
#define iarcp64Free CPL64FRE
#define iarcp64Delete CPL64DEL

#endif

uint64_t iarcp64Create(bool isCommon, int cellSize, int *returnCodePtr, int *reasonCodePtr);
void *iarcp64Get(uint64_t cpid, int *returnCodePtr, int *reasonCodePtr);
void iarcp64Free(uint64_t cpid, void *cellAddr);
int iarcp64Delete(uint64_t cpid, int *reasonCodePtr);


#endif /* SRC_CPOOL64_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
