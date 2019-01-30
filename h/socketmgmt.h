

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __SOCKETMGMT__
#define __SOCKETMGMT__ 1


typedef struct SXChainElement_tag{
  char *data;
  int allocatedLength;
  int usedLength;
  SocketAddress *socketAddress;         /* for UDP, this might be set, because multiple senders can send to same socket */
  struct SXChainElement_tag *prev;
  struct SXChainElement_tag *next;
} SXChainElement;

/* there is a set of registered protocol types
   for common usage of socket services to allow all network handling code
   to peacefully co-exist.
   */

#define SOCKEXTN_TAG "SOCKEXTN"

typedef struct SocketExtension_tag{
  char    eyecatcher[8]; /* "SOCKEXTN" */
  Socket *socket;
  ShortLivedHeap *slh;
  int   moduleID;    /* see stcbase.h only three things right now */
  void *protocolHandler; /* AuthTxn in this sense */
  int   readBufferLength;
  char *readBuffer;
  SXChainElement *readQHead;
  int   isServerSocket;  /* only meaningful for TCP and similar protocols. */
  int   tlsFlags;       /* WANT_ flags inherited from Socket, HAVE_ flags and
                            peerCertificate members populated in SocketExtension */
  unsigned char *peerCertificate; /* allocated in SLH */
  int   peerCertificateLength;
} SocketExtension;

SocketExtension *makeSocketExtension(Socket *socket,
                                     ShortLivedHeap *slh,
                                     int allocateInSLH,
                                     void *protocolHandler,
                                     int readBufferLength);

void sxEnqueueLastRead(SocketExtension *socketExtension,
                       SocketAddress *address,
                       int length);


int sxDequeueRead(SocketExtension *socketExtension, 
                  char **buffer, 
                  int *length,
                  SocketAddress **socketAddress);

/* use either AT-TLS ioctls or analogous rs_ssl functions to populate HAVE_ flags */
int sxUpdateTLSInfo(SocketExtension *sext, int onceOnly);

#endif 


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

