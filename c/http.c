

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
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef __ZOWE_OS_WINDOWS
#include <time.h>
#endif

#endif /* METTLE */

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "bpxnet.h"
#include "xlate.h"
#include "charsets.h"

#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif

#include "collections.h"
#include "le.h"
#include "scheduling.h"
#include "socketmgmt.h"
#include "fdpoll.h"
#include "logging.h"

#include "http.h"


void requestStringHeader(HttpRequest *request, int inEbcdic, char *name, char *value){
  HttpHeader *header = (HttpHeader*)SLHAlloc(request->slh,sizeof(HttpHeader));

  if (inEbcdic){
    header->nativeName = name;
    header->nativeValue = value;
  } else{
    header->name = name;
    header->value = value;
  }
  header->next = NULL;
  HttpHeader *headerChain = request->headerChain;
  if (headerChain){
    HttpHeader *prev;
    while (headerChain){
      prev = headerChain;
      headerChain = headerChain->next;
    }
    prev->next = header;
  } else{
    request->headerChain = header;
  }
}

void requestIntHeader(HttpRequest *request, int inEbcdic, char *name, int value){
  HttpHeader *header = (HttpHeader*)SLHAlloc(request->slh,sizeof(HttpHeader));

  if (inEbcdic){
    header->nativeName = name;
    header->nativeValue = NULL;
  } else{
    header->name = name;
    header->value = NULL;
  }

  header->intValue = value;
  header->next = NULL;
  HttpHeader *headerChain = request->headerChain;
  if (headerChain){
    HttpHeader *prev;
    while (headerChain){
      prev = headerChain;
      headerChain = headerChain->next;
    }
    prev->next = header;
  } else{
    request->headerChain = header;
  }
}

char *copyString(ShortLivedHeap *slh, char *s, int len){
  char *copy = SLHAlloc(slh,len+1);
  memcpy(copy,s,len);
  copy[len] = 0;
  return copy;
}

/*
  this is used to copy headers to the "native" charset of the server.  Which can be EBCDIC 1047
  usually on mainframe.  And probably other things in other places.
 */
char *copyStringToNative(ShortLivedHeap *slh, char *s, int len)
{
  char *copy = SLHAlloc(slh,len+1);
  memcpy(copy,s,len);
  copy[len] = 0;
#ifdef __ZOWE_OS_ZOS
  destructivelyUnasciify(copy);
#endif
  return copy;
}

#ifdef __ZOWE_OS_ZOS
void asciify(char *s, int len){
  e2a(s,len);
}
#else
void asciify(char *s, int len){
  /* do nothing, assume native implementation is already in the ASCII/UTF8 common subset that is used in HTTP headers */
}
#endif

char* destructivelyNativize(char *s)
{
#ifdef __ZOWE_OS_ZOS
  return destructivelyUnasciify(s);
#else
  return s;
#endif
}

int headerMatch(HttpHeader *header, char *s){
  return !strcmp(header->nativeValue,s);
}

#ifdef __ZOWE_OS_ZOS
static const char* iso8859_1_Table = (const char*) 0;
#endif

/* Assumes IBM1047 (program literals are in 1047) */
char *toASCIIUTF8(char *buffer, int len) {
  int i;
#ifdef __ZOWE_OS_ZOS
  if (0 == iso8859_1_Table) {
    iso8859_1_Table = (const char*) getTranslationTable("ibm1047_to_iso88591");
  }
#endif

  for (i=0; i<len; i++) {
#ifdef __ZOWE_OS_ZOS
    char translatedChar = iso8859_1_Table[buffer[i]];
#else
    char translatedChar = buffer[i];
#endif
    if (translatedChar > 126){
      printf("toASCIIUTF8 warning, saw value 0x%x which is not in the 7-bit subset\n",translatedChar);
    }
    buffer[i] = translatedChar;
  }
  return buffer;
}

static int WRITE_FORCE = TRUE;

int writeFully(Socket *socket, char *buffer, int len){
#define POLL_TIME 200

#ifdef METTLE
#define EAGAIN      112
#define EWOULDBLOCK 1102
#endif

  int returnCode = 0;
  int reasonCode = 0;
  int bytesWritten = 0;

  /* this is a sanity check for a bug that once happened on ZOS */
#ifdef __ZOWE_OS_ZOS
  if (((int)socket) < 0x1000000){
    printf("*** WARNING *** bad socket pointer!! 0x%x\n",socket);
    return 0;
  }
#endif

  while (bytesWritten < len){
    int status = socketWrite(socket,buffer+bytesWritten,len-bytesWritten,&returnCode,&reasonCode);
    if (status >= 0){
      bytesWritten += status;
    } else {
      /* Check for both EAGAIN and EWOULDBLOCK on "older" unix systems
       * for portability.
       */
      if (WRITE_FORCE && returnCode == EAGAIN || returnCode == EWOULDBLOCK) {
        PollItem item = {0};
        item.fd = socket->sd;
        item.events = POLLEWRNORM;
        returnCode = 0;
        reasonCode = 0;
        int status = fdPoll(&item, 0, 1, POLL_TIME, &returnCode, &reasonCode);
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "BPXPOL: status = %d, ret: %d, rsn: %d\n", status, returnCode, reasonCode);
        if (status == -1) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "Waited out full duration. Trying again.\n");
          continue; /* For some reason, 200 milliseconds wasn't long enough...
                     * so just continue anyways.
                     */
        }
        if (item.revents & POLLRWRNORM) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "Socket polled. OK to write.\n");
          continue; /* The socket is writable again before the 200 milliseconds
                     * timeout. Continue streaming.
                     */
        }
      } else {
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "IO error while writing, errno=%d reason=%x\n \
               Aborting...\n", returnCode,reasonCode);
        return 0;
      }
    }
  }

  return 1;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
