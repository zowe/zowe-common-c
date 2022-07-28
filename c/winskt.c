

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
#include <sys/stat.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "collections.h"
#include "bpxnet.h"
#include "unixfile.h"

/*
  this pragma tells the compiler/linker to link in the right, modern WinSock stuff 

 */

#pragma comment(lib, "Ws2_32.lib")

/*
  https://msdn.microsoft.com/en-us/library/windows/desktop/ms741394(v=vs.85).aspx
  https://tangentsoft.net/wskfaq/newbie.html

  fcntl = ioctlsocket

  IOCTL / FCNTL
    https://msdn.microsoft.com/en-us/library/windows/desktop/ms740126(v=vs.85).aspx

    
    Various C language run-time systems use the IOCTLs for purposes
    unrelated to Windows Sockets. As a consequence, the ioctlsocket
    function and the WSAIoctl function were defined to handle socket
    functions that were performed by IOCTL and fcntl in the Berkeley
    Software Distribution.
 */

int tcpInit(char *uniqueName){
  /* do nothing for now */
  return 0;
}

static int socketTrace = TRUE;

SocketAddress *makeSocketAddr(InetAddr *addr, 
			      unsigned short port){
  SocketAddress *address = (SocketAddress*)safeMalloc(sizeof(SocketAddress),"SocketAddress");
  memset(address,0,sizeof(SocketAddress));
  if (socketTrace){
    printf("socket address at 0x%p\n",address);
  }
  address->family = AF_INET;
  address->port = ((port & 0xff)<<8)|((port&0xff00)>>8);
  if (addr){
    address->v4Address = addr->data.data4;
  } else{
    memset(&(address->v4Address),0,sizeof(struct in_addr));
  }
  if (socketTrace){
    printf("about to return socket address at 0x%p\n",address);
  }
  return address;
}

void freeSocketAddr(SocketAddress *address){
  safeFree((char*)address,sizeof(SocketAddress));
}

/* SD or, on windows the handle */
int getSocketDebugID(Socket *s){
  return (int)s->windowsSocket;
}


/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/

#define SO_ERROR    0x1007

Socket *tcpClient2(SocketAddress *socketAddress, 
		   int timeoutInMillis,
		   int *returnCode, /* errnum */
		   int *reasonCode){ /* errnum - JR's */
  int socketVector[2];
  int *reasonCodePtr;
  int status;
  int madeConnection = FALSE;
  SOCKET windowsSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  struct sockaddr *windowsSocketAddress = (struct sockaddr *)socketAddress;

  printf("socket = 0x%llx\n",windowsSocket);
  if (windowsSocket == INVALID_SOCKET){
    if (socketTrace){
      printf("Failed to create socket\n");
    }
    return NULL;
  } else{
    int socketAddrSize = sizeof(SocketAddress);
    if (timeoutInMillis >= 0){
      Socket tempSocket;
      tempSocket.windowsSocket = windowsSocket;

      /* setSocketBlockingMode(&tempSocket, TRUE, returnCode, reasonCode);  */
      printf("windowsSocket:\n");
      dumpbuffer((char*)windowsSocketAddress,20);
      status = connect(windowsSocket,windowsSocketAddress,sizeof(struct sockaddr_in));
      *returnCode = WSAGetLastError();
      printf("socket connect status=%d errno=%d\n",status,*returnCode);
      fflush(stdout);
      if (socketTrace) {
        printf("connect() status %d\n",status);
      }
      /* EINPROGRESS is the expected return code here but we are just being careful by checking 
         for EWOULDBLOCK as well 

         WINDOWS NOTE: status needs to 0, opposite of z/OS.  But maybe the zOS code is getting
                       lucky.   The windows return value for connect() seems to have a clearer
                       interpretation here. 
      */
      if (status == 0){
        *returnCode  = 0;
        *reasonCode  = 0;
        status = tcpStatus(&tempSocket, timeoutInMillis, 1, returnCode, reasonCode);
        printf("after tcpStatus() = %d\n",status);
        if (status == SD_STATUS_TIMEOUT) {
          int sd = socketVector[0];
          if (socketTrace) {
            printf("Failed to connect socket, will clean up sd=%d\n", sd);
          }
          *returnCode = 0;
          *reasonCode = 0;
          status = closesocket(windowsSocket);
          if (socketTrace) {
            printf("closeSocket() for time out connect status %d\n",status);
          }
          return NULL;
        }
        printf("tcpStatus return and did not timeout status=%d\n",status);
        fflush(stdout);
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
      status = connect(windowsSocket,windowsSocketAddress,14);
      if (socketTrace){
        printf("BPXCON status %d returnCode %x reasonCode %x\n",
           status,*returnCode,*reasonCode);
      }
      madeConnection =  (status == 0 ? TRUE : FALSE);
    }
    if (!madeConnection){
      if (socketTrace){
        printf("Failed to connect socket, will clean up, SOCKET at 0x%llx\n",windowsSocket);
      }
      *returnCode  = 0;
      *reasonCode  = 0; 
      status = closesocket(windowsSocket);
      if (socketTrace){
        printf("closesocket() for failed connect status %d\n",status);
      }
      return NULL;
    } else{
      Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
      socket->windowsSocket = windowsSocket;
      sprintf(socket->debugName,"HNDL=0X%llx",socket->windowsSocket);
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
  SOCKET windowsSocket = socket->windowsSocket;
  int getOption = 1;
  int setOption = 2;
  int setIBMOption = 3;
  int level = 0x0000FFFF;
  
  if (socketTrace){
    printf("before get socket option optionName=0x%x dataBufferLen=%d\n",optionName,*optionDataLength);
  }
  int status = setsockopt(windowsSocket,
                          SOL_SOCKET, /* is this always right */
                          optionName,
                          optionData,
                          *optionDataLength);
  if (socketTrace){
    printf("after getsockopt, status=%d ret code %d reason 0x%x\n",status,*returnCode,*reasonCode);
  }
  if (status < 0){
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
    printf("get sockopt failed, ret code %d reason 0x%x\n",*returnCode,*reasonCode);
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
  safeFree((char*)socket,sizeof(Socket));
}

static
int setSocketOptionEx(Socket *socket,
                      int level, const char* levelDesc, 
                      int optionName, const char* optionDesc,
                      int optionDataLength, char *optionData,
                      int *returnCode, int *reasonCode)
{
  SOCKET windowsSocket = socket->windowsSocket;
  int status =  setsockopt(windowsSocket, level, optionName, optionData, (socklen_t) optionDataLength);
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


int getSocketKeepAliveMode(Socket *socket, int *returnCode, int *reasonCode){

  int optval = 0;
  int optlen = sizeof(optval);
  int returnValue = getSocketOption(socket,SO_KEEPALIVE,&optlen,(char*)&optval,
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
  int returnValue = setSocketOption(socket,SOL_SOCKET,SO_KEEPALIVE,sizeof(optval),(char*)&optval,
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
  SOCKET windowsSocket = socket->windowsSocket;
  int flags = 0; /* no special behaviors */

  int bytesReadOrError = recv(windowsSocket,buffer,desiredBytes,flags);
  if (bytesReadOrError < 0){
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
    return -1;
  } else {
    if (socketTrace){
      printf("read %d bytes\n",bytesReadOrError);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return bytesReadOrError;
  }
}

int socketWrite(Socket *socket, const char *buffer, int desiredBytes, 
                int *returnCode, int *reasonCode){
  SOCKET windowsSocket = socket->windowsSocket;
  int flags = 0;

  if (socketTrace){
    printf("writing desired=%d retCode=%d reasonCode=%d\n",
       desiredBytes,*returnCode,*reasonCode);
    dumpbuffer(buffer, desiredBytes); 
  }

  int bytesWrittenOrError = send(windowsSocket,buffer,desiredBytes,0);
  if (bytesWrittenOrError < 0){
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
    printf("write failed, socketHandle=0x%llx,desired write len %d buffer at 0x%p, ret code %d reason 0x%x\n",
	   windowsSocket,desiredBytes,buffer,*returnCode,*reasonCode);
    return -1;
  } else {
    if (socketTrace){
      printf("wrote %d bytes\n",bytesWrittenOrError);fflush(stdout);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return bytesWrittenOrError;
  }
}

static void setupFDSets(fd_set *readFDs, 
                        fd_set *writeFDs, 
                        fd_set *exceptionFDs, 
                        SOCKET  windowsSocket,
                        int checkWrite,
                        int checkRead){
    FD_ZERO(readFDs);
    FD_ZERO(writeFDs);
    FD_ZERO(exceptionFDs);
    
    if (checkWrite){
      FD_SET(windowsSocket,writeFDs);
    }
    if (checkRead){
      FD_SET(windowsSocket,readFDs);
      FD_SET(windowsSocket,exceptionFDs);
    }

}

static int tcpStatusInternal(Socket *socket, 
                             int timeout,   /* in milliseconds */
                             int checkWrite, int checkRead, 
                             int *returnCode, int *reasonCode){
  /* what does it MEAN?? */
  int status = 0;
  SOCKET windowsSocket = socket->windowsSocket;
  fd_set readFDs, writeFDs, exceptionFDs;
  struct timeval timeValue;
  const struct timeval *timeValuePtr;
  
  if (timeout >= 0)
  {
    timeValue.tv_sec = timeout/1000; /* millis to seconds */
    timeValue.tv_usec = (timeout-(timeout/1000)*1000)*1000; /* micros */
    timeValuePtr = &timeValue;
  } else {
    timeValuePtr = NULL;
  }

  setupFDSets(&readFDs,&writeFDs,&exceptionFDs,windowsSocket,checkWrite,checkRead);
  status = select(0,&readFDs,&writeFDs,&exceptionFDs,timeValuePtr);

  if (socketTrace){
    printf("BPXSEL status %d returnCode %x reasonCode %x\n",
       status,*returnCode,*reasonCode);
  }
  if (status > 0){                            
    int sdStatus = 0;                           
                                                
    if (FD_ISSET(windowsSocket,&readFDs)){
      sdStatus  |= SD_STATUS_RD_RDY;
    }
    if (FD_ISSET(windowsSocket,&writeFDs)){
      sdStatus  |= SD_STATUS_WR_RDY;
    }
    return sdStatus;                            
  } else if (status == 0){ /* timeout */      
    return SD_STATUS_TIMEOUT;                   
  } else if (FD_ISSET(windowsSocket,&exceptionFDs)){
    return SD_STATUS_ERROR;                     
  } else{
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
    return SD_STATUS_FAILED; /* look in errno */
  }
}

int tcpStatus(Socket *socket, 
              int timeout,   /* in milliseconds */
              int checkWrite, /* otherwise check for read and error */
              int *returnCode, int *reasonCode){
  return tcpStatusInternal(socket,timeout,checkWrite,!checkWrite,returnCode,reasonCode);
}

#define O_NONBLOCK   0x04
#define F_GETFL       3
#define F_SETFL       4

/* Note: Windows "ioctl" known as ioctlsocket is not as complete
   as it is in posix/unix.  See WSAIoctl for richer functionality */

int setSocketBlockingMode(Socket *socket, int isNonBlocking,
                          int *returnCode, int *reasonCode){
  SOCKET windowsSocket = socket->windowsSocket;
  int status = 0;
  long command = FIONBIO;
  unsigned long argument = (isNonBlocking ? 1 : 0);
  
  status = ioctlsocket(windowsSocket,command,&argument);

  if (status < 0){
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
  
    printf("ioctlsocket(1) failed, ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    return -1;
  } else {
    if (socketTrace){
      printf("ioctlsocket succeeded, nonblocking=%d \n",isNonBlocking);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return 0;
  }
}

Socket *udpPeer(SocketAddress *socketAddress, 
                int *returnCode, /* errnum */
                int *reasonCode){  /* errnum - JR's */
  int status;
  struct sockaddr *windowsSocketAddress = (struct sockaddr *)socketAddress;
  
  SOCKET windowsSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

  if (windowsSocket == INVALID_SOCKET){
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
    if (socketTrace){
      printf("Failed to create socket, error=%d\n",*returnCode);
    }
    return NULL;
  } else{
    int socketAddressSize = sizeof(SocketAddress);
    status = bind(windowsSocket,windowsSocketAddress,sizeof(struct sockaddr_in));
    if (status != 0){
      *returnCode = WSAGetLastError();
      *reasonCode = *returnCode;
    }
    if (socketTrace) {
      printf("BPXBND status %d returnCode %x reasonCode %x\n",
             status, *returnCode, *reasonCode);
    }
    if (status != 0){
      return NULL;
    } else{
      Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
      socket->windowsSocket = windowsSocket;
      sprintf(socket->debugName,"HNDL=0X%llx",socket->windowsSocket);
      socket->isServer = FALSE;
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
                   int *reasonCode){
  int socketVector[2];
  int status;

  SOCKET windowsSocket = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if (windowsSocket == INVALID_SOCKET){
    if (socketTrace){
      printf("Failed to create socket\n");
    }
    return NULL;
  } else{
    int sd = socketVector[0];
    int socketAddressSize = sizeof(SocketAddress);
    int inetAddressSize = sizeof(InetAddr);
    SocketAddress *socketAddress = makeSocketAddr(addr,(unsigned short)port);
    struct sockaddr *windowsSocketAddress = (struct sockaddr *)socketAddress;
    InetAddr inetAddr;
    memset(&inetAddr,0,sizeof(InetAddr));
    inetAddr.type = AF_INET;
    inetAddr.port = (unsigned short)port;
    memset(&inetAddr.data.data4,0,4);
    if (socketTrace){
      printf("server SocketAddress = %d\n",sd);
      dumpbuffer((char*)socketAddress,sizeof(SocketAddress));
      dumpbuffer((char*)&inetAddr,sizeof(InetAddr));
    }
    status = bind(windowsSocket,windowsSocketAddress,sizeof(struct sockaddr_in));
    if (status != 0){
      *returnCode = WSAGetLastError();
      *reasonCode = *returnCode;
    }
    if (status != 0){
      if (socketTrace){
        printf("Failed to bind server socket errno=%d reason=0x%x\n",
               *returnCode,*reasonCode);
      }
      safeFree((char*)socketAddress,sizeof(SocketAddress));
      return NULL;
    } else{
      int backlogQueueLength = 100;
      status = listen(windowsSocket,backlogQueueLength);
      
      if (status != 0){
        *returnCode = WSAGetLastError();
        *reasonCode = *returnCode;
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
        
        socket->windowsSocket = windowsSocket;
        sprintf(socket->debugName,"HNDL=0X%llx",socket->windowsSocket);
        socket->isServer = TRUE;
        socket->protocol = IPPROTO_TCP;
        if (socketTrace) {
          printf("Immediately before return from tcpServer, socket contains:\n");
          dumpbuffer((char*)socket, sizeof(Socket));
        }
        return socket;
      }
    }
  }
  
}


Socket *tcpServer(InetAddr *addr, /* usually NULL/0 */
                  int port,
                  int *returnCode,
                  int *reasonCode){
  return tcpServer2(addr, port, 0, returnCode, reasonCode);
}


/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/


Socket *socketAccept(Socket *serverSocket, int *returnCode, int *reasonCode){
  SocketAddress *newSocketAddress 
    = (SocketAddress*)safeMalloc(sizeof(SocketAddress),"Socket Address");
  struct sockaddr *windowsSocketAddress = (struct sockaddr *)newSocketAddress;
  SOCKET windowsSocket = serverSocket->windowsSocket;
  int status;
  int newSocketAddressSize = sizeof(struct sockaddr_in);

  printf("newSocketAddressSize=%d sizeof(SocketAddress)=%lld\n",newSocketAddressSize,sizeof(SocketAddress));
  fflush(stdout);

  SOCKET newWindowsSocket = accept(windowsSocket,windowsSocketAddress,&newSocketAddressSize);
  if (newWindowsSocket == INVALID_SOCKET){
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
    if (socketTrace){
      printf("Failed to accept new socket errno=%d reasonCode=0x%x\n",
             *returnCode,*reasonCode);
    }
    return NULL;
  } else{
    Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
    memset(socket,0,sizeof(Socket));
    socket->windowsSocket = newWindowsSocket;
    socket->isServer = 0;
    socket->protocol = IPPROTO_TCP;
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
    printf("hostent->length=%d name=%s\n",hostEntPtr->h_length,hostEntPtr->h_name);
    fflush(stdout);
    do {
      char *hostAddr = hostEntPtr->h_addr_list[i];
      if (hostAddr == NULL){
        break;
      }
      printf("hostAddr = 0x%p %s\n",hostAddr,hostAddr);
      numericAddress = *((int*)hostAddr); /*very IP-v4 here */  
      printf("numeric=0x%x\n",numericAddress);
      i++;
    } while (TRUE);
    return numericAddress;
  } else{
    returnCode = WSAGetLastError();
    reasonCode = returnCode;
    printf("getHostName V4 failure, returnCode %d reason code %d\n",returnCode,reasonCode);
    return 0;
  }
}


InetAddr *getAddressByName(char *addressString){
  int numericAddress = 0x7F123456;

  if (!isV4Numeric(addressString,&numericAddress)){
    numericAddress = getV4HostByName(addressString);
    /* printf("Host names is DNS NAME (or crap), %x\n",numericAddress); */
  } else{
    /* printf("Host name is numeric %x\n",numericAddress); */
  }
  
  if (numericAddress != 0x7F123456){
    InetAddr *addr = (InetAddr*)safeMalloc(sizeof(InetAddr),"Inet Address");
    memset(addr,0,sizeof(InetAddr));
    addr->type = AF_INET;
    addr->port = 0;
    memcpy(&(addr->data.data4),&numericAddress,4);
    printf("InetAddr structure: AF_INET=0x%x\n",AF_INET);
    dumpbuffer((char*)addr,sizeof(InetAddr));
    fflush(stdout);
    return addr;
  } else{
    return NULL;
  }
}

int socketInit(char *uniqueName){
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;
  
  /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
  wVersionRequested = MAKEWORD(2, 0);
  
  err = WSAStartup(wVersionRequested, &wsaData);
  if (err != 0) {
    /* Tell the user that we could not find a usable */
    /* Winsock DLL.                                  */
    printf("WSAStartup failed with error: %d\n", err);
    return 1;
  }
  return 0;
}

int socketClose(Socket *socket, int *returnCode, int *reasonCode){
  SOCKET windowsSocket = socket->windowsSocket;
  int status;

  status = closesocket(windowsSocket);
  if (status == 0){
    return 0;
  } else {
    *returnCode = WSAGetLastError();
    *reasonCode = *returnCode;
    return -1;
  }
} 

SocketSet *makeSocketSet(int highestAllowedSD){
  SocketSet *set = (SocketSet*)safeMalloc(sizeof(SocketSet),"SocketSet");
  memset(set,0,sizeof(SocketSet));

  set->highestAllowedSD = highestAllowedSD;

  int socketArrayLength = highestAllowedSD;
  set->sockets = (Socket**)safeMalloc(socketArrayLength*sizeof(Socket*),"SocketSetSocketArray");
  memset(set->sockets,0,socketArrayLength*sizeof(Socket*));

  set->revisionNumber = 0;
  return set;
}

void freeSocketSet(SocketSet *set) {
  int socketArrayLength = set->highestAllowedSD;
  safeFree((char*)(void*)set->sockets, socketArrayLength * sizeof(Socket*));
  safeFree((char*)set, sizeof(SocketSet));
  return;
}

int socketSetAdd(SocketSet *set, Socket *socket){
  printf("winskt.socketSetAdd set=0x%p socket=0x%p\n",set,socket);
  fflush(stdout);
  SOCKET windowsSocket = socket->windowsSocket;
  if (socketTrace)
  {
    printf ("Adding socket, SOCKET HANDLE=0x%llx\n", windowsSocket);
  }

  if (set->socketCount >= set->highestAllowedSD){
    printf("Socket set is full at %d sockets\n",set->highestAllowedSD);
    return 12;
  }

  set->sockets[set->socketCount] = socket;
  set->socketCount++;
  set->revisionNumber++;

  return 0;
}

int socketSetRemove(SocketSet *set, Socket *socket){
  SOCKET windowsSocket = socket->windowsSocket;
  if (socketTrace)
  {
    printf ("Removing socket, SOCKET HANDLE=0x%llx\n", windowsSocket);
  }

  set->socketCount--;
  set->sockets[set->socketCount] = NULL;
  set->revisionNumber++;
  return 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
