

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
#include <errno.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"

#ifdef USE_RS_SSL
#include "rs_ssl.h"
#endif

#ifdef _LP64
#pragma linkage(BPX4SOC,OS)
#pragma linkage(BPX4CON,OS)
#pragma linkage(BPX4GHN,OS)
#pragma linkage(BPX4SLP,OS)
#pragma linkage(BPX4BND,OS)
#pragma linkage(BPX4LSN,OS)
#pragma linkage(BPX4ACP,OS)
#pragma linkage(BPX4SEL,OS)
#pragma linkage(BPX4OPT,OS)
#pragma linkage(BPX4GNM,OS)
#pragma linkage(BPX4STO,OS)
#pragma linkage(BPX4RFM,OS)
#pragma linkage(BPX4HST,OS)
#pragma linkage(BPX4IOC,OS)
#pragma linkage(BPX4RED,OS)
#pragma linkage(BPX4WRT,OS)
#pragma linkage(BPX4FCT,OS)
#pragma linkage(BPX4CLO,OS)

#define BPXSOC BPX4SOC
#define BPXCON BPX4CON
#define BPXGHN BPX4GHN
#define BPXSLP BPX4SLP
#define BPXBND BPX4BND
#define BPXLSN BPX4LSN
#define BPXACP BPX4ACP
#define BPXSEL BPX4SEL
#define BPXOPT BPX4OPT
#define BPXGNM BPX4GNM
#define BPXSTO BPX4STO
#define BPXRFM BPX4RFM
#define BPXHST BPX4HST
#define BPXIOC BPX4IOC
#define BPXRED BPX4RED
#define BPXWRT BPX4WRT
#define BPXFCT BPX4FCT
#define BPXCLO BPX4CLO

#else

#pragma linkage(BPX1SOC,OS)
#pragma linkage(BPX1CON,OS)
#pragma linkage(BPX1GHN,OS)
#pragma linkage(BPX1SLP,OS)
#pragma linkage(BPX1BND,OS)
#pragma linkage(BPX1LSN,OS)
#pragma linkage(BPX1ACP,OS)
#pragma linkage(BPX1SEL,OS)
#pragma linkage(BPX1OPT,OS)
#pragma linkage(BPX1GNM,OS)
#pragma linkage(BPX1STO,OS)
#pragma linkage(BPX1RFM,OS)
#pragma linkage(BPX1HST,OS)
#pragma linkage(BPX1IOC,OS)
#pragma linkage(BPX1RED,OS)
#pragma linkage(BPX1WRT,OS)
#pragma linkage(BPX1FCT,OS)
#pragma linkage(BPX1CLO,OS)

#define BPXSOC BPX1SOC
#define BPXCON BPX1CON
#define BPXGHN BPX1GHN
#define BPXSLP BPX1SLP
#define BPXBND BPX1BND
#define BPXLSN BPX1LSN
#define BPXACP BPX1ACP
#define BPXSEL BPX1SEL
#define BPXOPT BPX1OPT
#define BPXGNM BPX1GNM
#define BPXSTO BPX1STO
#define BPXRFM BPX1RFM
#define BPXHST BPX1HST
#define BPXIOC BPX1IOC
#define BPXRED BPX1RED
#define BPXWRT BPX1WRT
#define BPXFCT BPX1FCT
#define BPXCLO BPX1CLO

#endif

#define SOCK_SO_REUSEADDR 0x00000004
#define SOCK_SO_SNDBUF    0x00001001
#define SOCK_SO_RCVBUF    0x00001002
#define SOCK_SO_SNDLOWAT  0x00001003
#define SOCK_SO_RCVLOWAT  0x00001004
#define SOCK_SO_SNDTIMEO  0x00001005
#define SOCK_SO_RCVTIMEO  0x00001006

#ifndef SOL_SOCKET
#define SOL_SOCKET  0x0000FFFF
#endif

#ifndef TCP_NODELAY
#define TCP_NODELAY 0x00000001
#endif

static int socketTrace = 0;
int setSocketTrace(int toWhat) {
  int was = socketTrace;
#ifndef METTLE
  socketTrace = toWhat;
#endif
  return was;
}

unsigned int sleep(unsigned int seconds){
  unsigned int returnValue;
  unsigned int *returnValuePtr;
  
#ifndef _LP64
  returnValuePtr = (unsigned int*) (0x80000000 | ((int)&returnValue));
#else
  returnValuePtr = &returnValue;
#endif
  BPXSLP(seconds,returnValuePtr); 
  return returnValue;
}

void bpxSleep(int seconds)
{
  sleep(seconds);
}

int socketInit(char *uniqueName){
  /* do nothing for now */
}

SocketAddress *makeSocketAddr(InetAddr *addr, 
			      unsigned short port){
  SocketAddress *address = (SocketAddress*)safeMalloc31(sizeof(SocketAddress),"BPX SocketAddress");
  memset(address,0,sizeof(SocketAddress));
  address->length = 14;
  address->family = AF_INET;
  address->port = port;
  if (addr){
    address->v4Address = addr->data.data4.addrBytes;
  } else{
    address->v4Address = 0; /* 0.0.0.0 */
  }
  return address;
}

SocketAddress *makeSocketAddrIPv6(InetAddr *addr, unsigned short port){
  SocketAddress *address = (SocketAddress*)safeMalloc31(sizeof(SocketAddress),"BPX SocketAddress");
  memset(address,0,sizeof(SocketAddress));
  if (socketTrace){
    printf("socket address at 0x%x\n",address);
  }
  address->length = 26;
  address->family = AF_INET6;
  address->port = port;
  if (addr){
    address->data6 = addr->data.data6;
  }
  if (socketTrace){
    printf("about to return socket address at 0x%x\n",address);
  }
  return address;
}

/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/
#define SO_ERROR    0x1007

Socket *tcpClient3(SocketAddress *socketAddress,
         int timeoutInMillis,
         int tlsFlags,
         int *returnCode,
         int *reasonCode) {
/* BPX1SOC,(Domain,
              Type,                                                  
              Protocol,
              Dimension,
              Socket_vector,
              Return_value,
              Return_code,
              Reason_code)
    */
  int socketVector[2];
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int status = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXSOC(AF_INET,
         SOCTYPE_STREAM,
         IPPROTO_TCP,
         1,
         socketVector,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (socketTrace){
    printf("BPXSOC returnValue %d returnCode %d reasonCode %d\n",
           returnValue,*returnCode,*reasonCode);
    printf("socketVector[0] = %d\n",socketVector[0]);
  }
  if (returnValue != 0){
    if (socketTrace){
      printf("Failed to create socket\n");
    }
    return NULL;
  } else{
    int socketAddrSize = SOCKET_ADDRESS_SIZE_IPV4;
    if (timeoutInMillis >= 0){
      Socket tempSocket;
      tempSocket.sd = socketVector[0];

      setSocketBlockingMode(&tempSocket, TRUE, returnCode, reasonCode);
      BPXCON(socketVector,
              &socketAddrSize,
              socketAddress,
              &returnValue,
              returnCode,
              reasonCodePtr);
      if (socketTrace) {
        printf("BPXCON returnValue %d returnCode %x reasonCode %x\n",
                returnValue, *returnCode, *reasonCode);
      }
      /* EINPROGRESS is the expected return code here but we are just being careful by checking
         for EWOULDBLOCK as well
      */
      if (status != 0){
        returnValue = 0;
        *returnCode  = 0;
        *reasonCode  = 0;
        status = tcpStatus(&tempSocket, timeoutInMillis, 1, returnCode, reasonCode);
        if (status == SD_STATUS_TIMEOUT) {
          int sd = socketVector[0];
          if (socketTrace) {
            printf("Failed to connect socket, will clean up sd=%d\n", sd);
          }
          returnValue = 0;
          *returnCode = 0;
          *reasonCode = 0;
          BPXCLO(&sd,
                  &returnValue,
                  returnCode,
                  reasonCodePtr);
          if (socketTrace) {
            printf("BPXCLO for time out connect returnValue %d returnCode %d reasonCode %d\n",
                    returnValue, *returnCode, *reasonCode);
          }
          return NULL;
        }
        *returnCode  = 0;
        *reasonCode  = 0;
        setSocketBlockingMode(&tempSocket, FALSE, returnCode, reasonCode);
        int optionLength = sizeof(int);
        int optionData = 0;
        *returnCode  = 0;
        *reasonCode  = 0;
        getSocketOption(&tempSocket, SO_ERROR, &optionLength, (char *)&optionData, returnCode, reasonCode);
        if( optionData > 0 )
        {
          if (socketTrace){
            printf("Failed to connect socket, returnCode %d\n", optionData);
          }
          *returnCode = optionData;
          returnValue = -1;
        }
        else
        {
          returnValue = 0;
        }
      }
    }
    else{
      BPXCON(socketVector,
              &socketAddrSize,
              socketAddress,
              &returnValue,
              returnCode,
              reasonCodePtr);
      if (socketTrace){
        printf("BPXCON returnValue %d returnCode %x reasonCode %x\n",
           returnValue,*returnCode,*reasonCode);
      }
    }
    if (returnValue){
      int sd = socketVector[0];

      if (socketTrace){
        printf("Failed to connect socket, will clean up sd=%d\n",sd);
      }
      *returnCode  = 0;
      *reasonCode  = 0;
      BPXCLO(&sd,
          &returnValue,
          returnCode,
          reasonCodePtr);
      if (socketTrace){
        printf("BPXCLO for failed connect returnValue %d returnCode %d reasonCode %d\n",
          returnValue,*returnCode,*reasonCode);
      }
      return NULL;
    } else{
      Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
      socket->sd = socketVector[0];
      sprintf(socket->debugName,"SD=%d",socket->sd);
      socket->isServer = 0;
      socket->tlsFlags = tlsFlags;
      /* manual says returnCode and value only meaningful if return value = -1 */
      *returnCode = 0;
      *reasonCode = 0;
      socket->protocol = IPPROTO_TCP;
      return socket;
    }
  }                    
}

Socket *tcpClient2(SocketAddress *socketAddress,
       int timeoutInMillis,
       int *returnCode, /* errnum */
       int *reasonCode) { /* errnum - JR's */
  return tcpClient3(socketAddress,
                    timeoutInMillis,
                    0,
                    returnCode,
                    reasonCode);
}

Socket *tcpClient(SocketAddress *socketAddress,
       int *returnCode, /* errnum */
       int *reasonCode){ /* errnum - JR's */
  return tcpClient2(socketAddress,-1,returnCode,reasonCode);
}


void socketFree(Socket *socket){
  safeFree((char*)socket,sizeof(Socket));
}

Socket *udpPeer(SocketAddress *socketAddress, 
                int *returnCode, /* errnum */
                int *reasonCode){  /* errnum - JR's */
  int socketVector[2];
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int status;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXSOC(AF_INET,
         SOCTYPE_DATAGRAM,
         IPPROTO_UDP,
         1,
         socketVector,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (socketTrace){
    printf("BPXSOC returnValue %d returnCode %d reasonCode %d\n",
	   returnValue,*returnCode,*reasonCode);
    printf("socketVector[0] = %d\n",socketVector[0]);
  }
  if (returnValue != 0){
    if (socketTrace){
      printf("Failed to create socket\n");
    }
    return NULL;
  } else{
    int sd = socketVector[0];
    int socketAddressSize = SOCKET_ADDRESS_SIZE_IPV4;
    BPXBND(&sd,
           &socketAddressSize,
           socketAddress,
           &returnValue,
           returnCode,
           reasonCodePtr);
    if (socketTrace) {
      printf("BPXBND returnValue %d returnCode %x reasonCode %x\n",
             returnValue, *returnCode, *reasonCode);
    }
    if (returnValue != 0){
      return NULL;
    } else{
      Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
      socket->sd = socketVector[0];
      sprintf(socket->debugName,"SD=%d",socket->sd);
      socket->isServer = 0;
      /* manual says returnCode and value only meaningful if return value = -1 */
      *returnCode = 0;
      *reasonCode = 0;
      socket->protocol = IPPROTO_UDP;
      return socket;
    }
  }
}

Socket *tcpServer2(InetAddr *addr,
                   int port,
                   int tlsFlags,
                   int *returnCode,
                   int *reasonCode)
{
  int socketVector[2];
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int status;
  int *reasonCodePtr;
  int inet = 0;
  if (addr != NULL)
  {
    inet = addr->type;
  }
  else
  {
    inet = AF_INET6;
  }

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXSOC(inet,
         SOCTYPE_STREAM,
         IPPROTO_TCP,
         1,
         socketVector,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (returnValue != 0) {
    if (*returnCode == EAFNOSUPPORT && (inet == AF_INET6)){
      /* Fall back to IPv4 - see https://www-304.ibm.com/servers/resourcelink/svc00100.nsf/pages/zosv2r3sc273663/$file/hale001_v2r3.pdf */
      if (socketTrace) {
        printf("IPv6 is not enabled on this system.  Trying an IPv4 socket.\n");
      }
      inet = AF_INET;
      *returnCode = 0; *reasonCode = 0;
      status = BPXSOC(inet,
                      SOCTYPE_STREAM,
                      IPPROTO_TCP,
                      1,
                      socketVector,
                      &returnValue,
                      returnCode,
                      reasonCodePtr);
    }
    if (returnValue != 0) {
      if (socketTrace) {
        printf("Failed to create socket\n");
      }
      return NULL;
    } else {
      if (socketTrace) {
        printf("Socket created successfully!\n");
      }
    }
  }
  if (returnValue == 0) {
    int sd = socketVector[0];
    int socketAddressSize = (inet == AF_INET6) ? SOCKET_ADDRESS_SIZE_IPV6 : SOCKET_ADDRESS_SIZE_IPV4;
    int inetAddressSize = sizeof(InetAddr);
    SocketAddress *socketAddress = (inet == AF_INET6) ? makeSocketAddrIPv6(addr, (unsigned short)port) : makeSocketAddr(addr, (unsigned short)port);


    if (socketTrace){
      printf("server SocketAddress = %d\n",sd);
      dumpbuffer((char*)socketAddress,sizeof(SocketAddress));
      dumpbuffer((char*)addr,sizeof(InetAddr));
    }
    setSocketReuseAddr(&sd,
                       returnCode,
                       reasonCodePtr);
    if (returnValue != 0){
      if (socketTrace){
        printf("Failed to set SO_REUSEADDR errno=%d reason=0x%x\n",
               *returnCode,*reasonCode);
      }
    }
    BPXBND(&sd,
           &socketAddressSize,
           socketAddress,
           &returnValue,
           returnCode,
           reasonCodePtr);
    if (returnValue != 0){
      if (socketTrace){
        printf("Failed to bind server socket errno=%d reason=0x%x\n",
               *returnCode,*reasonCode);
      }
      safeFree31((char*)socketAddress,sizeof(SocketAddress));
      return NULL;
    } else{
      int backlogQueueLength = 100;
      BPXLSN(&sd,
             &backlogQueueLength,
             &returnValue,
             returnCode,
             reasonCodePtr);
      if (returnValue){
        if (socketTrace){
          printf("Failed to start server socket listen errno=%d reason=0x%x\n",
                 *returnCode,*reasonCode);
        }
        return NULL;
      } else {
        Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
        /* manual says returnCode and value only meaningful if return value = -1 */
        *returnCode = 0;
        *reasonCode = 0;

        socket->sd = sd;
        sprintf(socket->debugName,"SD=%d",socket->sd);
        socket->isServer = 1;
        socket->tlsFlags = tlsFlags;
        socket->protocol = IPPROTO_TCP;
        if (socketTrace) {
          printf("Immediately before return from tcpServer, socket contains:\n");
          dumpbuffer((char*)socket, sizeof(Socket));
        }
        return socket;
      }
    }
  }
  return NULL;
}

Socket *tcpServer(InetAddr *addr, /* usually NULL/0 */
                  int port,
                  int *returnCode,
                  int *reasonCode){
  return tcpServer2(addr, port, 0, returnCode, reasonCode);
}

/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/


Socket *socketAccept(Socket *serverSocket, int *returnCode, int *reasonCode){
  int sd = serverSocket->sd;
  int status;
  int socketAddressSize = 0; // We do not care what the socket address is                                                                                                                                                                                      
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXACP(&sd,
         &socketAddressSize,
         NULL,             
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (returnValue == -1){
    printf("Failed to accept new socket errno=%d reasonCode=0x%x\n",
           *returnCode,*reasonCode);
    return NULL;
  } else{
    Socket *socket = (Socket*)safeMalloc31(sizeof(Socket),"ServerSideSocket");
    socket->sd = returnValue;
    sprintf(socket->debugName,"SD=%d",socket->sd);
    socket->isServer = 0;
    socket->protocol = IPPROTO_TCP;
    socket->tlsFlags = serverSocket->tlsFlags;
    return socket;
  }
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
    *addr = addressValue;
    return (value < 256);
  } else{
    return 0;
  }
}

char* getV4HostEntByName(char *string, int* rc, int* rsn){
  int returnValue = 0;
  int *reasonCodePtr;
  int len = strlen(string);
  char *hostEntPtr;
  int status;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)rsn));
#else
  reasonCodePtr = rsn;
#endif
  status = BPXGHN(string,
                  &len,
                  &hostEntPtr,
                  &returnValue,
                  rc,
                  reasonCodePtr);
  if (socketTrace){
    printf("hostent addr = %x\n",*((int*)hostEntPtr));
  }

  return hostEntPtr;
}

int getV4HostByName(char *string){
  int returnValue = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int *reasonCodePtr;
  int len = strlen(string);
  char *hostEntPtr;
  int status;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)&reasonCode));
#else
  reasonCodePtr = &reasonCode;
#endif
  BPXGHN(string,
         &len,
         &hostEntPtr,
         &returnValue,
         &returnCode,
         reasonCodePtr);
  /* TBD: Check return codes */
  if (socketTrace){
    printf("hostent addr = %x\n",*((int*)hostEntPtr));
  }
  if (hostEntPtr){
    Hostent *hostent = (Hostent*)hostEntPtr;
    int i;
    int numericAddress = 0;
    /* dumpbuffer((char*)hostent,20); */
    for (i=0; i<hostent->length; i++){
      if (socketTrace){
	printf("  addr[%d] = %x\n",i,hostent->addrList[i]);
      }
      if (hostent->addrList[i]){
        numericAddress = *(hostent->addrList[i]);
        break;
      }
    }
    return numericAddress;
  } else{
    return 0;
  }
}

#define FIOASYNC 0x8004A77D

int tcpIOControl(Socket *socket, int command, int argumentLength, char *argument, 
                 int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  /* printf("before read sd=%d buffer=%x\n",sd,buffer); */
  BPXIOC(&sd,
          &command,
          &argumentLength,
          argument,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    printf("ioctl failed ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    return -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }


}

/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/

#define SOCKET_MASK_SIZE 4               
#define MAX_SD_NUMBER (32*sizeof(int))   

static int tcpStatusInternal(Socket *socket, int timeout, 
                             int checkWrite, int checkRead,
                             int *returnCode, int *reasonCode){
  /* what does it MEAN?? */
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int userOptionField = 0; /* bits backward */
  int maxSoc = sd+1;
  int socketMaskSize = SOCKET_MASK_SIZE;
  int wordIndex = sd / (8*sizeof(int));
  int activeLength = (sd+32)/32;
  int wordPos = activeLength-(1+wordIndex);
  int bitIndex = sd-(wordIndex*sizeof(int));
  int rsndmsk[SOCKET_MASK_SIZE];
  int wsndmsk[SOCKET_MASK_SIZE];
  int esndmsk[SOCKET_MASK_SIZE];
  int timeoutSpec[4];
  int *timeoutSpecPtr;
  int **timeoutSpecHandle;
  
  memset(rsndmsk,0,SOCKET_MASK_SIZE*sizeof(int));
  memset(wsndmsk,0,SOCKET_MASK_SIZE*sizeof(int));
  memset(esndmsk,0,SOCKET_MASK_SIZE*sizeof(int));
  if (timeout >= 0) {
#ifndef _LP64
    timeoutSpec[0] = timeout/1000; /* millis to seconds */
    timeoutSpec[1] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#else
    timeoutSpec[0] = 0; 
    timeoutSpec[1] = timeout/1000; /* millis to seconds */
    timeoutSpec[2] = 0; /* padding */
    timeoutSpec[3] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#endif

    timeoutSpecPtr = timeoutSpec;
  } else{
    // setting timeout_pointer to 0 implies wait indefinitely
    timeoutSpecPtr = 0; 
  }
  timeoutSpecHandle = &timeoutSpecPtr;

  
  printf("tcpStatus sd=%d wordPos %d bitIndex %d maxSoc %d al %d\n",
         sd,wordPos,bitIndex,maxSoc,activeLength);
  if (checkRead){
    rsndmsk[wordPos] = 1 << bitIndex;
  }
  esndmsk[wordPos] = 1 << bitIndex;
  if (checkWrite){
    wsndmsk[wordPos] = 1 << bitIndex;
  }
  
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXSEL(&maxSoc,
         (checkRead  ? &socketMaskSize : &zero),
         rsndmsk,
         (checkWrite ? &socketMaskSize : &zero),
         wsndmsk,
         &socketMaskSize,
         esndmsk,
         timeoutSpecHandle,
         &zero, /* ecb */
         &userOptionField,
         &returnValue,
         returnCode,
         reasonCodePtr);
  
  if (socketTrace){
    printf("BPXSEL (single) maxSoc=0x%x returnValue 0x%x returnCode 0x%x reasonCode 0x%x\n",
           maxSoc,returnValue,*returnCode,*reasonCode);
  }
  if (returnValue > 0){                            
    int sdStatus = 0;                           
                                                
    if (rsndmsk[wordPos])                       
      sdStatus  |= SD_STATUS_RD_RDY;            
    if (wsndmsk[wordPos])                       
      sdStatus  |= SD_STATUS_WR_RDY;            
    return sdStatus;                            
  } else if (returnValue == 0){ /* timeout */      
    return SD_STATUS_TIMEOUT;                   
  } else if (esndmsk[wordPos]){                
    return SD_STATUS_ERROR;                     
  } else{                                      
    return SD_STATUS_FAILED; /* look in errno */
  }                                             
}

int tcpStatus(Socket *socket, 
              int timeout,   /* in milliseconds */
              int checkWrite, /* otherwise check for read and error */
              int *returnCode, int *reasonCode){
  return tcpStatusInternal(socket,timeout,checkWrite,!checkWrite,returnCode,reasonCode);
}

int tcpStatus2(Socket *socket, 
               int timeout,   /* in milliseconds */
               int checkWrite, int checkRead,
               int *returnCode, int *reasonCode){
  return tcpStatusInternal(socket,timeout,checkWrite,checkRead,returnCode,reasonCode);
}

#define O_NONBLOCK   0x04
#define F_GETFL       3
#define F_SETFL       4

int setSocketBlockingMode(Socket *socket, int isNonBlocking,
                          int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int *reasonCodePtr;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int action = F_GETFL;
  int argument = 0;

  BPXFCT(&sd,
          &action,
          &argument,
          &returnValue,
          returnCode,
          reasonCodePtr);
  
  if (returnValue < 0){
    printf("BPXFCT failed, ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    return -1;
  } else {
    if (socketTrace){
      printf("BPXFCT value %d returnValue %d \n",isNonBlocking, returnValue);
    }
    *returnCode = 0;
    *reasonCode = 0;
  }

  action = F_SETFL;
  if (isNonBlocking)
  {
    argument = returnValue | O_NONBLOCK;
  }
  else
  {
    argument = returnValue & ~(O_NONBLOCK);
  }
  returnValue = 0;
  
  BPXFCT(&sd,
          &action,
          &argument,
          &returnValue,
          returnCode,
          reasonCodePtr);
  
  if (returnValue < 0){
    printf("BPXFCT failed, ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    return returnValue;
  } else {
    if (socketTrace){
      printf("BPXFCT value %d returnValue %d \n",isNonBlocking, returnValue);
    }
    /* this seems bogus 
     *returnCode = 0;
     *reasonCode = 0;
     */
    return returnValue;
  }
}

int socketRead(Socket *socket, char *buffer, int desiredBytes, 
               int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifdef USE_RS_SSL
  if (NULL != socket->sslHandle) { /* sslHandle is an RS_SSL_CONNECTION */
    int bytesRead = 0;
    status = rs_ssl_read(socket->sslHandle, buffer, desiredBytes, &bytesRead);
    if (0 != status) {
      printf("socketRead: rs_ssl_read failed with status: %d\n", status);
      return -1;
    } else {
      return bytesRead;
    }
  }
#endif

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  /* printf("before read sd=%d buffer=%x\n",sd,buffer); */
  BPXRED(&sd,
          &buffer,
          &zero,
          &desiredBytes,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    printf("read failed ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    return -1;
  } else {
    if (socketTrace > 2){
      printf("read %d bytes\n",returnValue);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int socketWrite(Socket *socket, const char *buffer, int desiredBytes, 
	       int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifdef USE_RS_SSL
  if (NULL != socket->sslHandle) { /* sslHandle is RS_SSL_CONNECTION in this case */
    int bytesWritten = 0;
    status = rs_ssl_write(socket->sslHandle, buffer, desiredBytes, &bytesWritten);
    if (0 != status) {
      printf("socketWrite: rs_ssl_write failed with status: %d\n", status);
      return -1;
    } else {
      return bytesWritten;
    }
  }
#endif

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  /* printf("before write sd=%d buffer=%x\n",sd,buffer); */
  BPXWRT(&sd,
	  &buffer,
	  &zero,
	  &desiredBytes,
	  &returnValue,
	  returnCode,
	  reasonCodePtr);
  if (socketTrace > 2){
    printf("SocketWrite writing desired=%d retVal=%d retCode=%d reasonCode=%d\n",
	   desiredBytes,returnValue,*returnCode,*reasonCode);
    fflush(stdout);
  }
  if (returnValue < 0){
    /* using MVD Over VPN, I get a massive stream of the following in the log:
     * "write failed ret code 1102 reason 0x76690000"
     * so I think we should only see those if socketTrace is TRUE
     */
    if ((socketTrace > 2) || !(*returnCode == 1102 && *reasonCode == 0x76690000)){
      printf("write failed ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    }
    return -1;
  } else {
    if (socketTrace > 2){
      printf("wrote %d bytes\n",returnValue);fflush(stdout);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int udpSendTo(Socket *socket, 
              SocketAddress *destinationAddress,
              char *buffer, int desiredBytes, 
              int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int flags = 0;  /* some exotic stuff in doc
                     http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/bpxzb1c0/B.30?SHELF=all13be9&DT=20110609191818#HDRYMSGF
                   */
  int socketAddressSize = SOCKET_ADDRESS_SIZE_IPV4;

  if (socketTrace > 2){
    printf("sendTo desired=%d retVal=%d retCode=%d reasonCode=%d\n",
       desiredBytes,returnValue,*returnCode,*reasonCode);
    dumpbuffer(buffer, desiredBytes); 
  }

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  /* printf("before write sd=%d buffer=%x\n",sd,buffer); */

  BPXSTO(&sd,
          &desiredBytes,
          buffer,
          &zero,
          &flags,
          &socketAddressSize,
          destinationAddress,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    printf("send failed, sd=%d desired write len %d buffer at 0x%x, ret code %d reason 0x%x\n",
	   sd,desiredBytes,buffer,*returnCode,*reasonCode);
    return -1;
  } else {
    if (socketTrace > 2){
      printf("send %d bytes\n",returnValue);fflush(stdout);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int getSocketOption(Socket *socket, int optionName, int *optionDataLength, char *optionData,
        int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int getOption = 1;
  int setOption = 2;
  int setIBMOption = 3;
  int level = 0x0000FFFF;


#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  if (socketTrace){
    printf("before get socket option optionName=0x%x dataBufferLen=%d\n",optionName,*optionDataLength);
  }
  BPXOPT(&sd,
   &getOption,
   &level,
   &optionName,
   optionDataLength,
   optionData,
   &returnValue,
   returnCode,
   reasonCodePtr);
  if (socketTrace){
    printf("after getsockopt, retval=%d ret code %d reason 0x%x\n",returnValue,*returnCode,*reasonCode);
  }
  if (returnValue < 0){
    printf("get sockopt failed, ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    return -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int setSocketOption(Socket *socket, int level, int optionName, int optionDataLength, char *optionData,
		    int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int getOption = 1;
  int setOption = 2;
  int setIBMOption = 3;
  

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  /* printf("before read sd=%d buffer=%x\n",sd,buffer); */
  BPXOPT(&sd,
	 &setOption,
	 &level,
	 &optionName,
	 &optionDataLength,
	 optionData,
	 &returnValue,
	 returnCode,
	 reasonCodePtr);
  if (returnValue < 0){
    printf("set sockopt failed, level=0x%x, option=0x%x ret code %d reason 0x%x\n",level,optionName,*returnCode,*reasonCode);
    return -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int setSocketNoDelay(Socket *socket, int noDelay, int *returnCode, int *reasonCode){
  return setSocketOption(socket,IPPROTO_TCP,TCP_NODELAY,sizeof(int),(char*)&noDelay,returnCode,reasonCode);
}

int setSocketWriteBufferSize(Socket *socket, int bufferSize, int *returnCode, int *reasonCode){
  return setSocketOption(socket,SOL_SOCKET,SOCK_SO_SNDBUF,sizeof(int),(char*)&bufferSize,returnCode,reasonCode);
}

int setSocketReadBufferSize(Socket *socket, int bufferSize, int *returnCode, int *reasonCode){
  return setSocketOption(socket,SOL_SOCKET,SOCK_SO_RCVBUF,sizeof(int),(char*)&bufferSize,returnCode,reasonCode);
}

int setSocketReuseAddr(Socket *socket, int *returnCode, int *reasonCode){
  int on = 1;
  return setSocketOption(socket,SOL_SOCKET,SOCK_SO_REUSEADDR,sizeof(int),(char*)&on,returnCode,reasonCode);
}

int udpReceiveFrom(Socket *socket, 
                   SocketAddress *sourceAddress,  /* will be overwritten */
                   char *buffer, int bufferLength,
                   int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int flags = 0;  /* some exotic stuff in doc
                     http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/bpxzb1c0/B.30?SHELF=all13be9&DT=20110609191818#HDRYMSGF
                   */
  int socketAddressSize = SOCKET_ADDRESS_SIZE_IPV4;

  if (socketTrace > 2){
    printf("receiveFrom into buffer=0x%x bufLen=%d retVal=%d retCode=%d reasonCode=%d\n",
           buffer,bufferLength,returnValue,*returnCode,*reasonCode);
  }

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXRFM(&sd,
          &bufferLength,
          buffer,
          &zero,
          &flags,
          &socketAddressSize,
          sourceAddress,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    printf("recvFrom failed, sd=%d desired buffer len %d buffer at 0x%x, ret code %d reason 0x%x\n",
	   sd,bufferLength,buffer,*returnCode,*reasonCode);
    return -1;
  } else {
    if (socketTrace > 2){
      printf("recvFrom into buffer=0x%x %d bytes\n",buffer,returnValue);fflush(stdout);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

/**** Socket Sets and Extended Select */

SocketSet *makeSocketSet(int highestAllowedSD){
  SocketSet *set = (SocketSet*)safeMalloc(sizeof(SocketSet),"SocketSet");
  memset(set,0,sizeof(SocketSet));

  set->highestAllowedSD = highestAllowedSD;

  int socketArrayLength = highestAllowedSD+1;
  set->sockets = (Socket**)safeMalloc(socketArrayLength*sizeof(Socket*),"SocketSetSocketArray");
  memset(set->sockets,0,socketArrayLength*sizeof(Socket*));

  int maxArrayLength = (highestAllowedSD+32) / 32;
  set->allSDs = (int*)safeMalloc(sizeof(int)*maxArrayLength,"ReadSendMask");
  memset(set->allSDs,0,sizeof(int)*maxArrayLength);

  set->scratchReadMask = (int*)safeMalloc(sizeof(int)*maxArrayLength,"ReadSendMask");
  set->scratchWriteMask = (int*)safeMalloc(sizeof(int)*maxArrayLength,"WriteSendMask");
  set->scratchErrorMask = (int*)safeMalloc(sizeof(int)*maxArrayLength,"ErrorSendMask");

  return set;
}

void freeSocketAddr(SocketAddress *address){
  safeFree31((char*)address,sizeof(SocketAddress));
}

void freeSocketSet(SocketSet *set){
  printf("implement me - freeSocketSet\n");
  return;
}

int socketSetAdd(SocketSet *set, Socket *socket){
  int sd = socket->sd;

  if (sd > set->highestAllowedSD){
    printf("SD=%d out of range (> %d)\n",sd,set->highestAllowedSD);
    return 12;
  }

  int wordIndex = sd/ 32;
  int bitIndex = sd - (32 * wordIndex);

  set->allSDs[wordIndex] |= (1<<bitIndex);
  set->sockets[sd] = socket;

  return 0;
}

int socketSetRemove(SocketSet *set, Socket *socket){
  int sd = socket->sd;

  if (sd > set->highestAllowedSD){
    printf("SD=%d out of range (> %d)\n",sd,set->highestAllowedSD);
    return 12;
  }

  int wordIndex = sd/ 32;
  int bitIndex = sd - (32 * wordIndex);

  set->allSDs[wordIndex] &= ~(1<<bitIndex);
  set->sockets[sd] = NULL;
  
  return 0;
}

int extendedSelect(SocketSet *set,
                   int timeout,             /* in milliseconds */
                   void *ecbPointer,
                   int checkWrite, int checkRead, 
                   int *returnCode, int *reasonCode){
  int status = 0;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int userOptionField = 0; /* bits backward */
  int maxSoc = set->highestAllowedSD+1;
  int timeoutSpec[4];
  int *timeoutSpecPtr;
  int **timeoutSpecHandle;

  int maxArrayLengthInWords = (set->highestAllowedSD+32) /  32;
  int socketMaskSize = maxArrayLengthInWords*sizeof(int);
  *returnCode = 0;

  if (socketTrace > 2){
    printf("maxArrayLength in words = 0x%x socketMaskSize=0x%x\n",maxArrayLengthInWords,socketMaskSize);
  }
  memcpy(set->scratchReadMask,set->allSDs,maxArrayLengthInWords*sizeof(int));
  memcpy(set->scratchWriteMask,set->allSDs,maxArrayLengthInWords*sizeof(int));
  memcpy(set->scratchErrorMask,set->allSDs,maxArrayLengthInWords*sizeof(int));
  
  
  /* set timeout */
  if (timeout >= 0)
  {
#ifndef _LP64
    timeoutSpec[0] = timeout/1000; /* millis to seconds */
    timeoutSpec[1] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#else
    timeoutSpec[0] = 0; 
    timeoutSpec[1] = timeout/1000; /* millis to seconds */
    timeoutSpec[2] = 0; /* padding */
    timeoutSpec[3] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#endif

    timeoutSpecPtr = timeoutSpec;
  }
  else
  {
    // setting timeout_pointer to 0 implies wait indefinitely
    timeoutSpecPtr = 0; 
  }
  timeoutSpecHandle = &timeoutSpecPtr;
  
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
  ecbPointer =    (int*) (0x7FFFFFFF & ((int)ecbPointer));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXSEL(&maxSoc,
         (checkRead ? &socketMaskSize : &zero),
         set->scratchReadMask,
         (checkWrite ? &socketMaskSize : &zero),
         set->scratchWriteMask,
         (checkRead ? &socketMaskSize : &zero),
         set->scratchErrorMask,
         timeoutSpecHandle, /* ptr to ptr */
         &ecbPointer,
         &userOptionField,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (socketTrace > 1){
    printf("BPXSEL (extended) maxSoc=0x%x returnValue 0x%x returnCode 0x%x reasonCode 0x%x\n",
           maxSoc,returnValue,*returnCode,*reasonCode);
  }
  return returnValue;
}



/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/

/* doesn't support v6 yet, but could soon */

InetAddr *getAddressByName(char *addressString){
  int numericAddress = 0x7F123456;

  if (!isV4Numeric(addressString,&numericAddress)){
    numericAddress = getV4HostByName(addressString);
    printf("Host name is DNS or non-numeric, %x\n",numericAddress);
  } else{
    printf("Host name is numeric %x\n",numericAddress);
  }
  
  if (numericAddress != 0x7F123456){
    InetAddr *addr = (InetAddr*)safeMalloc31(sizeof(InetAddr),"InetAddr");
    memset(addr,0,sizeof(InetAddr));
    addr->type = AF_INET;
    addr->port = 0;
    addr->data.data4.addrBytes = numericAddress;
    return addr;
  } else{
    return NULL;
  }
}

int getSocketName(
    Socket *socket,
    SocketAddress *socketAddress)
{
    int returnValue = 0;
    int returnCode;
    int reasonCode;
    int *reasonCodePtr;

#ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)&reasonCode));
#else
    reasonCodePtr = &reasonCode;
#endif

    int socketAddrSize = SOCKET_ADDRESS_SIZE_IPV4;

    BPXGNM(socket,
            1,
            &socketAddrSize,
            socketAddress,
            &returnValue,
            &returnCode,
            reasonCodePtr);

    if (returnValue == 0){
      return returnValue;
    } else {
      return returnCode;
    }
}

int getSocketName2(
    Socket *socket,
    SocketAddress *socketAddress)
{
    int returnValue = 0;
    int returnCode;
    int reasonCode;
    int *reasonCodePtr;

#ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)&reasonCode));
#else
    reasonCodePtr = &reasonCode;
#endif

    int socketAddrSize = SOCKET_ADDRESS_SIZE_IPV4;

    BPXGNM(socket,
            2,
            &socketAddrSize,
            socketAddress,
            &returnValue,
            &returnCode,
            reasonCodePtr);

    if (returnValue == 0){
      return returnValue;
    } else {
      return returnCode;
    }
}

int getLocalHostName(char* inout_hostname,
                     unsigned int* inout_hostname_len,
                     int *returnCode, int *reasonCode)
{
  int sts = 0;
  *returnCode = *reasonCode = 0;
  int cs_retval = 0;
  int *reasonCodePtr;

  if ((NULL == inout_hostname) ||
      (NULL == inout_hostname_len) ||
      (NULL == returnCode) || (NULL == reasonCode))
  {
    sts = -1;
  }
  else
  {
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif    
    BPXHST(AF_INET,
            inout_hostname_len,
            inout_hostname,
            &cs_retval,
            returnCode,
            reasonCode);

    sts = cs_retval;
  }

  return sts;
}

InetAddr* getLocalHostAddress(int *rc, int *rsn)
{
  InetAddr* inetAddr = NULL;
  int cs_retval = 0;
  int v4addr = 0;
  do {
    if ((NULL == rc) || (NULL == rsn)) {
      break;
    }
    BPXHST(AF_INET,
           &v4addr,
           NULL,
           &cs_retval,
           rc,
           rsn);
    if (0 != *rc) {
      break;
    }

    inetAddr = (InetAddr*)safeMalloc(sizeof(InetAddr),"local InetAddr");
    memset(inetAddr,0,sizeof(InetAddr));
    inetAddr->type = AF_INET;
    inetAddr->port = 0;
    inetAddr->data.data4.addrBytes = v4addr;

  } while(0);

  return inetAddr;
}

/* The character set tables and a2e/e2a functions have
   moved to xlate.[hc] */

int socketClose(Socket *socket, int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;

#ifdef USE_RS_SSL
  if ((0 == socket->isServer) && (NULL != socket->sslHandle)) {
    /* sslHandle is RS_SSL_CONNECTION in this case */
    /* this call does a C stdlib shutdown/close internally */
    returnValue = rs_ssl_releaseConnection(socket->sslHandle);
    return returnValue;
  }
#endif

  int *reasonCodePtr;
  int status;

#ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
    reasonCodePtr = reasonCode;
#endif

  status = BPXCLO(&sd,
		   &returnValue,
		   returnCode,
		   reasonCodePtr);

  return returnValue;
}

static
int not_main(int argc, char **argv){
  char *addressString = argv[1];
  int port = argc >= 3 ? atoi(argv[2]) : 80;
  InetAddr *inetAddr = NULL;
  char readBuffer[1024];
  int __errno;
  int retcode;

  inetAddr = getAddressByName(addressString);

  if (inetAddr){
    SocketAddress *socketAddress 
      = makeSocketAddr(inetAddr,(unsigned short)port);
    Socket *socket = tcpClient(socketAddress,&__errno,&retcode);
    int i;
    int bytesRead;

    fflush(stdout);
    bytesRead = socketRead(socket,readBuffer,200,&__errno,&retcode);
    dumpbuffer(readBuffer,bytesRead);
    for (i=0; i<5; i++){
      printf("SLEEP %d\n",i+1);fflush(stdout);
      sleep(1);
    }
  }
  
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

