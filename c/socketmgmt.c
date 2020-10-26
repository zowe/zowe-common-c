

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/string.h>
#include <metal/stdlib.h>
#include "metalio.h"

#else
#include <stdio.h>                                                   
#include <string.h>
#include <stdlib.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"
#include "socketmgmt.h"

#ifdef __ZOWE_OS_ZOS
#include "ezbztlsc.h" /* for TTLS ioctl support */
#endif

#ifdef USE_RS_SSL
#include "rs_ssl.h"
#endif
#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif

#define SOCKETMGMT_TRACE 0

void sxEnqueueLastRead(SocketExtension *socketExtension,
                       SocketAddress *address,
                       int length){
  SXChainElement *newElement = (SXChainElement*)SLHAlloc(socketExtension->slh,sizeof(SXChainElement));
  memset(newElement,0,sizeof(SXChainElement));
  newElement->data = SLHAlloc(socketExtension->slh,length);
  memcpy(newElement->data,socketExtension->readBuffer,length);
  newElement->allocatedLength = length;
  newElement->usedLength = length;
  if (address){
    newElement->socketAddress = (SocketAddress*)SLHAlloc(socketExtension->slh,sizeof(SocketAddress));
    memcpy(newElement->socketAddress,address,sizeof(SocketAddress));
  }
  newElement->next = NULL;
  if (socketExtension->readQHead){
    SXChainElement *element = socketExtension->readQHead;
    while (element->next){      
      element = element->next;
    } 
    element->next = newElement;
  } else{
    socketExtension->readQHead = newElement;
  }
}

int sxDequeueRead(SocketExtension *socketExtension, char **buffer, int *length, SocketAddress **socketAddress){
  if (socketExtension->readQHead){
    SXChainElement *dequeuedElement = socketExtension->readQHead;
    socketExtension->readQHead = socketExtension->readQHead->next;
    *buffer = dequeuedElement->data;
    *length = dequeuedElement->usedLength;
    if (dequeuedElement->socketAddress){
      *socketAddress = dequeuedElement->socketAddress;
    } else{
      socketAddress = NULL;
    }
    return 1;
  } else{
    return 0;
  }
}

SocketExtension *makeSocketExtension(Socket *socket,
                                     ShortLivedHeap *slh,
                                     int allocateInSLH,
                                     void *protocolHandler,
                                     int readBufferLength)
{
  SocketExtension *socketExtension = (SocketExtension*)(allocateInSLH ? 
                                                        SLHAlloc(slh,sizeof(SocketExtension)) :
                                                        safeMalloc31(sizeof(SocketExtension),"SocketExtension"));

  memset(socketExtension,0,sizeof(SocketExtension));
  memcpy(socketExtension->eyecatcher,SOCKEXTN_TAG,8);
  socketExtension->socket = socket;
  socketExtension->protocolHandler = protocolHandler;
  socketExtension->readQHead = NULL;
  socketExtension->slh = slh;
  socketExtension->readBufferLength = readBufferLength;
  socketExtension->readBuffer = SLHAlloc(slh,readBufferLength);
#if SOCKETMGMT_TRACE > 0
  printf("SocketMgmt.madeSocketExtension\n");
#endif
  socketExtension->isServerSocket = socket->isServer;
#if defined(__ZOWE_OS_ZOS) || defined(USE_RS_SSL) || defined(USE_ZOWE_TLS)
  socketExtension->tlsFlags = socket->tlsFlags;
  socketExtension->peerCertificate = NULL;
  socketExtension->peerCertificateLength = 0;
#endif
  return socketExtension;
}

#if defined(__ZOWE_OS_ZOS) || defined(USE_RS_SSL) || defined(USE_RS_SSL)
int sxUpdateTLSInfo(SocketExtension *sext, int onceOnly)
{
  int sts=0;
  int iocmd=0, arglen=0;
  int bpxrc=0, bpxrsn=0;
  char* arg = NULL;

#if !defined(USE_RS_SSL) && !defined(USE_ZOWE_TLS) /* AT-TLS ioctl version */
  struct TTLS_IOCTL ioc;            /* ioctl data structure          */
  char certbuf[2048];               /* buffer for certificate        */
  memset(&ioc,0,sizeof(ioc));       /* set all unused fields to zero */
  ioc.TTLSi_Ver = TTLS_VERSION1;

  /* only call ioctl's if we WANT_TLS */
  if (onceOnly && (sext->tlsFlags & RS_TTLS_IOCTL_DONE)) {
    return 0;
  }
  if (sext->tlsFlags & RS_TLS_WANT_TLS)
  {
    if (sext->tlsFlags & RS_TLS_WANT_PEERCERT)
    {
      ioc.TTLSi_Req_Type = TTLS_RETURN_CERTIFICATE;
      ioc.TTLSi_BufferPtr = &(certbuf[0]);
      ioc.TTLSi_BufferLen = sizeof(certbuf);
    } else
    {
      ioc.TTLSi_Req_Type = TTLS_QUERY_ONLY;
    }
    arglen = sizeof(ioc);
    sts = tcpIOControl(sext->socket, SIOCTTLSCTL, arglen, (char*)&ioc, &bpxrc, &bpxrsn);
    if ((0 == sts) && (0 == bpxrc))
    {
      sext->tlsFlags = sext->tlsFlags | RS_TTLS_IOCTL_DONE;

#ifdef DEBUG
      printf("contents of TTLS_IOCTL struct:\n");
      dumpbuffer((char*) &ioc, sizeof(struct TTLS_IOCTL));
#endif
      if (TTLS_CONN_SECURE == ioc.TTLSi_Stat_Conn)
      {
        sext->tlsFlags = sext->tlsFlags | RS_TLS_HAVE_TLS;
      }
      if ((sext->tlsFlags & RS_TLS_WANT_PEERCERT) &&
          (0 < ioc.TTLSi_Cert_Len))
      {
        sext->peerCertificate = SLHAlloc(sext->slh, ioc.TTLSi_Cert_Len);
        memcpy(sext->peerCertificate, certbuf, ioc.TTLSi_Cert_Len);
        sext->peerCertificateLength = ioc.TTLSi_Cert_Len;
        sext->tlsFlags = sext->tlsFlags | RS_TLS_HAVE_PEERCERT;
#ifdef DEBUG
        printf("copied peer certificate (%d bytes):\n", sext->peerCertificateLength);
        dumpbuffer(sext->peerCertificate, sext->peerCertificateLength);
#endif
      }
    }
    else
    {
      /* error from tcpIOControl */
#ifdef DEBUG
      printf("sxUpdateTTLSInfo: error from tcpIOControl (sts=%d,rc=%d,rsn=0x%x)\n",
             sts, bpxrc, bpxrsn);
#endif
    }
  }
#elif USE_RS_SSL /* RS_SSL version */
  if ((0 == sext->isServerSocket) &&
      (NULL != sext->socket->sslHandle))
  {
    sext->tlsFlags = sext->tlsFlags | RS_TLS_HAVE_TLS;
  }

  if ((0 != (sext->tlsFlags & RS_TLS_WANT_PEERCERT)) &&
      (NULL != sext->socket->sslHandle))
  {
    /* try to get the peer cert out of the connection */
    unsigned char* derbuf = NULL;
    int derlen = 0;
    sts = rs_ssl_getPeerCertificateDER(sext->socket->sslHandle, &derbuf, &derlen);
    if ((0 == sts) && (NULL != derbuf))
    {
      sext->peerCertificate = SLHAlloc(sext->slh, derlen);
      memcpy(sext->peerCertificate, derbuf, derlen);
      sext->peerCertificateLength = derlen;
      sext->tlsFlags = sext->tlsFlags | RS_TLS_HAVE_PEERCERT;
      safeFree(derbuf, derlen);
    }
    sts = 0;
  }
#elif USE_ZOWE_TLS
  if (!sext->isServerSocket && sext->socket->tlsSocket) {
    sext->tlsFlags = sext->tlsFlags | RS_TLS_HAVE_TLS;
  }
#endif

  return sts;
}
#endif /* only Z or rs_ssl users can build this */




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

