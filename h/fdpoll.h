#ifndef __ZOWE_FDPOLL_
#define __ZOWE_FDPOLL_ 1

#include "zowetypes.h"

#ifdef __ZOWE_OS_ZOS
#define POLLEPRI         0x0010
#define POLLEWRBAND      0x0008
#define POLLEWRNORM      0x0004
#define POLLEOUT         0x0004
#define POLLEIN          0x0003
#define POLLERDBAND      0x0002
#define POLLERDNORM      0x0001

#define POLLRNVAL        0x0080
#define POLLRHUP         0x0040
#define POLLRERR         0x0020
#define POLLRPRI         0x0010
#define POLLRWRBAND      0x0008
#define POLLRWRNORM      0x0004
#define POLLROUT         0x0004
#define POLLRIN          0x0003
#define POLLRRDBAND      0x0002
#define POLLRRDNORM      0x0001

typedef struct PollItem_tag {
  int fd; /* or mq, on z/OS only... */
  short events;
  short revents;
} PollItem;

#elif defined(__ZOWE_OS_LINUX)

#include <poll.h>

typedef struct pollfd PollItem;

#define POLLEIN  POLLIN
#define POLLRIN  POLLIN
#define POLLEOUT POLLOUT
#define POLLROUT POLLOUT
#define POLLRHUP POLLHUP

#elif defined(__ZOWE_OS_WINDOWS)

#include <winsock2.h>

#define POLLEWRNORM POLLWRNORM 
#define POLLRWRNORM POLLWRNORM

/* Note:

   typedef struct pollfd {
     SOCKET fd;
     SHORT  events;
     SHORT  revents;
   } WSAPOLLFD, *PWSAPOLLFD, *LPWSAPOLLFD;

*/

typedef WSAPOLLFD PollItem;

#else
#error Unknown OS
#endif


/* File descriptor items must precede message queue items. */
int fdPoll(PollItem* fds, short nmqs, short nfds, int timeout, int *returnCode, int *reasonCode);

#endif
