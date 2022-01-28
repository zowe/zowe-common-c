

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"
#include "unixfile.h"

#include <sys/types.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#ifndef __ZOWE_OS_AIX
#include <sys/eventfd.h>
#endif

#include <netinet/in.h>
#include <errno.h>

#include <fcntl.h>
#include <netdb.h>   /* gethostbyname */
#include <unistd.h>
#include <netinet/tcp.h>
#include <inttypes.h>

#ifdef USE_RS_SSL
#include "rs_ssl.h"
#endif

int socketInit(char *uniqueName){
  /* don't do anything in POSIX */
  return 0;
}

static int socketTrace = 0;
int setSocketTrace(int toWhat) {
  int was = socketTrace;
  socketTrace = toWhat;
  return was;
}

typedef union SockAddrAll_tag {
  struct sockaddr_in v4;
  struct sockaddr_in6 v6;
} SockAddrAll;

#define IPPROTO_EVENT 254 /* semi-official "experimental" protocol number */

static
SocketAddress * setSocketAddr(SocketAddress *address,
                              const InetAddr *addr, 
                              unsigned short port)
{
  memset(address,0,sizeof(SocketAddress));

  address->family = (uint8_t) (addr ? addr->type : AF_INET);
  if (addr && (port == 0)) {
    address->port = (uint16_t) addr->port;
  } else {
    address->port = (uint16_t) htons(port);
  }

  if (addr) {
    switch (address->family) {
    case AF_INET:
      address->internalAddress.v4Address.sin_family = AF_INET;
      address->internalAddress.v4Address.sin_port = (in_port_t) (port == 0 ? address->port : htons(port));
      memcpy(&(address->internalAddress.v4Address.sin_addr), &(addr->data.data4), sizeof(struct in_addr));
      break;

    case AF_INET6:
      address->internalAddress.v6Address.sin6_family = AF_INET6;
      address->internalAddress.v6Address.sin6_port = (in_port_t) (port == 0 ? address->port : htons(port));
      memcpy(&(address->internalAddress.v6Address.sin6_addr), &(addr->data.data6), sizeof(struct in6_addr));
      break;

    default:
      if (socketTrace){
        printf("setSocketAddr: Unknown address family %d\n", (int) address->family);
      }
      return NULL;
    }
  }

if (socketTrace) {
   dumpbuffer(address, sizeof(SocketAddress));
}

  return address;
}

SocketAddress *makeSocketAddr(InetAddr *addr, 
			      unsigned short port){
  SocketAddress *address = (SocketAddress*)safeMalloc(sizeof(SocketAddress),"SocketAddress");
  if (0 == setSocketAddr(address, addr, port)) {
      freeSocketAddr(address);
      address =  (SocketAddress *) NULL;
  }
  return address;
}

void freeSocketAddr(SocketAddress *address){
  safeFree((char*)address,sizeof(SocketAddress));
}

static
int getEndPointName(int sd, InetAddr *name, int which /* 0 = this end, other = peer */)
{
  SockAddrAll socketAddr;
  memset(&socketAddr, 0, sizeof(SockAddrAll));
  int status = 0;
  socklen_t sockLen = sizeof(SockAddrAll);
  if (which == 0) {
    status = getsockname(sd, (struct sockaddr *)&socketAddr, &sockLen);
  } else {
    status = getpeername(sd, (struct sockaddr *)&socketAddr, &sockLen);
  }

  if (0 == status) {
    /* fill in the address information */
    memset(name, 0, sizeof(InetAddr));
    sa_family_t domain = socketAddr.v4.sin_family; /* same place for all address types */
    name->type = (uint16_t) domain;
    switch(domain) {
    case AF_INET:
      name->port = (uint16_t) socketAddr.v4.sin_port;
      memcpy(&(name->data), &socketAddr.v4.sin_addr, sizeof(struct in_addr));
      break;

    case AF_INET6: 
      name->port = (uint16_t) socketAddr.v6.sin6_port;
      memcpy(&(name->data), &socketAddr.v6.sin6_addr, sizeof(struct in6_addr));
      break;

    default:
      status = EAFNOSUPPORT;
      break;
    }
  } else {
    status = errno;
    if (socketTrace){
      printf("get%sname failed, errno=%d (%s)\n",
             (which == 0 ? "sock" : "peer"), status, strerror(status));
    }
  }
  return status;
}

/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/

/*
  TBC-JLC: This code currently supports only IPv4.
 */
Socket *tcpClient2(SocketAddress *socketAddress, 
		   int timeoutInMillis,
		   int *returnCode, /* errnum */
		   int *reasonCode){ /* errnum - JR's */

  sa_family_t domain = 0;

  int sd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (sd < 0) {
    *reasonCode = *returnCode = errno;
    if (socketTrace){
      printf("Failed to create client socket, errno=%d (%s)\n", 
             *returnCode, strerror(*returnCode));
    }
    return NULL;
  } else { 
    if (socketTrace){
      printf("Created client socket, sd=%d\n", sd);
    }
  }

  int socketVector[2] = {0};
  int status;
  int madeConnection = FALSE;
  struct sockaddr *sockAddr = (struct sockaddr*)&(socketAddress->internalAddress.v4Address);

  {
    int socketAddrSize = sizeof(SocketAddress);
    if (timeoutInMillis >= 0){
      Socket tempSocket;
      tempSocket.sd = sd;

      /* setSocketBlockingMode(&tempSocket, TRUE, returnCode, reasonCode);  */
      status = connect(sd,sockAddr,sizeof(struct sockaddr_in)); /* v4 only */
      *returnCode = *reasonCode = errno;
      if (socketTrace || (status < 0)) {
        printf("connect: status=%d errno=%d (%s)\n",status,*returnCode, strerror(*returnCode));
        fflush(stdout);
      }
      /* EINPROGRESS is the expected return code here but we are just being careful by checking 
         for EWOULDBLOCK as well 
      */
      if (status == 0){
        *returnCode  = 0;
        *reasonCode  = 0;
        status = tcpStatus(&tempSocket, timeoutInMillis, 1, returnCode, reasonCode);
#ifdef DEBUG
        printf("after tcpStatus() = %d\n",status);
#endif
        if (status == SD_STATUS_TIMEOUT) {
          int sd = socketVector[0];
          if (socketTrace) {
            printf("Failed to connect socket, will clean up sd=%d\n", sd);
          }
          *returnCode = 0;
          *reasonCode = 0;
          status = close(sd);
          if (socketTrace) {
            printf("closeSocket() for time out connect status %d\n",status);
          }
          return NULL;
        }
#ifdef DEBUG
        printf("tcpStatus return and did not timeout status=%d\n",status);
        fflush(stdout);
#endif
        *returnCode  = 0;
        *reasonCode  = 0;
        /* setSocketBlockingMode(&tempSocket, FALSE, returnCode, reasonCode); */
        int optionLength = sizeof(int);
        int optionData = 0;
        *returnCode  = 0;
        *reasonCode  = 0;
        status = getSocketOption(&tempSocket, SO_ERROR, &optionLength, (char *)&optionData, returnCode, reasonCode);
        if( optionData > 0 ){
          if (socketTrace){
            printf("Failed to connect socket, returnCode %d\n", optionData);
          }
          *returnCode = optionData;
          madeConnection = FALSE;
        } else {
          madeConnection = TRUE;
        }
      }
      if (socketTrace){
        printf("did we make connection (timeout case) = %d\n",madeConnection);
      }
    } else{ /* if timeouts are not specified */
      status = connect(sd,sockAddr,sizeof(struct sockaddr_in)); /* v4 only */
      *returnCode = *reasonCode = errno;
      if (socketTrace || (status < 0)) {
        printf("connect: status=%d errno=%d (%s)\n",status,*returnCode, strerror(*returnCode));
        fflush(stdout);
      }
      madeConnection =  (status == 0 ? TRUE : FALSE);
    }
    if (!madeConnection){
      if (socketTrace){
        printf("Failed to connect socket, will clean up, sd was %d\n",sd);
      }
      status = close(sd);
      if (socketTrace || (status < 0)) {
        int error = (status < 0 ? errno : 0);
        printf("close(%d) for failed connect: status=%d, errno=%d (%s)\n",
               sd, status, error, strerror(error));
      }
      return NULL;
    } else{
      Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
      socket->sd = sd;
      sprintf(socket->debugName,"SD=%u",(unsigned) socket->sd);
      socket->isServer = FALSE;
      /* manual says returnCode and value only meaningful if return value = -1 */
      *returnCode = 0;
      *reasonCode = 0;
      socket->protocol = IPPROTO_TCP;
      return socket;
    }
  }                    
}

/* pointer to length is stupid */
int getSocketOption(Socket *socket, int optionName, int *optionDataLength, char *optionData,
		    int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int getOption = 1;
  int setOption = 2;
  int setIBMOption = 3;
  int level = 0x0000FFFF;
  
  if (socketTrace){
    printf("before get socket option optionName=0x%x dataBufferLen=%d\n",optionName,*optionDataLength);
  }
  int status = setsockopt(sd,
                          SOL_SOCKET, /* is this always right */
                          optionName,
                          optionData,
                          *optionDataLength);
  if (socketTrace){
    printf("after getsockopt, status=%d ret code %d reason 0x%x\n",status,*returnCode,*reasonCode);
  }
  if (status < 0){
    *returnCode = errno;
    *reasonCode = *returnCode;
    if (socketTrace) {
      printf("get sockopt failed, ret code %d reason 0x%x\n", *returnCode, *reasonCode);
    }
    return -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
    return status;
  }
}


Socket *tcpClient(SocketAddress *socketAddress, 
                  int *returnCode, /* errnum */
                  int *reasonCode){ /* errnum - JR's */
  return tcpClient2(socketAddress,-1,returnCode,reasonCode);
}

void socketFree(Socket *socket){
  if ((IPPROTO_EVENT != socket->protocol) && 
      (0 != socket->socketAddr)) {
    safeFree((char*)socket->socketAddr,sizeof(InetAddr));
  }
  safeFree((char*)socket,sizeof(Socket));
}

int udpReceiveFrom(Socket *socket, 
                   SocketAddress *sourceAddress,  /* will be overwritten */
                   char *buffer, int bufferLength,
                   int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int flags = 0;  
  struct sockaddr *sockAddr = (struct sockaddr*)&(sourceAddress->internalAddress.v4Address);                 
  int socketAddressSize = sizeof(struct sockaddr);

  *returnCode = *reasonCode = 0;

  if (socketTrace > 2){
    printf("receiveFrom into buffer=0x%x bufLen=%d\n", buffer,bufferLength);
  }

  int bytesReadOrError = recvfrom(sd, buffer, bufferLength, flags, sockAddr, &socketAddressSize);
  
  if (bytesReadOrError < 0){
    *returnCode = errno;
    *reasonCode = *returnCode;
    return -1;
  } else {
    if (socketTrace > 1){
      printf("read %d bytes\n",bytesReadOrError);
      dumpbuffer(buffer, bytesReadOrError);
    }
    *returnCode = 0;
    *reasonCode = 0;

    if (!(socket->lastDestination.port == sourceAddress->port) &&
         ((socket->lastDestination.family == AF_INET &&
            (0 == memcmp(&(socket->lastDestination.internalAddress.v4Address),
                         &(sourceAddress->internalAddress.v4Address),
                         sizeof(sourceAddress->internalAddress.v4Address))))
             ||
          (socket->lastDestination.family == AF_INET6 &&
            (0 == memcmp(&(socket->lastDestination.internalAddress.v6Address),
                         &(sourceAddress->internalAddress.v6Address),
                         sizeof(sourceAddress->internalAddress.v6Address))))))
    {
      if (socket->lastDestination.family == 0) {
        *reasonCode = 1;  // Indicate no last destination was set for the socket
      } else {
        *reasonCode = 2;  // Indicate source address/port did not match the last destination
      }
    } else {
      memset(&socket->lastDestination,0,sizeof(SocketAddress));
    }
    return bytesReadOrError;
  }
  
}

void setSocketLastDestination(Socket *socket, SocketAddress *sourceAddress) {
  socket->lastDestination = *sourceAddress;
}

void clearSocketLastDestination(Socket *socket) {
  memset(&socket->lastDestination,0,sizeof(SocketAddress));
}

int udpSendTo(Socket *socket,
              SocketAddress *destinationAddress,
              char *buffer, int desiredBytes,
              int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int flags = 0;
  struct sockaddr *sockAddr = (struct sockaddr*)&(destinationAddress->internalAddress.v4Address);
  int socketAddressSize = sizeof(struct sockaddr);

  *returnCode = *reasonCode = 0;

  if (socketTrace > 2){
    printf("sendTo desired=%d\n", desiredBytes);
    dumpbuffer(buffer, desiredBytes);
    dumpbuffer(destinationAddress, sizeof(SocketAddress));
  }

  int bytesSent = sendto(sd, buffer, desiredBytes, flags, sockAddr, socketAddressSize);

  if (bytesSent < 0){
    memset(&socket->lastDestination,0,sizeof(SocketAddress));
    if (socketTrace > 1){
      printf("error: %d\n", errno);
    }
    *returnCode = errno;
    *reasonCode = *returnCode;
    return -1;
  } else {
    socket->lastDestination = *destinationAddress;
    if (socketTrace > 1){
      printf("sent %d bytes\n",bytesSent);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return bytesSent;
  }
}

int getSocketKeepAliveMode(Socket *socket, int *returnCode, int *reasonCode){

  int optval = 0;
  int optlen = sizeof(optval);
  int returnValue = getSocketOption(socket,SO_KEEPALIVE,&optlen,&optval,
                                    returnCode,reasonCode);
  if (returnValue < 0) {       // If an error occurred 
    printf("Unable to get socket option SO_KEEPALIVE.\n");
    return returnValue;
  }
  returnValue = optval;

  *returnCode = 0;
  *reasonCode = 0;
  return returnValue;
}

// - An enableKeepAlive value of 0 will disable keepalive processing
// - An enableKeepAlive value of >0 will enable keepalive processing with the default
//   time values.
int setSocketKeepAliveMode(Socket *socket, int enableKeepAlive,
                           int *returnCode, int *reasonCode){

  // Enable/disable SO_KEEPALIVE, as requested
  int optval = (enableKeepAlive ? 1 : 0);
  int returnValue = setSocketOption(socket,SOL_SOCKET,SO_KEEPALIVE,sizeof(optval),&optval,
                                    returnCode,reasonCode);
  if (returnValue < 0) {       // If an error occurred 
    printf("Unable to set socket option SO_KEEPALIVE to %s.\n",(optval? "on" : "off"));
    return returnValue;
  }
  
  *returnCode = 0;
  *reasonCode = 0;
  return returnValue;
}

int socketRead(Socket *socket, char *buffer, int desiredBytes, 
               int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int flags = 0; /* no special behaviors */

#ifdef USE_RS_SSL
  if (NULL != socket->sslHandle) { /* sslHandle is RS_SSL_CONNECTION in this case */
    int bytesRead = 0;
    int status = rs_ssl_read(socket->sslHandle, buffer, desiredBytes, &bytesRead);
    if (0 != status) {
      if (socketTrace){
        printf("socketRead: rs_ssl_read failed with status: %d\n", status);
      }
      *returnCode = status;
      *reasonCode = *returnCode;
      return -1;
    } else {
      *returnCode = 0;
      *reasonCode = 0;
      return bytesRead;
    }
  }
  else
#endif
  {
    int bytesReadOrError = (int) read(sd,buffer,(size_t) desiredBytes);
    if (bytesReadOrError < 0){
      *returnCode = errno;
      *reasonCode = *returnCode;
      return -1;
    } else {
      if (socketTrace > 1){
        printf("read %d bytes\n",bytesReadOrError);
      }
      *returnCode = 0;
      *reasonCode = 0;
      return bytesReadOrError;
    }
  }
}

int socketWrite(Socket *socket, const char *buffer, int desiredBytes, 
                int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int flags = 0;

  if (socketTrace > 1){
    printf("socketWrite(%s, %d)\n",
           socket->debugName, desiredBytes);
    if (socketTrace > 2){
      dumpbuffer(buffer, desiredBytes); 
    }
  }

#ifdef USE_RS_SSL
  if (NULL != socket->sslHandle) { /* sslHandle is RS_SSL_CONNECTION in this case */
    int bytesWritten = 0;
    int status = rs_ssl_write(socket->sslHandle, buffer, desiredBytes, &bytesWritten);
    if (0 != status) {
      if (socketTrace) {
        printf("socketWrite: rs_ssl_write failed with status: %d\n", status);
      }
      *returnCode = status;
      *reasonCode = *returnCode;
      return -1;
    } else {
      *returnCode = 0;
      *reasonCode = 0;
      return bytesWritten;
    }
  }
  else
#endif
  {
    int bytesWrittenOrError = (int) write(sd,buffer,(size_t) desiredBytes);
    if (bytesWrittenOrError < 0){
      *returnCode = errno;
      *reasonCode = *returnCode;
      return -1;
    } else {
      if (socketTrace > 2){
        printf("socketWrite(%s, %d)wrote %d bytes\n",
               socket->debugName, desiredBytes, bytesWrittenOrError);
        fflush(stdout);
      }
      *returnCode = 0;
      *reasonCode = 0;
      return bytesWrittenOrError;
    }
  }
}


static int tcpStatusInternal(Socket *socket, int timeout, int checkWrite,
                             int checkRead, int *returnCode, int *reasonCode){
  *reasonCode = 0;  // unnecessary argument for POSIX implementation
  *returnCode = 0;
  
  int socketDescriptor = socket->sd;
  int maxSoc = socketDescriptor + 1;
  
  fd_set setRead;
  fd_set setWrite;
  fd_set setError;
  
  FD_ZERO(&setRead);
  FD_ZERO(&setWrite);
  FD_ZERO(&setError);
  
  struct timeval timeOut;
  timeOut.tv_sec = timeout / 1000;
  timeOut.tv_usec = (timeout % 1000) * 1000;
  struct timeval *timeoutPtr = NULL;
  
  if (timeout >= 0){
    timeoutPtr = &timeOut;
  }
  
  if (checkRead){
    FD_SET(socketDescriptor, &setRead);
  }
  
  FD_SET(socketDescriptor, &setError);

  if (checkWrite){
    FD_SET(socketDescriptor, &setWrite);
  }
  
  int retValue = select(maxSoc, (checkRead ? &setRead : NULL), (checkWrite ? &setWrite : NULL), &setError, timeoutPtr);

  if (retValue < 0)  {
    *reasonCode = *returnCode = errno;
    if (socketTrace) {
      printf("Select failed for sd %d, socket %s: errno=%d (%s)\n",
             socketDescriptor, socket->debugName, *returnCode, strerror(*returnCode));
    }
    return SD_STATUS_FAILED;
  }
  
  if (retValue > 0){
    int sdStatus = 0;
    
    if (FD_ISSET(socketDescriptor, &setRead)){
      sdStatus |= SD_STATUS_RD_RDY;
    }
    if (FD_ISSET(socketDescriptor, &setWrite)){
      sdStatus |= SD_STATUS_WR_RDY;
    }
    if (FD_ISSET(socketDescriptor, &setError)){
      return SD_STATUS_ERROR;
    }
    
    return sdStatus;
  }
  
  return SD_STATUS_TIMEOUT;
 }

int tcpStatus(Socket *socket, 
              int timeout,   /* in milliseconds */
              int checkWrite, /* otherwise check for read and error */
              int *returnCode, int *reasonCode){
  return tcpStatusInternal(socket,timeout,checkWrite,!checkWrite,returnCode,reasonCode);
}

#define F_GETFL       3
#define F_SETFL       4

int setSocketBlockingMode(Socket *socket, int isNonBlocking,
                          int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int status = 0;
  long command = FIONBIO;
  unsigned long argument = (isNonBlocking ? 1 : 0);
  
  status = ioctl(sd,command,&argument);

  if (status < 0){
    *reasonCode = *returnCode = errno;
    if (socketTrace) {
      printf("setSocketBlockingMode(%s, %s) failed: errno = %d (%s)\n",
           socket->debugName, (isNonBlocking ? "NONBLOCKING (true)" : "BLOCKING (false)"),
           *reasonCode, strerror(*reasonCode));
    }
    return -1;
  } else {
    if (socketTrace){
    printf("setSocketBlockingMode(%s, %s) succeeded\n",
           socket->debugName, (isNonBlocking ? "NONBLOCKING (true)" : "BLOCKING (false)"));
    }
    *returnCode = *reasonCode = 0;
    return 0;
  }
}

int setSocketReuseAddr(Socket *socket, int *returnCode, int *reasonCode){
  int on = 1;
  return setSocketOption(socket,SOL_SOCKET,SO_REUSEADDR,sizeof(int),(char*)&on,returnCode,reasonCode);
}

Socket *udpPeer(SocketAddress *socketAddress, int *returnCode, /* errnum */
                int *reasonCode) { /* errnum - JR's */
  int status;
  struct sockaddr *sockAddr = (struct sockaddr*) &(socketAddress->internalAddress.v4Address);

  int sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  
  if (sd == -1) {
    *returnCode = errno;
    *reasonCode = *returnCode;
    if (socketTrace) {
      printf("Failed to create socket, error=%d\n", *returnCode);
    }
    return NULL;
  }
  else {
    int socketAddressSize = sizeof(SocketAddress);

    status = bind(sd, sockAddr, sizeof(struct sockaddr_in));
    if (status != 0) {
      *returnCode = errno;
      *reasonCode = *returnCode;
    }
    if (socketTrace) {
      printf("bind status %d returnCode %x reasonCode %x\n", status, *returnCode, *reasonCode);
    }
    if (status != 0) {
      /* polaris noted that we are leaking the socket in this failure to bind case*/
      close(sd);
      return NULL;
    }
    else {
      Socket *socket = (Socket*) safeMalloc(sizeof(Socket), "Socket");
      socket->sd = sd;
      sprintf(socket->debugName, "SD=%u", (unsigned) socket->sd);
      socket->isServer = FALSE;
      /* manual says returnCode and value only meaningful if return value = -1 */
      *returnCode = 0;
      *reasonCode = 0;
      socket->protocol = IPPROTO_UDP;

      return socket;
    }
  }
}

Socket *tcpServer(InetAddr *addr, /* usually NULL/0 */
                  int port,
                  int *returnCode,
                  int *reasonCode){
  int status;
  socklen_t sockLen = 0;
  SockAddrAll socketAddr;
  memset(&socketAddr, 0, sizeof(SockAddrAll));

  sa_family_t domain = (sa_family_t) (addr ? addr->type : AF_INET);
  switch(domain) {
  case AF_INET:
    socketAddr.v4.sin_family = domain;
    socketAddr.v4.sin_port = htons((in_port_t)port);
    if (addr) {
      memcpy(&(socketAddr.v4.sin_addr.s_addr), &(addr->data.data4), sizeof(struct in_addr));
    }
    sockLen = sizeof(struct sockaddr_in);
    break;

  case AF_INET6: 
    socketAddr.v6.sin6_family = domain;
    socketAddr.v6.sin6_port = (in_port_t)port;
    if (addr) {
      memcpy(&(socketAddr.v6.sin6_addr.s6_addr), &(addr->data.data6), sizeof(struct in6_addr));
    }
    sockLen = sizeof(struct sockaddr_in6);
    break;

  default:
    if (socketTrace){
      printf("tcpserver: Unknown address family %d\n", domain);
    }
    *reasonCode = *returnCode = EAFNOSUPPORT;
    return NULL;
  }

  int sd = socket(domain,SOCK_STREAM,IPPROTO_TCP);
  if (sd == -1){
    *reasonCode = *returnCode = errno;
    if (socketTrace){
      printf("socket(%d,SOCK_STREAM,IPPROTO_TCP) failed, errno=%d (%s)\n",
             domain, *returnCode, strerror(*returnCode));
    }
    return NULL;
  }

  status = setSocketReuseAddr(&sd,
                     returnCode,
                     reasonCode);
  if (status != 0){
    if (socketTrace){
      printf("Failed to set SO_REUSEADDR errno=%d reason=0x%x\n",
             *returnCode,*reasonCode);
    }
    /* treat as not fatal?*/
  }

  if (port != 0) {
    /* bind to the port provided */
    status = bind(sd,(struct sockaddr*)&socketAddr,sockLen);
    if (status != 0){
      *reasonCode = *returnCode = errno;
      if (socketTrace){
        printf("Failed to bind server socket errno=%d (%s)\n",
               *returnCode,strerror(*returnCode));
      }
      close(sd);
      return NULL;
    }
  }

  int backlogQueueLength = 100;
  status = listen(sd,backlogQueueLength);
  if (status != 0){
    *returnCode = errno;
    *reasonCode = *returnCode;
    if (socketTrace){
      printf("Failed to start server socket listen, errno=%d (%s)\n",
             *returnCode,strerror(*returnCode));
    }
    close(sd);
    return NULL;
  }

  /* Yea, team - we now have a listening socket. */

  *reasonCode = *returnCode = 0;
  Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
  memset(socket, 0, sizeof(Socket));
  socket->sd = sd;
  socket->isServer = TRUE;
  socket->protocol = IPPROTO_TCP;

  InetAddr name;
  if (0 == getEndPointName(sd, &name, 0)) {
    /* fill in the address information */
    socket->socketAddr = (InetAddr *)safeMalloc(sizeof(InetAddr),"InetAddr");
    memcpy(socket->socketAddr, &name, sizeof(InetAddr));
    port = (int) name.port;
  }
  sprintf(socket->debugName,"SD=%d,port=%u",socket->sd, (unsigned) port);

  if (socketTrace) {
    printf("Immediately before return from tcpServer, socket contains:\n");
    dumpbuffer((char*)socket, sizeof(Socket));
  }

  return socket;
}

Socket *tcpServer2(InetAddr *addr,
                   int port,
                   int tlsFlags,
                   int *returnCode,
                   int *reasonCode) 
{
  Socket* socket  = NULL;
  socket  = tcpServer(addr, port, returnCode, reasonCode);
  if (socket != NULL) {
    socket->tlsFlags = tlsFlags;
  }

  return socket;
}
/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/


Socket *socketAccept(Socket *serverSocket, int *returnCode, int *reasonCode){

  SockAddrAll newSocketAddr;
  socklen_t newSocketAddrSize = sizeof(SockAddrAll);
  int newSD = accept(serverSocket->sd,(struct sockaddr *)&newSocketAddr,&newSocketAddrSize);
  if (newSD < 0){
    *reasonCode = *returnCode = errno;
    if (socketTrace){
      printf("Failed to accept new socket errno=%d (%s)\n",
             *returnCode,strerror(*returnCode));
    }
    return NULL;
  }

  Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
  memset(socket,0,sizeof(Socket));
  socket->sd = newSD;
  socket->isServer = 0;
  socket->protocol = IPPROTO_TCP;
  socket->tlsFlags = serverSocket->tlsFlags;
  sprintf(socket->debugName,"SD=%d",socket->sd);
  return socket;
}


#define DOT 1
#define FIRST 2
#define SECOND 3
#define THIRD 4

int isV4Numeric(char *chars, int *addr){
  int i,len = strlen(chars);
  int state = DOT;
  int dotCount = 0;
  int value;
  int addressValue = 0;
  for (i=0; i<len; i++){
    char ch = chars[i];
    switch (state){
    case DOT:
      if (ch >= '0' && ch <= '9'){
        if (dotCount == 0 && ch == '0')
          return 0;
        else
          state = FIRST;
      } else {
        return 0;
      }
      break;
    case FIRST:
      if (ch >= '0' && ch <= '9'){
        state = SECOND;
      } else if (ch == '.'){
        state = DOT;
        value = (chars[i-1] - '0');
        addressValue = (addressValue << 8) | value;
        dotCount++;
      } else{
        return 0;
      }
      break;
    case SECOND:
      if (ch >= '0' && ch <= '9'){
        state = THIRD;
      } else if (ch == '.'){
        state = DOT;
        value = ((10 * (chars[i-2] - '0')) +
                 (chars[i-1] - '0'));
        addressValue = (addressValue << 8) | value;     
        dotCount++;
      } else{
        return 0;
      }
      break;
    case THIRD:
      if (ch >= '0' && ch <= '9'){
        return 0;
      } else if (ch == '.'){
        state = DOT;
        value = ((100 * (chars[i-3] - '0')) +
                 (10 * (chars[i-2] - '0')) +
                 (chars[i-1] - '0'));
        addressValue = (addressValue << 8) | value;
        if (value < 256)
          dotCount++;
        else
          return 0;
      } else{
        return 0;
      }
      break;
    }
  }
  if (dotCount == 3){
    switch (state){
    case FIRST:
      value = (chars[i-1] - '0');
      break;
    case SECOND:
      value = ((10 * (chars[i-2] - '0')) +
               (chars[i-1] - '0'));
      break;
    case THIRD:
      value = ((100 * (chars[i-3] - '0')) +
               (10 * (chars[i-2] - '0')) +
               (chars[i-1] - '0'));
      break;
    }
    addressValue = (addressValue << 8) | value;
    *addr = htonl(addressValue);
    return (value < 256);
  } else{
    return 0;
  }
}

/* the return value is in network byte order */

int getV4HostByName(char *hostName){
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int len = strlen(hostName);
  struct hostent *hostEntPtr;

  hostEntPtr = gethostbyname(hostName);
  if (socketTrace){
    printf("hostent addr = %x\n",*((int*)hostEntPtr));
  }
  if (hostEntPtr){
    int i = 0;
    int numericAddress = 0;
    if (socketTrace){
      dumpbuffer((char*)hostEntPtr,20);
    }
    if (socketTrace) {
      printf("hostent->length=%d name=%s\n", hostEntPtr->h_length, hostEntPtr->h_name);
      fflush(stdout);
    }
    do {
      char *hostAddr = hostEntPtr->h_addr_list[i];
      if (hostAddr == NULL){
        break;
      }
      numericAddress = *((int*)hostAddr); /*very IP-v4 here */  
      if (socketTrace) {
        printf("hostAddr is at 0x%x\n", hostAddr);
        printf("numeric=0x%x == %d.%d.%d.%d\n", numericAddress, (int) hostAddr[0], (int) hostAddr[1], (int) hostAddr[2],
            (int) hostAddr[3]);
      }
      i++;
    } while (TRUE);
    return numericAddress;
  } else{
    returnCode = errno;
    reasonCode = returnCode;
    if (socketTrace) {
      printf("getHostName V4 failure, returnCode %d reason code %d\n", returnCode, reasonCode);
    }
    return 0;
  }
}


InetAddr *getAddressByName(char *addressString){
  int numericAddress = 0x7F123456;

  if (!isV4Numeric(addressString,&numericAddress)) { /* updates numericAddress if addressString was a V4 IP address */
    numericAddress = getV4HostByName(addressString);
  }
  
  if ((numericAddress != 0x7F123456) &&
      (0 != numericAddress))
  {
    InetAddr *addr = (InetAddr*)safeMalloc(sizeof(InetAddr),"Inet Address");
    memset(addr,0,sizeof(InetAddr));
    addr->type = AF_INET;
    addr->port = 0;
    memcpy(&(addr->data.data4),&numericAddress,4);
#ifdef DEBUG
    printf("InetAddr structure: AF_INET=0x%x\n",AF_INET);
    dumpbuffer((char*)addr,sizeof(InetAddr));
    fflush(stdout);
#endif
    return addr;
  } else{
    return NULL;
  }
}

int getLocalHostName(char* inout_hostname,
                     unsigned int* inout_hostname_len,
                     int *returnCode, int *reasonCode)
{
  int result = 0;
  /* Parameter check code copied from z/OS implementation.
     Seems odd that this one routine would bother... */
  if ((NULL == inout_hostname) ||
      (NULL == inout_hostname_len) ||
      (NULL == returnCode) || (NULL == reasonCode))
  {
    result = -1;
  } else {
    memset(inout_hostname, 0, (size_t)(*inout_hostname_len));
    result = gethostname(inout_hostname, *inout_hostname_len);
    if (result < 0) {
      *reasonCode = *returnCode = errno;
      if (socketTrace) {
        printf("gethostname(*,%d) failed, errno=%d (%s)\n",
               *returnCode, strerror(*returnCode));
      }
    } else {
      /* gethostname doesn't provide a length, it just null-terminates 
         its output, if there is space. But the caller wants the length,
         so figure it out. Note that, idiotically, the length includes
         the trailing null (that's how z/OS does it). */
      if (inout_hostname[(*inout_hostname_len)-1] == 0) {
        /* it fit, including the terminator */
        *inout_hostname_len = 1+(int)strlen(inout_hostname);
      } /* else - it filled the available space. */
    }
  }
  return result;
}

int socketClose(Socket *socket, int *returnCode, int *reasonCode){
  if (socketTrace) {
    printf("socketClose(%s): isServer=%s,proto=%d: ",
           socket->debugName, (socket->isServer ? "true" : "false"), (int) socket->protocol);
  }

  int status = 0;
  *reasonCode = *returnCode = 0;

#ifdef USE_RS_SSL
  if ((0 == socket->isServer) && (NULL != socket->sslHandle)) {
    /* sslHandle is RS_SSL_CONNECTION in this case */
    /* this call does a C stdlib shutdown/close internally */
    status = rs_ssl_releaseConnection(socket->sslHandle);
    return status;
  }
#endif

  status = close(socket->sd);

  if (0 == status) {
    if (socketTrace) {
      printf(" OK\n");
    }
  } else {
    *reasonCode = *returnCode = errno;
    if (socketTrace) {
      printf("failed: %d (%s)\n", errno, strerror(errno));
    }
  }
  return status;
} 

int getSocketName(Socket *socket, SocketAddress *socketAddress)
{
  InetAddr name;
  int status = getEndPointName(socket->sd, &name, 0);
  if (0 == status) {
    setSocketAddr(socketAddress, &name, 0);
  }
  return status;
}

int getSocketName2(Socket *socket, SocketAddress *socketAddress)
{
  InetAddr name;
  int status = getEndPointName(socket->sd, &name, 1);
  if (0 == status) {
    setSocketAddr(socketAddress, &name, 0);
  }
  return status;
}

/***************************************************************/

#ifdef __ZOWE_OS_AIX
Socket** makeEventSockets()
{
  int eventFD[2];
  int sts = 0;
  Socket** rtnSockets = NULL;

  do {
    sts = pipe(eventFD);
    if (sts < 0) {
      if (socketTrace) {
        printf("makeEventSockets failed in pipe command. errno = %d (%s)\n", errno, strerror(errno));
        break;
      }
    }

    rtnSockets = (Socket**)safeMalloc(2*sizeof(Socket*), "evensockets array");
    rtnSockets[0] = (Socket*)safeMalloc(sizeof(Socket),"Socket1");
    memset(rtnSockets[0], 0, sizeof(Socket));
    rtnSockets[0]->sd = eventFD[0];
    rtnSockets[0]->protocol = IPPROTO_EVENT;
    sprintf(rtnSockets[0]->debugName,"EVENT0=%d",eventFD[0]);
    if (socketTrace) {
      printf("makeEventSocket() returned %s\n", rtnSockets[0]->debugName);
    }
    rtnSockets[1] = (Socket*)safeMalloc(sizeof(Socket),"Socket2");
    memset(rtnSockets[1], 0, sizeof(Socket));
    rtnSockets[1]->sd = eventFD[1];
    rtnSockets[1]->protocol = IPPROTO_EVENT;
    sprintf(rtnSockets[1]->debugName,"EVENT1=%d",eventFD[1]);
    if (socketTrace) {
      printf("makeEventSocket() returned %s\n", rtnSockets[1]->debugName);
    }
  } while(0);

  return rtnSockets;
}
#else
Socket* makeEventSocket() 
{
  int eventFD;
  if (0 > (eventFD = eventfd(0, 0))) {
    if (socketTrace) {
      int error = errno;
      printf("makeEventSocket() failed creating eventfd. errno=%d (%s)\n",
             error, strerror(error));
    }
    return NULL;
  }
  if (eventFD >= FD_SETSIZE) {
    if (socketTrace) {
      printf("makeEventSocket() failed; read side FD (%d) exceeds FD_SETSIZE (%d)\n",
             eventFD, (int) FD_SETSIZE);
    }
    return NULL;
  }
  Socket* socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
  memset(socket, 0, sizeof(Socket));
  socket->sd = eventFD;
  socket->protocol = IPPROTO_EVENT;
  sprintf(socket->debugName,"EVENT=%d",eventFD);
  if (socketTrace) {
    printf("makeEventSocket() returned %s\n", socket->debugName);
  }
  return socket;
}
#endif

int postEventSocket(Socket* socket)
{
  if (socket->protocol != IPPROTO_EVENT) {
    if (socketTrace) {
      printf("postEventSocket called for non-event socket %s\n",
             socket->debugName);
      return EINVAL;
    }
  }

  uint64_t bump = 1;
  if (socketTrace > 1) {
    printf("postEventSocket called for %s\n", socket->debugName);
  }
  ssize_t written = write(socket->sd, &bump, sizeof(uint64_t));
  if (written != sizeof(uint64_t)) {
    if (socketTrace) {
      int error = errno;
      printf("postEventSocket write for %s failed. errno=%d (%s)\n",
             socket->debugName, error, strerror(error));
      return error;
    }
  }
  return 0;
}

uint64_t waitForEventSocket(Socket* socket)
{
  if (socket->protocol != IPPROTO_EVENT) {
    if (socketTrace) {
      printf("waitForEventSocket called for non-event socket %s\n",
             socket->debugName);
      return 0;
    }
  }

  uint64_t count = 0;
  ssize_t bytesRead = read(socket->sd, &count, sizeof(uint64_t));
  if (bytesRead != sizeof(uint64_t)) {
    if (socketTrace) {
      int error = errno;
      printf("waitForEventSocket read for %s failed. errno=%d (%s)\n",
             socket->debugName, error, strerror(error));
    }
    count = 0;
  } else {
    if (socketTrace) {
      printf("waitForEventSocket called for %s; returned %" PRIu64 "\n",
             socket->debugName, count);
    }
  }
  return count;
}

void clearEventSocket(Socket* socket)
{
  /* Cheesy */
  if (0 == postEventSocket(socket)) {
    uint64_t count =  waitForEventSocket(socket);
    if (socketTrace) {
      if (count > 0) {
        /* success */
        printf("clearEventSocket called for %s; there were %" PRIu64 " pending notifications\n",
               socket->debugName, count-1);
      } else {
        printf("clearEventSocket called for %s; wait failed.\n", socket->debugName);
      }
    }
  } else {
    if (socketTrace) {
      printf("clearEventSocket called for %s; post failed.\n", socket->debugName);
    }
  }
}

SocketSet *makeSocketSet(int highestAllowedSD){
  if ((highestAllowedSD < 1) || (highestAllowedSD >= FD_SETSIZE)) {
    if (socketTrace){
      printf("makeSocketSet(%d) invalid\n", highestAllowedSD);
    }
    return NULL;
  }
  SocketSet *set = (SocketSet*)safeMalloc(sizeof(SocketSet),"SocketSet");
  memset(set,0,sizeof(SocketSet));
  size_t socketArraySize = (size_t) ((highestAllowedSD+1)*sizeof(Socket*));
  set->sockets = (Socket**)safeMalloc(socketArraySize, "SocketArray");
  memset(set->sockets, 0, socketArraySize);
  set->highestAllowedSD = highestAllowedSD;
  FD_ZERO(&(set->allSDs));
  return set;
}

void freeSocketSet(SocketSet *set){
  safeFree((char*) set, sizeof(SocketSet));
}

void setSocketSetIdleTimeLimit(SocketSet *set, int idleSeconds) {
  set->idleTimeLimit = (uint64_t)idleSeconds;
}

int socketSetAdd(SocketSet *set, Socket *socket){
  int sd = socket->sd;

  if ((sd > set->highestAllowedSD) || (sd < 0)){
    if (socketTrace){
      printf("socketSetAdd for SD=%d (%s) out of range (> %d)\n",sd, socket->debugName, set->highestAllowedSD);
    }
    return 12;
  }

  if (FD_ISSET(sd, &(set->allSDs)) || (set->sockets[sd] != NULL)) {
    if (socketTrace){
      printf("socketSetAdd for SD=%d (%s) failed - socket already in set\n",sd, socket->debugName);
    }
    return 12;
  }

  if (NULL == set->sockets[sd])
  {
    if (socketTrace > 1){
      printf("socketSetAdd for SD=%d (%s) succeeded\n",sd, socket->debugName);
    }
    FD_SET(sd, &(set->allSDs));
    set->sockets[sd] = socket;
    ++(set->socketCount);
  }

  return 0;
}

int socketSetRemove(SocketSet *set, Socket *socket){
  int sd = socket->sd;

  if ((sd > set->highestAllowedSD) || (sd < 0)){
    if (socketTrace){
      printf("socketSetRemove for SD=%d (%s) out of range (> %d)\n",sd, socket->debugName, set->highestAllowedSD);
    }
    return 12;
  }
  if (!FD_ISSET(sd, &(set->allSDs)) || (set->sockets[sd] == NULL)) {
    if (socketTrace){
      printf("socketSetRemove for SD=%d (%s) failed - socket not in set\n",sd, socket->debugName);
    }
    return 12;
  }

  if (NULL != set->sockets[sd])
  {
    if (socketTrace > 1){
      printf("socketSetRemove for SD=%d (%s) succeeded\n",sd, socket->debugName);
    }
    FD_CLR(sd, &(set->allSDs));
    set->sockets[sd] = NULL;
    --(set->socketCount);
  }
  
  return 0;
}

static
void printSocketSetStatus(SocketSet *set, fd_set* fdset, const char* label)
{
  printf("%s: maxSD=%d\n", label, set->highestAllowedSD);
  for (int i = 0; i < set->highestAllowedSD+1; ++i) {
    if (FD_ISSET(i, fdset)) {
      Socket* s = set->sockets[i];
      printf("  [%02d]: %s\n", i, (s ? s->debugName : "INVALID"));
    }
  }
}

int extendedSelect(SocketSet *set,
                   int timeout,             /* in milliseconds */
                   int checkWrite, int checkRead, 
                   int *returnCode, int *reasonCode){
  *reasonCode = *returnCode = 0;
  struct timeval theTimeout = {0,0};
  struct timeval* theTimeoutPtr;

  memcpy(&(set->scratchReadMask), &(set->allSDs), sizeof(fd_set));
  memcpy(&(set->scratchWriteMask), &(set->allSDs), sizeof(fd_set));
  memcpy(&(set->scratchErrorMask), &(set->allSDs), sizeof(fd_set));
  
  if (timeout >= 0) {
    theTimeout.tv_sec = timeout/1000;
    theTimeout.tv_usec = 1000*(timeout%1000);
    theTimeoutPtr = &theTimeout;
  } else {
    theTimeoutPtr = (struct timeval*) NULL;
  }

  int maxSoc = set->highestAllowedSD+1;

  int status = select(maxSoc,
                      (checkRead  ? &(set->scratchReadMask)  : (fd_set*) 0),
                      (checkWrite ? &(set->scratchWriteMask) : (fd_set*) 0),
                      &(set->scratchErrorMask),
                      theTimeoutPtr);
  *reasonCode = *returnCode = (status < 0) ? errno : 0;

  if (socketTrace){
    printf("extendedSelect(maxSoc=%d, timeout={%d,%d}, write=%s, read=%s) "
           "returns %d with errno=%d (%s)\n",
           maxSoc, timeout/1000, 1000*(timeout%1000),
           (checkWrite ? "true" : "false"), (checkRead ? "true" : "false"), 
           status, *returnCode, strerror(*returnCode));
    if (socketTrace > 1) {
      if (socketTrace > 1) {
        printSocketSetStatus(set, &(set->allSDs), "allSDs");
      }
      if (checkRead) {
        printSocketSetStatus(set, &(set->scratchReadMask), "read");
      }
      if (checkWrite) {
        printSocketSetStatus(set, &(set->scratchWriteMask), "write");
      }
      if (socketTrace > 2) {
        printSocketSetStatus(set, &(set->scratchErrorMask), "error");
      }
    }
  }
  return status;
}

static
int setSocketOptionEx(Socket *socket,
                      int level, const char* levelDesc, 
                      int optionName, const char* optionDesc,
                      int optionDataLength, char *optionData,
                      int *returnCode, int *reasonCode)
{
  int status =  setsockopt(socket->sd, level, optionName, optionData, (socklen_t) optionDataLength);
  if (status < 0){
    *reasonCode = *returnCode = errno;
    if (socketTrace){
      printf("setsockopt(%s, level=%d%s, option=%d%s, len=%d) failed, errno=%d (%s)\n",
             socket->debugName, level, levelDesc, optionName, optionDesc, optionDataLength, *returnCode, strerror(*returnCode));
    }
  } else {
    *reasonCode = *returnCode = 0;
    if (socketTrace){
      printf("setsockopt(%s, level=%d%s, option=%d%s, len=%d) OK\n",
             socket->debugName, level, levelDesc, optionName, optionDesc, optionDataLength);
    }
  }
  return status;
}

int setSocketOption(Socket *socket, int level, int optionName, int optionDataLength, char *optionData,
		    int *returnCode, int *reasonCode)
{
  return setSocketOptionEx(socket, level, "", optionName, "", optionDataLength, optionData, returnCode, reasonCode);
}

void setSocketIdleTimeoutMode(Socket *socket, int enableIdleTimeout) {
  // Socket idle timeout support is currently only supported for TCPIP
  if (socket->protocol == IPPROTO_TCP) {
    if (enableIdleTimeout == 0) {
      socket->idleTimeoutFlags = IDLE_TIMEOUT_DISABLED;
    } else {
      socket->idleTimeoutFlags = IDLE_TIMEOUT_ENABLED;
      if (sxSocketIsReady(socket)) {               // TLS is not requested or handshake is complete
        socket->readyTime = socket->createTime;    // Use create for ready time
      }
    }
  }
}

int setSocketNoDelay(Socket *socket, int noDelay, int *returnCode, int *reasonCode){
  return setSocketOptionEx(socket,IPPROTO_TCP, " (IPPROTO_TCP)",TCP_NODELAY, " (TCP_NODELAY)", sizeof(int),(char*)&noDelay,returnCode,reasonCode);
}

int setSocketWriteBufferSize(Socket *socket, int bufferSize, int *returnCode, int *reasonCode){
  return setSocketOptionEx(socket,SOL_SOCKET, " (SOL_SOCKET)",SO_SNDBUF, " (SO_SNDBUF)", sizeof(int),(char*)&bufferSize,returnCode,reasonCode);
}

int setSocketReadBufferSize(Socket *socket, int bufferSize, int *returnCode, int *reasonCode){
  return setSocketOptionEx(socket,SOL_SOCKET, " (SOL_SOCKET)",SO_RCVBUF, " (SO_RCVBUF)", sizeof(int),(char*)&bufferSize,returnCode,reasonCode);
}
