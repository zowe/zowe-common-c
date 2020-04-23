

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __HTTPSERVER__
#define __HTTPSERVER__ 1

#include "stcbase.h"
#include "http.h"
#include "json.h"
#include "xml.h"
#include "unixfile.h"
#include "../jwt/jwt/jwt.h"

/** \file 
 *  \brief httpserver.h is the interface of an ultra-lightweight c-based web server.
 *
 * This is the definition of the HTTP server.  It is ultra-lightweight
 * and is built on the STCBase infrastructure. 
 * 
 * This server define a set of services through registerHttpService.  
 * This server supports WebSockets.
 */

#define HTTP_REQUEST_KEEP_ALIVE 1
#define HTTP_REQUEST_CHUNKED    2
#define HTTP_REQUEST_SESSIONID_FROM_COOKIE 4
#define HTTP_REQUEST_SESSIONID_FROM_URL 8

#define CHAR_ENCODING_ISO_8859_1 2

#define SERVICE_TYPE_FILES           1
#define SERVICE_TYPE_GENERATOR       2
#define SERVICE_TYPE_SIMPLE_TEMPLATE 3
#define SERVICE_TYPE_WEB_SOCKET      4
#define SERVICE_TYPE_PROXY           5
#define SERVICE_TYPE_FILES_SECURE    6

#define SERVICE_AUTH_NONE   1
#define SERVICE_AUTH_SAF    2
#define SERVICE_AUTH_CUSTOM 3 /* done by service */
#define SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN 4

#define SERVICE_AUTH_TOKEN_TYPE_LEGACY                    0
#define SERVICE_AUTH_TOKEN_TYPE_JWT_WITH_LEGACY_FALLBACK  1
#define SERVICE_AUTH_TOKEN_TYPE_JWT                       2

#define SERVICE_MATCH_WILD_RIGHT 1

#define HTTP_SERVICE_SUCCESS 0
#define HTTP_SERVICE_FAILED  8
#define FILE_STREAM_BUFFER_SIZE 1024000

#define HTTP_SERVER_MAX_SESSION_TOKEN_KEY_SIZE 1024u

#define HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY  "zisServerName"

typedef struct BigBuffer_tag{
  ShortLivedHeap *slh;  /* can be null */
  char *data;
  int pos;
  int size;
} BigBuffer;


typedef struct ChunkedOutputStream_tag{
  Socket *socket;
  char   *buffer;
  char   *translateBuffer;
  int     bufferSize;
  int     fill;
  struct  HttpResponse_tag *response;
  bool    isErrorState;

} ChunkedOutputStream;

typedef struct HttpRequestParser{
  ShortLivedHeap *slh;
  HttpRequest *request;  /* where the result gets built */
  int state;
  /* */
  int methodNameLength;
  char methodName[MAX_HTTP_METHOD_NAME+8];
  int versionLength;
  char version[MAX_HTTP_VERSION+8];
  int uriLength;
  char uri[MAX_HTTP_URI+8];
  int headerNameLength;
  char headerName[MAX_HTTP_FIELD_NAME+8];
  int headerValueLength;
  char headerValue[MAX_HTTP_FIELD_VALUE+8];
  int chunkSize;
  HttpHeader *headerChain;
  int isChunked;
  int isWebSocket;
  int entityLength;
  char *contentType; /* in ebcdic */
  int specifiedContentLength;
  int remainingContentLength; /* for fixed length body */
  char *content;
  char *contentTmp;
  int httpReasonCode;
  char *message;
  HttpRequest *requestQHead;
} HttpRequestParser;


typedef struct HttpResponse_tag{
  HttpRequest    *request;
  int             status;
  char           *message;
  HttpHeader     *headers;
  ShortLivedHeap *slh;
  int             runningInSubtask;
  Socket         *socket;
  int             responseTypeChosen;
  struct HttpConversation_tag *conversation;
  /* The following three fields embody the 3 ways output can be generated.  Their "respondWith..." functions will fail
     if responseTypeChosen is TRUE */
  ChunkedOutputStream *stream;
  xmlPrinter     *p;
  jsonPrinter    *jp;
  char           *sessionCookie;
  int             standaloneTestMode;
} HttpResponse;


typedef struct HttpTemplateTag_tag{
  char *placeName;
} HttpTemplateTag;

struct HttpService_tag;

typedef int HttpServiceInit(void);
typedef int HttpServiceServe(struct HttpService_tag *service, HttpResponse *response);
typedef int AuthExtract(struct HttpService_tag *service, HttpRequest *request);
typedef int AuthValidate(struct HttpService_tag *service, HttpRequest *request);
typedef int HttpServiceInsertCustomHeaders(struct HttpService_tag *service, HttpResponse *response);

/*
  returns HTTP_SERVICE_SUCCESS or other fail codes in same group 
*/
typedef int HttpServiceServeTemplateTag(struct HttpService_tag *service, HttpResponse *response,
					HttpTemplateTag *templateTag, ChunkedOutputStream *outStream);
typedef int HttpServiceTerm(void);
typedef int HttpServiceCleanup(struct HttpService_tag *service, HttpResponse *response);

#define SERVICE_ARG_OPTIONAL          0
#define SERVICE_ARG_REQUIRED          1
#define SERVICE_ARG_HAS_LOW_BOUND     2
#define SERVICE_ARG_HAS_HIGH_BOUND    4
#define SERVICE_ARG_HAS_VALUE_BOUNDS  6
#define SERVICE_ARG_HAS_MIN_WIDTH     8
#define SERVICE_ARG_HAS_MAX_WIDTH    16
#define SERVICE_ARG_HAS_WIDTH_BOUNDS 24

#define SERVICE_ARG_TYPE_INT      1
#define SERVICE_ARG_TYPE_STRING   2
#define SERVICE_ARG_TYPE_DATE     3
#define SERVICE_ARG_TYPE_HEX      4
#define SERVICE_ARG_TYPE_INT64    5

typedef struct HttpService_tag{
  struct HttpServer_tag *server;
  char  *name;
  char  *pathRoot; /* for file services */
  char  *urlMask;
  char  *templatePath; /* for templated services */
  int    parsedMaskPartCount;
  char **parsedMaskParts;
  int    matchFlags;
  int    serviceType; 
  int    authType;
  int    runInSubtask;
  void  *authority; /* NULL unless AUTH_CUSTOM */
  AuthExtract                    *authExtractionFunction;
  HttpServiceInit                *initFunction;
  HttpServiceServe               *serviceFunction;
  HttpServiceServeTemplateTag    *serviceTemplateTagFunction;
  HttpServiceInsertCustomHeaders *customHeadersFunction;
  HttpServiceTerm                *termFunction;
  HttpServiceCleanup             *cleanupFunction;
  HttpRequestParamSpec           *paramSpecList;
  struct WSEndpoint_tag          *wsEndpoint;
  /* only used for proxy service */
  int (*requestTransformer)(struct HttpConversation_tag *conversation,
                            HttpRequest *inputRequest,
                            HttpRequest *outputRequest);
  struct HttpService_tag      *next;
  uint64 serverInstanceUID;   /* may be something smart at some point. Now just startup STCK */
  void  *userPointer;      /* a place to hang service-wide data */
  void  *sharedServiceMem; /* server-wide, address shared by all HttpServices */
  const char *productURLPrefix; /* provided by the server */
  int doImpersonation;
  AuthValidate                   *authValidateFunction;
} HttpService;

typedef struct HTTPServerConfig_tag {
  int port;
  HttpService *serviceList;
  HttpService *serviceOfLastResort;
  unsigned int sessionTokenKeySize;
  unsigned char sessionTokenKey[HTTP_SERVER_MAX_SESSION_TOKEN_KEY_SIZE];
  JwtContext *jwtContext;
  int authTokenType; /* SERVICE_AUTH_TOKEN_TYPE_... */
} HttpServerConfig;

typedef struct HttpServer_tag{
  STCBase          *base;
  ShortLivedHeap   *slh;               /* not used too much */
  hashtable        *properties;
  HttpServerConfig *config;
  char             *defaultProductURLPrefix;
  uint64           serverInstanceUID;   /* may be something smart at some point. Now just startup STCK */
  void             *sharedServiceMem; /* address shared by all HttpServices */
  hashtable        *loggingIdsByName; /* contains a map of pluginID -> loggingID */
} HttpServer;

typedef struct WSReadMachine_tag{
  int  state;
  char headerBuffer[20];
  int  headerFill;
  int  headerNeed;
  int  maskValue;
  int  flagAndOpcodeByte;
  char maskBytes[4];
  int  payloadLengthLength;
  int  fin;
  int  headerRead;
  int  isMasked;
  int  trace;
  int64 payloadLength;
  int64 payloadFill;
  int maskModulus;
  BigBuffer *payloadStream;
  WSMessage *currentMessage;
  WSMessage *messageQueueHead;
} WSReadMachine;

struct WSSession_tag;

typedef struct WSMessageHandler_tag{
  void (*onMessage)(struct WSSession_tag *session,
                    struct WSMessage_tag *message);
  void *userPointer;
} WSMessageHandler;

typedef struct WSEndpoint_tag{
  int isServerEndpoint;
  void (*onOpen)(struct WSSession_tag *session);
  void (*onError)(struct WSSession_tag *session);
  void (*onClose)(struct WSSession_tag *session);
} WSEndpoint;

typedef struct WSSession_tag {
  struct HttpConversation_tag *conversation;
  HttpService      *service;
  WSReadMachine    *readMachine;
  Socket           *socket;  
  char             *negotiatedProtocol;
  char             *protocolVersion;
  int               isServer;
  int               isSecure;
  int               closeRequested;
  int               abortRequested;
  int               wsOutputState;
  WSMessageHandler *messageHandler;
  jsonPrinter      *jp;
  int               outputFrameSize;
  int               outputFramePos;
  char             *outputFrameBuffer;
  char             *conversionBuffer;
  int               isOutputBinary;
  int               outputCCSID;
  void             *userPointer;
} WSSession;

/*
  An HttpConversation is a single-struct omnibus data structure
  for integrating Http and WebSocket services into an asynchronous
  IO and task management framework. 
 */

#define CONVERSATION_UNKNOWN   1
#define CONVERSATION_HTTP      2
#define CONVERSATION_WEBSOCKET 3
#define CONVERSATION_HTTP_PROXY 4

#define HTTP_START_RESPONSE              (STC_MODULE_JEDHTTP|0x0001)
#define HTTP_CONTINUE_OUTPUT             (STC_MODULE_JEDHTTP|0x0002)
#define HTTP_WS_MESSAGE_RECEIVED         (STC_MODULE_JEDHTTP|0x0003)
#define HTTP_WS_MORE_INPUT               (STC_MODULE_JEDHTTP|0x0004)
#define HTTP_WS_OUTPUT                   (STC_MODULE_JEDHTTP|0x0005)
#define HTTP_WS_CLOSE_HANDSHAKE          (STC_MODULE_JEDHTTP|0x0006)
#define HTTP_CLOSE_CONVERSATION          (STC_MODULE_JEDHTTP|0x0007)
#define HTTP_CONSIDER_CLOSE_CONVERSATION (STC_MODULE_JEDHTTP|0x0008)

// When updating HttpCOnversationSerialize fields in the conversation
// block:
//
//   - shouldClose may be set to TRUE without serialization.  Once
//     set to TRUE it must never be reset.
//   - closeEnqueued may be set to TRUE without serialization.  Once
//     set to TRUE it must never be reset.
//   - considerCloseEnqueued must only be set to TRUE using serialization
//     to guarantee it is only set to TRUE one time.  Once set to
//     TRUE it must never be reset.
//   - runningTasks may be set to TRUE without serialization.  It must
//     only be set to FALSE using serialization to guarantee that a
//     concurrent change to shouldClose will be recognized and handled.
//
// Note: closeEnqueued does not need to be part of HttoConversationSerialize,
//       but was placed into it to keep all data related to close processing
//       together.
//
// When checking to see if a considerClose request should be enqueued
// by the main task because shouldClose is set-
//
//   1) Create a compare copy of serializedData.
//   2) The compare copy of serializedData should be checked for
//      runningTasks *not* being set, shouldClose being set, and
//      considerCloseEnqueued not being set.  If all conditions are
//      true then-
//      A) Create a replacement copy of serializedData with
//         considerCloseEnqueued set.
//      B) Use compare-and-swap to update serializedData.
//        i)  If the compare-and-swap succeeds, enqueue a considerClose
//            request.
//        ii) If the compare-and-swap fails, restart from 1).
//
// When checking to see if a considerClose request should be enqueued
// by a subtask because shouldClose is set during termination-
//
//   1) Create a compare copy of serializedData.
//   2) Create a replacement copy of serializedData with
//      runningTasks reset.
//   3) The compare copy of serializedData should be checked for
//      shouldClose being set, and if true the replacement copy of
//      serializedData updated to have considerCloseEnqueued set.
//   4) Use compare-and-swap to update serializedData.
//      A) If the compare-and-swap succeeds and compare copy has
//         shouldClose set, enqueue a considerClose request.
//      B) If the compare-and-swap fails, restart from 1).

typedef union HttpConversationSerialize_tag {
  unsigned int       serializedData;
  struct {
    char               shouldClose;    
    int                considerCloseEnqueued : 1;
    int                closeEnqueued : 1;
    int                reserved : 6; 
    short int          runningTasks;
  };
} HttpConversationSerialize;

typedef struct HttpConversation_tag{
  int                conversationType;
  int                serviceCount;
  union HttpConversationSerialize_tag;
  int                shouldError;
  int                httpErrorStatus;
  char              *readBuffer;
  HttpRequestParser *parser;
  HttpServer        *server;
  SocketExtension   *socketExtension;
  SocketExtension   *proxyExtension;     /* This is the "inner" extension */
  int                workingOnResponse;
  HttpService       *pendingService; /* set if working on response */
  WSSession         *wsSession;
  RLETask           *task;
  int                zeroLengthReadCount;
} HttpConversation;

typedef struct HttpWorkElement_tag{
  HttpConversation *conversation;
  HttpRequest      *request;
  HttpResponse     *response;
  int               bufferLength;
  char             *buffer;
  int               reclaimAfterWrite;
  int               wsResponseIsBinary;
  int               wsResponseIsLastFrame;
} HttpWorkElement;

/** registerHttpServerModuleWithBase is the function make an HTTP server get associated to an STCBase
    to allow it to receive events, do network IO and get operator commands.   An HttpServer relies on 
    the STCBase code for its general services.
*/

void registerHttpServerModuleWithBase(HttpServer *server, STCBase *base);


int httpServerSetSessionTokenKey(HttpServer *server, unsigned int size,
                                  unsigned char key[]);

/**
 *  Register an HttpService to an HttpServer.   When the server is called the service function of this service
 *  function of this service will be called.   There are default services provided to provide standard static content.
 */

int registerHttpService(HttpServer *server, HttpService *service);

HttpRequest *dequeueHttpRequest(HttpRequestParser *parser);
HttpRequestParser *makeHttpRequestParser(ShortLivedHeap *slh);
HttpResponse *makeHttpResponse(HttpRequest *request, ShortLivedHeap *slh, Socket *socket);

HttpServer *makeHttpServer2(STCBase *base, InetAddr *ip, int tlsFlags, int port, int *returnCode, int *reasonCode);
HttpServer *makeHttpServer(STCBase *base, int port, int *returnCode, int *reasonCode);

#ifdef USE_RS_SSL
HttpServer *makeSecureHttpServer(STCBase *base, int port,
                                 RS_SSL_ENVIRONMENT rsssl_env,
                                 int *returnCode, int *reasonCode);
#endif

HttpConversation *makeHttpConversation(SocketExtension *socketExtension,
                                       HttpServer *server);

HttpRequestParamSpec *makeIntParamSpec(char *name, int flags, 
				       int minWidth, int maxWidth,
				       int lowBound, int highBound,
				       HttpRequestParamSpec *next);

HttpRequestParamSpec *makeInt64ParamSpec(char *name, int flags, 
                                         int minWidth, int maxWidth,
                                         int64 lowBound, int64 highBound,
                                         HttpRequestParamSpec *next);

HttpRequestParamSpec *makeStringParamSpec(char *name, int flags, 
                                          HttpRequestParamSpec *next);

/**
 *   This is a function to generate a simple static content service that maps part of the URL space
 *   isomorphically to part somewhere in the server's file system. 
 */

HttpService *makeFileService(char *name, char *urlMask, char *pathRoot);
HttpService *makeGeneratedService(char *name, char *urlMask);
HttpService *makeSimpleTemplateService(char *name, char *urlMask, char *templatePath);

#define HTTP_PROXY_OK 0 
#define HTTP_PROXY_BLOCK 1

HttpService *makeProxyService(char *name, 
                              char *urlMask, 
                              int (*requestTransformer)(HttpConversation *conversation,
                                                        HttpRequest *inputRequest,
                                                        HttpRequest *outputRequest));

HttpResponse *pseudoRespond(HttpServer *server, HttpRequest *request, ShortLivedHeap *slh);

char *getQueryParam(HttpRequest *request, char *paramName);
HttpRequestParam *getCheckedParam(HttpRequest *request, char *paramName);

WSMessageHandler *makeWSMessageHandler(void (*h)(WSSession *session,
                                                 WSMessage *message));

WSEndpoint *makeWSEndpoint(int isServerEndpoint,
                           void (*onOpen)(WSSession  *session),
                           void (*onError)(WSSession *session),
                           void (*onClose)(WSSession *session));

HttpService *makeWebSocketService(char *name, char *urlMask, WSEndpoint *endpoint);

jsonPrinter *initWSJsonPrinting(WSSession *session, int maxFrameSize);
void flushWSJsonPrinting(WSSession *session);

void respondWithUnixFile2(HttpService* service, HttpResponse* response, char* absolutePath, int jsonMode, int autocvt, bool asB64);
void respondWithUnixFileContents(HttpResponse* response, char *absolutePath, int jsonMode);
void respondWithUnixFileContents2(HttpService* service, HttpResponse* response, char *absolutePath, int jsonMode);
void respondWithUnixFileContentsWithAutocvtMode(HttpService* service, HttpResponse* response, char *absolutePath, int jsonMode, int convert);

xmlPrinter *respondWithXmlPrinter(HttpResponse *response);

/** 
 * \brief  This function should be called at the beginning of a service function that responds in JSON.
 *
 */

jsonPrinter *respondWithJsonPrinter(HttpResponse *response);
ChunkedOutputStream *respondWithChunkedOutputStream(HttpResponse *response);

/** 
 * \brief finishResponse must be called to ensure full transmission of response to HTTP requestor.
 */

void finishResponse(HttpResponse *response);

/* returns ANSI status indicating whether request processing should proceed.
 * if this returns ANSI_FALSE, we have responded to the browser with a 405
 * response including an Allow header */
int shouldContinueGivenAllowedMethods(HttpResponse *response,
                                      char** nativeMethodList, int numMethods);

void writeString(ChunkedOutputStream *s, char *string);
void writeBytes(ChunkedOutputStream *s, char *data, int len, int translate);

void *getConfiguredProperty(HttpServer *server, char *key);

void setConfiguredProperty(HttpServer *server, char *key, void *value);


/* 
   These are not static in httpserver.c, so presumably they should be declared here. 
 */

void respondWithError(HttpResponse *response, int code, char *message);
void respondWithMessage(HttpResponse *response, int status,
                        const char *messageFormatString, ...);
void setResponseStatus(HttpResponse *response, int status, char *message);

void addHeader(HttpResponse *response, HttpHeader *header);
void addStringHeader(HttpResponse *response, char *name, char *value);
void addIntHeader(HttpResponse *response, char *name, int value);
HttpHeader *getHeader(HttpRequest *request, char *name);
void requestStringHeader(HttpRequest *request, int inEbcdic, char *name, char *value);
void requestIntHeader(HttpRequest *request, int inEbcdic, char *name, int value);
void setContentType(HttpResponse *response, char *contentType);
void writeHeader(HttpResponse *response);
void writeRequest(HttpRequest *request, Socket *socket);
void finishResponse(HttpResponse *response);
int registerHttpService(HttpServer *server, HttpService *service);
int registerHttpServiceOfLastResort(HttpServer *server, HttpService *service);
int processHttpFragment(HttpRequestParser *parser, char *data, int len);
int mainHttpLoop(HttpServer *server);
int streamBinaryForFile(Socket *socket, UnixFile *in, bool asB64);
int streamTextForFile(Socket *socket, UnixFile *in, int encoding,
                      int sourceCCSID, int targetCCSID, bool asB64);
int makeHTMLForDirectory(HttpResponse *response, char *dirname, char *stem, int includeDotted);
int makeJSONForDirectory(HttpResponse *response, char *dirname, int includeDotted);

/**
   Convenience function to set headers specific to sending small JSON objects for a REST API
 */
void setDefaultJSONRESTHeaders(HttpResponse *response);

int setHttpParseTrace(int toWhat);
int setHttpDispatchTrace(int toWhat);
int setHttpHeadersTrace(int toWhat);
int setHttpSocketTrace(int toWhat);
int setHttpCloseConversationTrace(int toWhat);
int setHttpAuthTrace(int toWhat);
#endif

int httpServerInitJwtContext(HttpServer *self,
                             bool legacyFallback,
                             const char *pkcs11TokenName,
                             const char *keyName,
                             int keyType,
                             int *makeContextRc, int *p11Rc, int *p11Rsn);


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

