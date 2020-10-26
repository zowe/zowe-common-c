

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __BPXNET__
#define __BPXNET__

#include "zowetypes.h"

#ifdef USE_RS_SSL
#include "rs_ssl.h"
#endif
#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif

#ifdef __ZOWE_OS_ZOS

ZOWE_PRAGMA_PACK

#define AF_INET 2
#define AF_INET6 19

#define SOCTYPE_STREAM    1
#define SOCTYPE_DATAGRAM  2
#define SOCTYPE_RAW       3

#define IPPROTO_IP  0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

//https://www.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.bpxa800/errno.htm
#define EWOULDBLOCK  1102
#define EINPROGRESS  1103
#define EAFNOSUPPORT 1114
#define EADDRINUSE   1115
#define COMMON_PATH_MAX 1024

struct ipV4Data{
  int addrBytes; /* presumed to be in network byte order */
  char reserved[8];
};

struct ipV6Data{
  int flowInfo;
  char addrData[16];
  int scopeId;
};

union ipAddrData{
  struct ipV4Data data4;
  struct ipV6Data data6;
};

typedef struct inetAddr_tag{
  int type:16; /* 2 for IPV4, 19 for IPV6 */
  int port:16;
  union ipAddrData data;
} InetAddr;

#define SOCKET_ADDRESS_SIZE_IPV4 16
#define SOCKET_ADDRESS_SIZE_IPV6 sizeof(SocketAddress)

typedef struct socketAddr_tag{
  char length;
  char family;  
  unsigned short port;
  union {
    struct {
      int v4Address;
      char reserved1[8];
    };
    struct {
      struct ipV6Data data6;
    };
  };
} SocketAddress;

#define GET_V4_ADDR_FOR_SOCKET_AS_INT(SOCKET) \
  ((SOCKET).v4Address)


ZOWE_PRAGMA_PACK_RESET

#elif defined(__ZOWE_OS_WINDOWS)

#include <Ws2tcpip.h>


union ipAddrData{
  struct in_addr data4;
  struct in6_addr data6;
};


typedef struct inetAddr_tag{
  int type:16; /* 2 for IPV4, 19 for IPV6 */
  int port:16;
  union ipAddrData data;
} InetAddr;

#define SOCKET_ADDRESS_SIZE_IPV4 SOCKET_ADDRESS_SIZE_IPV6
#define SOCKET_ADDRESS_SIZE_IPV6 sizeof(SocketAddress)

typedef struct socketAddr_tag{
  short family;
  unsigned short port;
  struct in_addr v4Address;
  char reserved1[8];
} SocketAddress;

#define GET_V4_ADDR_FOR_SOCKET_AS_INT(SOCKET) \
  ((int)((SOCKET).v4Address.s_addr))

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <limits.h>

#define COMMON_PATH_MAX PATH_MAX

union ipAddrData{
  struct in_addr data4;
  struct in6_addr data6;
};

typedef struct inetAddr_tag{
  uint16_t type:16; /* 2 for IPV4, 19 for IPV6 */
  uint16_t port:16;
  union ipAddrData data;
} InetAddr;

#define SOCKET_ADDRESS_SIZE_IPV4 SOCKET_ADDRESS_SIZE_IPV6
#define SOCKET_ADDRESS_SIZE_IPV6 sizeof(SocketAddress)

typedef struct socketAddr_tag{
  uint8_t length;
  uint8_t family;  
  uint16_t port;
  union socketAddrUnion_tag {
    struct sockaddr_in v4Address;
    struct sockaddr_in6 v6Address;
  } internalAddress;
} SocketAddress;

#define GET_V4_ADDR_FOR_SOCKET_AS_INT(SOCKET) \
  ((int)((SOCKET).internalAddress.v4Address.sin_addr.s_addr))

#endif

#define SOCKET_DEBUG_NAME_LENGTH 20

typedef struct socket_tag{
#ifdef __ZOWE_OS_WINDOWS
  SOCKET windowsSocket;
#else 
  int sd;
#endif
  short isServer;        /* inherited by SocketExtension */

#define RS_TLS_WANT_TLS       (1 << 0) /* suitable for passing to tcpClient3 or tcpServer2 */
#define RS_TLS_HAVE_TLS       (1 << 1) /* don't set this yourself */
#define RS_TLS_WANT_PEERCERT  (1 << 2) /* suitable for passing to tcpClient3 or tcpServer2 */
#define RS_TLS_HAVE_PEERCERT  (1 << 3) /* don't set this yourself */
#ifdef __ZOWE_OS_ZOS
  #define RS_TTLS_IOCTL_DONE     (1 << 16) /* set on first ttls ioctl success */
#endif

  int tlsFlags;         /* inherited and used by SocketExtension */

  /* If populated in a server socket, sslHandle is an RS_SSL_ENVIRONMENT that should
   * be used to negotiate an RS_SSL_CONNECTION.
   * If populated in a peer socket, sslHandle is a negotiated RS_SSL_CONNECTION. */
  void* sslHandle;

  InetAddr *socketAddr;  /* uniquely owned */
  char* id;              /* id for give/take socket api's */
  int protocol;          /* 6 TCP, 17 UDP, etc */
  void *userData;        /* a place where users can associate a socket to application data for cleaner
                            callback handling */
  char debugName[SOCKET_DEBUG_NAME_LENGTH];           /* a debugging name */
#ifdef USE_ZOWE_TLS
  TlsEnvironment *tlsEnvironment;
  TlsSocket *tlsSocket;
#endif
} Socket;

typedef struct SocketSet_tag{
  int highestAllowedSD;
  Socket **sockets;
#ifdef __ZOWE_OS_WINDOWS
  int     socketCount;
  SOCKET *allWindowsSockets;
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  fd_set allSDs;
  fd_set scratchReadMask;
  fd_set scratchWriteMask;
  fd_set scratchErrorMask;
#elif defined(__ZOWE_OS_ZOS)
  int *allSDs;
  int *scratchReadMask;
  int *scratchWriteMask;
  int *scratchErrorMask;
#else
#error Unknown OS
#endif
  int  revisionNumber;
} SocketSet;

typedef struct hostent_tag{
  char  *name;
  void **aliasList;
  int    addrType;
  int    length;
  int  **addrList;
} Hostent;

/* sleep(int seconds) is standard in linux */
#if !defined(__ZOWE_OS_LINUX) && !defined(__ZOWE_OS_AIX)
void sleep(int secs);
#endif 

/* Set socket tracing; returns prior value */
int setSocketTrace(int toWhat); 

int socketInit(char *uniqueName);

/* handles named and numeric */

int getLocalHostName(char* inout_hostname,
                     unsigned int* inout_hostname_len,
                     int *returnCode, int *reasonCode);

InetAddr* getLocalHostAddress(int *returnCode, int *reasonCode); /* AKA gethostid */

/* zero-terminated */
InetAddr *getAddressByName(char *addressString);

int getSocketName(Socket *socket, SocketAddress *socketAddress); /* AKA getsockname */
int getSocketName2(Socket *socket, SocketAddress *socketAddress); /* AKA getpeername */
Socket *tcpClient(SocketAddress *socketAddress,
		  int *returnCode, int *reasonCode);

#define getSocketOption gtsktopt

int getSocketOption(Socket *socket, int optionName, int *optionDataLength, char *optionData,
        int *returnCode, int *reasonCode);

#define tcpClient2 tcpclie2

Socket *tcpClient2(SocketAddress *socketAddress, 
		   int timeoutInMillis,
		   int *returnCode, /* errnum */
		   int *reasonCode); /* errnum - JR's */

Socket *tcpServer(InetAddr *addr, /* usually NULL/0 */
                  int port,
		  int *returnCode,
		  int *reasonCode);

#define tcpClient3 tcpclie3

Socket *tcpClient3(SocketAddress *socketAddress,
       int timeoutInMillis,
       int tlsFlags,
       int *returnCode,
       int *reasonCode);

/* 
   * Create a TCP socket
   * Bind it to the specified address & port
   * Listen for connections
 */
Socket *tcpServer2(InetAddr *addr,
                   int port,
                   int tlsFlags,
                   int *returnCode,
                   int *reasonCode);

#ifdef __ZOWE_OS_ZOS
void bpxSleep(int seconds);
#endif

int tcpIOControl(Socket *socket, int command, int argumentLength, char *argument,
                 int *returnCode, int *reasonCode);

Socket *udpPeer(SocketAddress *socketAddress, 
                int *returnCode, /* errnum */
                int *reasonCode);


int udpReceiveFrom(Socket *socket, 
                   SocketAddress *sourceAddress,  /* will be overwritten */
                   char *buffer, int bufferLength,
                   int *returnCode, int *reasonCode);

int udpSendTo(Socket *socket, 
              SocketAddress *destinationAddress,
              char *buffer, int desiredBytes, 
              int *returnCode, int *reasonCode);

SocketSet *makeSocketSet(int highestAllowedSD);
void freeSocketSet(SocketSet *set);
int socketSetAdd(SocketSet *set, Socket *socket);
int socketSetRemove(SocketSet *set, Socket *socket);

#if defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
/*
  Linux has no generalized event mechanism like ECBs or Windows events; 
  you can select on file descriptors, and that's about it. HOWEVER: as
  of the 2.6.27 kernel, there is now an "eventfd" gizmo that can be used
  with select. Yea!
 */
#ifdef __ZOWE_OS_AIX
Socket** makeEventSockets();
#else
Socket* makeEventSocket();
#endif

/*
  Post an event to the socket, bumping the internal counter by 1. If
  successful, 0 is returned. Otherwise, the errno associated with the 
problem is returned.
 */
int postEventSocket(Socket* socket);

/*
  Wait for an event to arrive on the socket and return the current
  counter value. If the counter is zero, this will block until it
  becomes non-zero,so a non-zero return value indicates success,
  and zero indicates an error.
 */
uint64_t waitForEventSocket(Socket* socket);

/*
  Clear pending events from the socket.
 */
void clearEventSocket(Socket* socket);

#endif


#define SD_STATUS_RD_RDY  0x1
#define SD_STATUS_WR_RDY  0x2
#define SD_STATUS_ERROR   0x4
#define SD_STATUS_TIMEOUT 0x8
#define SD_STATUS_FAILED  0x10

int tcpStatus(Socket *s, int timeout, int checkWrite,
	      int *returnCode, int *reasonCode);

int socketRead(Socket *socket, char *buffer, int desiredBytes, 
               int *returnCode, int *reasonCode);

int socketWrite(Socket *socket, const char *buffer, int desiredBytes, 
	       int *returnCode, int *reasonCode);

/* TBD: Not sure about guarding these defined with LONGNAME; shouldn't
   a bunch of other functions also be given short aliases? */
#ifndef __LONGNAME__

#define setSocketTimeout setsktto
#define setSocketNoDelay setsktnd
#define setSocketWriteBufferSize setsktwb
#define setSocketReadBufferSize setsktrb

#endif

int setSocketTimeout(Socket *socket, int timeout,
		     int *returnCode, int *reasonCode);

int setSocketNoDelay(Socket *socket, int noDelay, int *returnCode, int *reasonCode);
int setSocketWriteBufferSize(Socket *socket, int bufferSize, int *returnCode, int *reasonCode);
int setSocketReadBufferSize(Socket *socket,  int bufferSize, int *returnCode, int *reasonCode);
int setSocketBlockingMode(Socket *socket, int isNonBlocking,
                          int *returnCode, int *reasonCode);

#define setSocketOption setsktop

int setSocketOption(Socket *socket, int level, int optionName, int optionDataLength, char *optionData,
                    int *returnCode, int *reasonCode);

int socketSend(Socket *s, char *buffer, int len, int *errno);

Socket *socketAccept(Socket *serverSocket, int *returnCode, int *reasonCode);

int socketClose(Socket *socket, int *returnCode, int *reasonCode);

#ifndef __ZOWE_OS_WINDOWS
int extendedSelect(SocketSet *set,
                   int timeout,             /* in milliseconds */
#if !defined(__ZOWE_OS_LINUX) && !defined(__ZOWE_OS_AIX)

                   void *eventSocket,
#endif
                   int checkWrite, int checkRead, 
                   int *returnCode, int *reasonCode);
#endif

/* These have moved to xlate.h 

char* e2a(char *buffer, int len);
char* a2e(char *buffer, int len);
*/

SocketAddress *makeSocketAddr(InetAddr *addr, 
			      unsigned short port);
SocketAddress *makeSocketAddrIPv6(InetAddr *addr, 
			      unsigned short port);

void freeSocketAddr(SocketAddress *address);
void socketFree(Socket *socket);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

