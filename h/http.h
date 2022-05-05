

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef _COMMON_HTTP_H_
#define _COMMON_HTTP_H_

#define HTTP_STATUS_CONTINUE               100
#define HTTP_STATUS_SWITCHING_PROTOCOLS    101
#define HTTP_STATUS_PROCESSING             102     /* WebDAV is primary user */
#define HTTP_STATUS_OK                     200
#define HTTP_STATUS_CREATED                201
#define HTTP_STATUS_ACCEPTED               202
#define HTTP_STATUS_NO_CONTENT             204
#define HTTP_STATUS_PARTIAL_CONTENT        206
#define HTTP_STATUS_MULITPLE_CHOICES       300
#define HTTP_STATUS_MOVED_PERMANENTLY      301
#define HTTP_STATUS_FOUND                  302
#define HTTP_STATUS_SEE_OTHER              303
#define HTTP_STATUS_NOT_MODIFIED           304
#define HTTP_STATUS_USE_PROXY              305
#define HTTP_STATUS_BAD_REQUEST            400
#define HTTP_STATUS_UNAUTHORIZED           401
#define HTTP_STATUS_PAYMENT_REQUIRED       402
#define HTTP_STATUS_FORBIDDEN              403
#define HTTP_STATUS_NOT_FOUND              404
#define HTTP_STATUS_METHOD_NOT_FOUND       405
#define HTTP_STATUS_NOT_ACCEPTABLE         406
#define HTTP_STATUS_PROXY_AUTH_REQUIRED    407
#define HTTP_STATUS_REQUEST_TIMEOUT        408
#define HTTP_STATUS_RESOURCE_CONFLICT      409
#define HTTP_STATUS_GONE                   410
#define HTTP_STATUS_LENGTH_REQUIRED        411
#define HTTP_STATUS_PRECONDITION_FAILED    412
#define HTTP_STATUS_ENTITY_TOO_LARGE       413
#define HTTP_STATUS_URI_TOO_LONG           414
#define HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE 415
#define HTTP_STATUS_RANGE_NOT_SATISFIABLE  416
#define HTTP_STATUS_EXPECTATION_FAILED     417
#define HTTP_STATUS_PRECONDITION_REQUIRED  428
#define HTTP_STATUS_TOO_MANY_REQUESTS      429
#define HTTP_STATUS_INTERNAL_SERVER_ERROR  500
#define HTTP_STATUS_NOT_IMPLEMENTED        501
#define HTTP_STATUS_BAD_GATEWAY            502
#define HTTP_STATUS_SERVICE_UNAVAILABLE    503
#define HTTP_STATUS_GATEWAY_TIMEOUT        504
#define HTTP_STATUS_VERSION_NOT_SUPPORTED  505

#define MAX_HTTP_METHOD_NAME 256
#define MAX_HTTP_VERSION     256
#define MAX_HTTP_URI         1024
#define MAX_HTTP_FIELD_NAME  1024
#define MAX_HTTP_FIELD_VALUE 16384 /* Apache is 8K, IIS is 16K (hearsay) */
#define MAX_HTTP_CHUNK       2147483647 /* Technically unbounded, but ... */

#define HTTP_REQUEST_MAX_LOCALES 10

#define HTTP_PROTOCOL_1_0 1
#define HTTP_PROTOCOL_1_1 2

#define HTTP_METHOD_GET 1
#define HTTP_METHOD_PUT 2
#define HTTP_METHOD_POST 3
#define HTTP_METHOD_DELETE 4
#define HTTP_METHOD_HEAD 5

#define ENCODING_SIMPLE  1
#define ENCODING_CHUNKED 2
#define ENCODING_GZIP    3

/** HttpHeader is the holder of header data parsed from a request.
    It is held in the HttpRequest structure and has convenience functions for getting
    specific values.
*/
typedef struct HttpHeader_tag{
  char  *name;
  char  *nativeName;
  char  *value;
  char  *nativeValue;
  int    intValue;
  int64  int64Value;
  struct HttpHeader_tag *next;
} HttpHeader;


typedef struct BufferedInputStream_tag{
  Socket *socket;
  char *buffer;
  int bufferSize;
  int fill;
  int pos;
  int eof;
} BufferedInputStream;

typedef struct HttpRequestParamSpec_tag{
  char *name;
  int flags;
  int type;
  int minWidth;
  int maxWidth;
  int lowBound;
  int highBound;
  int64 lowBound64;
  int64 highBound64;
  struct HttpRequestParamSpec_tag *next;
} HttpRequestParamSpec;

typedef struct HttpRequestParam_tag{
  HttpRequestParamSpec *specification;
  int intValue;
  int64 int64Value;
  char *stringValue;
  struct HttpRequestParam_tag *next;
} HttpRequestParam;

typedef struct HttpRequest_tag{
  ShortLivedHeap *slh;
  Socket *socket;
  BufferedInputStream *input;
  int flags;
  int characterEncoding;
  int contentLength; /* -1 if unknown */
  char *contentType;
  char *contentBody;
  int protocol;  /* HTTP/1.1 */
  int localeCount;
  char *acceptLocales[HTTP_REQUEST_MAX_LOCALES];
  char *acceptCharset; /* preferred charset */
  /* Cookie[] cookies; */
  HttpHeader *headerChain;
  char *method;
  char *uri; /* everything after the host:port in browser location */
  char *file; /* rarely NULL */
  StringList *parsedFile;
  char *queryString; /* NULL if no "?" in URL */
  int  queryStringLen;
  char *fragmentIdentifier; /* NULL if no '#' in URL */
  /* Map parameters; */ /* set to Collections.EMPTY_MAP if no params. */
  int authenticated;
  int isWebSocket;
  char *username;
  char *password;
  char *remoteUser; /* null if not authenticated */
  char *requestedSessionId; /* session id from client */
  HttpRequestParam *processedParamList;
  struct HttpRequest_tag *next;
  const void *authToken;   /* a JWT or other */
  int keepAlive;
} HttpRequest;

/*************** WebSocket Stuff ********************/

#define STATE_NO_INPUT   1
#define FINISHING_HEADER 2
#define READING_PAYLOAD  3

#define FIN 0x80

#define OPCODE_CONTINUATION     0x00
#define OPCODE_TEXT_FRAME       0x01
#define OPCODE_BINARY_FRAME     0x02
#define OPCODE_CONNECTION_CLOSE 0x08
#define OPCODE_PING             0x09
#define OPCODE_PONG             0x0A

#define OPCODE_UNKNOWN          0x9999

typedef struct WSFrame_tag{
  int    opcodeAndFlags;
  char  *data;
  int    dataLength;
  struct WSFrame_tag *next;
} WSFrame;

typedef struct WSMessage_tag{
  int                   opcodeAndFlags;
  WSFrame              *firstFrame;
  int                   closeReasonCode;
  char                  data;               /* could be text or binary depending on opcode */
  int                   dataLength;
  struct WSMessage_tag *next;
} WSMessage;

char *copyString(ShortLivedHeap *slh, char *s, int len);
char *copyStringToNative(ShortLivedHeap *slh, char *s, int len);
char* destructivelyNativize(char *s);
void asciify(char *s, int len);

int headerMatch(HttpHeader *header, char *s);

char *toASCIIUTF8(char *buffer, int len);

int writeFully(Socket *socket, char *buffer, int len);

static const char methodGET[] = { 0x47, 0x45, 0x54, 0x00 };
static const char methodPOST[] = { 0x50, 0x4f, 0x53, 0x54, 0x00 };
static const char methodPUT[] = { 0x50, 0x55, 0x54, 0x00 };
static const char methodDELETE[] = { 0x44, 0x45, 0x4c, 0x45, 0x54, 0x45, 0x00};

void requestStringHeader(HttpRequest *request, int inEbcdic, char *name, char *value);
void requestIntHeader(HttpRequest *request, int inEbcdic, char *name, int value);

#endif /* _COMMON_HTTP_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

