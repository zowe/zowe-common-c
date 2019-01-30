

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_OS_ZOS
#error Compiling the wrong implementation for rawfd.h
#endif

#ifdef _LP64

#pragma linkage(BPX1PIP,OS)
#pragma linkage(BPX1FCT,OS)
#pragma linkage(BPX1CLO,OS)
#pragma linkage(BPX1RED,OS)
#pragma linkage(BPX1WRT,OS)
#define BPXPIP BPX1PIP
#define BPXFCT BPX1FCT
#define BPXCLO BPX1CLO
#define BPXRED BPX1RED
#define BPXWRT BPX1WRT

#else

#pragma linkage(BPX4PIP,OS)
#pragma linkage(BPX4FCT,OS)
#pragma linkage(BPX4CLO,OS)
#pragma linkage(BPX4RED,OS)
#pragma linkage(BPX4WRT,OS)
#define BPXPIP BPX4PIP
#define BPXFCT BPX4FCT
#define BPXCLO BPX4CLO
#define BPXRED BPX4RED
#define BPXWRT BPX4WRT

#endif

int fdPipe(FileDescriptor pipefd[2], int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXPIP(&pipefd[0],
         &pipefd[1],
         &returnValue,
         returnCode,
         reasonCodePtr);

  return returnValue;
}

int fdCntl(FileDescriptor fd, int action, void* arg, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXFCT(&fd,
         &action,
         arg,
         &returnValue,
         returnCode,
         reasonCode);

  return returnValue;
}

int fdDup2(FileDescriptor src, FileDescriptor dst, int *returnCode, int *reasonCode) {
  return fdCntl(src, F_DUPFD2, &dst, returnCode, reasonCode);
}

int fdClose(FileDescriptor fd, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXCLO(&fd,
         &returnValue,
         returnCode,
         reasonCodePtr);

  return returnValue;
}

int fdRead(FileDescriptor src, char *buffer, int len, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;
  int zero = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXRED(&src,
         &buffer,
         &zero,
         &len,
         &returnValue,
         returnCode,
         reasonCodePtr);

  return returnValue;
}

int fdWrite(FileDescriptor src, const char *buffer, int len, int *returnCode, int *reasonCode) {
  int *reasonCodePtr;
  int returnValue;
  int zero = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXWRT(&src,
         &buffer,
         &zero,
         &len,
         &returnValue,
         returnCode,
         reasonCodePtr);

  return returnValue;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

