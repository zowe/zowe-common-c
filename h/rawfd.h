

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef RAWFD_H
#define RAWFD_H

#ifdef __ZOWE_OS_ZOS
#define F_DUPFD          0
#define F_GETFD          1
#define F_SETFD          2
#define F_GETFL          3
#define F_SETFL          4
#define F_GETLK          5
#define F_SETLK          6
#define F_SETLKW         7
#define F_DUPFD2         8
#define F_CLOSFD         9
#define F_GETOWN         10
#define F_SETOWN         11
#define F_SETTAG         12
#define F_CONTROL_CVT    13

#define OPNFEXEC         0x00800000
#define O_NOLARGEFILE    0x00000800
#define O_LARGEFILE      0x00000400
#define O_ASYNCSIG       0x00000200
#define O_SYNC           0x00000100
#define O_CREXCL         0x000000C0
#define O_CREAT          0x00000080
#define O_EXCL           0x00000040
#define O_NOCTTY         0x00000020
#define O_TRUNC          0x00000010
#define O_APPEND         0x00000008
#define O_NONBLOCK       0x00000004
#define FNDELAY          0x00000004
#define O_RDWR           0x00000003
#define O_RDONLY         0x00000002
#define O_WRONLY         0x00000001
#define O_ACCMODE        0x00000003
#define O_GETFL          0x0000000F
#elif defined(__ZOWE_OS_LINUX)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
#error Unknown OS
#endif


#if defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_ZOS)
typedef int FileDescriptor;
extern FileDescriptor stdinDescriptor;
extern FileDescriptor stdoutDescriptor;
extern FileDescriptor stderrDescriptor;
#else
#error No FileDescriptor implementation for this platform
#endif

int fdPipe(FileDescriptor pipefd[2], int *returnCode, int *reasonCode);
int fdDup2(FileDescriptor src, FileDescriptor dst, int *returnCode, int *reasonCode);
int fdCntl(FileDescriptor fd, int action, void* arg, int *returnCode, int *reasonCode);
int fdClose(FileDescriptor fd, int *returnCode, int *reasonCode);
int fdRead(FileDescriptor src, char *buffer, int len, int *returnCode, int *reasonCode);
int fdWrite(FileDescriptor src, const char *buffer, int len, int *returnCode, int *reasonCode);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

