

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
#include <metal/ctype.h>
#include "metalio.h"

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>

#ifndef __ZOWE_OS_WINDOWS
#include <time.h>
#include <errno.h>
#endif

#endif /* METTLE */

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "openprims.h"
#include "xml.h"
#include "json.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "xlate.h"

#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#include "icsf.h"
#include "recovery.h"
#include "zis/client.h"
#else
#include <stdatomic.h>
#endif 

#include "collections.h"
#include "le.h"
#include "scheduling.h"

#include "utils.h"
#include "socketmgmt.h"
#include "crypto.h"
#include "charsets.h"
#include "impersonation.h"
#include "httpserver.h"

#ifdef USE_RS_SSL
#include "rs_ssl.h"
#endif
#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif // USE_ZOWE_TLS

#include "../jwt/jwt/jwt.h"

/* bool and time_t are not available for
 * METAL builds, but casting them to ints
 * is valid.
 */
#ifdef METTLE
typedef int time_t;
#endif

#define READ_BUFFER_SIZE 65536

#ifndef APF_AUTHORIZED
#define APF_AUTHORIZED  1
#endif

#define SAF_RC_PASSWORD_EXPIRED    8
#define RACF_RC_PASSWORD_EXPIRED   12

typedef struct AuthResponse_tag {
  int type;
#define AUTH_TYPE_RACF  1
  union {
#ifdef __ZOWE_OS_ZOS 
    SAFAuthStatus safStatus;
#else
    int someStatus;
#endif
  } responseDetails;
} AuthResponse;

/* FIX THIS: a temporary "low profile" way of hiding printfs. Improves
 * demo performance while we wait for
 * the introduction of a proper tracing infrastructure.
 */
#ifdef TRACEON
#define TEMP_TRACE(...) \
  printf(__VA_ARGS__);
#else
#define TEMP_TRACE(...) 3
#endif

#define AUTH_TRACE(...) do { \
  if (traceAuth) { \
    printf(__VA_ARGS__); \
    fflush(stdout); \
  } \
} while (0)

#define AUTH_DUMPBUF($buf, $len) do { \
  if (traceAuth) { \
    dumpbuffer((void *)(uintptr_t)$buf, $len); \
    fflush(stdout); \
  } \
} while (0)

#ifdef DEBUG
#define DEBUG_TRACE(...) do { \
    printf(__VA_ARGS__); \
    fflush(stdout); \
  } while (0)
#else
#  define DEBUG_TRACE(...)  do {} while (0)
#endif

/* A *very special* ifdef had been chosen so we never accidentally ship an
 * executable that can be persuaded to print out passwords
 */
#ifdef ENABLE_DANGEROUS_AUTH_TRACING
#  define DANGEROUS_TRACE        AUTH_TRACE
#  define DANGEROUS_DUMPBUF      AUTH_DUMPBUF
#else
#  define DANGEROUS_TRACE(...)   do {} while (0)
#  define DANGEROUS_DUMPBUF(...) do {} while (0)
#endif



/***** General Primitives ******************************/

static int traceParse = 0;
static int traceDispatch = 0;
static int traceHeaders = 0;
static int traceSocket = 0;
static int traceHttpCloseConversation = 0;
static int traceAuth = 0;

int setHttpParseTrace(int toWhat) {
  int was = traceParse;
#ifndef METTLE
  traceParse = toWhat;
#endif
  return was;
}

int setHttpDispatchTrace(int toWhat) {
  int was = traceDispatch;
#ifndef METTLE
  traceDispatch = toWhat;
#endif
  return was;
}

int setHttpHeadersTrace(int toWhat) {
  int was = traceHeaders;
#ifndef METTLE
  traceHeaders = toWhat;
#endif
  return was;
}

int setHttpSocketTrace(int toWhat) {
  int was = traceSocket;
#ifndef METTLE
  traceSocket = toWhat;
#endif
  return was;
}

int setHttpCloseConversationTrace(int toWhat) {
  int was = traceHttpCloseConversation;
#ifndef METTLE
  traceHttpCloseConversation = toWhat;
#endif
  return was;
}

int setHttpAuthTrace(int toWhat) {
  int was = traceAuth;
#ifndef METTLE
  traceAuth = toWhat;
#endif
  return was;
}


static char crlf[] ={ 0x0d, 0x0a};

#define TRANSFER_ENCODING "Transfer-Encoding" 
#define CHUNKED           "chunked"
#define CONTENT_LENGTH    "Content-Length"
#define UPGRADE           "Upgrade"
#define WEBSOCKET         "websocket"

/* Note:  I think DEFAULT_UMASK is obsolete. Joe on 4/28/2022 */

#ifdef __ZOWE_OS_ZOS
#define NATIVE_CODEPAGE CCSID_EBCDIC_1047
#define DEFAULT_UMASK 0022
#elif defined(__ZOWE_OS_WINDOWS)
#define NATIVE_CODEPAGE CP_UTF8
#define DEFAULT_UMASK 0022
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
#define NATIVE_CODEPAGE CCSID_ISO_8859_1
#warning ISO-8859-1 is not necessarily the default codepage on Linux
#define DEFAULT_UMASK 0022
#endif

//No need to fill logs with the same message, warn based on a desired frequency
#if defined(__ZOWE_OS_ZOS) || defined(USE_RS_SSL) || defined(USE_ZOWE_TLS)
#ifndef TLS_WARN_FREQUENCY
#define TLS_WARN_FREQUENCY 25
#endif
static unsigned int tlsWarnCounter = 0;
#endif

static int64 getFineGrainedTime();

static char *getSessionTokenCookieName(HttpService *service);


/* worry about compareIgnoringCase 
   worry about where ebcdic value is being used meaningfully */

HttpRequestParamSpec *makeParamSpec(char *name, int flags, int type, HttpRequestParamSpec *next){
  HttpRequestParamSpec *spec = (HttpRequestParamSpec*)safeMalloc31(sizeof(HttpRequestParamSpec),"HTTP Param Spec");
  spec->name = name;
  spec->flags = flags;
  spec->type = type;
  spec->next = next;
  spec->minWidth = 0;
  spec->maxWidth = 1;
  spec->lowBound = 0;
  spec->highBound = 1;
  return spec;
}

HttpRequestParamSpec *makeIntParamSpec(char *name, int flags, 
				       int minWidth, int maxWidth,
				       int lowBound, int highBound,
				       HttpRequestParamSpec *next){
  HttpRequestParamSpec *spec = (HttpRequestParamSpec*)safeMalloc(sizeof(HttpRequestParamSpec),"HTTP Int Param Spec");
  spec->name = name;
  spec->flags = flags;
  spec->type = SERVICE_ARG_TYPE_INT;
  spec->minWidth = minWidth;
  spec->maxWidth = maxWidth;
  spec->lowBound = lowBound;
  spec->highBound = highBound;
  spec->next = next;
  return spec;
}

HttpRequestParamSpec *makeInt64ParamSpec(char *name, int flags, 
                                         int minWidth, int maxWidth,
                                         int64 lowBound, int64 highBound,
                                         HttpRequestParamSpec *next){
  HttpRequestParamSpec *spec = (HttpRequestParamSpec*)safeMalloc(sizeof(HttpRequestParamSpec),"HTTP Int Param Spec");
  spec->name = name;
  spec->flags = flags;
  spec->type = SERVICE_ARG_TYPE_INT64;
  spec->minWidth = minWidth;
  spec->maxWidth = maxWidth;
  spec->lowBound64 = lowBound;
  spec->highBound64 = highBound;
  spec->next = next;
  return spec;
}

HttpRequestParamSpec *makeStringParamSpec(char *name, int flags, 
                                          HttpRequestParamSpec *next){
  HttpRequestParamSpec *spec = (HttpRequestParamSpec*)safeMalloc(sizeof(HttpRequestParamSpec),"HTTP Int Param Spec");
  spec->name = name;
  spec->flags = flags;
  spec->type = SERVICE_ARG_TYPE_STRING;
  spec->next = next;
  return spec;
}

static HttpRequestParam *makeIntParam(HttpRequest *request, HttpRequestParamSpec *spec, int value){
  HttpRequestParam *param = (HttpRequestParam*)SLHAlloc(request->slh,sizeof(HttpRequestParam));
  param->specification = spec;
  param->intValue = value;
  param->next = NULL;
  return param;
}

static HttpRequestParam *makeInt64Param(HttpRequest *request, HttpRequestParamSpec *spec, int64 value){
  HttpRequestParam *param = (HttpRequestParam*)SLHAlloc(request->slh,sizeof(HttpRequestParam));
  param->specification = spec;
  param->int64Value = value;
  param->next = NULL;
  return param;
}

static HttpRequestParam *makeStringParam(HttpRequest *request, HttpRequestParamSpec *spec, char *stringValue){
  HttpRequestParam *param = (HttpRequestParam*)SLHAlloc(request->slh,sizeof(HttpRequestParam));
  param->specification = spec;
  param->stringValue = stringValue;
  param->next = NULL;
  return param;
}

#define NO_TRANSLATE 0
#define TRANSLATE_8859_1 1

void initBufferedInput(BufferedInputStream *s, Socket *socket, char *buffer, int bufferSize){
  s->socket = socket;
  s->buffer = buffer;
  s->bufferSize = bufferSize;
  s->fill = 0;
  s->pos = 0;
  s->eof = 0;
}

int readByte(BufferedInputStream *s){
  if (s->eof){
    return -1;
  } else if (s->pos == s->fill){
    int returnCode;
    int reasonCode;
    /* how do we *WAIT* on a read */
    int bytesRead = socketRead(s->socket,s->buffer,s->bufferSize,&returnCode,&reasonCode);

    if (bytesRead > 0)
    {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "read more bytes = %d, showing upto 32\n",bytesRead);
#ifdef DEBUG
      if (bytesRead > 32){
	dumpbuffer(s->buffer,32);
      } else{
	dumpbuffer(s->buffer,bytesRead);
      }
#endif
      s->fill = bytesRead;
      s->pos = 0;
      return s->buffer[s->pos++]&0xff;
    }
    else if (bytesRead == -1)
    {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "socket read -1, errno = %d\n",returnCode);
      /* should check for errno==EINTR */
      s->eof = 1;
      return -1;
    }
    else
    {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "socket read 0, if blocking then EOF\n");
      s->eof = 1;
      return -1;
    }
  } else{
    return s->buffer[s->pos++]&0xff;
  }
}


void initChunkedOutput(ChunkedOutputStream *s, HttpResponse *response,
                       char *buffer, char *translateBuffer, int bufferSize){
  s->response = response;
  s->buffer = buffer;
  s->translateBuffer = translateBuffer;
  s->bufferSize = bufferSize;
  
  s->fill = 0;
}

static void chunkWrite(ChunkedOutputStream *s, char *data, int len){
  if (s->isErrorState) {
    return;
  }
  HttpResponse *response = s->response;
  if (response->runningInSubtask){
    HttpConversation *conversation = response->conversation;
    /* enqueue to main thread */
    STCBase           *stcBase = conversation->server->base;
    WorkElementPrefix *prefix = (WorkElementPrefix*)safeMalloc31(sizeof(WorkElementPrefix)+sizeof(HttpWorkElement),"HttpWorkElement and Prefix");
    HttpWorkElement   *workElement = (HttpWorkElement*)(((char*)prefix)+sizeof(WorkElementPrefix));
    memset(prefix,0,sizeof(WorkElementPrefix));
    memcpy(prefix->eyecatcher,"WRKELMNT",8);
    prefix->payloadCode = HTTP_CONTINUE_OUTPUT;
    prefix->payloadLength = sizeof(HttpWorkElement);
    memset(workElement,0,sizeof(HttpWorkElement));
    workElement->conversation = conversation;
    workElement->response = response;
    workElement->buffer = data;
    workElement->bufferLength = len;
    workElement->reclaimAfterWrite = FALSE;
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "ENQUEUE\n");
    stcEnqueueWork(stcBase,prefix);
  } else{
    int writeRC = writeFully(s->response->socket,data,len);
    if (writeRC == 0) {
      s->isErrorState = true;
    }
  }
}

void writeTransferChunkHeader(ChunkedOutputStream *s, int size){
  char buffer[100];
#ifdef __ZOWE_OS_ZOS
  int len = sprintf(buffer,"%x%c%c",size,0x0d,0x15); /*EBCDIC CRLF */
#else
  int len = sprintf(buffer,"%x%c%c",size,0x0d,0x0a); /* CRLF */
#endif
  toASCIIUTF8(buffer,len);
  chunkWrite(s,buffer,len);
}

void writeBytes(ChunkedOutputStream *s, char *data, int len, int translate){
  /* if data len greater than bufferSize
        1) finish chunk if fill > 0
	2) write data as a transfer chunk
        3) reset to 0
     if data fills or overfills
        1) lay in the section to fill current chunk
	2) flush buffer as transfer chunk
	3) accumulate remainder into buffer
        otherwise
        3) accumulate to buffer 
   */
  if (len > s->bufferSize){
    if (s->fill > 0){
      writeTransferChunkHeader(s,s->fill);
      /* HERE - translate */
      if (translate) {
        toASCIIUTF8(s->buffer,s->fill);
      }
      chunkWrite(s,s->buffer,s->fill);
      chunkWrite(s,crlf,2);
    }
    writeTransferChunkHeader(s,len);
    if (translate) {
      toASCIIUTF8(data,len);
    }
    chunkWrite(s,data,len);
    chunkWrite(s,crlf,2);
    s->fill = 0;
  } else if ((s->fill + len) >= s->bufferSize){
    int whatFits = (s->bufferSize - s->fill);
    writeTransferChunkHeader(s,s->fill+len);
    if (translate) {
      toASCIIUTF8(s->buffer,s->fill);
    }
    chunkWrite(s,s->buffer,s->fill);
    memcpy(s->translateBuffer,data,len);
    if (translate) {
      toASCIIUTF8(s->translateBuffer,len);
    }
    chunkWrite(s,s->translateBuffer,len);
    chunkWrite(s,crlf,2);
    s->fill = 0;
  } else{
    memcpy(s->buffer+s->fill,data,len);
    s->fill += len;
  }
}

void writeString(ChunkedOutputStream *s, char *string){
  int len = strlen(string);
  
  /* printf("writeString \"%s\" ...\n");fflush(stdout); */
  writeBytes(s,string,len,TRANSLATE_8859_1);
}

static void finishChunkedOutput(ChunkedOutputStream *s, int translate){
  /* flush the last chunk 
     write the 0 chunk
     */
  if (s->fill > 0){
    writeTransferChunkHeader(s,s->fill);
    if (translate)  {
      toASCIIUTF8(s->buffer,s->fill);
    }
    chunkWrite(s,s->buffer,s->fill);
    chunkWrite(s,crlf,2);
  }
  writeTransferChunkHeader(s,0);
  chunkWrite(s,crlf,2);
}

// **NOTE**
// **NOTE** finishResponse destroys the response block and the SLH storage associated with it.
// **NOTE**
// **NOTE**   - If finishResponse is *not* called upon completion of the response a storage
// **NOTE**     leak will occur.
// **NOTE**
// **NOTE**   - The response block must not be referenced after finishResponse has been called
// **NOTE**     for the response block.  Unpredictable results will occur if you do.
// **NOTE**

void finishResponse(HttpResponse *response){
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "finishResponse where response=0x%p\n",response);
  if (response->stream){
    finishChunkedOutput(response->stream,
        (response->jp == NULL)? TRANSLATE_8859_1 : 0);
  }
  if (response->jp) {
    freeJsonPrinter(response->jp);
    response->jp = NULL;
  }
  if (response->conversation){  /* should be true except when unit-testing a "pseudoResponse" */
    response->conversation->workingOnResponse = FALSE;
  }

  response->conversation = NULL;    // Prevent a second free of the response block

  SLHFree(response->slh);
}

/********** BIG BUFFER **********/

static BigBuffer *makeBigBuffer(int initialSize, ShortLivedHeap *slh){
  BigBuffer *buffer = (BigBuffer*)(slh? 
                                   SLHAlloc(slh,sizeof(BigBuffer)):
                                   safeMalloc31(sizeof(BigBuffer),"BigBuffer"));
  char *data = (slh? 
                SLHAlloc(slh,initialSize):
                safeMalloc31(initialSize,"BigBuffer Data"));
  buffer->slh = slh;
  buffer->data = data;
  buffer->pos = 0;
  buffer->size = initialSize;
  return buffer;
} 

static void freeBigBuffer(BigBuffer *buffer, int freeData){
  if (buffer->slh == NULL){
    if (freeData){
      safeFree31(buffer->data,buffer->size);
    }
    safeFree31((char*)buffer,sizeof(BigBuffer));
  }
}



static void writeToBigBuffer(BigBuffer *buffer, char *newData, int length){
  if (buffer->pos + length > buffer->size){
    int newSize = buffer->size * 2;
    char *newArray = (buffer->slh? 
                      SLHAlloc(buffer->slh,newSize) :
                      safeMalloc31(newSize,"BigBuffer Data Extension"));
    memcpy(newArray,buffer->data,buffer->pos);
    if (buffer->slh == NULL){
      safeFree31(buffer->data,buffer->size);
    }
    buffer->data = newArray;
    buffer->size = newSize;
  } 
  memcpy(buffer->data+buffer->pos,newData,length);
  buffer->pos += length;
}

static void writeByteToBigBuffer(BigBuffer *buffer, int b){
  char c;

  c = b&0xff;
  writeToBigBuffer(buffer,&c,1);
}


static void resetBigBuffer(BigBuffer *buffer){
  buffer->pos = 0;
}

/********** WEB SOCKET PRIMITIVES ************/


static WSFrame *makeFrame(int opcodeAndFlags, ShortLivedHeap *slh,
                          char *data,int dataLength){
  WSFrame *frame = (WSFrame*)(slh ? 
                              SLHAlloc(slh,sizeof(WSFrame)) :
                              safeMalloc31(sizeof(WSFrame),"WSFrame"));
  memset(frame,0,sizeof(WSFrame));
  frame->opcodeAndFlags = opcodeAndFlags;
  frame->data = data;
  frame->dataLength = dataLength;
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WSFrame data in makeFrame length=0x%x\n",dataLength);
  dumpbuffer(data,dataLength);
  dumpbufferA(data,dataLength);
  return frame;
}

WSMessage *makeMessage(int opcode, ShortLivedHeap *slh){
  WSMessage *message = (WSMessage*)(slh ? 
                                    SLHAlloc(slh,sizeof(WSMessage)) :
                                    safeMalloc31(sizeof(WSMessage),"WSMessage"));
  memset(message,0,sizeof(WSMessage));
  message->opcodeAndFlags = opcode; /* flags get OR-ed in later */
  return message;
}

static void addWSFrame(WSMessage *message, WSFrame *newFrame){
  if (message->firstFrame){
    WSFrame *frame = message->firstFrame;
    while (frame->next){  
      frame = frame->next;
    } 
    frame->next = newFrame;
  } else{
    message->firstFrame = newFrame;
  }
}

static WSReadMachine *makeWSReadMachine(){
  WSReadMachine *machine = (WSReadMachine*)safeMalloc(sizeof(WSReadMachine),"WSReadMachine");
  memset(machine,0,sizeof(WSReadMachine));
  machine->payloadStream = makeBigBuffer(256,NULL);
  return machine;
}

static void enqueueInputMessage(WSReadMachine *machine, WSMessage *newMessage){
  if (machine->messageQueueHead){
    WSMessage *message = machine->messageQueueHead;
    while (message->next){
      message = message->next;
    } 
    message->next = newMessage;
  } else{
    machine->messageQueueHead = newMessage;
  }
}

/*adds new server WS msg to provided work element*/
static void addNewWSFrame(HttpWorkElement *workElement, 
                          WSSession *session,
                          int opcodeAndFlags, 
                          int payloadLength, 
                          char *payload, 
                          int isLastFrame) {
  /* build header here */
  int isMasked = FALSE; /* false for servers */
  int headerLength = 2;
  if (payloadLength >= 65536){
    headerLength += 8;
  } else if (payloadLength >= 126){
    headerLength += 2;
  }                            
                            
  workElement->buffer = safeMalloc(payloadLength+headerLength,"ws write buffer");
  workElement->buffer[0] = opcodeAndFlags;
  if (payloadLength >= 65536){
    workElement->buffer[1] = 127;
    int64 longLength = payloadLength;
    memcpy(workElement->buffer+2,&longLength,8);
  } else if (payloadLength >= 126){
    workElement->buffer[1] = 126;
    unsigned short shortLength = payloadLength&0xFFFF;
    memcpy(workElement->buffer+2,&shortLength,2);
  } else{
    workElement->buffer[1] = payloadLength;
  }

  memcpy(workElement->buffer+headerLength,payload,payloadLength);
  workElement->bufferLength = payloadLength+headerLength;
  workElement->reclaimAfterWrite = TRUE;
  workElement->wsResponseIsBinary = session->isOutputBinary;
  workElement->wsResponseIsLastFrame = isLastFrame;
}

static void wsMessageClose(HttpConversation *conversation,
                           WSSession *session,
                           int opcodeAndFlags,
                           char *payload, 
                           int payloadLength) {
  STCBase           *stcBase = conversation->server->base;
  WorkElementPrefix *prefix = (WorkElementPrefix*)safeMalloc31(sizeof(WorkElementPrefix)+sizeof(HttpWorkElement),"HttpWorkElement and Prefix for WS");
  HttpWorkElement   *workElement = (HttpWorkElement*)(((char*)prefix)+sizeof(WorkElementPrefix));
  memset(prefix,0,sizeof(WorkElementPrefix));
  memcpy(prefix->eyecatcher,"WRKELMNT",8);
  prefix->payloadCode = HTTP_WS_CLOSE_HANDSHAKE;
  prefix->payloadLength = sizeof(HttpWorkElement);
  memset(workElement,0,sizeof(HttpWorkElement));
  workElement->conversation = conversation;

  addNewWSFrame(workElement, session, opcodeAndFlags, payloadLength, payload, TRUE);
  stcEnqueueWork(stcBase,prefix);                          
}

static int readMachineAdvance(WSReadMachine *m, char *data, WSSession *wsSession, int offset, int length){
  int available = length;
  int loopMax = 10;
  int loopCount = 0;
  int anyMessagesReady = FALSE;
  int shouldClose = FALSE;
  m->trace = TRUE;
  while (available > 0){
    if (TRUE || m->trace){
      printf("advanceLoop offset=%02d available=%02d headerFill=%02d headerNeed=%02d\n",
             offset,available,m->headerFill,m->headerNeed);
      dumpbuffer(data+offset,available);
      dumpbufferA(data+offset,available);
    }
    if (m->headerFill < 2){
      while ((m->headerFill < 2) && (available > 0)){
        m->headerBuffer[m->headerFill++] = data[offset++];
        available--;
      }
      if (m->headerFill < 2){
        return FALSE; /* little progress on header */
                           }
      /* compute header need */
      m->flagAndOpcodeByte = m->headerBuffer[0]&0xff;
      shouldClose = m->flagAndOpcodeByte & 0x08;
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "readMachineAdvance: local shouldClose=%d,wsSession 0x%p, flagAndOpcodeByte 0x%X\n",
              shouldClose, wsSession,m->flagAndOpcodeByte);
      m->fin = (m->flagAndOpcodeByte&0x80)!=0;
      int maskAndPayloadLengthByte = m->headerBuffer[1]&0xff;
      int lengthByte = maskAndPayloadLengthByte & 0x7F;
      m->isMasked = (maskAndPayloadLengthByte & 0x80) != 0;
      m->headerNeed = 2;
      if (m->isMasked){
        m->headerNeed += 4;
      }
      if (lengthByte == 127){
        m->payloadLengthLength = 8;
      } else if (lengthByte == 126){
        m->payloadLengthLength = 2;
      }
      m->headerNeed += m->payloadLengthLength;
      if (m->trace){
        printf("computed header need %d\n",m->headerNeed);
      }
      if (m->headerNeed == 2){
        m->headerRead = TRUE;
        m->payloadLength = lengthByte;
      }
    }
    /* header need is established */
    if (m->headerFill < m->headerNeed){
      while ((m->headerFill < m->headerNeed) && (available > 0)){
        m->headerBuffer[m->headerFill++] = data[offset++];
        available--;
      }
      if (m->trace){
        printf("headerFill=%d headerNeed=%d\n",m->headerFill,m->headerNeed);
      }
      if (m->headerFill < m->headerNeed){
        return FALSE;
      }
      switch (m->payloadLengthLength){
      case 0:
        m->payloadLength = m->headerBuffer[1]&0x7F;
        break;
      case 2:
        m->payloadLength = (int64)(*((unsigned short*)(&m->headerBuffer[2])));
        printf("m->payloadLength (firstGo) is %lld\n",m->payloadLength);
        break;
      case 8:
        m->payloadLength = *((int64*)(&m->headerBuffer[2]));
        break;
      }
      if (m->isMasked){
        memcpy(m->maskBytes,m->headerBuffer+(2+m->payloadLengthLength),4);
      }
      if (m->trace){
        printf("established payload length %lld\n",m->payloadLength);
      }
      m->headerRead = TRUE;
    }
    if (m->headerRead){
      int writeLength = ((available > (m->payloadLength - m->payloadFill)) ?
                         (int)(m->payloadLength - m->payloadFill) :
                         available);
      if (m->trace){
        printf("data offset=%d, writeLength=%d\n",offset,writeLength);
      }
      if (m->isMasked){
        dumpbuffer(data,writeLength);
        for (int i=0; i<writeLength; i++){
          int d = data[offset+i]&0xff;
          int maskingByte = m->maskBytes[m->maskModulus]&0xff;
          int u = d^maskingByte;
          /* System.out.printf("data[%d]=0x%x m=0x%02x u=0x%02x\n",i,d,maskingByte,u); */
          writeByteToBigBuffer(m->payloadStream,u);
          
          m->maskModulus++;
          if (m->maskModulus == 4){
            m->maskModulus = 0;
          }
        }
      } else{
        writeToBigBuffer(m->payloadStream,data+offset,writeLength);
      }
      if (m->trace){
        printf("writeLength=%d payloadFill=%lld\n",writeLength,m->payloadFill);
      }
      offset += writeLength;
      available -= writeLength;
      m->payloadFill = m->payloadFill + ((int64)writeLength);
      if (m->trace){
        printf("payloadFill=%lld payloadLength=%lld\n",m->payloadFill,m->payloadLength);
      }
      if (m->payloadFill == m->payloadLength){
        if (m->trace){
          printf("should enqueue offset=%d avail=%d\n",offset,available);
        }
        if (m->currentMessage == NULL){
          m->currentMessage = makeMessage(OPCODE_UNKNOWN,NULL);
        }
        /* in the follwing 3 lines ownership of the big buffer goes to the new frame */
        addWSFrame(m->currentMessage,makeFrame(m->flagAndOpcodeByte,NULL,m->payloadStream->data,m->payloadStream->pos));
        freeBigBuffer(m->payloadStream,FALSE);
        m->payloadStream = makeBigBuffer(256,NULL);

        if (m->fin){
          if (shouldClose != FALSE) {
            wsSession->closeRequested = TRUE;
            wsMessageClose((HttpConversation*)wsSession->conversation, wsSession, m->flagAndOpcodeByte, 
                            m->currentMessage->firstFrame->data, m->currentMessage->firstFrame->dataLength);
          }      
          enqueueInputMessage(m,m->currentMessage);
          m->currentMessage = NULL;
        }
        anyMessagesReady = TRUE;
        m->headerFill = 0;
        m->headerNeed = 0;
        m->payloadFill = 0;
        m->payloadLength = 0;
        m->payloadLengthLength = 0;
        m->flagAndOpcodeByte = 0;
        m->maskModulus = 0;
        m->isMasked = FALSE;
        m->headerRead = FALSE;
        m->fin = FALSE;
      }
    }
    if (m->trace){
      printf("bottom of loop offset=%d avail=%d\n",offset,available); 
    }
  }
  return anyMessagesReady;
}

WSMessageHandler *makeWSMessageHandler(void (*h)(WSSession *session,
                                                 WSMessage *message)){
  printf("makeWSMessageHandler 1\n");
  fflush(stdout);
  WSMessageHandler *handler = (WSMessageHandler*)safeMalloc31(sizeof(WSMessageHandler),"WSMessageHandler");

  printf("makeWSMessageHandler 2\n");
  fflush(stdout);


  memset(handler,0,sizeof(WSMessageHandler));
  handler->onMessage = h;
  
  printf("about to return new messageHandler 0x%p\n",handler);
  fflush(stdout);
  return handler;
}

WSEndpoint *makeWSEndpoint(int isServerEndpoint,
                           void (*onOpen)(WSSession  *session),
                           void (*onError)(WSSession *session),
                           void (*onClose)(WSSession *session)){
  WSEndpoint *endpoint = (WSEndpoint*)safeMalloc31(sizeof(WSEndpoint),"WSEndpoint");

  endpoint->isServerEndpoint = isServerEndpoint;
  endpoint->onOpen = onOpen;
  endpoint->onError = onError;
  endpoint->onClose = onClose;

  return endpoint;
}


WSMessage *getNextMessage(WSReadMachine *machine){
  WSMessage *res = machine->messageQueueHead;
  if (res != NULL){
    machine->messageQueueHead = res->next;
  }
  return res;
}

#define OUTPUT_STATE_ANY 1
#define OUTPUT_STATE_TEXT_PARTIAL 2
#define OUTPUT_STATE_BINARY_PARTIAL 3


WSSession *makeWSSession(HttpConversation *conversation,
                         HttpService *service,
                         char        *negotiatedProtocol){
  WSSession *session = (WSSession*)safeMalloc31(sizeof(WSSession),"WS Session");
  memset(session,0,sizeof(WSSession));
  session->conversation = conversation;
  session->service = service;
  session->readMachine = makeWSReadMachine();
  session->isServer = TRUE;
  session->wsOutputState = OUTPUT_STATE_ANY;
  session->negotiatedProtocol = negotiatedProtocol;
  return session;
}

static void wsMessageEnqueue(HttpConversation *conversation,
                             WSSession *session, 
                             int isLastFrame){
  STCBase           *stcBase = conversation->server->base;
  WorkElementPrefix *prefix = (WorkElementPrefix*)safeMalloc31(sizeof(WorkElementPrefix)+sizeof(HttpWorkElement),"HttpWorkElement and Prefix for WS");
  HttpWorkElement   *workElement = (HttpWorkElement*)(((char*)prefix)+sizeof(WorkElementPrefix));
  memset(prefix,0,sizeof(WorkElementPrefix));
  memcpy(prefix->eyecatcher,"WRKELMNT",8);
  prefix->payloadCode = HTTP_WS_OUTPUT;
  prefix->payloadLength = sizeof(HttpWorkElement);
  memset(workElement,0,sizeof(HttpWorkElement));
  workElement->conversation = conversation;
  int conversionLength = 0;
  char *payload = NULL;
  int payloadLength = 0;
  if (session->isOutputBinary){
    payload = session->outputFrameBuffer;
    payloadLength = session->outputFramePos;
  } else{
    int conversationLength = 0;
    int reasonCode = 0;

    int status = convertCharset(session->outputFrameBuffer,
                                session->outputFramePos,
                                session->outputCCSID,
                                CHARSET_OUTPUT_USE_BUFFER,
                                &(session->conversionBuffer),
                                session->outputFrameSize*2,
                                CCSID_UTF_8,
                                NULL,
                                &conversionLength,
                                &reasonCode);

    payload = session->conversionBuffer;
    payloadLength = conversionLength;
  }

  int opcodeAndFlags = ((isLastFrame ? 0x80 : 0x00)|
                        (session->isOutputBinary ? OPCODE_BINARY_FRAME : OPCODE_TEXT_FRAME));
  addNewWSFrame(workElement, session, opcodeAndFlags, payloadLength, payload, isLastFrame);
  stcEnqueueWork(stcBase,prefix);
}

static void wsJsonWriteCallback(jsonPrinter *p, char *text, int len){
  WSSession *session = (WSSession*)p->customObject;
  int remainingSpace = (session->outputFrameSize - session->outputFramePos);
  if (len < remainingSpace){
    memcpy(session->outputFrameBuffer+session->outputFramePos,text,len);
    session->outputFramePos += len;
  } else{
    memcpy(session->outputFrameBuffer+session->outputFramePos,text,remainingSpace);
    wsMessageEnqueue(session->conversation,session,FALSE);
    int leftover = len-remainingSpace;
    memcpy(session->outputFrameBuffer,text+remainingSpace,leftover);
    session->outputFramePos = leftover;
  }
}

jsonPrinter *initWSJsonPrinting(WSSession *session, int maxFrameSize){
  session->outputFrameSize = maxFrameSize;
  session->outputFramePos = 0;
  session->outputFrameBuffer = safeMalloc31(maxFrameSize,"WS JSON Frame Buffer");
  session->conversionBuffer = safeMalloc31(maxFrameSize*2,"WS JSON Text Conversion Buffer");
  session->jp = makeCustomJsonPrinter(wsJsonWriteCallback,
                                      session);  
  return session->jp;
}

void flushWSJsonPrinting(WSSession *session){
  wsMessageEnqueue(session->conversation,session,TRUE);
  session->outputFramePos = 0;
}

int wsSessionMoreInput(WSSession *session){
  return 0;
}


/******** Request/Response and Header Management ***********/

void setResponseStatus(HttpResponse *response, int status, char *message){
  response->status = status;
  response->message = message;
  response->headers = NULL;
}



void addHeader(HttpResponse *response, HttpHeader *header){
  HttpHeader *headerChain = response->headers;
  if (headerChain){
    HttpHeader *prev;
    while (headerChain){
      prev = headerChain;
      headerChain = headerChain->next;
    }
    prev->next = header;
  } else{
    response->headers = header;
  }
}

void addStringHeader(HttpResponse *response, char *name, char *value){
  HttpHeader *header = (HttpHeader*)SLHAlloc(response->slh,sizeof(HttpHeader));
  
  header->name = name;
  header->value = value;
  header->next = NULL;
  addHeader(response,header);
}

void addIntHeader(HttpResponse *response, char *name, int value){
  HttpHeader *header = (HttpHeader*)SLHAlloc(response->slh,sizeof(HttpHeader));
  
  header->name = name;
  header->value = NULL;
  header->intValue = value;
  header->next = NULL;
  addHeader(response,header);
}


void setContentType(HttpResponse *response, char *contentType){
  addStringHeader(response,"Content-Type",contentType);
}



static void traceHeader(const char*line, int len)
{
  if (traceHeaders > 0) {
#ifndef METTLE
  /*
   * this should be using a logging context instead of fwrite.
   */
    fwrite(line, 1, (size_t) len, stdout);
    fwrite("\n", 1, 1, stdout);
#endif
  }
}

#define JED_HTTP_KEEP_ALIVE_MAX 1000

void writeHeader(HttpResponse *response){
  if (response->sessionCookie) {
    addStringHeader(response, "Set-Cookie", response->sessionCookie);
    if (response->sessionTimeout) {
      addIntHeader(response, "Session-Expires-Seconds", response->sessionTimeout);
    } else {
      AUTH_TRACE("\ncouldnt set expiration during writeheader\n");
    }
  }
  if (response->conversation->isKeepAlive) {
    if (response->conversation->requestCount == JED_HTTP_KEEP_ALIVE_MAX) {
      addStringHeader(response, "Connection", "close");
    }
    else {
      addStringHeader(response, "Connection", "keep-alive");
      addStringHeader(response, "Keep-Alive", "max=1000");
    }
  }

  Socket *socket = response->socket;
  int returnCode;
  int reasonCode;
  char line[1024];
  int len = 0;
  HttpHeader *headerChain = response->headers;
  
  len = sprintf(line,"HTTP/1.1 %d %s",response->status,response->message);
  asciify(line,len);
  traceHeader(line,len);
  writeFully(socket,line,len);
  writeFully(socket,crlf,2);
  while (headerChain){
    if (headerChain->value){
      len = sprintf(line,"%s: %s",headerChain->name, headerChain->value);
    } else{
      len = sprintf(line,"%s: %d",headerChain->name, headerChain->intValue);
    }
    asciify(line,len);
    traceHeader(line,len);
    writeFully(socket,line,len);
    writeFully(socket,crlf,2);
    headerChain = headerChain->next;
  }
  writeFully(socket,crlf,2);
}

void writeRequest(HttpRequest *request, Socket *socket){
  int returnCode;
  int reasonCode;
  char line[1024];
  int len = 0;
  HttpHeader *headerChain = request->headerChain;
  
  len = sprintf(line,"%s %s HTTP/1.1",request->method,request->uri);
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "header: %s\n",line);
  asciify(line,len);
  writeFully(socket,line,len);
  writeFully(socket,crlf,2);
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "write header chain 0x%p\n",headerChain);

  while (headerChain){

#ifdef DEBUG
    printf("headerChain %s %s or %d\n",headerChain->nativeName, 
           (headerChain->nativeValue ? 
            headerChain->nativeValue :
            "<n/a>"), 
           headerChain->intValue);
#endif
    
    if (headerChain->nativeValue){
      len = sprintf(line,"%s: %s",headerChain->nativeName, headerChain->nativeValue);
    } else{
      len = sprintf(line,"%s: %d",headerChain->nativeName, headerChain->intValue);
    }
    asciify(line,len);
    writeFully(socket,line,len);
    writeFully(socket,crlf,2);
    headerChain = headerChain->next;
  }
  writeFully(socket,crlf,2);
}


typedef struct HTMLTemplate_tag{
  UnixFile *in;
  HttpResponse *response;
  char *currentPlaceholder;
  char *placeholderBuffer;
} HTMLTemplate;

#define MAX_HTML_LINE 1024

HTMLTemplate *openHTMLTemplate(HttpResponse *response, char *filename){
  HttpRequest *request = response->request;
  int returnCode;
  int reasonCode;
  HTMLTemplate *template = (HTMLTemplate*)SLHAlloc(request->slh,sizeof(HTMLTemplate));
  template->in = fileOpen(filename,FILE_OPTION_READ_ONLY,0,0,&returnCode,&reasonCode);
  
  if (template->in){
    template->response = response;
    template->currentPlaceholder = NULL;
    template->placeholderBuffer = SLHAlloc(request->slh,MAX_HTML_LINE);
    return template;
  } else{
    return NULL;
  }
}

void closeHTMLTemplate(HTMLTemplate *template){
  int returnCode;
  int reasonCode;
  fileClose(template->in,&returnCode,&reasonCode);
}

int streamToSubstitution(HTMLTemplate *template, ChunkedOutputStream *responseStream){
  char buffer[MAX_HTML_LINE+1];
  int returnCode;
  int reasonCode;
   
  template->currentPlaceholder = NULL;
  while (!fileEOF(template->in)){
    int bytesRead = fileRead(template->in,buffer,MAX_HTML_LINE,&returnCode,&reasonCode);
    if (bytesRead > 0){
      buffer[bytesRead] = '\0';
    } else{
      return 0;
    }
    if (sscanf(buffer," <substitute place=\"%s\">", template->placeholderBuffer) == 1){
      char *place = template->placeholderBuffer;
      int i,len = strlen(place);

      for (i=0; i<len; i++){
	if (place[i] == '"' || place[i] == '\''){
	  place[i] = 0;
	  break;
	}
      }
      template->currentPlaceholder = place;      
      return 1;
    }
    writeString(responseStream,buffer);
  }
  return 0;
}

void parseURI(HttpRequest *request);
void respondWithError(HttpResponse *response, int code, char *message);
void serveFile(HttpService *service, HttpResponse *response);
void serveSimpleTemplate(HttpService *service, HttpResponse *response);
char *processServiceRequestParams(HttpService *service,HttpResponse *response);

#define ASCII_COLON 0x3A




#define MAX_HEADER_LINE 1024 /* check the RFC !! */

#define ASCII_SPACE     0x20
#define ASCII_HASH      0x23
#define ASCII_AMPERSAND 0x26
#define ASCII_PLUS      0x2B
#define ASCII_SLASH     0x2F
#define ASCII_COLON     0x3A
#define ASCII_EQUALS    0x3D
#define ASCII_QUESTION  0x3F

char *getStringToken(HttpRequest *request, int stopChar, int *eofp){
  char buffer[MAX_HEADER_LINE];
  int pos = 0;
  int c;

  if (traceParse){
    printf("getStringToken\n");
  }
  while ((c = readByte(request->input)) != 13){
    if (traceParse){
      printf("  loop %d pos=%d\n",c,pos);
    }
    if (c == 0){
      return NULL;
    } else if (c == -1){
      /* probably EOF */
      *eofp = 1;
      return NULL;
    } else if (pos >= (MAX_HEADER_LINE - 1)){
      return NULL;
    } else if (c == stopChar){
      break;
    } else{
      buffer[pos++] = c;
    }
  }
  if (pos){
    char *string = SLHAlloc(request->slh,pos+1); /* +1 for null-term */
    memcpy(string,buffer,pos);
    string[pos] = 0;
    return string;
  } else{
    return NULL;
  }
}

char *getHttpVersion(HttpRequest *request){
  return NULL;
}

HttpHeader *getHeaderLine(HttpRequest *request){
  int pos = 0;
  char buffer[MAX_HEADER_LINE];
  char aBuffer[MAX_HEADER_LINE];
  int c;
  int colonPos = -1;
  int firstNonWhitespace = -1;

  if (traceParse){
    printf("getHeaderLine\n");
  }
  while ((c = readByte(request->input)) != 13){
    if (traceParse){
      printf(" %02x",c);
    }
    if (c == 0){

    } else if (c == -1){

    } else if (pos >= (MAX_HEADER_LINE - 1)){
      return NULL;
    } else{
      if ((colonPos == -1) && (c == ASCII_COLON)){
	colonPos = pos;
	buffer[pos++] = c;
      } else{
	if ((colonPos != -1) && 
	    (firstNonWhitespace == -1) &&
	    (c > 0x20) &&
	    (c < 0x7F)){
	  firstNonWhitespace = pos;
	}	  
	buffer[pos++] = c;
      }
    }
  }
  
  if (traceParse){
    printf("\n colonPos=%d firstNonWhite=%d\n",colonPos,firstNonWhitespace);
  }
  if (readByte(request->input) != 10){ /* proper CR/LF */
    memcpy(aBuffer,buffer,MAX_HEADER_LINE);
    a2e(aBuffer,MAX_HEADER_LINE);
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "bad cr/lf %s\n",aBuffer);
    return NULL;
  } else if (colonPos == -1){
    if (pos == 0){
      /* empty line is end of headers */
      return NULL;
    } else{
      memcpy(aBuffer,buffer,MAX_HEADER_LINE);
      a2e(aBuffer,MAX_HEADER_LINE);
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "no colon seen in header line %s\n",aBuffer);
      return NULL;
    }
  } else if (firstNonWhitespace == -1){
    return NULL;
  } else{
    HttpHeader *header = (HttpHeader*)SLHAlloc(request->slh,sizeof(HttpHeader));
    int valueLength = pos-firstNonWhitespace;
    char *name = SLHAlloc(request->slh,colonPos+1);
    char *nativeName = SLHAlloc(request->slh,colonPos+1);
    char *value = SLHAlloc(request->slh,valueLength+1);
    char *nativeValue = SLHAlloc(request->slh,valueLength+1);
    /* printf("about to make header line colonPos=%d pos=%d valueLength=%d\n",colonPos,pos,valueLength); */
    memcpy(name,buffer,colonPos);
    memcpy(nativeName,buffer,colonPos);
    name[colonPos] = 0;
    nativeName[colonPos] = 0;
    memcpy(value,buffer+firstNonWhitespace,valueLength);
    memcpy(nativeValue,buffer+firstNonWhitespace,valueLength);
    value[valueLength] = 0;
    nativeValue[valueLength] = 0;
    header->name = name;
    header->nativeName = destructivelyNativize(nativeName);
    header->value = value;
    header->nativeValue = destructivelyNativize(nativeValue);
    if (request->headerChain == NULL){
      request->headerChain = header;
    } else{
      HttpHeader *headerChain = request->headerChain;
      while (headerChain->next){
	headerChain = headerChain->next;
      }
      headerChain->next = header;
    }

    return header;
  }
}

HttpHeader *getHeader(HttpRequest *request, char *name){
  HttpHeader *headerChain = request->headerChain;
  while (headerChain){
    if (!compareIgnoringCase(name, headerChain->nativeName, strlen(name))){
      return headerChain;
    }
    headerChain = headerChain->next;
  }
  return NULL;
}

#define HTTPSERVER_SESSION_TOKEN_KEY_SIZE  32

typedef struct SessionTokenKey_tag {
  char value[HTTPSERVER_SESSION_TOKEN_KEY_SIZE];
} SessionTokenKey;

static int initSessionTokenKey(SessionTokenKey *key) {

#ifdef __ZOWE_OS_ZOS

  int icsfRSN = 0;
  int icsfRC = icsfGenerateRandomNumber(key, sizeof(SessionTokenKey), &icsfRSN);
  if (icsfRC != 0) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
            "Error: ICSF generation of random number failed. Session token key not generated, RC = %d, RSN = %d\n",
            icsfRC, icsfRSN);
    return -1;
  }

#else

  printf("*** WARNING *** low quality random number in use for SessionToken \n");
  for (int i=0; i<HTTPSERVER_SESSION_TOKEN_KEY_SIZE; i++){
    key->value[i] = (char)(rand()&0xFF);
  }

#endif

  return 0;
}

#define trivialEncipher memcpy
#define trivialDecipher memcpy

static int encodeSessionToken(ShortLivedHeap *slh,
                              const HttpServerConfig *config,
                              const char *tokenText,
                              unsigned int tokenTextLength,
                              char **result) {

  unsigned int encodedTokenTextLength = tokenTextLength;
  char *encodedTokenText = SLHAlloc(slh, encodedTokenTextLength);
  if (encodedTokenText == NULL) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
    		    "Error: encoded session token buffer not allocated "
            "(size=%u, SLH=%p)\n", encodedTokenTextLength, slh);
    return -1;
  }


#ifdef __ZOWE_OS_ZOS


  int icsfRSN = 0;
  int icsfRC = icsfEncipher(config->sessionTokenKey,
                            config->sessionTokenKeySize,
                            tokenText,
                            tokenTextLength,
                            encodedTokenText,
                            encodedTokenTextLength,
                            &icsfRSN);
  if (icsfRC != 0) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
    		    "Error: session token encoding failed, RC = %d, RSN = %d\n",
            icsfRC, icsfRSN);
    return -1;
  }

  *result = encodedTokenText;
  return 0;

#elif defined(__ZOWE_OS_WINDOWS)

  printf("*WARNING* - Trivial Cipher in use\n");
  trivialEncipher(encodedTokenText,tokenText,tokenTextLength);
  *result = encodedTokenText;
  return 0;
  
#else

#error Session token encoding has been implemented for z/OS only

#endif /* __ZOWE_OS_ZOS */

}

static int decodeSessionToken(ShortLivedHeap *slh,
                              const HttpServerConfig *config,
                              const char *encodedTokenText,
                              unsigned int encodedTokenTextLength,
                              char **result) {
  unsigned int tokenTextLength = encodedTokenTextLength;
  unsigned int paddingLength = 4;
  char *tokenText = SLHAlloc(slh, tokenTextLength+paddingLength);
  if (tokenText == NULL) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
    		    "Error: decoded session token buffer not allocated "
            "(size=%u, SLH=%p)\n", encodedTokenTextLength, slh);
    return -1;
  }

#ifdef __ZOWE_OS_ZOS

  int icsfRSN = 0;
  int icsfRC = icsfDecipher(config->sessionTokenKey,
                            config->sessionTokenKeySize,
                            encodedTokenText,
                            encodedTokenTextLength,
                            tokenText,
                            tokenTextLength,
                            &icsfRSN);
  if (icsfRC != 0) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
    		    "Error: session token decoding failed, RC = %d, RSN = %d\n",
            icsfRC, icsfRSN);
    return FALSE;
  }

  // always null terminate because consumer might be expecting null terminate on other side
  tokenText[tokenTextLength]=0;
  *result = tokenText;
  return 0;

#elif defined(__ZOWE_OS_WINDOWS)

  printf("*WARNING* - Trivial Cipher in use\n");
  trivialDecipher(tokenText,encodedTokenText,tokenTextLength);
  *result = tokenText;
  return 0;


#else

#error Session token decoding has been implemented for z/OS only

#endif /* __ZOWE_OS_ZOS */

}

static
HttpServer *makeHttpServerInner(STCBase *base,
                                InetAddr *addr,
                                int port,
                                void *tlsEnv,
                                int tlsFlags,
                                char *cookieName,
                                int *returnCode, int *reasonCode){
  //logConfigureComponent(NULL, LOG_COMP_HTTPSERVER, "httpserver", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  Socket *listenerSocket = NULL;
  /* if "noTCP" is true, this server only receives IO thru tunnelling */
  bool noTCP = false;
  SessionTokenKey sessionTokenKey = {0};
  if (initSessionTokenKey(&sessionTokenKey) != 0) {
    return NULL;
  }
  if (addr != NULL && port != HTTP_DISABLE_TCP_PORT){
    listenerSocket = tcpServer2(addr,port,tlsFlags,returnCode,reasonCode);
    if (listenerSocket == NULL){
      return NULL;
    }
#ifdef USE_ZOWE_TLS
    listenerSocket->tlsEnvironment = tlsEnv;
#endif // USE_ZOWE_TLS
  } else{
    noTCP = true;
  }
  HttpServer *server = (HttpServer*)safeMalloc31(sizeof(HttpServer),"HTTP Server");
  memset(server,0,sizeof(HttpServer));
  server->base = base;
  server->slh = makeShortLivedHeap(65536,100);
  server->cookieName = cookieName;
  SocketExtension *listenerSocketExtension = makeSocketExtension(listenerSocket,server->slh,FALSE,server,65536);
  listenerSocket->userData = listenerSocketExtension;
  /*
    FIX THIS: Will make a more clever UID later. For now, we just need something
    that is guaranteed to be different with each run of the same server
    */
  server->serverInstanceUID = (uint64)getFineGrainedTime();
  stcRegisterSocketExtension(base, listenerSocketExtension, STC_MODULE_JEDHTTP);

  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "ListenerSocket on SD/WinSock=0x%x\n",
	  (listenerSocket ? getSocketDebugID(listenerSocket) : 0));

  server->config = (HttpServerConfig*)safeMalloc31(sizeof(HttpServerConfig),"HttpServerConfig");
  server->syntheticPipeSockets = htCreate(4001, NULL, NULL, NULL, NULL);
  server->properties = htCreate(4001,stringHash,stringCompare,NULL,NULL);
  memset(server->config,0,sizeof(HttpServerConfig));

  server->config->sessionTokenKeySize = sizeof (sessionTokenKey);
  memcpy(&server->config->sessionTokenKey[0], &sessionTokenKey,
         sizeof (sessionTokenKey));
  server->config->authTokenType = SERVICE_AUTH_TOKEN_TYPE_LEGACY;

  return server;
}

int httpServerEnablePipes(HttpServer *server,
			  int fromDispatcherFD,
			  int toDispatcherFD){
  Socket *pipeSocket = makePipeBasedSyntheticSocket(IPPROTO_SYNTHETIC_PIPE_DEMULTIPLEXER,
						    fromDispatcherFD,toDispatcherFD);
  /* pipe tracking and the arguments to makeSExt() */
  SocketExtension *pipeExtension = makeSocketExtension(pipeSocket,
                                                       server->slh,
                                                       FALSE, /* allocateInSLH */
                                                       server,
                                                       READ_BUFFER_SIZE);
  pipeSocket->userData = pipeExtension;
  stcRegisterSocketExtension(server->base, pipeExtension, STC_MODULE_JEDHTTP);
  
  server->singleUserMode = true; /* this should be somewhere else some day */
  
  return 0;
}


HttpServer *makeHttpServer2(STCBase *base,
                           InetAddr *addr,
                           int port,
                           int tlsFlags,
                           int *returnCode,
                           int *reasonCode){
  return makeHttpServerInner(base, addr, port, NULL, tlsFlags, SESSION_TOKEN_COOKIE_NAME, returnCode, reasonCode);
}

HttpServer *makeHttpServer3(STCBase *base,
                           InetAddr *addr,
                           int port,
                           int tlsFlags,
                           char *cookieName, 
                           int *returnCode,
                           int *reasonCode){
  return makeHttpServerInner(base, addr, port, NULL, tlsFlags, cookieName, returnCode, reasonCode);
}

#ifdef USE_RS_SSL
HttpServer *makeSecureHttpServer2(STCBase *base, int port,
                                  RS_SSL_ENVIRONMENT sslEnvironment,
                                  char *cookieName, 
                                  int *returnCode, int *reasonCode) {
  return makeSecureHttpServerInner(base, port, sslEnvironment, cookieName, returnCode, reasonCode);
}

HttpServer *makeSecureHttpServer(STCBase *base, int port,
                                 RS_SSL_ENVIRONMENT sslEnvironment,
                                 int *returnCode, int *reasonCode) {
  return makeSecureHttpServerInner(base, port, sslEnvironment, SESSION_TOKEN_COOKIE_NAME, returnCode, reasonCode);
}
static HttpServer *makeSecureHttpServerInner(STCBase *base, int port,
                                             RS_SSL_ENVIRONMENT sslEnvironment,
                                             char *cookieName,
                                             int *returnCode, int *reasonCode) {
  Socket *listenerSocket = tcpServer(NULL,port,returnCode,reasonCode);
  if (listenerSocket == NULL){
    return NULL;
  }
  listenerSocket->sslHandle = sslEnvironment;

  HttpServer *server = (HttpServer*)safeMalloc31(sizeof(HttpServer),"HTTP Server");
  memset(server,0,sizeof(HttpServer));
  server->base = base;
  server->slh = makeShortLivedHeap(65536,100);
  server->cookieName = cookieName;
  SocketExtension *listenerSocketExtension = makeSocketExtension(listenerSocket,server->slh,FALSE,server,65536);
  listenerSocket->userData = listenerSocketExtension;
  /*
    FIX THIS: Will make a more clever UID later. For now, we just need something
    that is guaranteed to be different with each run of the same server
    */
  server->serverInstanceUID = (uint64)getFineGrainedTime();
  stcRegisterSocketExtension(base, listenerSocketExtension, STC_MODULE_JEDHTTP);

#ifdef __ZOWE_OS_WINDOWS
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "ListenerSocket on SocketHandle=0x%x\n",listenerSocket->windowsSocket);
#else
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "ListenerSocket on SD=%d\n",listenerSocket->sd);
#endif

  server->config = (HttpServerConfig*)safeMalloc31(sizeof(HttpServerConfig),"HttpServerConfig");
  server->properties = htCreate(4001,stringHash,stringCompare,NULL,NULL);
  memset(server->config,0,sizeof(HttpServerConfig));
  int64 now = getFineGrainedTime();
  server->config->sessionTokenKeySize = sizeof (now);
  memcpy(&server->config->sessionTokenKey[0], &now, sizeof (now));
  return server;
}
#endif

#ifdef USE_ZOWE_TLS
HttpServer *makeSecureHttpServer(STCBase *base,
                                 InetAddr *addr,
                                 int port,
                                 TlsEnvironment *tlsEnv,
                                 int tlsFlags,
                                 int *returnCode,
                                 int *reasonCode
                                ) {
  return makeHttpServerInner(base, addr, port, tlsEnv, tlsFlags, SESSION_TOKEN_COOKIE_NAME, returnCode, reasonCode);
}
HttpServer *makeSecureHttpServer2(STCBase *base,
                                  InetAddr *addr,
                                  int port,
                                  TlsEnvironment *tlsEnv,
                                  int tlsFlags,
                                  char *cookieName,
                                  int *returnCode,
                                  int *reasonCode
                                  ) {
  return makeHttpServerInner(base, addr, port, tlsEnv, tlsFlags, cookieName, returnCode, reasonCode);
}

#endif // USE_ZOWE_TLS

void *getConfiguredProperty(HttpServer *server, char *key){
  return htGet(server->properties,key);
}

void setConfiguredProperty(HttpServer *server, char *key, void *value){
  htPut(server->properties,key,value);
}

int httpServerSetSessionTokenKey(HttpServer *server, unsigned int size,
                                  unsigned char key[]){
  if ((size == 0) || (size > HTTP_SERVER_MAX_SESSION_TOKEN_KEY_SIZE)) {
    return -1;
  }
  server->config->sessionTokenKeySize = size;
  memcpy(server->config->sessionTokenKey, key, size);
  return 0;
}

HttpServer *makeHttpServer(STCBase *base, int port, int *returnCode, int *reasonCode){
  return makeHttpServer2(base, NULL, port, 0, returnCode, reasonCode);
}

int registerHttpService(HttpServer *server, HttpService *service){
  HttpServerConfig *config = server->config;
  service->next = NULL;
  if (config->serviceList){
    HttpService *s = config->serviceList;
    while (s->next){
      s = s->next;
    }
    s->next = service;
  } else{
    config->serviceList = service;
  }

  if (server->sharedServiceMem != NULL) {
    service->sharedServiceMem = server->sharedServiceMem;
  }

  service->serverInstanceUID = server->serverInstanceUID;
  service->productURLPrefix = server->defaultProductURLPrefix;
  return 0;
}

int registerHttpServiceOfLastResort(HttpServer *server, HttpService *service){
  server->config->serviceOfLastResort = service;
  return 0;
}

static char *getNative(char *s){
  int len = strlen(s);
  char *native = safeMalloc(len+1,"Native string");
  strcpy(native,s);
#ifdef __ZOWE_OS_ZOS
  destructivelyUnasciify(native);
#endif
  return native;
}

/* makeHttpResponse alloc's the response structure on the passed SLH,
 * but now infuses the HttpResponse with its own SLH */
HttpResponse *makeHttpResponse(HttpRequest *request, ShortLivedHeap *slh, Socket *socket){
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "makeHttpResponse called with req=0x%p, slh=0x%p, socket=0x%p\n",
         request, slh, socket);
  ShortLivedHeap *responseSLH = makeShortLivedHeap(65536,100);
  HttpResponse *response = (HttpResponse*)SLHAlloc(responseSLH,sizeof(HttpResponse));
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "makeHttpResponse after SLHAlloc, resp=0x%p\n", response);
  if (NULL != request) {
    response->request = request;
  	request->slh = slh;
  }
  response->status = -1;
  response->headers = NULL;
  response->slh = responseSLH;
  response->socket = socket;
  return response;
}


static char *responseAlloc(HttpResponse *response, int size){
  char *data = SLHAlloc(response->slh,size);
  return data;
}

static ChunkedOutputStream *makeChunkedOutputStreamInternal(HttpResponse *response){
  ChunkedOutputStream *out = (ChunkedOutputStream*)responseAlloc(response,sizeof(ChunkedOutputStream));
  char *buffer = responseAlloc(response,1024);
  char *translateBuffer = responseAlloc(response,1024);
  
  initChunkedOutput(out,response,buffer,translateBuffer,1024);
  return out;
}


static void writeXmlFullyCallback(xmlPrinter *p, char *text, int len){
  HttpResponse *response = (HttpResponse*)p->customObject;
  writeBytes(response->stream,text,len,TRANSLATE_8859_1);
}

static void writeJsonFullyCallback(jsonPrinter *p, char *text, int len){
  HttpResponse *response = (HttpResponse*)p->customObject;
  writeBytes(response->stream,text,len,0);
  if (response->stream->isErrorState) {
    jsonSetIOErrorFlag(p);
  }
}

static void writeXmlByteCallback(xmlPrinter *p, char c){
  HttpResponse *response = (HttpResponse*)p->customObject;
  writeBytes(response->stream,&c,1,TRANSLATE_8859_1);
}

xmlPrinter *respondWithXmlPrinter(HttpResponse *response){
  if (response->responseTypeChosen){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "*** WARNING *** response type already chosen\n");
    return NULL;
  }
  response->stream = makeChunkedOutputStreamInternal(response);
  response->responseTypeChosen = TRUE;
  response->p = makeCustomXmlPrinter(NULL,
				     writeXmlFullyCallback,
				     writeXmlByteCallback,
				     response);
  return response->p;
}

jsonPrinter *respondWithJsonPrinter(HttpResponse *response){
  if (response->responseTypeChosen){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "*** WARNING *** response type already chosen\n");
    return NULL;
  }
  response->stream = makeChunkedOutputStreamInternal(response);
  response->responseTypeChosen = TRUE;
#ifdef __ZOWE_EBCDIC
  response->jp = makeCustomUtf8JsonPrinter(writeJsonFullyCallback,
                                       response, CCSID_IBM1047);
#else
  response->jp = makeCustomUtf8JsonPrinter(writeJsonFullyCallback,
                                       response, CCSID_ISO_8859_1);
#endif

  return response->jp;
}

ChunkedOutputStream *respondWithChunkedOutputStream(HttpResponse *response){
  if (response->responseTypeChosen){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "*** WARNING *** response type already chosen\n");
    return NULL;
  }
  response->stream = makeChunkedOutputStreamInternal(response);
  response->responseTypeChosen = TRUE;
  return response->stream;
}

static HttpService *findHttpService(HttpServer *server, HttpRequest *request){
  HttpService *service = server->config->serviceList;
  StringList *parts = request->parsedFile;
  int partsCount = (parts? parts->count : 0);
  if (traceDispatch){
    printf("find service: serviceListHead=0x%p parsedFile=0x%p\n",service,request->parsedFile);
  }
  while (service){
    if (traceDispatch){
      printf("  find service '%s' while loop top service->parsedMaskPartCount=%d parts->count=%d, next=0x%p\n",
	     service->name,service->parsedMaskPartCount,partsCount,service->next);
      fflush(stdout);
    }
    if (service->parsedMaskPartCount == 0){ /* special case for "home URL" */
      if (partsCount == 0){
	return service;
      }
    } else if (service->parsedMaskPartCount <= partsCount){
      int match = TRUE;
      int matchedAnyParts = FALSE;
      int i;
      StringListElt *partsList = firstStringListElt(parts);

      if (traceDispatch){
	printf("find count = %d %s\n",partsCount,(partsList ? partsList->string : "<N/A>") );fflush(stdout);
      }
      for (i=0; i < service->parsedMaskPartCount; i++){
	char *desiredPart = service->parsedMaskParts[i];
        if (traceDispatch){
          printf("top of match loop desiredPart=%s\n",desiredPart); 
        }
	char *part = partsList->string;
	if (traceDispatch){
	  printf("    find: for loop top '%s' '%s' \n",desiredPart,part); fflush(stdout);
	}
	if (!strcmp(desiredPart,"*") ||
	    !strcmp(desiredPart,part)){
	  matchedAnyParts = TRUE;
	  partsList = partsList->next;
	} else{
	  match = FALSE;
	  break;
	}
      }
      if (traceDispatch){
        printf("out of loop match=%d matchedAnyParts=%d\n",match,matchedAnyParts);
      }
      if (match){
	if (partsList){
	  if (service->matchFlags & SERVICE_MATCH_WILD_RIGHT){
	    return service;
	  } else{
	    if (traceDispatch) {
	      printf("did not match whole service spec expecting part='%s'\n",partsList->string);
	    }
	    return NULL;
	  } 
	} else{
	  return service;
	}
      }
    }
    service = service->next;
  }
  return NULL;
}

#define HTTP_STATE_REQUEST_METHOD           1 
#define HTTP_STATE_REQUEST_GAP1             2
#define HTTP_STATE_REQUEST_URI              3
#define HTTP_STATE_REQUEST_GAP2             4
#define HTTP_STATE_REQUEST_VERSION          5
#define HTTP_STATE_REQUEST_CR_SEEN          6
#define HTTP_STATE_HEADER_FIELD_NAME        7
#define HTTP_STATE_HEADER_GAP1              8
#define HTTP_STATE_HEADER_FIELD_CONTENT     9
#define HTTP_STATE_HEADER_GAP2             10
#define HTTP_STATE_HEADER_CR_SEEN          11
#define HTTP_STATE_END_CR_SEEN             12
#define HTTP_STATE_READING_FIXED_BODY      13
#define HTTP_STATE_READING_CHUNK_SIZE      14
#define HTTP_STATE_READING_CHUNK_EXTENSION 15
#define HTTP_STATE_CHUNK_SIZE_CR_SEEN      16
#define HTTP_STATE_READING_CHUNK_DATA      17
#define HTTP_STATE_CHUNK_DATA_CR_SEEN      18
#define HTTP_STATE_READING_CHUNK_TRAILER   19
#define HTTP_STATE_CHUNK_TRAILER_CR_SEEN   20

static char *stateNames[21] = {
  "initial",
  "reqMethod",
  "reqGap1", /* 2 */
  "reqURI",
  "requGap2",
  "reqVersion", /* 5 */
  "reqCRSeen",
  "headerFieldName",
  "headerGap1",       /* 6 */
  "headerFieldValue",
  "headerGap2",
  "headerCRSeen",
  "endCRSeen",
  "readingFixedBody",
  "readingChunkSize",
  "readingChunkExtensionXXX-UNSUPPORTED",
  "chunkCRSeen",
  "readingChunkData",
  "dataCRSeen",
  "readingChunkTrailer",
  "chunkTrailerCRSeen"
};


HttpRequestParser *makeHttpRequestParser(ShortLivedHeap *slh){
  /* these parser structures were being leaked, so now we put them
   * on the conversation-lifetime SLH that gets free'd when the socket closes */
  HttpRequestParser *parser = (HttpRequestParser*)SLHAlloc(slh, sizeof(HttpRequestParser));
  memset(parser,0,sizeof(HttpRequestParser));
  
  parser->slh = slh;
  parser->state = HTTP_STATE_REQUEST_METHOD;
  parser->specifiedContentLength = -1; /* unspecified */
  
  return parser;
}

/*
  returns still-ok value 
 */


static void addRequestHeader(HttpRequestParser *parser){
  HttpHeader *newHeader = (HttpHeader*)SLHAlloc(parser->slh,sizeof(HttpHeader));
  newHeader->name = copyString(parser->slh,parser->headerName,parser->headerNameLength);
  newHeader->nativeName = copyStringToNative(parser->slh,parser->headerName,parser->headerNameLength);
  newHeader->value = copyString(parser->slh,parser->headerValue,parser->headerValueLength);
  newHeader->nativeValue = copyStringToNative(parser->slh,parser->headerValue,parser->headerValueLength);


  /* pull out enough data for parsing the entity body */
  if (!compareIgnoringCase(newHeader->nativeName,"Transfer-Encoding",parser->headerNameLength)){
    if (!compareIgnoringCase(newHeader->nativeValue,"chunked",parser->headerValueLength)){
      parser->isChunked = TRUE;
    }
  } else if (!compareIgnoringCase(newHeader->nativeName,"Content-Length",parser->headerNameLength)){
    parser->specifiedContentLength = atoi(newHeader->nativeValue);
  } else if (!compareIgnoringCase(newHeader->nativeName,"Content-Type",parser->headerNameLength)){
    parser->contentType = newHeader->nativeValue;
  } else if (!compareIgnoringCase(newHeader->nativeName,"Upgrade",parser->headerNameLength)){
    if (!compareIgnoringCase(newHeader->nativeValue,"websocket",parser->headerValueLength)){
      parser->isWebSocket = TRUE;
    }
  }
  else if (!compareIgnoringCase(newHeader->nativeName, "Connection",
                                  parser->headerNameLength)) {
    if (!compareIgnoringCase(newHeader->nativeValue, "Keep-Alive",
                             parser->headerValueLength)) {
      parser->keepAlive = TRUE;
    }
  }

  HttpHeader *headerChain = parser->headerChain;
  if (headerChain){
    HttpHeader *prev;
    while (headerChain){
      prev = headerChain;
      headerChain = headerChain->next;
    }
    prev->next = newHeader;
  } else{
    parser->headerChain = newHeader;
  }
}

static void enqueueLastRequest(HttpRequestParser *parser) {
  HttpRequest *newRequest = (HttpRequest*)SLHAlloc(parser->slh,sizeof(HttpRequest));
  newRequest->method = copyString(parser->slh, parser->methodName, parser->methodNameLength);
  newRequest->uri = copyString(parser->slh, parser->uri, parser->uriLength);
  newRequest->headerChain = parser->headerChain;
  newRequest->isWebSocket = parser->isWebSocket;
  newRequest->keepAlive = parser->keepAlive;

  /* adding support for POST data -jph */
  if (-1 != parser->specifiedContentLength) {
    newRequest->contentLength = parser->specifiedContentLength;
    if (0 < newRequest->contentLength) {
      newRequest->contentBody = copyString(parser->slh, parser->content, parser->specifiedContentLength);
    }
  }

  parser->headerChain = NULL;
  if (parser->requestQHead){
    HttpRequest *request = parser->requestQHead;
    while (request->next){  
      request = request->next;
    } 
    request->next = newRequest;
  } else{
    parser->requestQHead = newRequest;
  }
}

HttpRequest *dequeueHttpRequest(HttpRequestParser *parser){
  if (parser->requestQHead){
    HttpRequest *dequeuedRequest = parser->requestQHead;
    parser->requestQHead = parser->requestQHead->next;
    dequeuedRequest->next = NULL;
    return dequeuedRequest;
  } else{
    return NULL;
  }
}

static int resetParserAndEnqueue(HttpRequestParser *parser){
  enqueueLastRequest(parser);
  parser->methodNameLength = 0;
  parser->uriLength = 0;
  parser->versionLength = 0;
  parser->headerNameLength = 0;
  parser->headerValueLength = 0;
  parser->isChunked = FALSE;
  parser->isWebSocket = FALSE;
  parser->specifiedContentLength = -1;
  parser->state = HTTP_STATE_REQUEST_METHOD;
  parser->keepAlive = FALSE;
  return HTTP_SERVICE_SUCCESS;
}

/* returns ANSI status indicating whether the parsed METHOD is valid per HTTP1.1 */
static bool parserMethodIsValid(HttpRequestParser *parser) {
  bool isValid = FALSE;
  char* nativeMethod = getNative(parser->methodName);
  if ((0 == strncmp(nativeMethod, "GET", 3)) ||
      (0 == strncmp(nativeMethod, "POST", 4)) ||
      (0 == strncmp(nativeMethod, "PUT", 3)) ||
      (0 == strncmp(nativeMethod, "HEAD", 4)) ||
      (0 == strncmp(nativeMethod, "DELETE", 6)) ||
      (0 == strncmp(nativeMethod, "TRACE", 5)) ||
      (0 == strncmp(nativeMethod, "OPTIONS", 7)) ||
      (0 == strncmp(nativeMethod, "CONNECT", 7)))
  {
    isValid = TRUE;
  }
  safeFree(nativeMethod, 1 + strlen(nativeMethod));
  nativeMethod = NULL;
  return isValid;
}

/* returns ANSI status indicating whether request processing should proceed.
 * if this returns ANSI_FALSE, we have responded to the browser with a 405
 * response including an Allow header */
// Response is finished if the method is not valid
int shouldContinueGivenAllowedMethods(HttpResponse *response,
                                      char** nativeMethodList, int numMethods) {
  int ansiStatus=0;
  char* nativeRequestMethod = getNative(response->request->method);
  char allowValue[128] = {0};
  do {
    if ((NULL == nativeMethodList) || (0 >= numMethods)) {
      /* NOOP if we weren't given any basis for filterizng */
      ansiStatus = 1;
      break;
    };
    for (int i=0; i < numMethods; i++) {
      if (0 == strcmp(nativeRequestMethod, nativeMethodList[i])) {
        ansiStatus = 1;
        break;
      }
    }
    if (ansiStatus) {
      break;
    }

    /* compose Allow header */
    strcat(allowValue, nativeMethodList[0]);
    for (int i=1; i < numMethods; i++) {
      strcat(allowValue, ", ");
      strcat(allowValue, nativeMethodList[i]);
    }
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Allow", allowValue);
    addIntHeader(response,"Content-Length",0);
    writeHeader(response);
    finishResponse(response); /* noop */

  } while(0);

  if (nativeRequestMethod) {
    safeFree(nativeRequestMethod, 1 + strlen(nativeRequestMethod));
  }
  return ansiStatus;
}

int processHttpFragment(HttpRequestParser *parser, char *data, int len){
  for (int i=0; i<len; i++){
    char c = data[i];
    int isWhitespace = FALSE;
    int isCR = FALSE;
    int isLF = FALSE;
    int isAsciiPrintable = FALSE;
    int isHex = FALSE;
    switch (c){
    case 10:
      isLF = TRUE;
      break;
    case 13:
      isCR = TRUE;
      break;
    default:
      if (c > 1 && c <= 32){
        isWhitespace = TRUE;
      } else if (c > 32 && c < 127){
        isAsciiPrintable = TRUE;
      }

      if ((c >= 48 && c < 58) || (c >= 65 && c < 71) || (c >= 97 && c < 103)) {
        isHex = TRUE;
      }
    }
    if (parser->state >= HTTP_STATE_END_CR_SEEN){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "loop top i=%d c=0x%x wsp=%d cr/lf=%d state=%s\n",
              i,c,isWhitespace,(isCR||isLF),stateNames[parser->state]);
    }
    switch (parser->state){
    case HTTP_STATE_REQUEST_METHOD:
      if (isWhitespace){
        if (parser->methodNameLength < 1){
          parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
          return 0;
        }
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "METHOD_NAME state to GAP1\n");
        if (!parserMethodIsValid(parser)) { /* reject METHODs not defined in HTTP 1.1 */
          parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
          return 0;
        }
        parser->state = HTTP_STATE_REQUEST_GAP1;
      } else if (isAsciiPrintable){
        if (parser->methodNameLength >= MAX_HTTP_METHOD_NAME){
          parser->httpReasonCode = HTTP_STATUS_METHOD_NOT_FOUND;
          return 0;
        } else{
          parser->methodName[parser->methodNameLength++] = c;
        }
      } else{
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_REQUEST_GAP1:
      if (!isWhitespace){
        parser->uriLength = 0;
        parser->uri[parser->uriLength++] = c;
        parser->state = HTTP_STATE_REQUEST_URI;
      } else if (isCR || isLF){
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_REQUEST_URI:
      if (isCR || isLF){ 
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "ReqURI CR/LF\n");
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      } else if (isWhitespace){
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "ReqURI white\n");
        parser->state = HTTP_STATE_REQUEST_GAP2;
      } else{
        parser->uri[parser->uriLength++] = c;
      }
      break;
    case HTTP_STATE_REQUEST_GAP2:
      if (isCR || isLF){ 
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      } else if (isWhitespace){
        /* do nothing */
      } else{
        parser->state = HTTP_STATE_REQUEST_VERSION;
        parser->versionLength = 0;
        parser->version[parser->versionLength++] = c;
      }
      break;
    case HTTP_STATE_REQUEST_VERSION:
      if (isCR){
        parser->state = HTTP_STATE_REQUEST_CR_SEEN;
      } else if (isAsciiPrintable){
        parser->version[parser->versionLength++] = c;
      } else{
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_REQUEST_CR_SEEN:
      if (isLF){
        parser->headerNameLength = 0;
        parser->state = HTTP_STATE_HEADER_FIELD_NAME;
      } else{
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_HEADER_FIELD_NAME:
      if (isCR){
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "field name CR seen: NameLen=%d\n",
                parser->headerNameLength);
        if (parser->headerNameLength == 0){
          parser->state = HTTP_STATE_END_CR_SEEN;
        } else{
          parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
          return 0;
        }
      } else if (c == ASCII_COLON){
        if (parser->headerNameLength == 0){
          parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
          return 0;
        } else{
          parser->state = HTTP_STATE_HEADER_GAP1;
        }
      } else if (isAsciiPrintable){
        parser->headerName[parser->headerNameLength++] = c;
      } else{
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;        
      }
      break;
    case HTTP_STATE_HEADER_GAP1:
      if (isWhitespace){
        /* do nothing */
      } else if (isCR) {
        /* No content and no whitespace */
        parser->state = HTTP_STATE_HEADER_CR_SEEN;
      } else if (isAsciiPrintable){
        parser->headerValueLength = 0;
        parser->headerValue[parser->headerValueLength++] = c;
        parser->state = HTTP_STATE_HEADER_FIELD_CONTENT;
      } else{
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;        
      }
      break;
    case HTTP_STATE_HEADER_FIELD_CONTENT:
      if (isCR){
        parser->state = HTTP_STATE_HEADER_CR_SEEN;
        addRequestHeader(parser);
      } else if (isLF){
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;        
      } else{
        if (parser->headerValueLength < MAX_HTTP_FIELD_VALUE) {
        	parser->headerValue[parser->headerValueLength++] = c;
        } else {
          /* The client is attempting to pass a header value that
           * is larger than we support. Return error 413. */
          parser->httpReasonCode = HTTP_STATUS_ENTITY_TOO_LARGE;
          return 0;
        }
      }
      break;
    case HTTP_STATE_HEADER_GAP2:
      break;
    case HTTP_STATE_HEADER_CR_SEEN:
      if (isLF){
        parser->state = HTTP_STATE_HEADER_FIELD_NAME;
        parser->headerNameLength = 0;
        parser->headerValueLength = 0;
        
      } else{
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;        
      }
      break;
    case HTTP_STATE_END_CR_SEEN:
      if (isLF){
        /* read entity body - if present */
        if (parser->isChunked){
          parser->state = HTTP_STATE_READING_CHUNK_SIZE;
          parser->chunkSize = 0;
          parser->specifiedContentLength = 0;
          parser->content = NULL;
        } else if (parser->specifiedContentLength <= 0){ 
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "____ NO BODY TO READ ______\n");
          resetParserAndEnqueue(parser);
        } else{
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "_____ END OF MESSAGE HEADER _________\n");
          parser->state = HTTP_STATE_READING_FIXED_BODY;
          parser->content = SLHAlloc(parser->slh,parser->specifiedContentLength);
          parser->remainingContentLength = parser->specifiedContentLength;
        }
      } else{
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_READING_FIXED_BODY:
      parser->content[parser->specifiedContentLength-parser->remainingContentLength] = (char)c;
      --(parser->remainingContentLength);
      if (parser->remainingContentLength <= 0){
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "_____ END OF FIXED BODY _________\n");
        resetParserAndEnqueue(parser);
      }
      break;
    case HTTP_STATE_READING_CHUNK_SIZE:
      if (isHex) {
        /* Detect overflow */
        int value;
        if (c >= 48 && c < 58) {
          value = c - 48; /* digit */
        } else if (c >= 65 && c < 71) {
          value = (c - 65) + 10; /* uppercase */
        } else {
          value = (c - 97) + 10; /* lowercase */
        }

        if (parser->chunkSize > (MAX_HTTP_CHUNK - value) / 16) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "MAX CHUNK SIZE EXCEEDED\n");
          parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
          return 0;
        } else {
          parser->chunkSize *= 16;
          parser->chunkSize += value;
        }
      } else if (isCR) {
        parser->state = HTTP_STATE_CHUNK_SIZE_CR_SEEN;
      } else if (c == 59 /* semicolon */) {
        parser->state = HTTP_STATE_READING_CHUNK_EXTENSION;
      } else {
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_READING_CHUNK_EXTENSION:
      if (isCR) {
        parser->state = HTTP_STATE_CHUNK_SIZE_CR_SEEN;
      } else {
        /* throw out extension */
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "Warning: ignoring chunk extension\n");
      }
      break;
    case HTTP_STATE_CHUNK_SIZE_CR_SEEN:
      if (isLF) {
        if (parser->chunkSize == 0) {
          /* terminal chunk */
          parser->state = HTTP_STATE_READING_CHUNK_TRAILER;
        } else {
          parser->contentTmp = parser->content;
          parser->content = SLHAlloc(parser->slh, parser->specifiedContentLength + parser->chunkSize);
          if (parser->content == NULL) {
            /* allocation failure */
            parser->httpReasonCode = 500;
            return 0;
          }
          if (parser->contentTmp != NULL) {
            memcpy(parser->content, parser->contentTmp, parser->specifiedContentLength);
          }
          parser->specifiedContentLength += parser->chunkSize;
          parser->remainingContentLength = parser->chunkSize;
          parser->state = HTTP_STATE_READING_CHUNK_DATA;
        }
      } else {
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_READING_CHUNK_DATA:
      if (parser->remainingContentLength <= 0){
        /* Expect CR */
        if (isCR) {
          parser->state = HTTP_STATE_CHUNK_DATA_CR_SEEN;
        } else {
          parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
          return 0;
        }
      } else {
        parser->content[parser->specifiedContentLength-parser->remainingContentLength] = (char)c;
        --(parser->remainingContentLength);
      }
      break;
    case HTTP_STATE_CHUNK_DATA_CR_SEEN:
      if (isLF) {
        parser->state = HTTP_STATE_READING_CHUNK_SIZE;
        parser->chunkSize = 0;
      } else {
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_READING_CHUNK_TRAILER:
      if (isCR) {
        parser->state = HTTP_STATE_CHUNK_TRAILER_CR_SEEN;
      } else {
        /* TODO */
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "PANIC: unsupported chunk trailers\n");
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    case HTTP_STATE_CHUNK_TRAILER_CR_SEEN:
      if (isLF) {
        resetParserAndEnqueue(parser);
      } else {
        parser->httpReasonCode = HTTP_STATUS_BAD_REQUEST;
        return 0;
      }
      break;
    }
  } 
  return 1;
}


static int proxyServe(HttpService *service,
                      HttpRequest *request,
                      HttpResponse *response){
  HttpConversation *conversation = response->conversation;
  conversation->conversationType = CONVERSATION_HTTP_PROXY;
  printf("proxyServe started, conversation=0x%p\n",conversation);
  HttpRequest *innerRequest = (HttpRequest*)SLHAlloc(response->slh,sizeof(HttpRequest));
  memset(innerRequest,0,sizeof(HttpRequest));
  int transformationStatus = service->requestTransformer(conversation,request,innerRequest);
  if (transformationStatus == HTTP_PROXY_OK){
    Socket *innerSocket = NULL;
    writeRequest(innerRequest,innerSocket);
  }
  return 1;
}

/* SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN 

   a Session Cookie is used 
*/



static char *getCookieValue(HttpRequest *request, char *cookieName){
  HttpHeader *cookieHeader = getHeader(request,"Cookie");
  ShortLivedHeap *slh = request->slh;
  int cookieTextLength = 0;
  int cookieNameLength = 0;
  char *cookieText = NULL;
  if (cookieHeader != NULL){
    cookieText = cookieHeader->nativeValue;
    cookieTextLength = strlen(cookieText);
    cookieNameLength = strlen(cookieName);
  } else{
    return NULL;
  }
  int pos = 0;
  while (pos<cookieTextLength){
    if (isspace(cookieText[pos])) {
      pos++;
      continue;
    }
    int semiPos = indexOf(cookieText,cookieTextLength,';',pos);
    int keyValuePairLength = 0;
    int nextPos = 0;
    if (semiPos == -1){
      keyValuePairLength = cookieTextLength-pos;
      nextPos = cookieTextLength;
    } else{
      keyValuePairLength = semiPos-pos;
      nextPos = semiPos+1;
    }
    int equalsPos = indexOf(cookieText, cookieTextLength, '=', pos);
    if ((equalsPos != -1) &&
        !memcmp(cookieText+pos,cookieName,equalsPos-pos)){
      char *cookieValue = copyString(slh, &cookieText[equalsPos + 1],
          pos + keyValuePairLength - (equalsPos + 1));
      return cookieValue;
    }
    pos = nextPos;
  }
  return NULL;
}

#ifdef __ZOWE_OS_ZOS
int isLowerCasePasswordAllowed(){
  RCVT* rcvt = getCVT()->cvtrac;
  return (RCVTFLG3_BIT_RCVTPLC & (rcvt->rcvtflg3));
  /* if lower-case pw allowed */
}
#else
int isLowerCasePasswordAllowed(){
  return TRUE;
}
#endif

#ifdef __ZOWE_OS_ZOS

/*
  MD5, although not a great algorithm anymore, is used because
  this is being used to validate a short string (userid.len+password.len).
  There is little to no chance of collision attacks on short strings.  This
  function is only used in zowe single-user mode.  Single-user mode does not
  open up network ports.  It only opens to SSH connections for customers that
  have valid SAF credentials, and only allows them to execute code as their
  own ID.
  */

#define MD5_LENGTH 16

static char *md5It(char *s, int len, char *hash){
  ICSFDigest digest;
  memset(hash,0,MD5_LENGTH);
  icsfDigestInit(&digest,ICSF_DIGEST_MD5);
  icsfDigestUpdate(&digest,s,len);
  icsfDigestFinish(&digest,hash);
  return hash;
}

static ACEE *getACEE(){
  TCB *tcb = getTCB();
  ACEE *acee = NULL;
  if (tcb->tcbsenv){
    acee = (ACEE*)tcb->tcbsenv;
  } else{
    ASCB *ascb = getASCB();
    ASXB *asxb = ascb->ascbasxb;
    acee = (ACEE*)asxb->asxbsenv;
  }
  return acee;
}

static int singleUserTrace = 1;

#define MAX_USERID_LENGTH 32 /* very generous to be forward compatible */
#define MAX_PASSWORD_LENGTH 64 /* very generous to be forward compatible */
#define AUTH_BUFFER_LENGTH (MAX_USERID_LENGTH + MAX_PASSWORD_LENGTH + 32)

static int doSingleUserAuth(HttpServer *server, HttpRequest *request, AuthResponse *authResponse){
  if (singleUserTrace >= 1){
    printf("JOE: single-user auth\n");
  }
  ACEE *acee = getACEE();
  HttpHeader *authenticationHeader = getHeader(request,"Authorization");
  /* the basic auth blob cam come from different places, it's in JSON in a /login
     type request, but in Auth/Basic in other places */
  if (singleUserTrace >= 1){
    printf("JOE: 1U u: '%s' p: '%s' authHeader=0x%p\n",request->username,request->password,authenticationHeader);
  }
  char cleartextBuffer[AUTH_BUFFER_LENGTH];
  char asciiHeaderBuffer[AUTH_BUFFER_LENGTH];
  char *asciiHeader = NULL;
  if (authenticationHeader){
    asciiHeader = authenticationHeader->value;
  } else{
    if (strlen(request->username) > MAX_USERID_LENGTH || strlen(request->password) > MAX_PASSWORD_LENGTH){
      printf("username or password too long\n");
      return 0;
    }
    snprintf(cleartextBuffer,AUTH_BUFFER_LENGTH,"Basic %s:%s",request->username,request->password);
    strupcase(cleartextBuffer+6);
    if (singleUserTrace >= 1){
      printf("JOE: before ascii and B64 '%s'\n",cleartextBuffer);
    }
    e2a(cleartextBuffer,strlen(cleartextBuffer));
    if (singleUserTrace >= 1){
      printf("JOE: as ascii\n");
      dumpbuffer(cleartextBuffer,strlen(cleartextBuffer));
    }
    asciiHeader = asciiHeaderBuffer;
    int encodedSize = 0;
    encodeBase64NoAlloc(cleartextBuffer+6, strlen(cleartextBuffer)-6, asciiHeader+6,&encodedSize,FALSE);
    encodedSize += 6;
    memcpy(asciiHeader,cleartextBuffer,6);
    asciiHeader[encodedSize] = 0;
    if (singleUserTrace >= 1){
      printf("JOE: Built ascii header\n");
      dumpbuffer(asciiHeader,encodedSize+1);
    }
  }
  char md5Hash[MD5_LENGTH];
  md5It(asciiHeader,strlen(asciiHeader),md5Hash);
  strupcase(request->username); /* upfold username */
  char *safUser = acee->aceeuser;
  int safUserLength = safUser[0];
  safUser++;
  if (singleUserTrace >= 1){
    printf("JOE: single-user requestUName: '%s' ACEE uname: '%*.*s'\n",
           request->username,safUserLength,safUserLength,safUser);
    dumpbuffer(asciiHeader,strlen(asciiHeader));
    printf("and blob hash\n");
    dumpbuffer(md5Hash,MD5_LENGTH);
    printf("and singleUserAuthBlob at 0x%p\n",server->singleUserAuthBlob);
    dumpbuffer(server->singleUserAuthBlob,MD5_LENGTH);
  }
  /*
     do we need this stuff? and for what?
     authResponse->type = AUTH_TYPE_RACF;
     authResponse->responseDetails.safStatus = 0;
   */
  return ((strlen(request->username) == safUserLength) &&           /* Same User as ACEE */
          !memcmp(request->username,safUser,safUserLength) &&       /* Same User as ACEE */
	  !memcmp(server->singleUserAuthBlob,md5Hash,MD5_LENGTH));  /* and the Same Blob  */
}

static int safAuthenticate(HttpService *service, HttpRequest *request, AuthResponse *authResponse){
  int safStatus = 0, racfStatus = 0, racfReason = 0;
  int options = VERIFY_CREATE;
  int authDataFound = FALSE;

  if (request->username != NULL) {
     size_t usernameLen = strlen(request->username);
     if ((usernameLen > 0) && !isBlanks(request->username, 0, usernameLen)) {
       authDataFound = TRUE;
     }
  }
  if (traceAuth){
    printf("safAutheniticate: authDataFound=%d\n",authDataFound);
  }
  if (authDataFound && service->server->singleUserMode){
    return doSingleUserAuth(service->server,request,authResponse);
  } else if (authDataFound){
    ACEE *acee = NULL;
    strupcase(request->username); /* upfold username */
#ifdef ENABLE_DANGEROUS_AUTH_TRACING
 #ifdef METTLE
    printf("SAF auth for user: '%s'\n", request->username);
 #else
    printf("u: '%s' p: '%s'\n",request->username,request->password);
 #endif
#endif
    if (isLowerCasePasswordAllowed() || isPassPhrase(request->password)) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "mixed-case system or a pass phrase, not upfolding password\n");
      /* don't upfold password */
    } else {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "non-mixed-case system, not a pass phrase, upfolding password\n");
      strupcase(request->password); /* upfold password */
    }

#if APF_AUTHORIZED
    safStatus = safVerify(options,request->username,request->password,&acee,&racfStatus,&racfReason);
#else
    ZISAuthServiceStatus status = {0};

    CrossMemoryServerName *privilegedServerName = getConfiguredProperty(service->server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
    int pwdCheckRC = 0, pwdCheckRSN = 0;
    pwdCheckRC = zisCheckUsernameAndPassword(privilegedServerName,
        request->username, request->password, &status);
    authResponse->type = AUTH_TYPE_RACF;
    authResponse->responseDetails.safStatus = status.safStatus;

    if (pwdCheckRC != 0) {
#ifdef DEBUG_AUTH
#define FORMAT_AUTH_ERROR($fmt, ...) printf("error: zisCheckUsernameAndPassword" \
    $fmt "\n", ##__VA_ARGS__)
    ZIS_FORMAT_AUTH_CALL_STATUS(pwdCheckRC, &status, FORMAT_AUTH_ERROR);
#undef FORMAT_AUTH_ERROR
#endif /* DEBUG_AUTH */
      return FALSE;
    }
    safStatus = status.safStatus.safRC;
#endif /* APF_AUTHORIZED */

#ifdef DEBUG_AUTH
    printf("saf status = 0x%x racfStatus=0x%x racfReason=0x%x\n",safStatus,racfStatus,racfReason);
    fflush(stdout);
#endif
    return (safStatus == 0); /* SAF status is mainframe status 0 good, others warning or error */
  } 
  return FALSE;
}
#else

static int safAuthenticate(HttpService *service, HttpRequest *request, AuthResponse *authResponse){
  printf("*** ERROR **** calling safAuth off-mainframe\n");
  return FALSE;
}
#endif

static int nativeAuth(HttpService *service, HttpRequest *request, AuthResponse *authResponse){
#ifdef __ZOWE_OS_ZOS
  int retValue = safAuthenticate(service, request, authResponse);
  return retValue;
#else
#ifdef DEBUG_AUTH
  printf("*** ERROR *** native auth not implemented for this platform\n");
#endif
  return TRUE;
#endif
}

static int startImpersonating(HttpService *service, HttpRequest *request) {
  if (service->server->singleUserMode){
    /* Why is this valid?
       Because all requests must be from the same userid in request as
       what is running this httpserver, therefore impersonation is idempotent.
       */
    if (singleUserTrace >= 1){
      printf("JOE - idem-personating (ha!) in 1U Mode\n");
    }
    return TRUE;
  }

  int impersonating = FALSE;
  if (service->doImpersonation) {
    if (service->runInSubtask) {
#ifdef __ZOWE_OS_ZOS

#if (APF_AUTHORIZED == 1) && !defined(HTTPSERVER_BPX_IMPERSONATION)
      impersonating = tlsImpersonate(request->username, NULL, TRUE, traceAuth) == 0;
#else
      /* WARNING: BPX impersonate requires you application to have UPDATE access to BPX.SERVER and BPX.DEAMON */
      impersonating = bpxImpersonate(request->username, NULL, TRUE, traceAuth) == 0;
#endif /* not APF-authorized or willing to use BPX impersonation */

#else
#ifdef DEBUG_AUTH
      printf("*** ERROR *** impersonation not implemented for this platform\n");
#endif
      impersonating = FALSE;
#endif /*__ZOWE_OS_ZOS */
    } else {
#ifdef DEBUG_AUTH
      printf("*** ERROR *** impersonation couldn't be done because service %s didn't run in subtask \n", service->name);
#endif
      impersonating = FALSE;
    }
  } else {
    impersonating = FALSE;
  }
  return impersonating;
}

static int endImpersonating(HttpService *service, HttpRequest *request) {
  if (service->server->singleUserMode){
    return TRUE;
  }
#ifdef __ZOWE_OS_ZOS

#if (APF_AUTHORIZED == 1) && !defined(HTTPSERVER_BPX_IMPERSONATION)
  return tlsImpersonate(request->username, NULL, FALSE, traceAuth) == 0;
#else
  /* WARNING: BPX impersonate requires you application to have UPDATE access to BPX.SERVER and BPX.DEAMON */
  return bpxImpersonate(request->username, NULL, FALSE, traceAuth) == 0;
#endif /* not APF-authorized or willing to use BPX impersonation */

#else
#ifdef DEBUG_AUTH
  printf("*** ERROR *** impersonation not implemented for this platform\n");
#endif
  return TRUE;
#endif /*__ZOWE_OS_ZOS */
}

static int isImpersonationValid(HttpService *service, int isImpersonating) {

  if (!isImpersonating != !service->doImpersonation) {
    return FALSE;
  }

  return TRUE;
}

static void reportImpersonationError(HttpService *service, int isImpersonating) {

  if (!isImpersonating && service->doImpersonation) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_SEVERE,
            "Error: service has no impersonation; make sure process user has "
            "sufficient authority:\n"
            "  z/OS: program control flag must be set, UPDATE access to "
            "BPX.SERVER and BPX.DAEMON SAF resources is required\n"
            "  Other platforms: impersonation is not supported\n");
    return;
  }

  if (isImpersonating && !service->doImpersonation) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_SEVERE,
            "Error: service runs with impersonation when none is required\n");
    return;
  }

}

int extractBasicAuth(HttpRequest *request, HttpHeader *authHeader){
  ShortLivedHeap *slh = request->slh;
  char *asciiHeader = authHeader->value;
  char *ebcdicHeader = authHeader->nativeValue;
  int headerLength = strlen(asciiHeader);
  if (traceAuth){
    AUTH_TRACE("authHeader(A): \n");
    AUTH_DUMPBUF(asciiHeader,strlen(asciiHeader));
    AUTH_TRACE("authHeader(E): \n");
    AUTH_DUMPBUF(ebcdicHeader,strlen(ebcdicHeader));
  }
  if (!strncmp(ebcdicHeader+0,"Basic ",6)){
    int authStart = 6;
    int authEnd = authStart;
    int authLen;
    int decodedLength;
    char *encodedAuthString = NULL;
    char *authString = NULL;
    AUTH_TRACE("start authEnd loop\n");
    while ((authEnd < headerLength) && (ebcdicHeader[authEnd] > 0x041)){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "authEnd=%d\n",authEnd);
      authEnd++;
    }
    authLen = authEnd-authStart;
    encodedAuthString = SLHAlloc(slh,authLen+1);
    authString = SLHAlloc(slh,authLen+1);
    memcpy(encodedAuthString,ebcdicHeader+authStart,authLen);
    encodedAuthString[authLen] = 0;
    DANGEROUS_TRACE("encoded auth string\n");
    decodedLength = decodeBase64(encodedAuthString,authString);
    authString[decodedLength] = 0;
    if (FALSE) {
      DANGEROUS_TRACE("decoded base 64, no unascify %s, len=%d\n",
          authString,
          strlen(authString));
      DANGEROUS_DUMPBUF(authString,strlen(authString));
    }
    destructivelyUnasciify(authString); /* don't upfold case */
    DANGEROUS_TRACE("encoded auth '%s' decoded '%s'\n",encodedAuthString,authString);
    char *password = SLHAlloc(slh,decodedLength);
    int colonPos = indexOf(authString,decodedLength,':',0);
    if (colonPos){
      request->username = authString;
      authString[colonPos] = 0;
      memcpy(password,authString+colonPos+1,decodedLength-colonPos+1);
      request->password = password;
      strupcase(request->username); /* upfold username */
      if (isLowerCasePasswordAllowed() || isPassPhrase(request->password)) {
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "mixed-case system or a pass phrase, not upfolding password\n");
        /* don't upfold password */
      } else {
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "non-mixed-case system, not a pass phrase, upfolding password\n");
        strupcase(request->password); /* upfold password */
      }
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "returning TRUE from extractBasicAuth\n");
      return TRUE;
    } else{
      AUTH_TRACE("no colon seen in basic auth string, returning FALSE\n");
      return FALSE;
    }
  } else{
    AUTH_TRACE("Non-basic auth\n");
    return FALSE;
  }

}

int extractBearerToken(HttpRequest *request, HttpHeader *authHeader) {
  const char *const asciiHeader = authHeader->value;
  const char *const ebcdicHeader = authHeader->nativeValue;
  const int headerLength = strlen(asciiHeader);

  AUTH_TRACE("authHeader(A): \n");
  AUTH_DUMPBUF(asciiHeader, strlen(asciiHeader));
  AUTH_TRACE("authHeader(E): \n");
  AUTH_DUMPBUF(ebcdicHeader, strlen(ebcdicHeader));

  if (strncmp(ebcdicHeader, "Bearer ", 7) != 0) {
    AUTH_TRACE("Non-bearer auth\n");
    return FALSE;
  }

  const int tokenStart = 7;
  int tokenEnd = tokenStart;
  AUTH_TRACE("start tokenEnd loop\n");
  while ((tokenEnd < headerLength) && (ebcdicHeader[tokenEnd] > 0x041)){
    tokenEnd++;
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "tokenEnd=%d\n", tokenEnd);
  }
  const int tokenLen = tokenEnd - tokenStart;
  AUTH_TRACE("bearer token length = %d\n", tokenLen);

  char *const tokenString = SLHAlloc(request->slh, tokenLen + 1);
  AUTH_TRACE("bearer token buffer at 0x%p\n", tokenString);
  if (tokenString == NULL) {
    return FALSE;
  }
  memcpy(&tokenString[0], &ebcdicHeader[tokenStart], tokenLen);
  tokenString[tokenLen] = 0;

  /* choosing *very special* ifdef so we never accidentally ship an
   * executable that can be persuaded to print out passwords
   */

  request->authToken = tokenString;
  return TRUE;
}

#ifdef __ZOWE_OS_ZOS

#define ONE_SECOND (4096*1000000)    /* one second in STCK */

static int64 getFineGrainedTime(){
  int64 stck = 0;
  unsigned long long outSeconds = 0;
  __asm(ASM_PREFIX
        " STCK %0 "
        :
        "=m"(stck)
        :
        : "r15");

  return stck;
}
#else

#define ONE_SECOND 1     /* unix 32 bit time is usually in integral seconds */

static int64 getFineGrainedTime(){
  time_t rawtime;

  time ( &rawtime );
  if (sizeof(time_t) == 4){
    return ((int64)rawtime)<<32;
  } else{
    return (int64)rawtime;
  }
}
#endif

#define SESSION_VALIDITY_IN_SECONDS 3600

//validitySec 0=not found, -1=no expiration, positive int=session in seconds
static int getGroupSessionValidity(int groupId, const HttpServerConfig *config,
                                  int *validitySec, int *returnCode, int *reasonCode) {
  int retVal = 0;
  if (config->groupTimeouts) {
    *validitySec = INT_FROM_POINTER(htGet(config->groupTimeouts, POINTER_FROM_INT(groupId)));
    AUTH_TRACE("exp=%d for gid=%d\n",*validitySec, groupId);
    if (*validitySec){
      return 0;
    }
    retVal = -1;
  } else {
    retVal = -1;
  }
  return retVal;
}

//validitySec 0=not found, -1=no expiration, positive int=session in seconds
static int getUserSessionValidity(char *username, const HttpServerConfig *config,
                                  int *validitySec, int *returnCode, int *reasonCode) {
  strupcase(username);   /* upfold username */
  if (config->userTimeouts) {
    *validitySec = INT_FROM_POINTER(htGet(config->userTimeouts, (void*)username));
    AUTH_TRACE("user validitySec found=%d\n",*validitySec);
    if (*validitySec){
       return 0;
    }
  }
  if (!*validitySec) {
    int groupCount = 0;
    int retVal = getGroupList(username, NULL, &groupCount, returnCode, reasonCode);
    if (retVal) {
      return retVal;
    }
    int *groups = (int*)safeMalloc(sizeof(int) * groupCount, "groups");
    retVal = getGroupList(username, groups, &groupCount, returnCode, reasonCode);
    if (!retVal) {
      int currentValiditySec = 0;
      for (int i = 0; i < groupCount; i++) {
        retVal = getGroupSessionValidity(groups[i], config, &currentValiditySec, returnCode, reasonCode);
        if (!retVal) {
          if (currentValiditySec && *validitySec != -1 && ((currentValiditySec == -1) || (currentValiditySec > *validitySec))) {
            *validitySec = currentValiditySec;
            AUTH_TRACE("longer session duration=%d\n",*validitySec);
          }
        }
      }
      if (!*validitySec){
        //the default
        *validitySec= config->defaultTimeout ? config->defaultTimeout : SESSION_VALIDITY_IN_SECONDS;
      }
      retVal = 0;
    }
    safeFree((char*)groups, sizeof(int) * groupCount);
    return retVal;
  } else {
    return 0;
  }
}

static int sessionTokenStillValid(HttpService *service, HttpRequest *request, char *sessionTokenText, int *sessionValiditySec, uint64 *sessionTimeRemaining){
  HttpServer *server = service->server;
  ShortLivedHeap *slh = request->slh;
  char *decodedData = SLHAlloc(slh,strlen(sessionTokenText));
  int decodedDataLength = decodeBase64(sessionTokenText,decodedData);

  AUTH_TRACE("decoded session token data for text %s\n",sessionTokenText);
  AUTH_DUMPBUF(decodedData,decodedDataLength);

  char *plaintextSessionToken = NULL;
  int decodeRC = decodeSessionToken(slh, server->config,
                                    decodedData,
                                    decodedDataLength,
                                    &plaintextSessionToken);
  if (decodeRC != 0) {
    return FALSE;
  }

  int colonPos = indexOf(plaintextSessionToken, decodedDataLength, ':', 0);
  if (traceAuth){
    AUTH_TRACE("colon pos %d;decoded data token:\n",colonPos);
    AUTH_DUMPBUF(decodedData,decodedDataLength);
    AUTH_TRACE("EBCDIC session token:\n");
    AUTH_DUMPBUF(plaintextSessionToken,decodedDataLength);
  }
  if (colonPos == -1){
    return FALSE;
  }

  int colonPos2 = indexOf(plaintextSessionToken, decodedDataLength, ':', colonPos+1);
  AUTH_TRACE("colon pos2 %d;\n",colonPos2);
  if (colonPos2 == -1){
    return FALSE;
  }
  
  char *username = SLHAlloc(slh, colonPos+1);
  memcpy(username, plaintextSessionToken, colonPos);
  username[colonPos]=0;
  strupcase(username);

  uint64 decodedTimestamp= strtoull(plaintextSessionToken+colonPos+1, NULL, 16);
  uint64 serverInstanceUID = strtoull(plaintextSessionToken+colonPos2+1, NULL, 16);
  uint64 now = getFineGrainedTime();
  //determined by lookup or default
  int reasonCode = 0;
  int returnCode = 0;

  int retVal = getUserSessionValidity(username, server->config, sessionValiditySec, &returnCode, &reasonCode);
  AUTH_TRACE("got secs=%d for user=%s, retv=%d, rc=%d, rsn=%d\n",*sessionValiditySec,username,retVal, returnCode, reasonCode);
  if (retVal) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_WARNING, "Error when getting session duration for user '%s'. rc=%d, rsn=%d\n",
            username, returnCode, reasonCode);
  }
  AUTH_TRACE("session validity sec = %d\n",*sessionValiditySec);
  uint64 interval = ((uint64)(*sessionValiditySec))*ONE_SECOND;

  if (serverInstanceUID != service->serverInstanceUID){
    AUTH_TRACE("token from other server, returning FALSE\n");
    return FALSE;
  }

  if (*sessionValiditySec > 0) {
    uint64 difference = now-decodedTimestamp;
    *sessionTimeRemaining = difference;

    AUTH_TRACE("decodedTimestamp=%llx;now=%llx;difference=%llx;interval=%llx;tokenUID=%llx;serverUID=%llx\n",
               decodedTimestamp,now,difference,interval, serverInstanceUID, service->serverInstanceUID);

    if (difference > interval){
      AUTH_TRACE("returning FALSE\n");
      return FALSE;
    }
  }

  request->username = username;

  AUTH_TRACE("returning TRUE\n");
  return TRUE;
}

static char *getSessionTokenCookieName(HttpService *service) {
  return service->server->cookieName;
}

static char *generateSessionTokenKeyValue(HttpService *service, HttpRequest *request, char *username){
  HttpServer *server = service->server;
  ShortLivedHeap *slh = request->slh;
  char *tokenPlaintextBuffer = SLHAlloc(slh,528); /* started at 512, and I added a STCK. May be overkill, but I felt the number needed an explanation */
  /* NOTE: could add randomness in addition to getFineGrainedTime() */
  int tokenPlaintextLength = sprintf(tokenPlaintextBuffer,"%s:%llx:%llx",username,getFineGrainedTime(),service->serverInstanceUID);

  char *tokenCiphertext = NULL;
  int encodeRC = encodeSessionToken(slh, server->config,
                                    tokenPlaintextBuffer,
                                    tokenPlaintextLength,
                                    &tokenCiphertext);
  if (encodeRC != 0) {
    return NULL;
  }

  int encodedLength = 0;
  char *base64Output = encodeBase64(slh,tokenCiphertext,tokenPlaintextLength,&encodedLength,TRUE);

  char *cookieName = getSessionTokenCookieName(service);

#ifdef INSECURE_COOKIE
  int keyValueBufferSize = encodedLength + strlen(cookieName) + 16; //16 for trailing ; Path=/ inclusion
  char *keyValueBuffer = SLHAlloc(slh, keyValueBufferSize);
  snprintf(keyValueBuffer, keyValueBufferSize, "%s=%s; Path=/", cookieName, base64Output);
#else
  int keyValueBufferSize = encodedLength + strlen(cookieName) + 40; //40 for cookie properties
  char *keyValueBuffer = SLHAlloc(slh, keyValueBufferSize);
  snprintf(keyValueBuffer, keyValueBufferSize, "%s=%s; Path=/; HttpOnly; SameSite=Strict", cookieName, base64Output);
#endif
  return keyValueBuffer;
}

static void logTimeoutLookupError(const char *username, const int rc, const int rsn){
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_WARNING,
          "serviceAuthNativeWithSessionToken: Error when getting session duration for user '%s'. rc=%d, rsn=%d\n",
          username, rc, rsn);
}

static int serviceAuthNativeWithSessionToken(HttpService *service, HttpRequest *request,  HttpResponse *response,
                                             int *clearSessionToken, AuthResponse *authResponse){
  int authDataFound = FALSE; 
  HttpHeader *authenticationHeader = getHeader(request,"Authorization");
  char *tokenCookieText = getCookieValue(request,getSessionTokenCookieName(service));
  
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
          "serviceAuthNativeWithSessionToken: authenticationHeader 0x%p\n",
          "extractFunction 0x%p\n",
          authenticationHeader,
          service->authExtractionFunction);
  
  if (authenticationHeader) {
    if (extractBasicAuth(request,authenticationHeader)){
      authDataFound = TRUE;
    }
  } else {
    if (service->authExtractionFunction != NULL){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
             "serviceAuthNativeWithSessionToken: authExtractionFunction 0x%p\n",
             service->authExtractionFunction);
             
      if (service->authExtractionFunction(service, request) == 0){
        authDataFound = TRUE;
      }
    }
  }
  
  response->sessionCookie = NULL;

  AUTH_TRACE("AUTH: tokenCookieText: %s\n",(tokenCookieText ? tokenCookieText : "<noAuthToken>"));
  int returnCode = 0;
  int reasonCode = 0;
  int retVal = 0;
  if (tokenCookieText){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
           "serviceAuthNativeWithSessionToken: tokenCookieText: %s\n",
           (tokenCookieText ? tokenCookieText : "<noAuthToken>"));
    uint64 timeRemainingStck = 0;
    int sessionLengthSec = 0;
    if (sessionTokenStillValid(service,request,tokenCookieText,&sessionLengthSec,&timeRemainingStck)){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
              "serviceAuthNativeWithSessionToken: Cookie still good, renewing cookie\n");
      char *sessionToken = generateSessionTokenKeyValue(service, request,request->username);
      if (sessionToken == NULL){
        return FALSE;
      }
      response->sessionTimeout = sessionLengthSec;
      response->sessionCookie = sessionToken;
      return TRUE;
    } else if (authDataFound){
      if (nativeAuth(service,request,authResponse)){
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
               "serviceAuthNativeWithSessionToken: Cookie not valid, auth is good\n");
        char *sessionToken = generateSessionTokenKeyValue(service,request,request->username);
        retVal = getUserSessionValidity(request->username, service->server->config,
                                      &response->sessionTimeout, &returnCode, &reasonCode);
        if (retVal) {
          logTimeoutLookupError(request->username, returnCode, reasonCode);
          return FALSE;
        }
        response->sessionCookie = sessionToken;
        return TRUE;
      } else{
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
                 "serviceAuthNativeWithSessionToken: Cookie not valid, auth is bad\n");
        /* NOTES: CLEAR SESSION TOKEN */
        response->sessionCookie = "non-token";
        return FALSE;
      }
    } else{
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
                "serviceAuthNativeWithSessionToken: Cookie not valid, no auth provided\n");
      /* NOTES: CLEAR SESSION TOKEN */
      response->sessionCookie = "non-token";
      return FALSE;
    }
  } else if (authDataFound){
    if (nativeAuth(service,request,authResponse)){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
              "serviceAuthNativeWithSessionToken: auth header provided and works, "
              "before generate session token req=0x%p, username=0x%p, response=0x%p\n",
              request,request->username,response);

      char *sessionToken = generateSessionTokenKeyValue(service,request,request->username);
      retVal = getUserSessionValidity(request->username, service->server->config,
                                      &response->sessionTimeout, &returnCode, &reasonCode);
      if (retVal) {
        logTimeoutLookupError(request->username, returnCode, reasonCode);
        return FALSE;
      }
      response->sessionCookie = sessionToken;
      return TRUE;
    } else{
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
               "serviceAuthNativeWithSessionToken: No cookie, but auth header, which wasn't good\n");

      return FALSE;
    }
  } else{    /* neither cookie */
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3,
           "serviceAuthNativeWithSessionToken: Neither cookie nor auth header\n");

    return FALSE;
  }
}

#define JWT_COOKIE_NAME "apimlAuthenticationToken"
#define JWT_COOKIE_NAME_LENGTH 24

static int serviceAuthWithJwt(HttpService *service,
                              HttpRequest *request,
                              HttpResponse *response) {
  HttpHeader *const authorizationHeader = getHeader(request, "Authorization");
  char *jwtTokenText = getCookieValue(request,JWT_COOKIE_NAME);

  AUTH_TRACE("serviceAuthWithJwt: authenticationHeader 0x%p,"
      " extractFunction 0x%p\n",
      authorizationHeader,
      service->authExtractionFunction);

   request->authToken = jwtTokenText;

  /*
   * The extractor should look at the request and get the raw JWT from anywhere
   * in the request it thinks it can find it (Authorization: Bearer, cookie, URL,
   * etc) and store it in  request->authToken.
   *
   * Then we *replace* request->authToken with a parsed JWT structure, so that
   * the service handler can find the JWT there if it needs it.
   *
   * TODO: consider changing the architecture: what fields do we want in the
   * request structure?
   */

  if (authorizationHeader) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "serviceAuthWithJwt: auth header = 0x%p\n", authorizationHeader);
    if (request->authToken == NULL && extractBearerToken(request, authorizationHeader)) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "back inside serviceAuthWithJwt after call to extractBearerToken\n");
    }
  } else if (service->authExtractionFunction != NULL) {
    if (service->authExtractionFunction(service, request) == 0) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "auth extraction function returns 0!\n");
    }
  }

  AUTH_TRACE("serviceAuthWithJwt: request->authToken %p\n", request->authToken);
  if (request->authToken == NULL) {
    return FALSE;
  }

  JwtContext *const jwtContext = service->server->config->jwtContext;
  if (jwtContext == NULL) {
    return FALSE;
  }

  int jwtRc = 0;
  const Jwt *const jwt = jwtVerifyAndParseToken(
      jwtContext,
      request->authToken,
      true,
      request->slh,
      &jwtRc);
  AUTH_TRACE("serviceAuthWithJwt: jwtContext %p\n", jwtContext);
  if (jwtRc != RC_JWT_OK) {
    return FALSE;
  }

  if (!jwtAreBasicClaimsValid(jwt, NULL)) {
    AUTH_TRACE("serviceAuthWithJwt: basic claims invalid\n");
    return FALSE;
  }

  request->authToken = jwt;

  if (service->authValidateFunction != NULL) {
    return service->authValidateFunction(service, request);
  } else {
    if (jwt->subject == NULL) {
      return FALSE;
    }
    request->username = SLHAlloc(request->slh, 1 + strlen(jwt->subject));
    if (request->username == NULL) {
      return FALSE;
    }
    strcpy(request->username, jwt->subject);
    int jwtLen = strlen(jwtTokenText);
    int jwtCookieLen = JWT_COOKIE_NAME_LENGTH + jwtLen + 2; //+1 for '=', +1 for null term
    char jwtCookie[jwtCookieLen];
    snprintf(jwtCookie, jwtCookieLen, "%s=%s", JWT_COOKIE_NAME, jwtTokenText);
    addStringHeader(response, "Set-Cookie", jwtCookie);
    response->sessionCookie = NULL;
    char *sessionToken = generateSessionTokenKeyValue(service,request,request->username);
    response->sessionCookie = sessionToken;
    addStringHeader(response, "Set-Cookie", response->sessionCookie);
    strupcase(request->username);
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "TOKEN VALIDATED\n");
    return TRUE;
  }
}

int httpServerInitJwtContext(HttpServer *self,
                             bool legacyFallback,
                             const char *pkcs11TokenName,
                             const char *keyName,
                             int keyType,
                             int *makeContextRc, int *p11Rc, int *p11Rsn) {
  JwtContext *const context = makeJwtContextForKeyInToken(
      pkcs11TokenName,
      keyName,
      keyType,
      makeContextRc,
      p11Rc,
      p11Rsn);

  AUTH_TRACE("jwt context for %s:%s: rc %d, context at %p\n",
      pkcs11TokenName, keyName,
      *makeContextRc, context);

  if (*makeContextRc != RC_JWT_OK) {
    return 1;
  }
  self->config->jwtContext = context;
  self->config->authTokenType = legacyFallback?
      SERVICE_AUTH_TOKEN_TYPE_JWT_WITH_LEGACY_FALLBACK
      : SERVICE_AUTH_TOKEN_TYPE_JWT;
  return 0;
}

int httpServerInitJwtContextCustom(HttpServer *self,
                                   bool legacyFallback,
                                   JwtCheckSignature checkSignatureFn,
                                   void *userData,
                                   int *makeContextRc) {
  JwtContext *const context = makeJwtContextCustom(checkSignatureFn, userData, makeContextRc);
  AUTH_TRACE("jwt context custom for checkSignatureFn 0x%p userData 0x%p: rc %d, context at %p\n",
             checkSignatureFn, userData, *makeContextRc, context);
  if (*makeContextRc != RC_JWT_OK) {
    return 1;
  }
  self->config->jwtContext = context;
  self->config->authTokenType = legacyFallback?
      SERVICE_AUTH_TOKEN_TYPE_JWT_WITH_LEGACY_FALLBACK
      : SERVICE_AUTH_TOKEN_TYPE_JWT;
  return 0;
}


#ifdef __ZOWE_OS_ZOS

typedef struct HTTPServiceABENDInfo_tag {
  char eyecatcher[8]; /* RSHTTPAI */
  int completionCode;
  int reasonCode;
} HTTPServiceABENDInfo;

static void extractABENDInfo(RecoveryContext * __ptr32 context, SDWA * __ptr32 sdwa, void * __ptr32 userData) {
  HTTPServiceABENDInfo *info = (HTTPServiceABENDInfo *)userData;
  recoveryGetABENDCode(sdwa, &info->completionCode, &info->reasonCode);
}

#endif

static WorkElementPrefix *makeConsiderCloseConversationWorkElement(HttpConversation *conversation);

static void serializeConsiderCloseEnqueue(HttpConversation *conversation, int subtaskTerm) {

  HttpConversationSerialize compare;

  HttpConversationSerialize replace;

  // Get a compare copy of the conversation serialized data
  memcpy(&compare,&conversation->serializedData,sizeof(compare));

  do {
    // Replicate the compare copy to a replace copy
    memcpy(&replace,&compare,sizeof(replace));
    if (subtaskTerm)            // If this is during subtask termination
      replace.runningTasks--;   // Decrement runningTasks

    if (!replace.runningTasks &&  // If a subtask is not active (or the
                                // subtask is terminating)
        replace.shouldClose &&  // And we should close the conversation
        !replace.considerCloseEnqueued) // And a close has not already
                                // been enqueued
      replace.considerCloseEnqueued = TRUE; // Indicate one has been

    // No update is needed if nothing has changed in the replace data
    if (!memcmp(&compare,&replace,sizeof(compare)))
      break;

    // Need to update the serialized data
#ifdef __ZOWE_OS_ZOS
    if (!cs(&compare.serializedData,      
            &conversation->serializedData,
            replace.serializedData)) {
      break;                    // Successful when function returns 0
    }
#elif defined __GNUC__ || defined __ZOWE_OS_AIX
    if (__sync_bool_compare_and_swap(&conversation->serializedData,
                                     compare.serializedData,replace.serializedData))
      break;                    // Successful when function returns 1
    // Refresh the compare copy of the conversation serialized data
    memcpy(&compare,&conversation->serializedData,sizeof(compare));
#elif defined __ZOWE_OS_WINDOWS
    if (atomic_compare_exchange_strong(&conversation->serializedData,
                                       (unsigned int *)&compare.serializedData,
                                       replace.serializedData))
      break;
#else
  #error Unsupported platform for atomic operation
#endif
  } while(1);

  // If considerCloseEnqueued was changed, enqueue a considerClose request
  if (!compare.considerCloseEnqueued && replace.considerCloseEnqueued)
    stcEnqueueWork(conversation->server->base,
      makeConsiderCloseConversationWorkElement(conversation));

  return;
}

static void serveRequest(HttpService* service, HttpResponse* response,
                         HttpRequest* request) {

  if ((SERVICE_TYPE_FILES == service->serviceType) ||
      (SERVICE_TYPE_FILES_SECURE == service->serviceType)) {
    serveFile(service, response);
  } else if (service->serviceType == SERVICE_TYPE_PROXY) {
    proxyServe(service, request, response);
  } else {
    char* serviceArgProblem = NULL;
    if ((serviceArgProblem = processServiceRequestParams(service, response)) != NULL){
      respondWithError(response, 404, serviceArgProblem);
      // Response is finished on return
    } else {
      if (service->serviceType == SERVICE_TYPE_SIMPLE_TEMPLATE) {
        serveSimpleTemplate(service, response);
        // Response is finished on return
      } else {
	/* 
	   bills
	   try normal login and trace with Sean
	 */
        service->serviceFunction(service, response);
      }
    }
  }

}

static void respondWithAuthError(HttpResponse *response, AuthResponse *authResponse) {
  switch (authResponse->type) {
#ifdef __ZOWE_OS_ZOS
    case AUTH_TYPE_RACF:
      if (authResponse->responseDetails.safStatus.safRC == SAF_RC_PASSWORD_EXPIRED &&
          authResponse->responseDetails.safStatus.racfRC == RACF_RC_PASSWORD_EXPIRED) {
        respondWithError(response, HTTP_STATUS_PRECONDITION_REQUIRED, "Password Expired");
      } else {
        respondWithError(response, HTTP_STATUS_UNAUTHORIZED, "Not Authorized");
      }
      break;
#endif
    default:
      respondWithError(response, HTTP_STATUS_UNAUTHORIZED, "Not Authorized");
      break;
  }
}

static int handleServiceFailed(HttpConversation *conversation,
                               HttpService *service,
                               HttpResponse *response) {

  if (service->cleanupFunction != NULL) {
    service->cleanupFunction(service, response);
  }

  /* Save the conversation response working state, and then set it to false */
  int workingOnResponse = conversation->workingOnResponse;
  conversation->workingOnResponse = FALSE;                

  /* Set shouldClose to indicate that the conversation can not continue further */
  conversation->shouldClose = TRUE;

  /* If not running in a subtask and a considerCloseConversation request has not been queued, queue one */
  serializeConsiderCloseEnqueue(conversation,FALSE);

  /* Attempt to prevent a double free of a response block

      In most abends it is unlikely that the response block will have been freed
      before the abend occurred.  In those cases the test below will succeed.  If
      the response block was freed then the test below will fail, and we must not
      attempt to free it again.                                                     

      finishResponse can not be used to cleanup for the response because
      we have already queued the conversation for close processing.  Just
      free the SLH associated with the response.                           */

  if (workingOnResponse == TRUE) {           
    SLHFree(response->slh);  
  }

  return HTTP_SERVICE_FAILED;
}

static int handleHttpService(HttpServer *server,
                             HttpService *service,
                             HttpRequest *request,
                             HttpResponse *response){
#ifdef __ZOWE_OS_ZOS
  HttpConversation *conversation = response->conversation;

  if (conversation->requestCount > JED_HTTP_KEEP_ALIVE_MAX) {
    return handleServiceFailed(conversation, service, response);
  }

  HTTPServiceABENDInfo abendInfo = {"RSHTTPAI", 0, 0};
  int recoveryRC = recoveryPush(service->name,
                                RCVR_FLAG_RETRY | RCVR_FLAG_SDWA_TO_LOGREC | RCVR_FLAG_DELETE_ON_RETRY,
                                NULL, extractABENDInfo, &abendInfo, NULL, NULL);

  /*
     TODO this function needs a new argument about recording program state at time of abend
     Until then, currently recoveryPush will prevent a CEEDUMP
     and if you need one, just comment out recoveryPush and recoveryPop
  */

  if (recoveryRC != RC_RCV_OK) {
    if (recoveryRC == RC_RCV_CONTEXT_NOT_FOUND) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_SEVERE, "httpserver: error running service %s, recovery context not found\n",
          service->name);
    }
    else if (recoveryRC == RC_RCV_ABENDED) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_SEVERE, "httpserver: ABEND %03X-%02X averted when handling %s\n",
          abendInfo.completionCode, abendInfo.reasonCode, service->name);
    }
    else {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_SEVERE, "httpserver: error running service %s unknown recovery code %d\n",
          service->name, recoveryRC);
    }
    return handleServiceFailed(conversation, service, response);
  }
#endif
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "service=%s authType = %d\n",service->name,service->authType);

  service->server = server;

  int clearSessionToken = FALSE;
  AuthResponse authResponse;

  switch (service->authType){
  case SERVICE_AUTH_NONE:
    request->authenticated = TRUE;
    break;
  case SERVICE_AUTH_SAF:
    /* SAF Authentication just checks that user is known at ALL to SAF.
       Additional privilege (Facility Class Profile) checking maybe done later
       or added to the generic SAF support in server.
       */
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "saf auth needed for service %s\n",service->name);
    request->authenticated = safAuthenticate(service, request, &authResponse);
    break;
  case SERVICE_AUTH_CUSTOM:
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "CUSTOM auth not yet supported\n");
    request->authenticated = FALSE;
    break;
  case SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN:
    switch (server->config->authTokenType) {
    case SERVICE_AUTH_TOKEN_TYPE_JWT:
    case SERVICE_AUTH_TOKEN_TYPE_JWT_WITH_LEGACY_FALLBACK:
      request->authenticated = serviceAuthWithJwt(service, request, response);
      if (request->authenticated  ||
          service->server->config->authTokenType
            != SERVICE_AUTH_TOKEN_TYPE_JWT_WITH_LEGACY_FALLBACK) {
        break;
      } /* else fall through */
    case SERVICE_AUTH_TOKEN_TYPE_LEGACY:
      request->authenticated = serviceAuthNativeWithSessionToken(service,request,response,&clearSessionToken, &authResponse);
      break;
    }
    break;
  }
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "service=%s authenticated=%d\n",service->name,request->authenticated);
  if (request->authenticated == FALSE){
    if (service->authFlags & SERVICE_AUTH_FLAG_OPTIONAL) {
      // Allow the service to decide when to respond with HTTP 401
      serveRequest(service, response, request);
    } else {
      respondWithAuthError(response, &authResponse);
    }
    // Response is finished on return
  } else {

    int impersonating = startImpersonating(service, request);

    if (isImpersonationValid(service, impersonating)) {
      serveRequest(service, response, request);
    } else {
      reportImpersonationError(service, impersonating);
      respondWithError(response, HTTP_STATUS_FORBIDDEN,
                       "Impersonation error");
    }

    if (impersonating) {
      endImpersonating(service, request);
    }

  }
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "service=%s auth succeeded\n",service->name);

#ifdef __ZOWE_OS_ZOS
  recoveryPop();
#endif

  return HTTP_SERVICE_SUCCESS;
  /* HERE
     fileService - what about mime guessing
     */
}

HttpConversation *makeHttpConversation(SocketExtension *socketExtension,
                                       HttpServer *server){
  HttpConversation *conversation = (HttpConversation*)safeMalloc31(sizeof(HttpConversation),"HttpConversation");
  memset(conversation,0,sizeof(HttpConversation));
  conversation->conversationType = CONVERSATION_HTTP;
  conversation->parser = makeHttpRequestParser(socketExtension->slh); /* allocates the parser on the SLH */
  conversation->server = server;
  conversation->socketExtension = socketExtension;
  conversation->runningTasks = 0;
  conversation->closeEnqueued = FALSE;
  conversation->requestCount = 0;
  conversation->isKeepAlive = FALSE;
  socketExtension->protocolHandler = conversation;
  socketExtension->moduleID = STC_MODULE_JEDHTTP;
  socketExtension->isServerSocket = FALSE;
  if (socketExtension->readBuffer) {
    conversation->readBuffer = socketExtension->readBuffer;
  }
  return conversation;
}

/* 
   1 make Parse state,
   2 feed buffer at parse state
   3 check some status variables
   - maybe close, maybe continue

   */

static int serviceLoop(Socket *socket){
  int serviceCount = 0;
  int returnCode;
  int reasonCode;
  int shouldClose = FALSE;
  ShortLivedHeap *slh = makeShortLivedHeap(READ_BUFFER_SIZE,16); /* refresh this every N-requests */
  HttpRequestParser *parser = makeHttpRequestParser(slh);
  char *readBuffer = SLHAlloc(slh,READ_BUFFER_SIZE);
  while (1){
    int socketStatus = tcpStatus(socket,0,0,&returnCode,&reasonCode);
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "socketStatus = %d, errno %d reason %d\n",
            socketStatus,returnCode,reasonCode);
    int bytesRead = socketRead(socket,readBuffer,READ_BUFFER_SIZE,&returnCode,&reasonCode);
    if (bytesRead < 1){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "bytesRead=%d, so a problem rc=0x%x, reason=0x%x, socket=0x%X\n",
              bytesRead,returnCode,reasonCode,socket);
      shouldClose = TRUE;
      break;
    } 
    int requestStreamOK = processHttpFragment(parser,readBuffer,bytesRead);
    if (!requestStreamOK){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "some issue with parser status, socket=0x%p\n", socket);
      shouldClose = TRUE;
      break;
    }
    HttpRequest *request = NULL;
    while ((request = dequeueHttpRequest(parser)) != NULL){
      HttpHeader *header;
      HttpResponse *response = makeHttpResponse(request,parser->slh,socket);
      /* parse URI after request and response ready for work, have SLH's, etc */
      parseURI(request);

      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "looking for service for URI %s\n",request->uri);
      header = request->headerChain;
      while (header){
#ifdef DEBUG        
        printf("  %s=%s\n",header->nativeName,header->nativeValue);
#endif
        header = header->next;
      }
      HttpService *service = findHttpService(NULL,request);
      if (service){
        handleHttpService(NULL,service,request,response);
      }
    }
  }
  
  return 0;
}



void parseURI(HttpRequest *request){
  char *uri = request->uri;
  int len = strlen(uri);
  int qPos = indexOf(uri,len,ASCII_QUESTION,0);
  int hPos = indexOf(uri,len,ASCII_HASH,(qPos >= 0 ? qPos : 0));
  char *filePart = uri;
  
  request->queryString = NULL;
  request->queryStringLen = 0;
  request->fragmentIdentifier = NULL;

  if (traceDispatch){
    printf("parseURI start qPos=%d hPos=%d\n",qPos,hPos);
  }
  if ((len == 0) || ((len == 1) && (uri[0] == ASCII_SLASH))){
    if (traceDispatch > 1){
      printf("CASE 1: trivial URL\n");
    }
    filePart = SLHAlloc(request->slh,2);
    filePart[0] = ASCII_SLASH;
    filePart[1] = 0;
  } else if (qPos >= 0){
    filePart = SLHAlloc(request->slh,qPos);
    memcpy(filePart,uri,qPos);
    if (hPos >= 0){
      if (traceDispatch > 1){
	printf("CASE 2 both q and h\n");
      }
      request->queryString = SLHAlloc(request->slh,hPos-qPos);
      request->fragmentIdentifier = uri+hPos+1;
      memcpy(request->queryString,uri+qPos+1,hPos-qPos);
    } else{
      if (traceDispatch > 1){
	printf("CASE 3 q only\n");
      }
      request->queryString = uri+qPos+1;
    }
  } else if (hPos >= 0){
    if (traceDispatch > 1){
      printf("CASE 4: h only\n");
    }
    filePart = SLHAlloc(request->slh,hPos);
    memcpy(filePart,uri,hPos);
    request->fragmentIdentifier = uri+hPos+1;
  } else{
    if (traceDispatch > 1){
      printf("CASE 5: nothing interesting\n");
    }
  }
  if (traceDispatch){
    fflush(stdout);
  }
  request->file = filePart;
  if (request->queryString){
    request->queryStringLen = strlen(request->queryString);
  }
  if (traceDispatch){
    printf("URI Parse Stage 1: file='%s' query='%s' fragment='%s'\n",
	   getNative(filePart),
	   (request->queryString ? getNative(request->queryString) : "<N/A>"),
	   (request->fragmentIdentifier ? getNative(request->fragmentIdentifier) : "<N/A>"));
  }
  if (filePart && strcmp(filePart,"/")){
    int fLen = strlen(filePart);
    int prevSlashPos = ((filePart[0] == ASCII_SLASH) ? 0 : -1);
    int slashPos;
    if (traceDispatch){
      printf("request->slh = 0x%p\n",request->slh);
      fflush(stdout);
    }
    StringList *parsedFile = makeStringList(request->slh);
    
    while ((slashPos = indexOf(filePart,fLen,ASCII_SLASH,prevSlashPos+1)) != -1){
      int start = prevSlashPos+1;
      char *part = SLHAlloc(request->slh,slashPos-start+1);
      if (traceDispatch){
        printf("parseURI while loop slashPos=%d prev=%d\n",slashPos,prevSlashPos); 
      }
      memcpy(part,filePart+start,(slashPos-start));
      part[slashPos-start] = 0;
      addToStringList(parsedFile,destructivelyUnasciify(part));
      prevSlashPos = slashPos;
    }
    if ((fLen - prevSlashPos) > 1){
      char *part = SLHAlloc(request->slh,fLen-prevSlashPos);
      memcpy(part,filePart+prevSlashPos+1,(fLen-prevSlashPos-1));
      part[fLen-prevSlashPos-1] = 0;
      addToStringList(parsedFile,destructivelyUnasciify(part));
    }
    request->parsedFile = parsedFile;
    if (traceDispatch){
      printf("URI Parse Stage 2: file parts are '%s'\n",stringListPrint(parsedFile,0,1000,"/",0));
    }
  } else{
    if (traceDispatch){
      printf("URI Parse Stage 2: file is trivial \n");
    }
    request->parsedFile = NULL;
  }
  
}

/*

  The 12-byte charset id is really filled with one of these,
  followed by 8 bytes of dung.

     struct file_tag {                 File Tag Attributes        
       unsigned short ft_ccsid;          Character Set Id          
       unsigned int   ft_txtflag :1;     Pure Text Flag            
       unsigned int   ft_deferred:1;     Defer Until 1st Write     
       unsigned int   ft_rsvflags:14;    Reserved Flags            
     };
*/

// Response must ALWAYS be finished on return
void respondWithError(HttpResponse *response, int code, char *message){
  char buffer[1024];
  int len = strlen(message);
  strncpy(buffer,message,len);
  toASCIIUTF8(buffer,len);
  setResponseStatus(response,code,message);
  setContentType(response,"text/plain");
  addStringHeader(response,"Server","jdmfws");
  /*   if (code == 401){
         addStringHeader(response,"WWW-Authenticate","Basic realm=\"System_Logon\"");
         }
         */
  addIntHeader(response,"Content-Length",len);
  writeHeader(response);
  writeFully(response->socket,buffer,len);
  finishResponse(response);
}

void respondWithMessage(HttpResponse *response, int status,
                        const char *messageFormatString, ...) {

  char errorMessage[512] = { 0 };
  char errorMessageASCII[512] = { 0 };

  va_list argPointer;
  va_start(argPointer, messageFormatString);
  {
    vsnprintf(errorMessage,
              sizeof(errorMessage),
              messageFormatString,
              argPointer);
  }
  va_end(argPointer);

  size_t messageLength = strlen(errorMessage);

  strcpy(errorMessageASCII, errorMessage);
  toASCIIUTF8(errorMessageASCII, messageLength);

  setResponseStatus(response, status, errorMessage);
  setContentType(response, "text/plain");
  addStringHeader(response, "Server", "jdmfws");
  addIntHeader(response, "Content-Length", messageLength);
  writeHeader(response);
  writeFully(response->socket, errorMessageASCII, messageLength);

  finishResponse(response);

}

// Response must ALWAYS be finished on return
void serveFile(HttpService *service, HttpResponse *response){
  HttpRequest *request = response->request;
  char *fileFrag = stringListPrint(request->parsedFile,0,1000,"/",0);
  char *filename = stringConcatenate(response->slh,service->pathRoot,fileFrag);
  
  respondWithUnixFileContents2(service, response, filename, FALSE);
  // Response is finished on return
}

#define MAX_COMPLAINT 512

static char *complain(HttpRequest *request, char *formatString, ...){
  va_list argPointer;
  char *complaint = SLHAlloc(request->slh,MAX_COMPLAINT);

  va_start(argPointer,formatString);
  vsnprintf(complaint,MAX_COMPLAINT,formatString,argPointer);
  va_end(argPointer);
  return complaint;
}

static void addProcessedParam(HttpRequest *request, HttpRequestParam *param){
  param->next = NULL;
  if (request->processedParamList){
    HttpRequestParam *list = request->processedParamList;
    while (list->next){
      list = list->next;
    }
    list->next = param;
  } else{
    request->processedParamList = param;
  }
}

char *processServiceRequestParams(HttpService *service, HttpResponse *response){
  HttpRequest *request = response->request;
  HttpRequestParamSpec *spec = service->paramSpecList;
  int allRequiredFound = TRUE;
  char *complaint = NULL;
  int intValue;
  int64 int64Value;
  char *stringValue;

  request->processedParamList = NULL;
  while (spec){
    char *paramString = getQueryParam(request,spec->name);
    /*    if (paramString){
            destructivelyUnasciify(paramString);
          }
          */
    if (traceDispatch){
      printf("param value for '%s' is '%s'\n",spec->name,(paramString ? paramString : "<N/A>"));
      printf("  not optional %d NULL? %d\n",!(spec->flags & SERVICE_ARG_OPTIONAL),(paramString == NULL));
      fflush(stdout);
    }
    if ((spec->flags & SERVICE_ARG_REQUIRED) && (paramString == NULL)){
      allRequiredFound = FALSE;
      complaint = complain(request,"Required Parameter '%s' Not Supplied",spec->name);
      break;
    }
    if (paramString){
      switch (spec->type){
      case SERVICE_ARG_TYPE_INT:
	intValue = atoi(paramString);
	if (traceDispatch > 1){
	  printf("check intValue %d with flags 0x%x lb %d hb %d\n",intValue,spec->flags,spec->lowBound, spec->highBound);
	}
	if (spec->flags & SERVICE_ARG_HAS_LOW_BOUND){
	  if (intValue < spec->lowBound){
	    complaint = complain(request,"value %d below acceptable value %d for %s",intValue,spec->lowBound,spec->name);
	  }
	}
	if (spec->flags & SERVICE_ARG_HAS_HIGH_BOUND){
	  if (intValue > spec->highBound){
	    complaint = complain(request,"value %d above acceptable value %d for %s",intValue,spec->highBound,spec->name);
	  }
	}
	addProcessedParam(request,makeIntParam(request,spec,intValue));
	break;
      case SERVICE_ARG_TYPE_INT64: 
        int64Value = strtoull(paramString,NULL,10);
	if (spec->flags & SERVICE_ARG_HAS_LOW_BOUND){
	  if (int64Value < spec->lowBound64){
	    complaint = complain(request,"value %lld below acceptable value %lld for %s",int64Value,spec->lowBound64,spec->name);
	  }
	}
	if (spec->flags & SERVICE_ARG_HAS_HIGH_BOUND){
	  if (int64Value > spec->highBound64){
	    complaint = complain(request,"value %lld above acceptable value %lld for %s",int64Value,spec->highBound64,spec->name);
	  }
	}
	addProcessedParam(request,makeInt64Param(request,spec,int64Value));
        break; 
      case SERVICE_ARG_TYPE_STRING:
	addProcessedParam(request,makeStringParam(request,spec,paramString));
	break;
      default:
	break;
      }
    }
    spec = spec->next;
  }

  if (traceDispatch){
    printf("done with process params complaint='%s'\n",(complaint ? complaint : "<NONE>"));
  }
  return complaint;
}

HttpRequestParam *getCheckedParam(HttpRequest *request, char *paramName){
  HttpRequestParam *list = request->processedParamList;
  while (list){
    if (!strcmp(list->specification->name,paramName)){
      return list;
    }
    list = list->next;
  }
  return NULL;
}

static char *getMimeType2(char *extension, int *isBinary, int dotPos, int ccsid);

char *getMimeType(char *extension, int *isBinary) {
  return getMimeType2(extension, isBinary, FALSE, -1);
}

typedef struct MimeType_tag {
  char *extension;
  char *mimeType;
  int isBinary;
} MimeType;

static MimeType MIME_TYPES[] = {
  {"7z", "application/x-7z-compressed", TRUE},
  {"asm", "text/plain", FALSE},
  {"avi", "video/x-msvideo", TRUE},
  {"bin", "application/octet-stream", TRUE},
  {"bmp", "image/bmp", TRUE},
  {"bz2", "application/x-bzip2", TRUE},
  {"ts", "text/typescript", FALSE},
  {"c", "text/plain", FALSE},
  {"cbl", "text/plain", FALSE},
  {"cob", "text/plain", FALSE},
  {"cpp", "text/plain", FALSE},
  {"cpy", "text/plain", FALSE},
  {"css", "text/css", FALSE},
  {"csv", "text/csv", FALSE},
  {"dbg", "application/octet-stream", TRUE},
  {"doc", "application/msword", TRUE },
  {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document", TRUE},
  {"py", "text/plain", FALSE},
  {"cbl", "text/plain", FALSE},
  {"cpp", "text/plain", FALSE},
  {"csv", "text/csv", FALSE},
  {"env", "text/plain", FALSE},
  {"gif", "image/gif", TRUE},
  {"gz", "application/gzip", TRUE},
  {"go", "text/plain", FALSE},
  {"h", "text/plain", FALSE},
  {"html", "text/html", FALSE},
  {"htm", "text/html", FALSE},
  {"ico", "image/vnd.microsoft.icon", TRUE},
  {"jar", "application/java-archive", TRUE},
  {"java", "text/plain", FALSE},
  {"jcl", "text/plain", FALSE},
  {"jpg", "image/jpg", TRUE},
  {"js", "text/javascript", FALSE},
  {"jsx", "text/plain", FALSE},
  {"json", "application/json", FALSE},
  {"jsonld", "application/ld+json", TRUE},
  {"log", "text/plain", FALSE},
  {"md", "text/markdown", FALSE},
  {"mpg", "video/mpeg", TRUE},
  {"o", "application/octet-stream", TRUE},
  {"rex", "text/plain", FALSE},
  {"rexx", "text/plain", FALSE},
  {"rs", "text/plain", FALSE},
  {"rlib", "text/plain", FALSE},
  {"php", "text/plain", FALSE},
  {"pl", "text/plain", FALSE},
  {"png", "image/png", TRUE}, 
  {"py", "text/plain", FALSE},
  {"woff2", "application/font-woff2", TRUE},
  {"ttf", "application/font-ttf", TRUE},
  {"mp3", "audio/mpeg", TRUE},
  {"sh", "application/x-sh", FALSE},
  {"sql", "application/sql", FALSE},
  {"swift", "text/plain", FALSE},
  {"pdf", "application/pdf", TRUE},
  {"tar", "application/x-tar", TRUE},
  {"ts", "text/plain", FALSE},
  {"tsx", "text/plain", FALSE},
  {"txt", "text/plain", FALSE},
  {"webm", "video/webm", TRUE},
  {"xls", "application/vnd.ms-excel", TRUE},
  {"xml", "application/xml", FALSE},
  {"zip", "application/zip", TRUE}
};

#define MIME_TYPE_COUNT sizeof(MIME_TYPES)/sizeof(MIME_TYPES[0])

static MimeType *findMimeTypeByExtension(const char *extention) {
  for (int i = 0; i < MIME_TYPE_COUNT; i++) {
    if (0 == strcmp(extention, MIME_TYPES[i].extension)) {
      return &MIME_TYPES[i];
    }
  }
  return NULL;
}

static char *getMimeType2(char *extension, int *isBinary, int isDotFile, int ccsid){
  bool isTaggedAsText = (ccsid > 0);
  if (isDotFile) {
    *isBinary = FALSE;
    return "text/plain";
  }
  MimeType *mimeType = findMimeTypeByExtension(extension);
  if (mimeType) {
    *isBinary = isTaggedAsText ? FALSE : mimeType->isBinary;
    return mimeType->mimeType;
  }
  if (isTaggedAsText) {
    *isBinary = FALSE;
    return "text/plain";
  }
  *isBinary = TRUE;
  return "application/octet-stream";
}

static void respondWithUnixFileInternal(HttpResponse* response, char* absolutePath, int jsonMode, int secureFlag);
static void respondWithUnixDirectoryInternal(HttpResponse* response, char* absolutePath, int jsonMode, int secureFlag);

static void respondWithUnixFile(HttpResponse* response, char* absolutePath, int jsonMode, bool asB64);
void respondWithUnixDirectory(HttpResponse *response, char* absolutePath, int jsonMode);
void respondWithUnixFileSafer(HttpResponse* response, char* absolutePath, int jsonMode);
void respondWithUnixDirectorySafer(HttpResponse* response, char* absolutePath, int jsonMode);

void respondWithUnixFileNotFound(HttpResponse* response, int jsonMode);
void respondWithJsonError(HttpResponse *response, char *error, int statusCode, char *statusMessage);

static uint64_t makeFileEtag(const FileInfo *file) {
  static const uint32_t prime = 31;
  uint64_t result = 1;
  uint32_t inode;
  time_t mtime;
  uint64_t size;

  inode = fileGetINode(file);
  mtime = fileInfoUnixModificationTime(file);
  size = fileInfoSize(file);
  result = prime * result + inode;
  result = prime * result + mtime;
  result = prime * result + size;
  return result;
}

// Response must ALWAYS be finished on return
void respondWithUnixFileContents2 (HttpService* service, HttpResponse* response, char* absolutePath, int jsonMode) {
  respondWithUnixFileContentsWithAutocvtMode(service, response, absolutePath, jsonMode, TRUE);
  // Response is finished on return
}

// Response must ALWAYS be finished on return
void respondWithUnixFileContentsWithAutocvtMode (HttpService* service, HttpResponse* response, char* absolutePath, int jsonMode, int autocvt) {
  FileInfo info;
  int returnCode;
  int reasonCode;
  int status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);

  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "finfo:\n");
#ifdef DEBUG
  dumpbuffer((char*)&info, sizeof(FileInfo));
#endif

  if (status != 0) {
    respondWithUnixFileNotFound(response, jsonMode);
    // Response is finished on return
  } else if(fileInfoIsDirectory(&info)) {
    respondWithUnixDirectory(response, absolutePath, jsonMode);
    // Response is finished on return
  } else {
    bool asB64 = TRUE;
    char *base64Param = getQueryParam(response->request, "responseType");

    if (NULL != base64Param) {
      if (!strcmp(strupcase(base64Param), "RAW")) {
        asB64 = FALSE;
      }
    }

    respondWithUnixFile2(service, response, absolutePath, jsonMode, autocvt, asB64);
    // Response is finished on return
  }
}

#ifdef USE_CONTINUE_RESPONSE_HACK
/*
  I put this hack in while trying to diagnose a problem that wound up being
  a character set issue, but manifested as a "data not being sent to the browser"
  issue. This doesn't seem to HURT anything, and it might someday prove 
  useful for debugging, so I'm leaving it here under this define.
 */
static char continueResponse[] = "HTTP/1.1 100 Continue\x0d\x0a\x0d\x0a";
#endif

// Response must ALWAYS be finished on return
void respondWithUnixFileContents(HttpResponse* response, char* absolutePath, int jsonMode) {
  respondWithUnixFileContents2(NULL, response, absolutePath, jsonMode);
  // Response is finished on return
}

/*
 * Commented out:
#define STRUCT_TO_C_FORMAT_STRING asctime_r // "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n"
#define STRUCT_TO_STRING strftime
#define STRING_TO_STRUCT strptime
#define TIMESTAMP_TO_GMT_STRUCT gmtime_r
#define TIMESTAMP_TO_LOCAL_STRUCT localtime_r
#define LOCAL_STRUCT_TO_TIMESTAMP mktime
#define GMT_STRUCT_TO_TIMESTAMP timegm
 */

static void addCacheRelatedHeaders(HttpResponse *response, time_t mtime,
    uint64_t etag) {
/*  struct tm mtimeTm; */
  char *lmBuf, *eTagBuf;

  /* This is actually interesting: "Cache-Control: no-cache", counter-intuitively,
     means *the same*, i.e. "you can cache but must revalidate each time". However,
     there's some info that browser support is inconsistent regarding this,
     possibly because page developers treated "no-cache" like literally, no cache
     and browser vendors decided to indulge them...
     addStringHeader(response, "Cache-Control", "max-age=0, must-revalidate");
     */
  /*
   * Commented out:
   *
  lmBuf = SLHAlloc(response->slh, 50);
  TIMESTAMP_TO_GMT_STRUCT(&mtime, &mtimeTm);
  STRUCT_TO_STRING(lmBuf, 33, "%a, %d %b %Y %H:%M:%S %Z",
      &mtimeTm);
  addStringHeader(response, "Last-Modified", lmBuf); */
  eTagBuf = SLHAlloc(response->slh, 33);
  snprintf(eTagBuf, 50, "%.16llx", etag);
  addStringHeader(response, "ETag", eTagBuf);
}

bool isCachedCopyModified(HttpRequest *req, uint64_t etag, time_t mtime) {
  HttpHeader *imsHeader = getHeader(req, "If-Modified-Since");
  HttpHeader *inmHeader = getHeader(req, "If-None-Match");

  /* printf("isCacheCopyModified: ims %p, inm %p, etag %.16llx, mtime %lu\n",
        imsHeader, inmHeader, etag, mtime);
        */
  if (imsHeader != NULL || inmHeader != NULL) {
    if (inmHeader) {
      uint64_t requestEtag;

      sscanf(inmHeader->nativeValue, "%llx", &requestEtag);
      /* printf("isCacheCopyModified: req etag %.16llx\n", requestEtag); */
      if (requestEtag != etag) {
        /* printf("isCacheCopyModified: modified (etag)\n"); */
        return true;
      }
    } else if (imsHeader) {
      /*
       * Commented out:
       *
      struct tm reqMtimeTm;
      time_t reqMtime;

      STRING_TO_STRUCT(imsHeader->nativeValue, "%a, %d %b %Y %H:%M:%S %Z", &reqMtimeTm);
      reqMtime = GMT_STRUCT_TO_TIMESTAMP(&reqMtimeTm);
      // printf("isCacheCopyModified: req mtime %lu\n", reqMtime); 
      if (mtime > reqMtime) {
        // printf("isCacheCopyModified: modified (mtime)\n"); 
        return true;
      }*/
      /*
       * Just say that it *is* modified since the date, if anyone asks.
       * Given that we never set the Last-Modified header, hopefully the browser
       * won't ask us either
       */
      /* printf("isCacheCopyModified: modified (If-Modified-Since present)\n"); */
      return true;
    }
    /* printf("isCacheCopyModified: not modified\n"); */
    return false;
  }
  /* printf("isCacheCopyModified: unknown\n"); */
  return true;
}

static int streamBinaryForFile2(HttpResponse *response, Socket *socket, UnixFile *in, int encoding, bool asB64);
static int streamTextForFile2(HttpResponse *response, Socket *socket, UnixFile *in, int encoding,
                      int sourceCCSID, int targetCCSID, bool asB64);

// Response must ALWAYS be finished on return
void respondWithUnixFile2(HttpService* service, HttpResponse* response, char* absolutePath, int jsonMode, int autocvt, bool asB64) {
  FileInfo info;
  int returnCode;
  int reasonCode;
  int status = fileInfo(absolutePath, &info, &returnCode, &reasonCode);
  HttpRequest *req = response->request;

  if(status == 0) {
    int filenameLen = strlen(absolutePath);
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Request for file=%s\n",absolutePath);
    int dotPos = lastIndexOf(absolutePath, filenameLen, '.');
    int isDotFile = FALSE;
    if (dotPos > 0 && (absolutePath[dotPos-1] == '/')){
      isDotFile = TRUE;
    }
    char *extension = (dotPos == -1) ? "NULL" : absolutePath + dotPos + 1;
    int isBinary = FALSE;
    long fileSize = fileInfoSize(&info);
    int ccsid = fileInfoCCSID(&info);
    char *mimeType = getMimeType2(extension,&isBinary,isDotFile, ccsid);
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "File ccsid=%d, mimetype=%s isBinary=%s\n",
            ccsid,mimeType,isBinary ? "true" : "false");
    char tmperr[256] = {0};
    time_t mtime = fileInfoUnixModificationTime(&info);
    uint64_t etag = makeFileEtag(&info);
    bool modified = isCachedCopyModified(req, etag, mtime);

    if (!modified) {
      setResponseStatus(response, 304, "Not modified");
      addStringHeader(response, "Cache-control", "no-store");
      addStringHeader(response, "Pragma", "no-cache");
      addStringHeader(response, "Server", "jdmfws");
      addCacheRelatedHeaders(response, mtime, etag);
      writeHeader(response);
      finishResponse(response);
      return;
    }
    /* attempt open before writing any headers so we have option of returning error */
    UnixFile *in = fileOpen(absolutePath,
                            FILE_OPTION_READ_ONLY,
                            0, 0,
                            &returnCode, &reasonCode);
    if (NULL == in) {
      sprintf(tmperr, "Forbidden (rc=%d, rsn=0x%x)", returnCode, reasonCode);
      if (jsonMode) {
        respondWithJsonError(response, "Forbidden", 403, tmperr);
        // Response is finished on return
      } else {
        respondWithError(response, 403, tmperr);
        // Response is finished on return
      }
      return;
    }

#ifdef __ZOWE_OS_ZOS
    if (!autocvt) {
      int disableCvt = fileDisableConversion(in, &returnCode, &reasonCode);
      if (disableCvt != 0) {
    	zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Warning: encoding conversion was not disabled, return = %d, reason = %d\n", returnCode, reasonCode);
      }
    }
#endif

    setResponseStatus(response,200,"OK");
    addStringHeader(response,"Server","jdmfws");
    addStringHeader(response, "Cache-control", "no-store");
    addStringHeader(response, "Pragma", "no-cache");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    setContentType(response, mimeType);
    addCacheRelatedHeaders(response, mtime, etag);

    if ((NULL != service) &&
        (NULL != service->customHeadersFunction))
    {
      service->customHeadersFunction(service, response);
    }

    if (isBinary || ccsid == -1) {
      writeHeader(response);
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Streaming binary for %s\n", absolutePath);
      
      streamBinaryForFile2(response, NULL, in, ENCODING_CHUNKED, asB64);
    } else {
      writeHeader(response);
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Streaming %d for %s\n", ccsid, absolutePath);

      /* TBD: This isn't really an OS dependency, but this is what I had
         to do to get this working on Linux. The problem is that there really
         are some of the JavaScript files encoded as UTF-8, that contain characters 
         outside the set representable ny ISO-8859-1. I'm not sure how this is 
         working on z/OS; I suspect that the encoding function is more permissive.
         I think we're going to need:

           * A separate lookaside file on non-z/OS platforms to provide the file
             encoding information available on z/OS through tagging.
           * Tagging the files encoded as UTF-8 (using whatever platform-dependent
             mechanism).
           * Setting the Content-type header with the appropriate encoding.
       */

      int webCodePage = 
#ifdef __ZOWE_OS_ZOS
        CCSID_ISO_8859_1
#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX) || defined(__ZOWE_OS_WINDOWS)
        CCSID_UTF_8
#else
#error Unknown OS
#endif
        ;
    char *forceEnabled = getQueryParam(response->request, "force");
    if (ccsid == 0 && !strcmp(forceEnabled, "enable")) {
        char *sourceEncoding = getQueryParam(response->request, "source");
        char *targetEncoding = getQueryParam(response->request, "target");
        int sEncoding;
        int tEncoding;
        if(sourceEncoding != NULL && targetEncoding != NULL) {
           int sscanfSource = sscanf(sourceEncoding, "%d", &sEncoding);
           int sscanfTarget = sscanf(targetEncoding, "%d", &tEncoding);
           if (sscanfSource != 1 || sscanfTarget != 1) {
             respondWithError(response, HTTP_STATUS_BAD_REQUEST, "source/target encoding value parsing error.");
             return;
           }
	         zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Sending with forced conversion between %d and %d\n", 
                   sscanfSource, sscanfTarget);
           streamTextForFile2(response, NULL, in, ENCODING_CHUNKED, sEncoding, tEncoding, asB64);
        }
        else {
          respondWithError(response, HTTP_STATUS_BAD_REQUEST, "force encoding enabled make sure to pass all the requried params");
          return;
        }
    }
    else if(ccsid == 0) {
	    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Sending with default conversion between %d and %d\n", 
              NATIVE_CODEPAGE, webCodePage);
      streamTextForFile2(response, NULL, in, ENCODING_CHUNKED, NATIVE_CODEPAGE, webCodePage, asB64);
    }
    else {
	    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Sending with tagged conversion between %d and %d\n", 
              ccsid, webCodePage);
      streamTextForFile2(response, NULL, in, ENCODING_CHUNKED, ccsid, webCodePage, asB64);
    }

#ifdef USE_CONTINUE_RESPONSE_HACK
      /* See comments above */
      writeFully(response->socket, continueResponse, strlen(continueResponse));
#endif

    }
    /* we opened the file and streamFoo doesn't close */
    fileClose(in,&returnCode,&reasonCode);
    finishResponse(response);
  }
  else {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "File not found within respondWithUnixFile.. This may be a problem\n");
    respondWithUnixFileNotFound(response, jsonMode);
    // Response is finished on return
  }
}

// Response must ALWAYS be finished on return
void respondWithUnixFile(HttpResponse* response, char* absolutePath, int jsonMode, bool asB64) {
  respondWithUnixFile2(NULL, response, absolutePath, jsonMode, 1, asB64);
  // Response is finished on return
}

// Response must ALWAYS be finished on return
void respondWithUnixDirectory(HttpResponse *response, char* absolutePath, int jsonMode) {
  int returnCode;
  int reasonCode;
    UnixFile *directory = NULL;

  
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "Directory case: %s\n",absolutePath);
  
 if ((directory = directoryOpen(absolutePath,&returnCode,&reasonCode)) == NULL){
    respondWithJsonError(response, "Permission denied", 403, "Forbidden");
    // Response is finished on return
  }
  else {
    directoryClose(directory,&returnCode,&reasonCode);
    setResponseStatus(response,200,"OK");
    addStringHeader(response, "Cache-control", "no-store");
    addStringHeader(response, "Pragma", "no-cache");  
    addStringHeader(response,"Server","jdmfws");
    addStringHeader(response,"Transfer-Encoding","chunked");
    if (jsonMode == 0) {
      setContentType(response,"text/html");
      writeHeader(response);
      StringListElt *parsedFileTail = firstStringListElt(response->request->parsedFile);
      while (parsedFileTail->next){
        parsedFileTail = parsedFileTail->next;
      }
      makeHTMLForDirectory(response, absolutePath, parsedFileTail->string,TRUE);
      // Response is finished on return
    }
    else {
      setContentType(response,"application/json");
      writeHeader(response);
      makeJSONForDirectory(response,absolutePath,TRUE);
      // Response is finished on return
    }
  }
}

// Response must ALWAYS be finished on return
void respondWithUnixFileNotFound(HttpResponse* response, int jsonMode) {
  if (jsonMode == 0) {
    char message[] = "File not found";
    int len = strlen(message);

    addStringHeader(response,"Server","jdmfws");
    setContentType(response,"text/plain");
    setResponseStatus(response,404,"Not Found");
    addStringHeader(response, "Cache-control", "no-store");
    addStringHeader(response, "Pragma", "no-cache");
    addIntHeader(response,"Content-Length",len);
    writeHeader(response);

    toASCIIUTF8(message,len);

    writeFully(response->socket,message,len);
    finishResponse(response);
  }
  else {
    respondWithJsonError(response, "File not found.", 404, "Not Found");
    // Response is finished on return
  }
}

void setDefaultJSONRESTHeaders(HttpResponse *response) {
  setContentType(response, "application/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  addStringHeader(response, "Cache-control", "no-store");
  addStringHeader(response, "Pragma", "no-cache");
}

// Response must ALWAYS be finished on return
void respondWithJsonError(HttpResponse *response, char *error, int statusCode, char *statusMessage) {
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response,statusCode,statusMessage);
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  jsonStart(out);
  jsonAddString(out, "error", error);
  jsonEnd(out);

  finishResponse(response);
}

#define ENCODE64_SIZE(SZ) (2 + 4 * ((SZ + 2) / 3))

/*
  * when encoding is ENCODING_SIMPLE then socket is mandatory
  * when encoding is ENCODING_CHUNKED then response is mandatory
*/
static int streamBinaryForFile2(HttpResponse *response, Socket *socket, UnixFile *in, int encoding, bool asB64) {
  int returnCode = 0;
  int reasonCode = 0;
  ChunkedOutputStream *stream = NULL;

  if ((response && socket) || (!response && !socket)) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "bad arguments: either response or socket must be not NULL, never both\n");	
    return 8;
  }
  if (encoding == ENCODING_GZIP) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "GZIP encoding not implemented\n");	
    return 8;
  }
  if (encoding == ENCODING_CHUNKED && !response) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "bad arguments: response must be not NULL to use chunked encoding\n");	
    return 8;
  }
  if (encoding == ENCODING_CHUNKED) {
    stream = makeChunkedOutputStreamInternal(response);
  }

  int bufferSize = FILE_STREAM_BUFFER_SIZE;
  char *buffer = safeMalloc(bufferSize+4, "streamBinaryBuffer");
  int encodedLength;

  while (!fileEOF(in)) {
    int bytesRead = fileRead(in,buffer,bufferSize,&returnCode,&reasonCode);
    if (bytesRead <= 0) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2,
              "Binary streaming has ended. (return = 0x%x, reason = 0x%x)\n",
              returnCode, reasonCode);
      break;
    }

    char *encodedBuffer = NULL;
    if (bytesRead % 3) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "buffer length not divisble by 3.  Base64Encode will fail if this is not the eof.\n");
    }
    if (asB64) {
      encodedBuffer = encodeBase64(NULL, buffer, bytesRead, &encodedLength, FALSE);
    }
    char *outPtr = asB64 ? encodedBuffer : buffer;
    int outLen = asB64 ? encodedLength : bytesRead;
    if (encoding == ENCODING_CHUNKED) {
      writeBytes(stream, outPtr, outLen, NO_TRANSLATE);
    } else {
      writeFully(socket, outPtr, outLen);
    }

    if (NULL != encodedBuffer) safeFree31(encodedBuffer, ENCODE64_SIZE(bytesRead)+1);
  }
  if (encoding == ENCODING_CHUNKED) {
    /* finish the chunked output here because finishResponse will not flush this stream's data */
    finishChunkedOutput(stream, NO_TRANSLATE);
  }

  safeFree(buffer, bufferSize+4);

  return 0;
}

int streamBinaryForFile(Socket *socket, UnixFile *in, bool asB64) {
  return streamBinaryForFile2(NULL, socket, in, ENCODING_SIMPLE, asB64);
}

/*
  * when encoding is ENCODING_SIMPLE then socket is mandatory
  * when encoding is ENCODING_CHUNKED then response is mandatory
*/
static int streamTextForFile2(HttpResponse *response, Socket *socket, UnixFile *in, int encoding,
                      int sourceCCSID, int targetCCSID, bool asB64) {
  int returnCode = 0;
  int reasonCode = 0;
  int bytesSent = 0;
  ChunkedOutputStream *stream = NULL;

  /* Q: How do we find character encoding for unix file? 
     A: You can't. There is no equivalent of USS character encoding tags on
        other Unix systems. Hence, things like the .htaccess (for Apache).
  */
  if ((response && socket) || (!response && !socket)) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "bad arguments: either response or socket must be not NULL, never both\n");	
    return 8;
  }
  switch (encoding){
  case ENCODING_CHUNKED:
    if (!response) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "bad arguments: response must be not NULL to use chunked encoding\n");	
      return 8;
    }
    stream = makeChunkedOutputStreamInternal(response);
    /* fallthrough */
  case ENCODING_SIMPLE: {
    int bufferSize = FILE_STREAM_BUFFER_SIZE;
    char *buffer = safeMalloc(bufferSize+4, "streamTextBuffer");
    char *translation = safeMalloc((2*bufferSize)+4, "streamTextConvertBuffer"); /* UTF inflation tolerance */
    int encodedLength;

    while (!fileEOF(in)){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "WARNING: UTF8 might not be aligned properly: preserve 3 bytes for the next read cycle to fix UTF boundaries\n");
      int bytesRead = fileRead(in,buffer,bufferSize,&returnCode,&reasonCode);
      if (bytesRead <= 0) {
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
                "Text streaming has ended. (return = 0x%x, reason = 0x%x)\n",
                returnCode, reasonCode);
        break;
      }
      unsigned int inLen, outLen;
      int rc;
      char *inPtr, *outPtr;

      if (sourceCCSID == targetCCSID) {
        outPtr = buffer;
        outLen = (unsigned int)bytesRead;
      } else {
        inPtr = buffer;
        outPtr = translation;
        inLen = (unsigned int)bytesRead;
        outLen = (unsigned int) 2 * bytesRead;

        /* TBD: I don't like this scheme.
         * As mentioned in the warning above, if the encodings are not single-byte encodings, either 
           the input or output buffers could break in the middle of a multi-byte character. The current
           API for convertCharset doesn't let you restart in such cases.
         * Every call to convertCharset will (in the current implementation) use iconv_open to get
           a converter. That's expensive.
         * If no conversion is necessary, Linuxland could use the sendfile(2) system call.
        */

        /* dumpbuffer(buffer,bytesRead); */
        int translationLength = 0;
        int reasonCode = 0;
        rc = convertCharset(inPtr,
                            inLen,
                            sourceCCSID,
                            CHARSET_OUTPUT_USE_BUFFER,
                            &outPtr,
                            outLen,
                            targetCCSID,
                            NULL,
                            &translationLength,
                            &reasonCode);

        if (inLen != translationLength) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "streamTextForFile(%d (%s), %d (%s), %d, %d, %d, %d): "
                 "after sending %d bytes got translation length error; expected %d, got %d\n",
                 getSocketDebugID(socket), socket->debugName, 
                 in->fd, in->pathname,
                 encoding, sourceCCSID, targetCCSID, asB64, bytesSent, inLen, translationLength);
        }
        if (TRACE_CHARSET_CONVERSION){
          printf("convertCharset transLen=%d\n",translationLength);
          dumpbuffer(translation,translationLength);
        }
        if (rc != 0){
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "iconv rc = %d, bytesRead=%d xlateLength=%d\n",rc,bytesRead,translationLength);
        }

        outPtr = translation;
        outLen = (unsigned int) translationLength;
      }
      int allocSize = 0;
      char *encodedBuffer = NULL;
      if (asB64) {
        if (outLen % 3) { 
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "buffer length not divisble by 3.  Base64Encode will fail if this is not the eof.\n");
        }
        allocSize = ENCODE64_SIZE(outLen)+1;
        encodedBuffer = encodeBase64(NULL, outPtr, outLen, &encodedLength, FALSE);
        outPtr = encodedBuffer;
        outLen = encodedLength;
      }
      if (encoding == ENCODING_CHUNKED) {
        writeBytes(stream, outPtr, (int) outLen, NO_TRANSLATE);
      } else {
        writeFully(socket,outPtr,(int) outLen);
      }
      if (NULL != encodedBuffer) safeFree31(encodedBuffer, allocSize);
      bytesSent += encodedLength;
    }
    if (encoding == ENCODING_CHUNKED) {
      /* finish the chunked output here because finishResponse will not flush this stream's data */
      finishChunkedOutput(stream, NO_TRANSLATE);
    }
    safeFree(buffer, bufferSize+4);
    safeFree(translation, (2*bufferSize)+4);
    break;
  }
  case ENCODING_GZIP:
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "HELP - not implemented\n");
    break;
  }
  if (traceSocket > 0) {
    printf("streamTextForFile(%d (%s), %d (%s), %d, %d, %d, %d) sent %d bytes\n",
           getSocketDebugID(socket), socket->debugName, 
           in->fd, in->pathname,
           encoding, sourceCCSID, targetCCSID, asB64, bytesSent);
  }
  return 1;
}

int streamTextForFile(Socket *socket, UnixFile *in, int encoding,
                      int sourceCCSID, int targetCCSID, bool asB64) {
  return streamTextForFile2(NULL, socket, in, encoding, sourceCCSID, targetCCSID, asB64);
}

int runServiceThread(Socket *socket){
#ifndef METTLE
  int threadID; /* pthread_t threadID;  */
  
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "runServiceThread\n");
  OSThread osThreadData;
  OSThread *osThread = &osThreadData;
  int createStatus = threadCreate(osThread,(void * (*)(void *))serviceLoop,socket);
  if (createStatus != 0) {
#ifdef __ZOWE_OS_WINDOWS
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "CREATE THREAD failure, code=0x%x\n",createStatus);
#else
    perror("pthread_create() error");
#endif
    exit(1);
  } else{
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "thread create succeeded!\n");
  }
#endif
  return 0;
}


#define DIR_BUFFER_SIZE 1024


// Response must ALWAYS be finished on return
int makeHTMLForDirectory(HttpResponse *response, char *dirname, char *stem, int includeDotted){
  int count;
  int returnCode;
  int reasonCode;
  char path[1025];
  char url[2049];
  char directoryDataBuffer[DIR_BUFFER_SIZE];
  int stemLen = strlen(stem);
  int needsSlash = stem[stemLen-1] != '/';
  ChunkedOutputStream *out = (response ? makeChunkedOutputStreamInternal(response) : NULL);
  UnixFile *directory = NULL;
  
  if ((directory = directoryOpen(dirname,&returnCode,&reasonCode)) == NULL){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "directory open (%s) failure rc=%d reason=0x%x\n",
            dirname,returnCode,reasonCode);
  } else {
    if (out){
      char line[1024];
      int len;

      writeString(out,"<HTML>\n");
      writeString(out,"<HEAD>\n");
      len = sprintf(line,"<TITLE>Index of %s</TITLE>\n",stem);
      writeBytes(out,line,len,TRUE);
      writeString(out,"</HEAD>\n");
      writeString(out,"<BODY>\n");
      len = sprintf(line,"<H1>Index of %s</H1>\n",stem);
      writeBytes(out,line,len,TRUE);
      writeString(out,"<HR/>\n");
      writeString(out,"<PRE>\n");
    }
    int entriesRead = 0;
    while ((entriesRead = directoryRead(directory,directoryDataBuffer,DIR_BUFFER_SIZE,&returnCode,&reasonCode)) > 0){
      char *entryStart = directoryDataBuffer;
      for (int e = 0; e<entriesRead; e++){
        int entryLength = ((short*)entryStart)[0];  /* I *THINK* the entry names are null-terminated */
        int nameLength = ((short*)entryStart)[1];
        if(entryLength == 4 && nameLength == 0) { /* Null directory entry */
          break;
        }
        char *name = entryStart+4;
        entryStart += entryLength;
        if (includeDotted || name[0] != '.'){
          int len;
          strcpy(path, stem);
          if (needsSlash){
            strcat(path, "/");
          }
          strncat(path, name, nameLength);
          len = sprintf(url,"<A HREF=\"%s\">%*.*s</A>\n",path,nameLength,nameLength,name);
          if (out){
            writeBytes(out,url,len,TRUE);
          } else{
            zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "%s\n",url);
          }
        }
      }
    }
    if (out){
      writeString(out,"</PRE>\n");
      writeString(out,"<HR/>\n");
      writeString(out,"</BODY>\n");
      writeString(out,"</HTML>\n");
      finishChunkedOutput(out, TRANSLATE_8859_1);
      fflush(stdout);
    }
    directoryClose(directory,&returnCode,&reasonCode);
  }
  finishResponse(response);
  return 0;
}

// Response must ALWAYS be finished on return
int makeJSONForDirectory(HttpResponse *response, char *dirname, int includeDotted){
  int count;
  int returnCode;
  int reasonCode;
  char path[1025];
  char url[2049];
  char directoryDataBuffer[DIR_BUFFER_SIZE];
  int stemLen = strlen(dirname);
  int needsSlash = dirname[stemLen-1] != '/';
  jsonPrinter *out = (response ? respondWithJsonPrinter(response) : NULL);

  if (out) {
    UnixFile *directory = NULL;
    if ((directory = directoryOpen(dirname,&returnCode,&reasonCode)) == NULL){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "directory open (%s) failure rc=%d reason=0x%x\n",
              dirname,returnCode,reasonCode);
    } else {
      jsonStart(out);
      jsonStartArray(out, "entries");
      int entriesRead = 0;
      while ((entriesRead = directoryRead(directory,directoryDataBuffer,DIR_BUFFER_SIZE,&returnCode,&reasonCode)) > 0){
        char *entryStart = directoryDataBuffer;
        for (int e = 0; e<entriesRead; e++){
          int entryLength = ((short*)entryStart)[0];  /* I *THINK* the entry names are null-terminated */
          int nameLength = ((short*)entryStart)[1];
          if(entryLength == 4 && nameLength == 0) { /* Null directory entry */
            break;
          }
          char *name = entryStart+4;
          entryStart += entryLength;
          if ((includeDotted && strncmp(name, ".", nameLength) && strncmp(name, "..", nameLength)) || name[0] != '.'){
            int len;
            strcpy(path, dirname);
          if (needsSlash){
            strcat(path, "/");
          }
          strncat(path, name, nameLength);

          FileInfo info = {0};
          symbolicFileInfo(path, &info, &returnCode, &reasonCode);
          int decimalMode = fileUnixMode(&info);
          int octalMode = decimalToOctal(decimalMode);

          ISOTime timeStamp = {0};
          int unixTime = fileInfoUnixCreationTime(&info);
          convertUnixToISO(unixTime, &timeStamp);

          char owner[USER_NAME_LEN+1] = {0};
          int status = userGetName(fileInfoOwnerUID(&info), owner, &returnCode, &reasonCode);
          if (status != 0) {
            zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, 
                    "failed to obtain user name for uid=%d, returnCode: %d, reasonCode: 0x%08x\n",
                    fileInfoOwnerUID(&info), returnCode, reasonCode);
            snprintf(owner, USER_NAME_LEN+1, "%d", info.ownerUID);
          }
          trimRight(owner, USER_NAME_LEN);
          
          char group[GROUP_NAME_LEN+1] = {0};
          status = groupGetName(fileInfoOwnerGID(&info), group, &returnCode, &reasonCode);
          if (status != 0) {
            zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, 
                    "failed to obtain group name for gid=%d, returnCode: %d, reasonCode: 0x%08x\n",
                    fileInfoOwnerGID(&info), returnCode, reasonCode);
            snprintf(owner, USER_NAME_LEN+1, "%d", info.ownerGID);
          }
          trimRight(group, GROUP_NAME_LEN);
          
          /*          if(status == 0) { */
            jsonStartObject(out, NULL);
            {
              jsonAddUnterminatedString(out, "name", name, nameLength);
              jsonAddString(out, "path", path);
              jsonAddBoolean(out, "directory", fileInfoIsDirectory(&info));
              jsonAddInt64(out, "size", fileInfoSize(&info));
              jsonAddInt(out, "ccsid", fileInfoCCSID(&info));
              jsonAddString(out, "createdAt", timeStamp.data);
              jsonAddInt(out, "mode", octalMode);
              jsonAddString(out, "owner", owner);
              jsonAddString(out, "group", group);
            }
            jsonEndObject(out);
            /*          }
                        else {
                  printf("fileInfo() error %d reason 0x%x\n",returnCode,reasonCode);
              }
              */
        }
      }
    }
    jsonEndArray(out);
    jsonEnd(out);
    directoryClose(directory,&returnCode,&reasonCode);
  }
  }
  finishResponse(response);
  return 0;
}
#define MAX_URL_PARTS 100

void parseURLMask(HttpService *service, char *urlMask){
  int len = strlen(urlMask);
  int i,count=0;
  int prevSlashPos = ((len > 0) && urlMask[0] == '/') ? 0 : -1;
  int slashPos;
  char *parts[MAX_URL_PARTS];
  int flags = 0;
#ifdef DEBUG
  TEMP_TRACE("parseURLMask '%s' len=%d\n",urlMask,len);fflush(stdout);
#endif


#define NORMALIZED_PART_LENGTH  256

  if (strcmp(urlMask,"/")){
    while ((slashPos = indexOf(urlMask,len,'/',prevSlashPos+1)) != -1){
      int partLen = slashPos - prevSlashPos;
     char *part = (char*) safeMalloc(NORMALIZED_PART_LENGTH, "urlMask part");
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "parse URL mask loop top len=%d slashPos=%d prevSlashPos=%d partLen=%d\n",
	     len,slashPos,prevSlashPos,partLen);
      parts[count++] = part;
      memcpy(part,urlMask+prevSlashPos+1,slashPos-prevSlashPos-1);
      part[slashPos-prevSlashPos-1] = 0;
      prevSlashPos = slashPos;
    }
    if (len > prevSlashPos+1){
      parts[count] = safeMalloc(NORMALIZED_PART_LENGTH, "parts[count]");
      memcpy(parts[count],urlMask+prevSlashPos+1,len-prevSlashPos-1);
      parts[count][len-prevSlashPos-1] = 0;
      count++;
    }
  } else{
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "trivial URL\n");
  }
  
  service->urlMask = urlMask;
  if (count > 0){
    service->parsedMaskParts = (char**)safeMalloc(count * sizeof(char*), "parsedMaskParts");
    for (i=0; i<count; i++){
      service->parsedMaskParts[i] = parts[i];
    }
    if (!strcmp(parts[count-1],"**")){
      count--;
      flags |= SERVICE_MATCH_WILD_RIGHT;
    }
  } else{
    service->parsedMaskParts = 0;
  }
  service->parsedMaskPartCount = count;
  service->matchFlags = flags;
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "parsed URL mask i=%d\n",count);
  for (i=0; i<count; i++){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "  %s\n", service->parsedMaskParts[i]);
  }
}


#define QP_INITIAL 1
#define QP_PARAM_NAME 2
#define QP_AMPERSAND 3
#define QP_EQUAL 4
#define QP_PARAM_VALUE 5

#define QP_TRACE 0

char *getQueryParam(HttpRequest *request, char *paramName){
  int len = request->queryStringLen;
  int i;
  int state = QP_INITIAL;
  char paramNameBuffer[1024];
  char ebcdicParamNameBuffer[1024];
  int namePos = 0;
  char paramValueBuffer[1024];
  int valuePos = 0;
  int matched = 0;
  if (QP_TRACE){
    printf("getQueryParam %s from '%s'\n",paramName,request->queryString);
    dumpbuffer(request->queryString,strlen(request->queryString));
    fflush(stdout);
  }

  for (i=0; i<len; i++){
    char c = request->queryString[i];
    /* printf("gQP loop state=%d c=%d i=%d\n",state,c,i); */
    switch (state){
    case QP_INITIAL:
      if (c == ASCII_EQUALS || c == ASCII_AMPERSAND){
	if (QP_TRACE > 1)
	  printf("no match case -1\n");
	return NULL;
      } else{
        paramNameBuffer[namePos++] = c;
	state = QP_PARAM_NAME;
      }
      break;
    case QP_PARAM_NAME:
      if (c == ASCII_EQUALS){
	state = QP_EQUAL;
	paramNameBuffer[namePos] = 0;
	memcpy(ebcdicParamNameBuffer,paramNameBuffer,namePos+1);
	destructivelyUnasciify(ebcdicParamNameBuffer);
	if (QP_TRACE > 0){
	  printf("comparing '%s' to '%s'\n",paramName,ebcdicParamNameBuffer);
	}
	if (strcmp(paramName,ebcdicParamNameBuffer) == 0){
	  matched = 1;
	}
      } else if (c == ASCII_AMPERSAND){
	if (QP_TRACE > 1)
	  printf("no match case 0\n");
	return NULL;
      } else{
	paramNameBuffer[namePos++] = c;
      }
      break;
    case QP_AMPERSAND:
      if (c == ASCII_AMPERSAND|| c == ASCII_EQUALS){
	if (QP_TRACE > 1)
	printf("no match case 1\n");
	return NULL;
      } else{
	state = QP_PARAM_NAME;
	paramNameBuffer[0] = c;
	namePos = 1;
      }
      break;
    case QP_EQUAL:
      if (c == ASCII_AMPERSAND || c == ASCII_EQUALS){
	if (QP_TRACE > 1)
	  printf("no match case 2\n");
	return NULL;
      } else{
	state = QP_PARAM_VALUE;
	paramValueBuffer[0] = c;
	valuePos = 1;
        if (QP_TRACE){
          printf("= case i=%d, len=%d\n",i,len);
        }
	if (i == len-1){
          if (matched){
            char *result = SLHAlloc(request->slh,2);
	    result[0] = c;
	    result[1] = 0;
	    if (QP_TRACE > 0){
	      printf("result one char (0x%02x) = %s\n",c,result);
	    }
            destructivelyUnasciify(result);
            char *cleaned = cleanURLParamValue(request->slh,result);
            /* printf("clean: before %s after %s\n",result,cleaned); */
            return cleaned;
          } else{
            return NULL;
          }
	}
      }
      break;
    case QP_PARAM_VALUE:
      if (QP_TRACE){
        printf("param value state c=%d i=%d len=%d matched=%d\n",c,i,len,matched); 
      }
      if (c == ASCII_AMPERSAND || i==(len-1)){ /* param value is done */
	if (c != ASCII_AMPERSAND){
	  paramValueBuffer[valuePos++] = c;
	}
	if (matched){
	  char *result = SLHAlloc(request->slh,valuePos+1);
	  char *cleaned = NULL;
	  paramValueBuffer[valuePos] = 0;
	  memcpy(result,paramValueBuffer,valuePos+1);
	  if (QP_TRACE > 0){
	    printf("matched: %s\n",result);
	  }
          destructivelyUnasciify(result);
	  cleaned = cleanURLParamValue(request->slh,result);
	  /* printf("clean: before %s after %s\n",result,cleaned); */
	  return cleaned;
	} else if (c == ASCII_AMPERSAND){
	  state = QP_AMPERSAND;
	}
      } else if (c == ASCII_EQUALS){
	if (QP_TRACE > 1)
	  printf("no match case 3");
	return NULL;
      } else{
	paramValueBuffer[valuePos++] = c;
      }
      break;
    }
  }
  if (QP_TRACE > 1)
    printf("no match case 4\n");
  return NULL;
}


// Response must ALWAYS be finished on return
void serveSimpleTemplate(HttpService *service, HttpResponse *response){
  HttpRequest *request = response->request;
  HTMLTemplate *template = openHTMLTemplate(response,service->templatePath);
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "serveSimpleTemplate %s, template=0x%p\n",service->templatePath,template);
  if (template){
    ChunkedOutputStream *outStream = makeChunkedOutputStreamInternal(response);
    setResponseStatus(response,200,"OK");
    setContentType(response,"text/html");
    addStringHeader(response,"Server","jdmfws");
    addStringHeader(response,"Transfer-Encoding","chunked");
    writeHeader(response);

    while (streamToSubstitution(template,outStream)){
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "serveSimpleTemplate place loop '%s'\n",template->currentPlaceholder);
      /* call magic function with place,name, additional args, outStream and response */
      HttpTemplateTag *tag = (HttpTemplateTag*)SLHAlloc(request->slh,sizeof(HttpTemplateTag));
      tag->placeName = template->currentPlaceholder;
      if (service->serviceTemplateTagFunction){
	service->serviceTemplateTagFunction(service,response,tag,outStream);
      } else{
    	zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "*** WARNING *** no template tag service function for %s\n", service->name);
      }
    }
    
    closeHTMLTemplate(template);
    finishChunkedOutput(outStream, TRANSLATE_8859_1);
  } else {
    /* some sort of error */
  }
  finishResponse(response);
}



HttpService *makeFileService(char *name, char *urlMask, char *pathRoot){
  HttpService *service = (HttpService*)safeMalloc(sizeof(HttpService),"HttpService-File");
  service->name = name;
  service->serviceType = SERVICE_TYPE_FILES;
  parseURLMask(service,urlMask);
  service->pathRoot = pathRoot;
  service->next = NULL;
  service->serviceFunction = NULL;
  service->serviceTemplateTagFunction = NULL;
  service->paramSpecList = NULL;
  return service;
}


HttpService *makeGeneratedService(char *name, char *urlMask){
  HttpService *service = (HttpService*)safeMalloc(sizeof(HttpService),"HttpService-Generated");
  service->name = name;
  service->serviceType = SERVICE_TYPE_GENERATOR;
  parseURLMask(service,urlMask);
  service->next = NULL;
  service->serviceFunction = NULL;
  service->serviceTemplateTagFunction = NULL;
  service->paramSpecList = NULL;
  return service;
}

HttpService *makeSimpleTemplateService(char *name, char *urlMask, char *templatePath){
  HttpService *service = (HttpService*)safeMalloc(sizeof(HttpService),"HttpService-Template");
  service->name = name;
  service->serviceType = SERVICE_TYPE_SIMPLE_TEMPLATE;
  parseURLMask(service,urlMask);
  service->templatePath = templatePath;
  service->next = NULL;
  service->serviceFunction = NULL;
  service->serviceTemplateTagFunction = NULL;
  service->paramSpecList = NULL;
  return service;
}

HttpService *makeWebSocketService(char *name, char *urlMask, WSEndpoint *endpoint){
  HttpService *service = (HttpService*)safeMalloc(sizeof(HttpService),"HttpService-WebSocket");
  service->name = name;
  service->serviceType = SERVICE_TYPE_WEB_SOCKET;
  parseURLMask(service,urlMask);
  service->next = NULL;
  service->wsEndpoint = endpoint;
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "putting endpoint 0x%p on service 0x%p\n",endpoint,service);

  service->paramSpecList = NULL;
  return service;
}

HttpService *makeProxyService(char *name, 
                              char *urlMask, 
                              int (*requestTransformer)(HttpConversation *conversation,
                                                        HttpRequest *inputRequest,
                                                        HttpRequest *outputRequest)){
  HttpService *service = (HttpService*)safeMalloc(sizeof(HttpService),"HttpService-WebSocket");
  service->name = name;
  service->serviceType = SERVICE_TYPE_PROXY;
  parseURLMask(service,urlMask);
  service->next = NULL;
  service->paramSpecList = NULL;
  service->requestTransformer = requestTransformer;
  return service;
}


static WorkElementPrefix *makeCloseConversationWorkElement(HttpConversation *conversation) {
  WorkElementPrefix *prefix = (WorkElementPrefix*)safeMalloc31(sizeof(WorkElementPrefix)+sizeof(HttpWorkElement),"HttpWorkElement and Prefix");
  HttpWorkElement *httpPayload = (HttpWorkElement*)(((char*)prefix)+sizeof(WorkElementPrefix));
  memset(prefix,0,sizeof(WorkElementPrefix));
  memcpy(prefix->eyecatcher,"WRKELMNT",8);
  prefix->payloadCode = HTTP_CLOSE_CONVERSATION;
  prefix->payloadLength = sizeof(HttpWorkElement);
  httpPayload->conversation = conversation;

  return prefix;
}

static WorkElementPrefix *makeConsiderCloseConversationWorkElement(HttpConversation *conversation) {
  WorkElementPrefix *prefix = (WorkElementPrefix*)safeMalloc31(sizeof(WorkElementPrefix)+sizeof(HttpWorkElement),"HttpWorkElement and Prefix");
  HttpWorkElement *httpPayload = (HttpWorkElement*)(((char*)prefix)+sizeof(WorkElementPrefix));
  memset(prefix,0,sizeof(WorkElementPrefix));
  memcpy(prefix->eyecatcher,"WRKELMNT",8);
  prefix->payloadCode = HTTP_CONSIDER_CLOSE_CONVERSATION;
  prefix->payloadLength = sizeof(HttpWorkElement);
  httpPayload->conversation = conversation;

  return prefix;
}

static void serializeStartRunning(HttpConversation *conversation) {

  HttpConversationSerialize compare;                                                                                                                                                                                
                                                                                                                                                                                                                 
  HttpConversationSerialize replace;                                                                                                                                                                                
                                                                                                                                                                                                                 
  // Get a compare copy of the conversation serialized data                                                                                                                                                         
  memcpy(&compare,&conversation->serializedData,sizeof(compare));                                                                                                                                                   
                                                                                                                                                                                                                 
  do {                                                                                                                                                                                                              
    // Replicate the compare copy to a replace copy                                                                                                                                                                 
    memcpy(&replace,&compare,sizeof(replace));                                                                                                                                                                      

    // Increment count of running tasks
    replace.runningTasks++;

    // Update the serialized data                                                                                                                                                                                   
#ifdef __ZOWE_OS_ZOS
    if (!cs(&compare.serializedData,      
            &conversation->serializedData,
            replace.serializedData)) {
      break;                    // Successful when function returns 0
    }
#elif defined __GNUC__ || defined __ZOWE_OS_AIX
    if (__sync_bool_compare_and_swap(&conversation->serializedData,                                                                                                                                                 
                                     compare.serializedData,replace.serializedData))                                                                                                                                
      break;                    // Successful when function returns 1                                                                                                                                               
    // Refresh the compare copy of the conversation serialized data                                                                                                                                                 
    memcpy(&compare,&conversation->serializedData,sizeof(compare));

#elif defined(__ZOWE_OS_WINDOWS)

    if (atomic_compare_exchange_strong(&conversation->serializedData,
                                       (unsigned int *)&compare.serializedData,
                                       replace.serializedData))
      break;

    
#else                                                                                                                                                                                                               
  #error Unsupported platform atomic operation                                                                                                                                                                 
#endif                                                                                                                                                                                                              
  } while(1);

  return;                                                                                                                                                                                                           
}                                                                                                                                                                                                                   

/******* Non Threading main loop *******/

#define MAIN_WAIT_MILLIS 10000

static int httpTaskMain(RLETask *task){
  int serviceResult = 0;

  HttpWorkElement *element = (HttpWorkElement*)task->userPointer;
  HttpConversation *conversation = element->conversation;
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "httpTaskMain element=0x%p elt->convo=0x%p\n",element,conversation);
  if (!conversation->shouldClose) {
    /* Execute only if the conversation is still open */
    serviceResult = handleHttpService(conversation->server,
                                      conversation->pendingService,
                                      element->request,
                                      element->response);
  }

  safeFree31((char *)element, sizeof(HttpWorkElement));
  element = NULL;
  task->userPointer = NULL;

  /* Decrement runningTasks and enqueue a considerClose request if one has been requested */
  serializeConsiderCloseEnqueue(conversation,TRUE);

  return serviceResult;
}

static void startHttpTask(RLETask *task){
#ifdef __ZOWE_OS_ZOS
  startRLETask(task,NULL);
#endif
}


static char *wsGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static char *makeWSAccept(ShortLivedHeap *slh, char *key){
  DigestContext context;

  int keyLength = strlen(key);
  int guidLength = strlen(wsGUID);
  char *asciiBuffer = SLHAlloc(slh,keyLength+guidLength);
  char *hash = SLHAlloc(slh,20);

  memcpy(asciiBuffer,key,keyLength);
  memcpy(asciiBuffer+keyLength,wsGUID,guidLength);
  asciify(asciiBuffer,keyLength+guidLength);
  
  digestContextInit(&context,CRYPTO_DIGEST_SHA1);
  digestContextUpdate(&context,asciiBuffer,keyLength+guidLength);

  digestContextFinish(&context,hash);
  int resultSize = 0;
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "before encode base 64 slh=0x%p hash at 0x%x\n",slh,hash);
  char *encoded = encodeBase64(slh,hash,20,&resultSize,
#ifdef __ZOWE_OS_ZOS
                               TRUE
#else
                               FALSE
#endif
                               );
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "b64 encoded size=%d\n", resultSize);
  dumpbuffer(encoded,resultSize);
  return encoded;
}

static char *chooseWSProtocol(ShortLivedHeap *slh, char *clientProtocols){
  int pos = indexOf(clientProtocols,strlen(clientProtocols),',',0);
  if (pos == -1){
    return clientProtocols;
  } else{
    return copyString(slh,clientProtocols,pos);
  }
}

// Response must ALWAYS be finished on return
static void upgradeToWebSocket(HttpConversation *conversation,
                               HttpRequest *request,
                               HttpResponse *response){
  HttpService *service = conversation->pendingService;
  HttpHeader *connection = getHeader(request,"Connection");
  HttpHeader *webSocketKey = getHeader(request,"Sec-WebSocket-Key");
  HttpHeader *webSocketVersion = getHeader(request,"Sec-WebSocket-Version");
  HttpHeader *webSocketProtocol = getHeader(request,"Sec-WebSocket-Protocol");
  HttpHeader *origin = getHeader(request,"origin");
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "ws req: cnxn=%s key=%s version=%s origin=%s\n",
         connection,
         (webSocketKey != NULL ? webSocketKey->nativeValue : "<n/a>"),
         (webSocketVersion != NULL ? webSocketVersion->nativeValue : "<n/a>"),
         (origin != NULL ? origin->nativeValue : "<n/a>"));
  
  if (!headerMatch(webSocketVersion,"13")){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WebSocket version\n");
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"bad web socket version");
    // Response is finished on return
  } else{
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "building web socket response\n");
    setResponseStatus(response,HTTP_STATUS_SWITCHING_PROTOCOLS,"Switching to WebSockets");
    addStringHeader(response,"Upgrade","websocket");
    addStringHeader(response,"Connection","Upgrade");
    addStringHeader(response,"Sec-WebSocket-Accept",makeWSAccept(response->slh,webSocketKey->nativeValue));
    char *negotiatedProtocol = NULL;
    if (webSocketProtocol != NULL){
      negotiatedProtocol = chooseWSProtocol(response->slh,webSocketProtocol->nativeValue);
      addStringHeader(response,"Sec-WebSocket-Protocol",negotiatedProtocol);
    }
    /* parse URI sets up all the URI fragments in request */
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WS Upgrade parse UI\n");
    parseURI(request);
    conversation->wsSession = makeWSSession(conversation,
                                            service,
                                            negotiatedProtocol);
    /* All we should need to write back is a header -
       client should not expect *ANY* body */
    writeHeader(response);
    finishResponse(response);

    WSEndpoint *endpoint = service->wsEndpoint; 
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WS upgrade service=0x%p name=%s endpoint=0x%p\n",
            service,service->name,endpoint);
    if (endpoint->onOpen){
      endpoint->onOpen(conversation->wsSession);
    } else{
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "*** WARNING *** no endpoint found for WS\n");
    }
  }
}

static void logHTTPMethodAndURI(ShortLivedHeap *slh, HttpRequest *request) {
#ifdef __ZOWE_OS_ZOS
  char *method = copyString(slh, request->method, strlen(request->method));
  char *uri = copyString(slh, request->uri, strlen(request->uri));
  destructivelyUnasciify(method);
  destructivelyUnasciify(uri);
#else
  char *method = request->method;
  char *uri = request->uri;
#endif
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "httpserver: method='%s', URI='%s'\n", method, uri);
}

// Response is finished on an error
static void doHttpResponseWork(HttpConversation *conversation)
{
  HttpRequestParser *parser = conversation->parser;
  HttpHeader *header = NULL;
  HttpResponse *response = NULL;

  do {

    if (conversation->shouldError) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "doHttpResponseWork in new shouldError case (%d). Conversation=0x%p\n", 
              conversation->httpErrorStatus,conversation);
      /* makeHttpResponse now gives the response its own SLH */
      response = makeHttpResponse(NULL, parser->slh, conversation->socketExtension->socket);
      response->conversation = conversation;
      conversation->workingOnResponse = TRUE;
      respondWithError(response, conversation->httpErrorStatus, "Error parsing request");
      // Response is finished on return
      break;
    }

    HttpRequest *firstRequest = dequeueHttpRequest(parser);
    if (firstRequest)
    {

      if (logGetLevel(NULL, LOG_COMP_HTTPSERVER) >= ZOWE_LOG_DEBUG2) {
        logHTTPMethodAndURI(parser->slh, firstRequest);
      }

      conversation->requestCount++;
      conversation->isKeepAlive = firstRequest->keepAlive;

      response = makeHttpResponse(firstRequest,parser->slh,conversation->socketExtension->socket);
      /* parse URI after request and response ready for work, have SLH's, etc */
      parseURI(firstRequest);
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "firstReq: looking for service for URI %s. Conversation=0x%p\n",
              firstRequest->uri,conversation);
      header = firstRequest->headerChain;
      while (header){
#ifdef DEBUG        
        printf("  %s=%s\n",header->nativeName,header->nativeValue);
#endif
        header = header->next;
      }
      HttpService *service = findHttpService(conversation->server,firstRequest);
      if (service){
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "doHttpResponseWork serviceName=%s req->isWebSocket=%d\n",
                service->name,firstRequest->isWebSocket);
        conversation->pendingService = service;
        response->conversation = conversation;
        conversation->workingOnResponse = TRUE;
        if (service->serviceType == SERVICE_TYPE_WEB_SOCKET){
          if (firstRequest->isWebSocket){
            upgradeToWebSocket(conversation,firstRequest,response);
            break;
          }
          respondWithError(response,HTTP_STATUS_BAD_REQUEST,"cannot respond with web socket to http request");
          // Response is finished on return
          break;
        }
        if (service->runInSubtask){
          /* response->runningInSubtask = TRUE; */
          conversation->task = makeRLETask(conversation->server->base->rleAnchor,
                                           RLE_TASK_TCB_CAPABLE | RLE_TASK_RECOVERABLE | RLE_TASK_DISPOSABLE,
                                           httpTaskMain);
          HttpWorkElement *workElement = (HttpWorkElement*)safeMalloc31(sizeof(HttpWorkElement),"HttpWorkElement");
          workElement->conversation = conversation;
          workElement->request = firstRequest;
          workElement->response = response;
          response = NULL; /* transfer the ownership of the response to the subtask */
          conversation->task->userPointer = workElement;
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG,
                  "about to start RLE Task from main at 0x%p wkElement=0x%p pendingService=0x%p\n",
                  conversation->task,workElement,conversation->pendingService);

          /* Keep track of number of running tasks */                                                                                                                                                            
          serializeStartRunning(conversation);                                                                                                                                                                   

          startHttpTask(conversation->task);
          break;
        }
        handleHttpService(conversation->server,service,firstRequest,response);
        break;
      }
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "doHttpResponseWork:  no service found. conversation=0x%p\n",conversation);
      respondWithError(response,HTTP_STATUS_NOT_FOUND,"resource or service not found");
      if (!firstRequest->keepAlive) {
        conversation->shouldClose = TRUE;
      }
      break;
    }
    /* didn't get a request; nothing to do */
  } while(0);

}
  
static void doHttpReadWork(HttpConversation *conversation, int readBufferSize){
  int returnCode = 0;
  int reasonCode = 0;
  SocketExtension *socketExtension = conversation->socketExtension;
  Socket *socket = socketExtension->socket;
  ShortLivedHeap *slh = socketExtension->slh; /* refresh this every N-requests */
  HttpRequestParser *parser = conversation->parser;
  char *readBuffer = (conversation->readBuffer ? conversation->readBuffer : SLHAlloc(slh,readBufferSize));

  int bytesRead = socketRead(socket,readBuffer,readBufferSize,&returnCode,&reasonCode);
  if (bytesRead < 1) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "HTTP desiredBytes = %d bytesRead=%d, so a problem or no more available rc=0x%x, reason=0x%x. conversation=0x%p\n",
            readBufferSize,bytesRead,returnCode,reasonCode,conversation);
    conversation->shouldClose = TRUE;
    return; /* can't respond on a bad socket, even with an error */
  } 
  int requestStreamOK = processHttpFragment(parser,readBuffer,bytesRead);
  if (!requestStreamOK) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "Issue with parser status %d\n", parser->httpReasonCode);
    conversation->shouldError = TRUE;
    conversation->httpErrorStatus = parser->httpReasonCode;
  }

  STCBase *stcBase = conversation->server->base;
  WorkElementPrefix *prefix = (WorkElementPrefix*)safeMalloc31(sizeof(WorkElementPrefix)+sizeof(HttpWorkElement),"HttpWorkElement and Prefix");
  HttpWorkElement   *workElement = (HttpWorkElement*)(((char*)prefix)+sizeof(WorkElementPrefix));
  memset(prefix,0,sizeof(WorkElementPrefix));
  memcpy(prefix->eyecatcher,"WRKELMNT",8);
  prefix->payloadCode = HTTP_START_RESPONSE;
  prefix->payloadLength = sizeof(HttpWorkElement);
  memset(workElement,0,sizeof(HttpWorkElement));
  workElement->conversation = conversation;
  stcEnqueueWork(stcBase,prefix);
}

static void doWSReadWork(HttpConversation *conversation, int readBufferSize){
  int returnCode = 0;
  int reasonCode = 0;
  SocketExtension *socketExtension = conversation->socketExtension;
  Socket *socket = socketExtension->socket;
  ShortLivedHeap *slh = socketExtension->slh; /* refresh this every N-requests */
  WSSession *wsSession = conversation->wsSession;
  WSReadMachine *readMachine = wsSession->readMachine;
  char *readBuffer = (conversation->readBuffer ? conversation->readBuffer : SLHAlloc(slh,readBufferSize));

  int bytesRead = socketRead(socket,readBuffer,readBufferSize,&returnCode,&reasonCode);
  if (bytesRead < 1){
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "WS desiredBytes = %d bytesRead=%d, so a problem or no more available rc=0x%x, reason=0x%x, conversation=0x%p\n",
            readBufferSize,bytesRead,returnCode,reasonCode,conversation);
    conversation->shouldClose = TRUE;
    return; /* dangerous to enqueue any work once shouldClose has been set */
  }
  int hasWSMessage = readMachineAdvance(readMachine,readBuffer,wsSession,0,bytesRead);
  if (hasWSMessage){
    STCBase           *stcBase = conversation->server->base;
    WorkElementPrefix *prefix = (WorkElementPrefix*)safeMalloc31(sizeof(WorkElementPrefix)+sizeof(HttpWorkElement),"HttpWorkElement and Prefix");
    HttpWorkElement   *workElement = (HttpWorkElement*)(((char*)prefix)+sizeof(WorkElementPrefix));
    memset(prefix,0,sizeof(WorkElementPrefix));
    memcpy(prefix->eyecatcher,"WRKELMNT",8);
    prefix->payloadCode = HTTP_WS_MESSAGE_RECEIVED;
    prefix->payloadLength = sizeof(HttpWorkElement);
    memset(workElement,0,sizeof(HttpWorkElement));
    workElement->conversation = conversation;
    stcEnqueueWork(stcBase,prefix);
  }
}

static int handlePeerSocketRead(STCBase *base,
                                STCModule *module,
                                Socket *socket,
                                SocketExtension *extension,
                                bool *breakOuterLoop){
  int returnCode = 0;
  int reasonCode = 0;
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "handle peer socket read\n");
  
  SocketExtension *peerExtension = extension;
  HttpConversation *conversation = (HttpConversation*)extension->protocolHandler;
  
  if (NULL == conversation) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "*** peerExtension protocolHandler is NULL ***\n");
    /* we can't do a full conversation cleanup, just close the socket and unregister the socketExtension, leaks be damned */
    socketClose(peerExtension->socket, &returnCode,&reasonCode);
    *breakOuterLoop = true;
    return 8;
  }
  
  if (conversation->shouldClose) {
    /* immediately unregister the socketExtension so we can't enqueue more than one CLOSE_CONVERSATION request */
    stcReleaseSocketExtension(base, peerExtension);
    
    serializeConsiderCloseEnqueue(conversation,FALSE);
    
    *breakOuterLoop = true;
    return 0;
  }

  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "peerExtension at 0x%p, httpConversation at 0x%p\n", 
              peerExtension, conversation);
  
#if defined(__ZOWE_OS_ZOS) || defined(USE_RS_SSL) || defined(USE_RS_TLS)
  int sxStatus = sxUpdateTLSInfo(peerExtension,
                                 1); /* prevent multiple ioctl calls on repeated reads */
  if (0 != sxStatus) {
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "error from sxUpdateTLSInfo: %d\n", sxStatus);
  } else if ((RS_TLS_WANT_TLS & peerExtension->tlsFlags) &&
             (0 == (RS_TLS_HAVE_TLS & peerExtension->tlsFlags)))
    {
      if (0 == (tlsWarnCounter % TLS_WARN_FREQUENCY)) {
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_WARNING, "Connection is insecure! TLS needed but not found on socket.\n");
      }
      tlsWarnCounter++;
    } else if ((RS_TLS_WANT_PEERCERT & peerExtension->tlsFlags) &&
               (0 == (RS_TLS_HAVE_PEERCERT & peerExtension->tlsFlags)))
      {
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "Connection is insecure! Peer certificate wanted but not found.\n");
      }
#endif
  
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "handle read JEDHTTP convo\n");
  /* if successful, these methods enqueue work */
  if (conversation->wsSession){
    doWSReadWork(conversation,READ_BUFFER_SIZE);
  } else {
    doHttpReadWork(conversation,READ_BUFFER_SIZE);
  }
  return 0;
}



static int httpHandleTCP(STCBase *base,
                         STCModule *module,
                         Socket *socket){
  int handlerStatus = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int rsStatus = 0; /* rs_ssl results */

  SocketExtension *extension = (SocketExtension*)socket->userData;

  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "TCP Socket %s is READY, at=0x%p extension=0x%p\n",
          socket->debugName,socket,extension);

  do {

    if ((extension == NULL) || (STC_MODULE_JEDHTTP != extension->moduleID)) {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "httpserver: *** no extension for socket or TCP event passed to wrong module! ***\n");
      handlerStatus = 12;
      break;
    }

    if (extension->isServerSocket) {
      Socket *peerSocket = socketAccept(socket,&returnCode,&reasonCode);
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "TCP Accept return=0x%x reason=0x%x socket %s\n",
              returnCode,reasonCode,(peerSocket ? peerSocket->debugName : "NONE"));
      if (peerSocket == NULL){
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, "httpserver: accept failed ret=%d reason=0x%x\n",returnCode,reasonCode);
        break; /* end server socket processing */
      }
#ifdef USE_RS_SSL
      if (NULL != socket->sslHandle) {
        /* server socket->sslHandle is RS_SSL_ENVIRONMENT */
        rsStatus = rs_ssl_negotiate_withConnectedSocket(socket->sslHandle, /* RS_SSL_ENVIRONMENT */
                                                        IOMODE_BLOCKING,
                                                        peerSocket->sd,
                                                        &(peerSocket->sslHandle)); /* RS_SSL_CONNECTION */
        if ((0 != rsStatus) || (NULL == peerSocket->sslHandle)) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, 
		  "httpserver failed to negotiate TLS with peer; closing socket, rsStatus=%d skt.dbg.name=%s\n",
		  rsStatus,(peerSocket ? peerSocket->debugName : "NONE"));
          socketClose(peerSocket, &returnCode, &reasonCode);
          break;
        }
      }
#endif
#ifdef USE_ZOWE_TLS
      if (NULL != socket->tlsEnvironment) {
        int rc = tlsSocketInit(socket->tlsEnvironment,
                               &peerSocket->tlsSocket,
                               peerSocket->sd,
                               true);
        if ((0 != rc) || (NULL == peerSocket->tlsSocket)) {
          zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG, 
		  "httpserver failed to negotiate TLS with peer; closing socket, tlsInit.rc=%d skt.dbg.name=%s\n",
		  rc,(peerSocket ? peerSocket->debugName : "NONE"));
          socketClose(peerSocket, &returnCode, &reasonCode);
          break;
        }
      }
#endif // USE_ZOWE_TLS
      ShortLivedHeap *slh = makeShortLivedHeap(READ_BUFFER_SIZE,100);
  #ifndef __ZOWE_OS_WINDOWS
      int writeBufferSize = 0x40000;
      setSocketWriteBufferSize(peerSocket,0x40000, &returnCode, &reasonCode);
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "set Write buffer size to 0x%x, return=%d reason=0x%x\n",
              writeBufferSize, returnCode, reasonCode);
  #else
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "must figure out how to set socket buffer size on windows\n");
  #endif
      int nbStatus = setSocketBlockingMode(peerSocket,TRUE,&returnCode,&reasonCode);
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "setNonBlock status=%d return=%d reason=0x%x\n",
              nbStatus,returnCode,reasonCode);
      SocketExtension *peerExtension = makeSocketExtension(peerSocket,
                                                           slh,
                                                           FALSE,
                                                           NULL,
                                                           READ_BUFFER_SIZE);
      peerSocket->userData = peerExtension;
      HttpConversation *httpConversation = makeHttpConversation(peerExtension,(HttpServer*)(module->data));
      stcRegisterSocketExtension(base, peerExtension, STC_MODULE_JEDHTTP);

      break; /* end server socket processing */

    } else {
      bool shouldBreak = false;
      handlerStatus = handlePeerSocketRead(base,module,socket,extension,&shouldBreak);
      if (shouldBreak){
        break;
      }
    } /* end peer socket handling */
    
  } while(0);

  return handlerStatus;
}

typedef struct PipeSocketEntry_tag{
  int uniqueID;
  Socket *demuxSocket;
  Socket *handlerSocket;  /* used by http/tcp code */
  Socket *muxSocket;      
} PipeSocketEntry;

static PipeSocketEntry *getOrMakePipeSocketEntry(HttpServer *server, 
						 STCModule *module, 
						 Socket *demuxSocket, 
						 int uniqueID,
						 int *pipeError){
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "getOrMake=%d\n",uniqueID);
  PipeSocketEntry *entry = htIntGet(server->syntheticPipeSockets,uniqueID);
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "ht result = 0x%p\n",entry);

  if (entry == NULL){
    int fdsToHttp[2] ={ -1, -1};  /* readEnd, writeEnd */
    int fdsFromHttp[2] ={ -1, -1};
    if (pipe(fdsToHttp)){
      *pipeError = errno;
      return NULL;
    }
    if (pipe(fdsFromHttp)){
      *pipeError = errno;
      return NULL;
    }
    int httpRequestIn = fdsToHttp[0];
    int httpResponseOut = fdsFromHttp[1];
    int httpRequestOut = fdsToHttp[1];
    int httpResponseIn = fdsFromHttp[0];

    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2,"hanlderSocket pipes %d %d\n",httpRequestIn,httpResponseOut);
    Socket *handlerSocket = makePipeBasedSyntheticSocket(IPPROTO_SYNTHETIC_PIPE_HANDLER,httpRequestIn,httpResponseOut);
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2,"muxSocket pipes %d %d\n",httpResponseIn,httpRequestOut);
    Socket *muxSocket = makePipeBasedSyntheticSocket(IPPROTO_SYNTHETIC_PIPE_MULTIPLEXER,httpResponseIn,httpRequestOut);
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2,"handlerSocket=0x%p mux(fwd)socket=0x%p\n",handlerSocket,muxSocket);
    SocketExtension *handlerExtension = makeSocketExtension(handlerSocket,
							     server->slh,
							     FALSE, /* allocateInSLH */
							     NULL,
							     READ_BUFFER_SIZE);
    SocketExtension *muxExtension = makeSocketExtension(muxSocket,
							server->slh,
							FALSE, /* allocateInSLH */
							NULL,
							READ_BUFFER_SIZE);
    
    handlerSocket->userData = handlerExtension;
    muxSocket->userData = muxExtension;

    HttpConversation *httpConversation = makeHttpConversation(handlerExtension,(HttpServer*)(module->data));

    stcRegisterSocketExtension(server->base, handlerExtension, STC_MODULE_JEDHTTP);
    stcRegisterSocketExtension(server->base, muxExtension, STC_MODULE_JEDHTTP);

    entry = (PipeSocketEntry*)safeMalloc(sizeof(PipeSocketEntry),"PipeSocketEntry");
    entry->uniqueID = uniqueID;
    entry->demuxSocket = demuxSocket;
    entry->handlerSocket = handlerSocket;
    entry->muxSocket = muxSocket;
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "PSEntry at 0x%p\n",entry );
    zowedump(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, (char*)entry,sizeof(PipeSocketEntry));
    muxExtension->protocolHandler = entry;

    htIntPut(server->syntheticPipeSockets,uniqueID,entry);
  }
  return entry; 
}

/* this is called when PIPE has data 
 */

/* this method *only* runs in the STCBase main thread, putting and effective mutex 
   on the pipes that talk up/down to process that owns this httpserver */
static int httpHandlePipe(STCBase *base,
			  STCModule *module,
			  Socket *socket){
  int handlerStatus = 0;
  int returnCode = 0;
  int reasonCode = 0;
  HttpServer *server = (HttpServer*)(module->data);
  SocketExtension *extension = (SocketExtension*)socket->userData;
  switch (socket->protocol){
  case IPPROTO_SYNTHETIC_PIPE_DEMULTIPLEXER:  
    {
      /* every pipe chunk for read ready must be > 4 in size */
      char *readBuffer = extension->readBuffer;
      
      int bytesRead = socketRead(socket,readBuffer,8,&returnCode,&reasonCode);
      if (returnCode){
        printf("pipe IO error 1 ret=%d reason=0x%x\n",returnCode,reasonCode);
        return 12;
      } else if (bytesRead != 8){
        printf("pipe fragment short read 1\n");
        return 12;
      }
      int magic = *((int*)readBuffer);
      if (magic != TCP_FRAGMENT_MAGIC){
	printf("pipe bad tunnel magic\n");
	return 12;
      }
      int headerLength = *((uint16_t*)(readBuffer+4));
      printf("pipe headerLength=0x%x\n",headerLength);
      int expectedBytes = headerLength-8;
      bytesRead = socketRead(socket,readBuffer+8,expectedBytes,&returnCode,&reasonCode);
      if (returnCode){
	printf("pipe IO error 2 ret=%d reason=0x%x\n",returnCode,reasonCode);
	return 12;
      } else if (bytesRead != expectedBytes){
	printf("pipe fragment short read 1\n");
	return 12;
      }
      TCPFragment *fragment = (TCPFragment*)readBuffer;
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "pipe header\n");
      zowedump(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, readBuffer,headerLength);
      bytesRead = socketRead(socket,readBuffer+headerLength,fragment->payloadLength,&returnCode,&reasonCode);
      if (returnCode){
	printf("pipe IO error 3 ret=%d reason=0x%x\n",returnCode,reasonCode);
	return 12;
      } else if (bytesRead != fragment->payloadLength){
	printf("pipe fragment short read 1\n");
	return 12;
      }
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "pipe payload\n");
      zowedump(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, readBuffer+headerLength,fragment->payloadLength);
      
      int uniqueID = fragment->remoteClientID;
      int pipeErrno = 0;
      PipeSocketEntry *entry = getOrMakePipeSocketEntry(server,module,socket,uniqueID,&pipeErrno);
      if (entry){
	Socket *forwardingSocket = entry->muxSocket;
	
	zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "entry=0x%p forwardingSocket=0x%p %p\n",
		entry,forwardingSocket,(forwardingSocket ? forwardingSocket->debugName : ""));
	socketWrite(forwardingSocket,readBuffer+headerLength,fragment->payloadLength,&returnCode,&reasonCode);
	if (returnCode){
	  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_SEVERE, "Pipe-forwarding tunneled Packet failed ret=%d, reason=0x%x\n",returnCode,reasonCode);
	}
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "done forwarding\n");
      } else{
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_WARNING, "pipe creation failure, errno was %d\n",pipeErrno);
      }
    }
  break;
  case IPPROTO_SYNTHETIC_PIPE_HANDLER:
    {
      /* simple case, we are just a normal socket, almost */
      bool breakOuterLoop = false;
      handlerStatus = handlePeerSocketRead(base,module,socket,extension,&breakOuterLoop);
    }
  break;
  case IPPROTO_SYNTHETIC_PIPE_MULTIPLEXER:
    {
      /* tack on Fragment header and go back */
      PipeSocketEntry *entry = (PipeSocketEntry*)extension->protocolHandler;
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "handlePipe MUX case, entry=0x%p\n",entry);
      if (entry == NULL){
	printf("no PipeSocketEntry found for socket %s\n",socket->debugName);
      } else{
	char *readBuffer = extension->readBuffer;
	Socket *demuxSocket = entry->demuxSocket;
	Socket *muxSocket = socket;  /* the one that is readable */
	int bytesRead = socketRead(muxSocket,readBuffer,READ_BUFFER_SIZE,&returnCode,&reasonCode);
	zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "MUX case: 0x%x bytes back from handler, ret=%d reason=0x%x\n",bytesRead,returnCode,reasonCode);
	if (bytesRead > 0){
	  int dumpLen = bytesRead > 0x400 ? 0x400 : bytesRead;
	  zowedump(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, readBuffer,dumpLen);
	  TCPFragment fragmentStorage;
	  TCPFragment *fragment = &fragmentStorage;
	  memset(fragment,0,sizeof(TCPFragment));
	  fragment->magic = TCP_FRAGMENT_MAGIC;
	  fragment->headerLength = sizeof(TCPFragment);
	  fragment->type = TCP_FRAGMENT_TO_REMOTE;
	  fragment->remoteClientID = entry->uniqueID;
	  fragment->payloadLength = bytesRead;
	  socketWrite(demuxSocket,(char*)fragment,sizeof(TCPFragment),&returnCode,&reasonCode);
	  FILE *tempOut = fdopen(demuxSocket->pipeOutputSD,"w");
	  if (tempOut){
	    fflush(tempOut);
	  } else{
	    printf("could not make temp out\n");
	  }
	  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG2, "MUX wrote to socket %s\n",demuxSocket->debugName);
	  if (returnCode){
	    printf("demultiplexing write failed ret=%d reason=0x%x\n",returnCode,reasonCode);
	  } else{
	    socketWrite(demuxSocket,readBuffer,bytesRead,&returnCode,&reasonCode);
	    if (returnCode){
	      printf("demultiplexing payload write failed ret=%d reason=0x%x\n",returnCode,reasonCode);
	    }
	  }
	} else{
	  printf("oddly, no data for MUX socket read\n");
	}
      }
    }
  break;
  }
  
  return handlerStatus;
}

static int httpServerIOTrace = FALSE;

/* this is a testing API for testing response generation without an HTTP server running */

HttpResponse *pseudoRespond(HttpServer *server, HttpRequest *request, ShortLivedHeap *slh){
  HttpResponse *response = makeHttpResponse(request,slh,NULL);
  response->standaloneTestMode = TRUE;
  parseURI(request);
  HttpService *service = findHttpService(server,request);
  zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "in pseudoRespond, service=0x%p\n",service);
  if (service){
    /* what about output streams */
    handleHttpService(server,service,request,response);
    return response;
  } else{
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "could not find service for pseudoRespond\n");
    return NULL;
  }
}

int httpWorkElementHandler(STCBase *base,
                           STCModule *module,
                           WorkElementPrefix *prefix) {
  int status = 0;
  switch (prefix->payloadCode) {
  case HTTP_CONSIDER_CLOSE_CONVERSATION:
    {
      if (traceHttpCloseConversation) {
        printf("Starting CONSIDER_CLOSE_CONVERSATION processing\n");
        fflush(stdout);
      }
      int bpxrc=0, bpxrsn=0;
      HttpWorkElement *workElement = (HttpWorkElement*)((char*)prefix + sizeof(WorkElementPrefix));
      HttpConversation *conversation = workElement->conversation;
      SocketExtension *sext = conversation->socketExtension;

      if (conversation->runningTasks == 0 && !conversation->closeEnqueued) {
        if (traceHttpCloseConversation) {
          printf("ready to close conversation at address 0x%p... enqueueing\n", conversation);
        }

        WorkElementPrefix *prefix = makeCloseConversationWorkElement(conversation);
        stcEnqueueWork(conversation->server->base, prefix);
        conversation->closeEnqueued = TRUE;
      } else {
        if (traceHttpCloseConversation) {
          printf("not ready to close conversation at address 0x%p... runningTasks=%d, closeEnqueued=%d\n",
                 conversation, conversation->runningTasks, conversation->closeEnqueued);
        }
      }

      break;
    }

  case HTTP_CLOSE_CONVERSATION:
    {
      if (traceHttpCloseConversation) {
        printf("Starting CLOSE_CONVERSATION processing\n");
        fflush(stdout);
      }
      int bpxrc=0, bpxrsn=0;
      HttpWorkElement *workElement = (HttpWorkElement*)((char*)prefix + sizeof(WorkElementPrefix));
      HttpConversation *conversation = workElement->conversation;
      SocketExtension *sext = conversation->socketExtension;

      if (conversation->runningTasks > 0) {
        if (traceHttpCloseConversation) {
          printf("*** PANIC: an RLETask race condition has occured. Abandoning cleanup ***");
        }

        break;
      }

      if (traceHttpCloseConversation) {
        printf("closing HttpConversation at address 0x%p...\n", conversation);
        fflush(stdout);
      }

      /* immediately close the socket and unregister the socket extension */
      if (traceHttpCloseConversation) {
        printf("closing the socket...\n");
        fflush(stdout);
      }
      socketClose(sext->socket, &bpxrc, &bpxrsn);
      if (traceHttpCloseConversation) {
        printf("socketClose results: rc=%d, rsn=0x%x\n", bpxrc, bpxrsn);
        fflush(stdout);
      }
      if (traceHttpCloseConversation) {
        printf("unregistering the socketExtension...\n");
        fflush(stdout);
      }
      stcReleaseSocketExtension(base, sext);

      /* clean up the non-SLH conversation contents */
      if (conversation->wsSession) {
        if (traceHttpCloseConversation) {
          printf("wsSession cleanup...\n");
          fflush(stdout);
        }
        WSSession *wss = conversation->wsSession;
        if (wss->readMachine) {
          if (traceHttpCloseConversation) {
            printf("WSReadMachine cleanup...\n");
            fflush(stdout);
          }
          WSReadMachine *wssrm = wss->readMachine;
          if (wssrm->payloadStream) {
            if (traceHttpCloseConversation) {
              printf("BigBuffer cleanup...\n");
              fflush(stdout);
            }
            freeBigBuffer(wssrm->payloadStream, 1);
            wssrm->payloadStream = NULL;
          }
          safeFree((char*)wssrm, sizeof(WSReadMachine));
        }
        if (wss->negotiatedProtocol) {
          if (traceHttpCloseConversation) {
            printf("negotiatedProtocol cleanup...\n");
            fflush(stdout);
          }
          safeFree((char*)wss->negotiatedProtocol, 1 + strlen(wss->negotiatedProtocol));
        }
        safeFree((char*)wss, sizeof(WSSession));
      } /* end wsSession cleanup */
      /* the HttpRequestParser was allocated on the (sext's) SLH */
      if (traceHttpCloseConversation) {
        printf("clearing and freeing the conversation structure...\n");
        fflush(stdout);
      }
      memset(conversation, 0x00, sizeof(HttpConversation));
      safeFree((char*)conversation, sizeof(HttpConversation));

      /* finally, free the socketExtension contents and the socketExtension itself */
      if (sext->socket) {
        if (traceHttpCloseConversation) {
          printf("freeing the socket...\n");
          fflush(stdout);
        }
        safeFree((char*)sext->socket, sizeof(Socket));
      }
      if (sext->slh) {
        if (traceHttpCloseConversation) {
          printf("freeing the SLH...\n");
          fflush(stdout);
        }
        SLHFree(sext->slh);
      }
      /* other socketExtension contents (peerCert, readBuffer) were on the SLH */
      if (traceHttpCloseConversation) {
        printf("clearing and freeing the socketExtension...\n");
        fflush(stdout);
      }
      memset(sext, 0x00, sizeof(SocketExtension));
      safeFree((char*)sext, sizeof(SocketExtension));
      
      if (traceHttpCloseConversation) {
        printf("free'ing CLOSE_CONVERSATION prefix and payload\n");
        fflush(stdout);
      }
      
      if (traceHttpCloseConversation) {
        printf("CLOSE_CONVERSATION processing complete\n");
        fflush(stdout);
      }
    }
    break; /* end HTTP_CLOSE_CONVERSATION */

  case HTTP_START_RESPONSE:
    {
      HttpWorkElement *workElement = (HttpWorkElement*)((char*)prefix + sizeof(WorkElementPrefix));
      doHttpResponseWork(workElement->conversation);
    }
    break;

  case HTTP_CONTINUE_OUTPUT:
    {
      HttpWorkElement *workElement = (HttpWorkElement*)((char*)prefix + sizeof(WorkElementPrefix));
      writeFully(workElement->response->socket,workElement->buffer,workElement->bufferLength);
      if (workElement->reclaimAfterWrite){
        safeFree31(workElement->buffer,workElement->bufferLength);
      }
      /* maybe close or mark for ready-to-close */
    }
    break;

  case HTTP_WS_MESSAGE_RECEIVED:
    {
      HttpWorkElement *workElement = (HttpWorkElement*)((char*)prefix + sizeof(WorkElementPrefix));
      HttpConversation *conversation = workElement->conversation;
      if (conversation->wsSession){
        WSSession *session = conversation->wsSession;
        while (TRUE) {
          WSMessage *message = getNextMessage(session->readMachine);
          if (message == NULL) {
            break;
          }
          if (session->messageHandler){
            session->messageHandler->onMessage(session,message);
          } else{
            zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WS Session has no message handler !!\n");
          }
        }
      } else{
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WS Message received without wsSession! conversation 0x%p\n", conversation);
        conversation->shouldClose = TRUE;
      }
    }
    break;
  case HTTP_WS_CLOSE_HANDSHAKE:
  case HTTP_WS_OUTPUT:
    {
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WS_OUTPUT\n");

      HttpWorkElement *workElement = (HttpWorkElement*)((char*)prefix + sizeof(WorkElementPrefix));
      HttpConversation *conversation = workElement->conversation;
      SocketExtension *socketExtension = conversation->socketExtension;
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "wkElt=0x%p convo=0x%p sockExt=0x%p\n",
              workElement,conversation,socketExtension);
      int dumpLength = workElement->bufferLength;
      if (dumpLength > 1024){
        dumpLength = 1024;
      }
      zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "HTTP_WS_OUTPUT: writing 0x%x bytes, dumping=0x%x\n",
              workElement->bufferLength,dumpLength);
      dumpbufferA(workElement->buffer,dumpLength);
      fflush(stdout);
      writeFully(socketExtension->socket,workElement->buffer,workElement->bufferLength);
      if (workElement->reclaimAfterWrite){
        safeFree31(workElement->buffer,workElement->bufferLength);
      }
          
      /* after sending a close response, close is ok */
      if (prefix->payloadCode == HTTP_WS_CLOSE_HANDSHAKE){
        zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "WS_CLOSE_HANDSHAKE: sent message, setting shouldClose. Conversation 0x%p \n",conversation);
        conversation->shouldClose = TRUE;
      }      
    }
    break;

  default:
    zowelog(NULL, LOG_COMP_HTTPSERVER, ZOWE_LOG_DEBUG3, "httpServer workElementHandler saw an unknown payloadCode\n");
    status = 8;
    break;

  }

  safeFree((char*)prefix, sizeof(WorkElementPrefix) + sizeof(HttpWorkElement));
  prefix = NULL;

  return status;
}

int httpBackgroundHandler(STCBase *base, STCModule *module, int selectStatus) {
  if (httpServerIOTrace){
#ifdef __ZOWE_OS_ZOS
    printf("SELECTX (Mk 2), selectStatus=0x%x, qReadyECB=0x%x\n",selectStatus,base->qReadyECB);
#else
    printf("SELECTX woke up due to selectFiring status=0x%x\n",selectStatus);
#endif
    if (selectStatus == STC_BASE_SELECT_FAIL){
      printf("stcBaseSelect Failed - internal error\n");
    } else if (selectStatus == STC_BASE_SELECT_TIMEOUT){
      printf("stcBaseSelect timed out \n");
    } else if (selectStatus == STC_BASE_SELECT_NOTHING_OBVIOUS_HAPPENED){
      printf("stcBaseSelect didn't see anything\n");
      fflush(stdout);
    }
  }
  return 0;
}

void registerHttpServerModuleWithBase(HttpServer *server, STCBase *base)
{
  /* server pointer will be copied/accessible from module->data */
  STCModule *httpModule = stcRegisterModule2(base,
                                             STC_MODULE_JEDHTTP,
                                             server,
                                             httpHandleTCP,
                                             NULL,
                                             httpHandlePipe,
                                             httpWorkElementHandler,
                                             httpBackgroundHandler);
}

int mainHttpLoop(HttpServer *server){
  STCBase *base = server->base;
  /* server pointer will be copied/accessible from module->data */
  STCModule *httpModule = stcRegisterModule2(base,
                                             STC_MODULE_JEDHTTP,
                                             server,
                                             httpHandleTCP,
                                             NULL,
                                             httpHandlePipe,
                                             httpWorkElementHandler,
                                             httpBackgroundHandler);
  
  return stcBaseMainLoop(base, MAIN_WAIT_MILLIS);
}




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
