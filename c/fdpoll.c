#include "fdpoll.h"

#ifdef __ZOWE_OS_ZOS
#ifdef _LP64

#pragma linkage(BPX4POL,OS)
#define BPXPOL BPX4POL

#else

#pragma linkage(BPX1POL,OS)
#define BPXPOL BPX1POL

#endif

/* xlclang and clang demand a prototype */

int BPXPOL();

int fdPoll(PollItem* fds, short nmqs, short nfds, int timeout, int *returnCode, int *reasonCode) {
  int returnValue;
  int *reasonCodePtr;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int counts = (nmqs << 16) | nfds;

  BPXPOL(&fds,
         &counts,
         &timeout,
         &returnValue,
         returnCode,
         reasonCodePtr);

  return returnValue;
}
#elif defined(__ZOWE_OS_LINUX)
#include <errno.h>

int fdPoll(PollItem* fds, short nmqs, short nfds, int timeout, int *returnCode, int *reasonCode) {
  int status = poll(fds, nfds, timeout);
  *returnCode = *reasonCode = (status >= 0 ? 0 : errno);
  return status;
}

#elif defined(__ZOWE_OS_WINDOWS)

int fdPoll(PollItem* fds, short nmqs, short nfds, int timeout, int *returnCode, int *reasonCode) {
  int status = WSAPoll(fds, nfds, timeout);
  *returnCode = *reasonCode = (status >= 0 ? 0 : WSAGetLastError());
  return status;
}


#else 
#error No implemention for fdpoll.h
#endif
