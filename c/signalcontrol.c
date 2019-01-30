

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include "signalcontrol.h"
#ifdef __ZOWE_OS_ZOS

#ifdef _LP64

#pragma linkage(BPX4SIA,OS)
#define BPXSIA BPX4SIA

#else

#pragma linkage(BPX1SIA,OS)
#define BPXSIA BPX1SIA

#endif

int signalControl(int signum, SignalHandler handler, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int zero = 0;
  SignalHandler oldHandler;
  int64 oldMask;
  int64 oldFlags;


  BPXSIA(&signum,
         &zero,
         &zero,
         &zero,
         &oldHandler,
         &oldMask,
         &oldFlags,
         &zero,
         &returnValue,
         returnCode,
         reasonCodePtr);

  oldFlags &= ~SA_SIGINFO;

  BPXSIA(&signum,
         &handler,
         &oldMask,
         &oldFlags,
         &zero,
         &zero,
         &zero,
         &zero,
         &returnValue,
         returnCode,
         reasonCodePtr);

  return returnValue;
}

#elif defined(__ZOWE_OS_LINUX)

int signalControl(int signum, SignalHandler handler, int *returnCode, int *reasonCode) {
#warning signalControl not yet implemented
  /*
    TBD: Implement this.
   */
  return 0;
}
#else
#error No implementation for signalcontrol.h
#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

