

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
#include <errno.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"
#include "socketmgmt.h"

#ifdef __ZOWE_OS_ZOS
#include "ezbztlsc.h" /* for TTLS ioctl support */
#include "logging.h"
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

#ifdef __ZOWE_OS_ZOS
static int sxQueryAttls(SocketExtension *sext, int onceOnly, int wantCert);
#endif

#if defined(__ZOWE_OS_ZOS) || defined(USE_RS_SSL) || defined(USE_ZOWE_TLS)
int sxUpdateTLSInfo(SocketExtension *sext, int onceOnly)
{
  int sts=0;

#if !defined(USE_RS_SSL) && !defined(USE_ZOWE_TLS) /* AT-TLS ioctl version */
  /* only call ioctl's if we WANT_TLS */
  if (sext->tlsFlags & RS_TLS_WANT_TLS)
  {
    sts = sxQueryAttls(sext, onceOnly, (sext->tlsFlags & RS_TLS_WANT_PEERCERT) != 0);
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
#ifdef __ZOWE_OS_ZOS
  else if (!sext->isServerSocket) {
    /* No GSK session, so TLS (if any) was done by AT-TLS; a plain socket just fails the ioctl. */
    sxQueryAttls(sext, onceOnly, TRUE);
  }
#endif
#endif

  return sts;
}

#ifdef __ZOWE_OS_ZOS

#define SX_ATTLS_CERT_INITIAL_SIZE 2048
#define SX_ATTLS_CERT_MAX_SIZE     65536

/* PassThru hands over whatever the client sent without validating its chain. */
static int sxAttlsValidatesClientCert(int secType) {
  return (secType == TTLS_SEC_SRV_CA_FULL ||
          secType == TTLS_SEC_SRV_CA_REQD ||
          secType == TTLS_SEC_SRV_CA_SAFCHK);
}

/* Returns 0 when the SIOCTTLSCTL ioctl succeeded, nonzero when it failed (not AT-TLS, for example). */
static int sxQueryAttls(SocketExtension *sext, int onceOnly, int wantCert)
{
  if (onceOnly && (sext->tlsFlags & RS_TTLS_IOCTL_DONE)) {
    return 0;
  }

  char initialBuffer[SX_ATTLS_CERT_INITIAL_SIZE];
  char *certBuffer = initialBuffer;
  int certBufferSize = sizeof(initialBuffer);
  char *heapBuffer = NULL;
  struct TTLS_IOCTL ioc;
  int sts = 0;
  int bpxrc = 0;
  int bpxrsn = 0;

  for (int attempt = 0; attempt < 2; attempt++) {
    memset(&ioc, 0, sizeof(ioc));
    ioc.TTLSi_Ver = TTLS_VERSION1;
    if (wantCert) {
      ioc.TTLSi_Req_Type = TTLS_RETURN_CERTIFICATE;
      ioc.TTLSi_BufferPtr = certBuffer;
      ioc.TTLSi_BufferLen = certBufferSize;
    } else {
      ioc.TTLSi_Req_Type = TTLS_QUERY_ONLY;
    }
    bpxrc = 0;
    bpxrsn = 0;
    sts = tcpIOControl(sext->socket, SIOCTTLSCTL, sizeof(ioc), (char*)&ioc, &bpxrc, &bpxrsn);

    /* On ENOBUFS the ioctl reports the size it needs in TTLSi_Cert_Len. */
    if (wantCert && attempt == 0 && bpxrc == ENOBUFS &&
        (int)ioc.TTLSi_Cert_Len > certBufferSize &&
        (int)ioc.TTLSi_Cert_Len <= SX_ATTLS_CERT_MAX_SIZE) {
      heapBuffer = safeMalloc(ioc.TTLSi_Cert_Len, "AT-TLS certificate");
      if (heapBuffer == NULL) {
        break;
      }
      certBuffer = heapBuffer;
      certBufferSize = ioc.TTLSi_Cert_Len;
      continue;
    }
    break;
  }

  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
          "SIOCTTLSCTL sts=%d rc=%d rsn=0x%x policy=%d conn=%d secType=%d certLen=%d\n",
          sts, bpxrc, bpxrsn, (int)ioc.TTLSi_Stat_Policy, (int)ioc.TTLSi_Stat_Conn,
          (int)ioc.TTLSi_Sec_Type, (int)ioc.TTLSi_Cert_Len);
#ifdef DEBUG
  printf("contents of TTLS_IOCTL struct:\n");
  dumpbuffer((char*) &ioc, sizeof(struct TTLS_IOCTL));
#endif

  int result = 0;
  if (sts != 0 || bpxrc != 0) {
    /* Not an AT-TLS connection (or the ioctl is unsupported); don't ask again. */
    sext->tlsFlags = sext->tlsFlags | RS_TTLS_IOCTL_DONE;
    result = sts ? sts : bpxrc;
  } else if (ioc.TTLSi_Stat_Conn == TTLS_CONN_HS_INPROGRESS) {
    /* Leave RS_TTLS_IOCTL_DONE clear so the next read asks again. */
  } else {
    sext->tlsFlags = sext->tlsFlags | RS_TTLS_IOCTL_DONE;
    if (TTLS_CONN_SECURE == ioc.TTLSi_Stat_Conn) {
      sext->tlsFlags = sext->tlsFlags | RS_TLS_HAVE_TLS;
      if (wantCert && (int)ioc.TTLSi_Cert_Len > 0) {
        if (!sxAttlsValidatesClientCert(ioc.TTLSi_Sec_Type)) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
                  "AT-TLS client certificate ignored, client authentication type %d does not validate it\n",
                  (int)ioc.TTLSi_Sec_Type);
        } else if ((int)ioc.TTLSi_Cert_Len > certBufferSize) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
                  "AT-TLS client certificate ignored, %d bytes exceeds the %d byte limit\n",
                  (int)ioc.TTLSi_Cert_Len, SX_ATTLS_CERT_MAX_SIZE);
        } else {
          unsigned char *copy = (unsigned char*)SLHAlloc(sext->slh, ioc.TTLSi_Cert_Len);
          if (copy != NULL) {
            memcpy(copy, certBuffer, ioc.TTLSi_Cert_Len);
            sext->peerCertificate = copy;
            sext->peerCertificateLength = ioc.TTLSi_Cert_Len;
            sext->tlsFlags = sext->tlsFlags | RS_TLS_HAVE_PEERCERT;
#ifdef DEBUG
            printf("copied peer certificate (%d bytes):\n", sext->peerCertificateLength);
            dumpbuffer(sext->peerCertificate, sext->peerCertificateLength);
#endif
          }
        }
      }
    }
  }

  if (heapBuffer != NULL) {
    safeFree(heapBuffer, certBufferSize);
  }
  return result;
}

#endif /* __ZOWE_OS_ZOS */

int sxGetPeerCertificate(SocketExtension *sext, char *buffer, unsigned int bufferSize,
                         unsigned int *certificateLength)
{
  if (sext == NULL || buffer == NULL || certificateLength == NULL) {
    return SX_PEERCERT_BAD_ARGUMENT;
  }
  *certificateLength = 0;
  if (0 == (sext->tlsFlags & RS_TLS_HAVE_PEERCERT) ||
      sext->peerCertificate == NULL ||
      sext->peerCertificateLength <= 0) {
    return SX_PEERCERT_NONE;
  }
  if ((unsigned int)sext->peerCertificateLength > bufferSize) {
    return SX_PEERCERT_BUFFER_TOO_SMALL;
  }
  memcpy(buffer, sext->peerCertificate, sext->peerCertificateLength);
  *certificateLength = (unsigned int)sext->peerCertificateLength;
  return SX_PEERCERT_OK;
}
#endif /* only Z or rs_ssl users can build this */




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

